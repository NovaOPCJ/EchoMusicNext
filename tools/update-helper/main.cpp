#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#include <sys/event.h>
#else
#include <openssl/evp.h>
#include <poll.h>
#include <sys/syscall.h>
#endif
#endif
namespace fs = std::filesystem;
namespace
{
using Clock = std::chrono::steady_clock;
void mark(const fs::path &directory, const char *name)
{
    std::ofstream file(directory / name, std::ios::binary | std::ios::trunc);
    file << "1\n";
    file.flush();
    if (!file)
        throw std::runtime_error("handoff-write");
}
bool marked(const fs::path &directory, const char *name)
{
    return fs::is_regular_file(directory / name) && !fs::is_symlink(directory / name);
}
class Parent
{
public:
    explicit Parent(unsigned long pid)
    {
#ifdef _WIN32
        m_handle = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!m_handle)
            throw std::runtime_error("parent-open");
#elif defined(__APPLE__)
        m_handle = kqueue();
        struct kevent event;
        EV_SET(&event, pid, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT, 0, nullptr);
        if (m_handle < 0 || kevent(m_handle, &event, 1, nullptr, 0, nullptr) < 0)
            throw std::runtime_error("parent-open");
#else
        m_handle = int(syscall(SYS_pidfd_open, pid, 0));
        if (m_handle < 0)
            throw std::runtime_error("parent-open");
#endif
        if (exited(0))
            throw std::runtime_error("parent-already-exited");
    }
    ~Parent()
    {
#ifdef _WIN32
        if (m_handle)
            CloseHandle(m_handle);
#else
        if (m_handle >= 0)
            close(m_handle);
#endif
    }
    bool exited(int milliseconds)
    {
#ifdef _WIN32
        return WaitForSingleObject(m_handle, DWORD(milliseconds)) == WAIT_OBJECT_0;
#elif defined(__APPLE__)
        struct kevent event;
        timespec timeout{milliseconds / 1000, (milliseconds % 1000) * 1000000L};
        return kevent(m_handle, nullptr, 0, &event, 1, &timeout) > 0;
#else
        pollfd descriptor{m_handle, POLLIN, 0};
        return poll(&descriptor, 1, milliseconds) > 0 && (descriptor.revents & POLLIN);
#endif
    }

private:
#ifdef _WIN32
    HANDLE m_handle = nullptr;
#else
    int m_handle = -1;
#endif
};
std::string digest(const fs::path &file)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
        throw std::runtime_error("package-open");
    std::array<unsigned char, 64> result{};
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    ULONG objectSize = 0, returned = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA512_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("hash-init");
    auto cleanup = [&] {
        if (hash)
            BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
    };
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                          sizeof(objectSize), &returned, 0) < 0) {
        cleanup();
        throw std::runtime_error("hash-init");
    }
    std::vector<unsigned char> object(objectSize);
    if (BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) < 0) {
        cleanup();
        throw std::runtime_error("hash-init");
    }
#elif defined(__APPLE__)
    CC_SHA512_CTX hash;
    CC_SHA512_Init(&hash);
#else
    auto *hash = EVP_MD_CTX_new();
    if (!hash || EVP_DigestInit_ex(hash, EVP_sha512(), nullptr) != 1) {
        EVP_MD_CTX_free(hash);
        throw std::runtime_error("hash-init");
    }
#endif
    std::array<char, 256 * 1024> buffer;
    bool ok = true;
    while (stream) {
        stream.read(buffer.data(), buffer.size());
        const auto size = stream.gcount();
        if (!size)
            break;
#ifdef _WIN32
        ok &= BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), ULONG(size), 0) >= 0;
#elif defined(__APPLE__)
        ok &= CC_SHA512_Update(&hash, buffer.data(), CC_LONG(size)) == 1;
#else
        ok &= EVP_DigestUpdate(hash, buffer.data(), size_t(size)) == 1;
#endif
    }
#ifdef _WIN32
    ok &= BCryptFinishHash(hash, result.data(), ULONG(result.size()), 0) >= 0;
    cleanup();
