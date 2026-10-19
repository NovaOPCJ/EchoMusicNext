#include "LinuxSystemProxy.h"
#include <proxy.h>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>

namespace EchoMusic
{
namespace
{
class SystemProxyWorker
{
public:
    SystemProxyWorker() : m_thread([this] { run(); }) {}
    ~SystemProxyWorker()
    {
        {
            std::lock_guard lock(m_mutex);
            m_closing = true;
        }
        m_available.notify_one();
        m_thread.join();
    }
    ProxyResolution resolve(const QUrl &url, ProxyResolveCancellation cancelled)
    {
        auto job = std::make_shared<Job>();
        job->url = url.toEncoded();
        job->cancelled = cancelled;
        auto result = job->result.get_future();
        {
            std::lock_guard lock(m_mutex);
            if (m_closing)
                return {{}, QObject::tr("系统代理服务已关闭")};
            std::erase_if(m_jobs, [](const auto &queued) {
                return queued->cancelled && queued->cancelled->load(std::memory_order_acquire);
            });
            if (m_jobs.size() >= 128)
                return {{}, QObject::tr("系统代理解析繁忙")};
            m_jobs.push_back(job);
        }
        m_available.notify_one();
        while (result.wait_for(std::chrono::milliseconds(20)) != std::future_status::ready)
            if (cancelled && cancelled->load(std::memory_order_acquire))
                return {{}, QObject::tr("代理解析已取消")};
        if (cancelled && cancelled->load(std::memory_order_acquire))
            return {{}, QObject::tr("代理解析已取消")};
        return result.get();
    }

private:
    struct Job
    {
        QByteArray url;
        ProxyResolveCancellation cancelled;
        std::promise<ProxyResolution> result;
    };
    void run()
    {
        // libproxy 0.4 can embed thread-affine JS engines. All creation, calls
        // and destruction stay on one non-Glib worker, not just behind a mutex.
        auto *factory = px_proxy_factory_new();
        while (true) {
            std::shared_ptr<Job> job;
            {
                std::unique_lock lock(m_mutex);
                m_available.wait(lock, [this] { return m_closing || !m_jobs.empty(); });
                if (m_closing && m_jobs.empty())
                    break;
                job = m_jobs.front();
                m_jobs.pop_front();
            }
            ProxyResolution result;
            if (factory && (!job->cancelled || !job->cancelled->load(std::memory_order_acquire))) {
                auto **proxies = px_proxy_factory_get_proxies(factory, job->url.constData());
                if (proxies) {
                    for (int index = 0; proxies[index]; ++index) {
                        auto endpoint = QUrl::fromEncoded(proxies[index]);
                        // The application's secure credential authority owns
                        // authentication; never propagate credential URLs.
                        endpoint.setUserInfo({});
                        const auto parsed = NetworkConfiguration::parseProxyRules(
                            endpoint.toString(QUrl::FullyEncoded));
                        if (parsed && parsed->size() == 1)
                            result.endpoints += parsed->first().endpoints;
                    }
                    px_proxy_factory_free_proxies(proxies);
                }
            }
            if (result.endpoints.isEmpty())
                result.error = QObject::tr("系统没有返回可用的代理");
            job->result.set_value(std::move(result));
        }
        if (factory)
            px_proxy_factory_free(factory);
    }
    std::mutex m_mutex;
    std::condition_variable m_available;
    std::deque<std::shared_ptr<Job>> m_jobs;
    bool m_closing = false;
    std::thread m_thread;
};
} // namespace
ProxyResolution resolveLinuxSystemProxy(const QUrl &url, ProxyResolveCancellation cancelled)
{
    static SystemProxyWorker worker;
    return worker.resolve(url, std::move(cancelled));
}
} // namespace EchoMusic