#elif defined(__APPLE__)
    ok &= CC_SHA512_Final(result.data(), &hash) == 1;
#else
    unsigned int length = 0;
    ok &= EVP_DigestFinal_ex(hash, result.data(), &length) == 1 && length == result.size();
    EVP_MD_CTX_free(hash);
#endif
    if (!ok || stream.bad())
        throw std::runtime_error("hash-read");
    static constexpr char hex[] = "0123456789abcdef";
    std::string value;
    for (auto byte : result) {
        value += hex[byte >> 4];
        value += hex[byte & 15];
    }
    return value;
}
#ifndef _WIN32
bool spawn(const fs::path &program, const std::vector<std::string> &arguments, bool wait)
{
    std::vector<std::string> values{program.string()};
    values.insert(values.end(), arguments.begin(), arguments.end());
    std::vector<char *> pointers;
    for (auto &value : values)
        pointers.push_back(value.data());
    pointers.push_back(nullptr);
    pid_t pid = 0;
    const auto error =
        posix_spawn(&pid, program.c_str(), nullptr, nullptr, pointers.data(), environ);
    if (error)
        return false;
    if (!wait)
        return true;
    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
        if (errno != EINTR)
            return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
#endif
bool apply(const std::string &kind, const fs::path &package, const fs::path &target,
           const fs::path &handoff, const std::string &expectedHash, bool silent)
{
#ifdef ECHO_UPDATE_HELPER_TESTING
    if (kind == "probe") {
        mark(handoff, "installed");
        return true;
    }
#endif
#ifdef _WIN32
    if (kind != "nsis")
        return false;
    auto parameters = (silent ? std::wstring(L"/S ") : std::wstring()) + L"/D=" +
                      target.parent_path().parent_path().wstring();
    SHELLEXECUTEINFOW execute{};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    execute.lpVerb = L"open";
    execute.lpFile = package.c_str();
    execute.lpParameters = parameters.c_str();
    execute.nShow = SW_SHOWNORMAL;
    bool ok = ShellExecuteExW(&execute) && execute.hProcess;
    if (ok) {
        WaitForSingleObject(execute.hProcess, INFINITE);
        DWORD code = 1;
        ok = GetExitCodeProcess(execute.hProcess, &code) && code == 0;
        CloseHandle(execute.hProcess);
    }
    mark(handoff, ok ? "finished" : "failed");
    SHELLEXECUTEINFOW restart{};
    restart.cbSize = sizeof(restart);
    restart.lpVerb = L"open";
    restart.lpFile = target.c_str();
    restart.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&restart) && ok;
#else
    (void)silent;
    if (kind == "appimage") {
#if defined(__APPLE__) && !defined(ECHO_UPDATE_HELPER_TESTING)
        return false;
#else
        const auto next = fs::path(target.string() + ".update-" + handoff.filename().string());
        const auto previous =
            fs::path(target.string() + ".previous-" + handoff.filename().string());
        fs::copy_file(package, next);
        if (digest(next) != expectedHash) {
            fs::remove(next);
            return false;
        }
        fs::permissions(next, fs::status(target).permissions() | fs::perms::owner_exec);
        fs::rename(target, previous);
        try {
            fs::rename(next, target);
        } catch (...) {
            fs::rename(previous, target);
            fs::remove(next);
            throw;
        }
        mark(handoff, "finished");
        if (!spawn(target, {}, false)) {
            fs::remove(target);
            fs::rename(previous, target);
            fs::remove(handoff / "finished");
            mark(handoff, "failed");
            spawn(target, {}, false);
            return false;
        }
        fs::remove(previous);
        return true;
#endif
    }
#ifdef __linux__
    std::vector<std::string> args;
    if (kind == "deb")
        args = {"/usr/bin/dpkg", "--install", package.string()};
    else if (kind == "rpm")
        args = {"/usr/bin/rpm", "--upgrade", "--replacepkgs", package.string()};
    else if (kind == "pacman")
        args = {"/usr/bin/pacman", "--upgrade", "--noconfirm", package.string()};
    else
        return false;
    const bool ok = spawn("/usr/bin/pkexec", args, true);
    mark(handoff, ok ? "finished" : "failed");
    const bool started = spawn(target, {}, false);
    return ok && started;
#else
    return false;
#endif
#endif
}
int run(const std::vector<std::string> &arguments)
{
    fs::path handoff;
    try {
        if (arguments.size() != 15)
            return 2;
        std::map<std::string, std::string> options;
        for (size_t i = 1; i + 1 < arguments.size(); i += 2)
            if (!options.emplace(arguments[i], arguments[i + 1]).second ||
                arguments[i + 1].size() > 4096)
                return 2;
        if (options.size() != 7 || !options.contains("--parent") ||
            !options.contains("--handoff") || !options.contains("--package") ||
            !options.contains("--sha512") || !options.contains("--kind") ||
            !options.contains("--target") || !options.contains("--silent"))
            return 2;
        const auto rawHandoff = fs::u8path(options.at("--handoff"));
        const auto rawPackage = fs::u8path(options.at("--package"));
        const auto rawTarget = fs::u8path(options.at("--target"));
        if (!rawHandoff.is_absolute() || !rawPackage.is_absolute() || !rawTarget.is_absolute() ||
            fs::is_symlink(rawHandoff) || fs::is_symlink(rawPackage) ||
            !fs::is_regular_file(rawPackage) || !fs::is_regular_file(rawTarget))
            return 2;
        handoff = fs::canonical(rawHandoff);
        const auto package = fs::canonical(rawPackage), target = fs::canonical(rawTarget);
        const auto expected = options.at("--sha512");
        const auto kind = options.at("--kind");
        bool supported = kind == "nsis" || kind == "appimage" || kind == "deb" || kind == "rpm" ||
                         kind == "pacman";
#ifdef ECHO_UPDATE_HELPER_TESTING
        supported |= kind == "probe";
#endif
        if (!supported || package.parent_path() != handoff.parent_path() ||
            expected.size() != 128 ||
            !std::all_of(expected.begin(), expected.end(),
                         [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }) ||
            (options.at("--silent") != "0" && options.at("--silent") != "1"))
            return 2;
        const auto parentId = options.at("--parent");
        if (parentId.empty() || !std::all_of(parentId.begin(), parentId.end(),
                                             [](char c) { return c >= '0' && c <= '9'; }))
            return 2;
        const auto id = std::stoul(parentId);
        if (!id || id > 0x7fffffffUL)
            return 2;
        Parent parent(id);
        if (digest(package) != expected)
            throw std::runtime_error("package-hash");
        for (const auto *name : {"ready", "commit", "committed", "quit-authorized"})
            if (fs::exists(handoff / name))
                throw std::runtime_error("handoff-reused");
        mark(handoff, "ready");
        const auto deadline = Clock::now() + std::chrono::seconds(10);
        while (!marked(handoff, "commit")) {
            if (Clock::now() > deadline || parent.exited(0))
                throw std::runtime_error("handoff-cancelled");
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (digest(package) != expected)
            throw std::runtime_error("package-changed");
        mark(handoff, "committed");
        if (!parent.exited(60000))
            throw std::runtime_error("parent-still-running");
        if (!marked(handoff, "quit-authorized"))
            throw std::runtime_error("quit-not-authorized");
        if (digest(package) != expected)
            throw std::runtime_error("package-changed");
        const bool ok =
            apply(kind, package, target, handoff, expected, options.at("--silent") == "1");
        mark(handoff, ok ? "finished" : "failed");
        return ok ? 0 : 1;
    } catch (const std::exception &error) {
        if (!handoff.empty()) {
            try {
                mark(handoff, "failed");
                std::ofstream(handoff / "error") << error.what();
            } catch (...) {
            }
        }
        return 1;
    }
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv)
{
    std::vector<std::string> arguments;
    for (int i = 0; i < argc; ++i) {
        const auto size =
            WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string value(size_t(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, value.data(), size, nullptr, nullptr);
        value.pop_back();
        arguments.push_back(std::move(value));
    }
    return run(arguments);
}
#else
int main(int argc, char **argv)
{
    return run(std::vector<std::string>(argv, argv + argc));
}
#endif
