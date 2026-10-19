#include "ReleaseManifest.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QSysInfo>
#include <algorithm>
namespace EchoMusic::ReleaseManifest
{
namespace
{
const QString releaseRoot = "https://github.com/hoowhoami/EchoMusicNext/releases/";
const QRegularExpression semver(
    R"(^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$)");
bool numeric(const QString &s)
{
    return !s.isEmpty() &&
           std::all_of(s.cbegin(), s.cend(), [](QChar c) { return c >= u'0' && c <= u'9'; });
}
int numberCompare(const QString &a, const QString &b)
{
    return a.size() == b.size() ? QString::compare(a, b, Qt::CaseSensitive)
                                : (a.size() < b.size() ? -1 : 1);
}
bool sameUrl(const QUrl &url, const QString &expected)
{
    return url == QUrl(expected) && url.userInfo().isEmpty() && !url.hasQuery() &&
           !url.hasFragment();
}
} // namespace
QString platform()
{
#ifdef Q_OS_WIN
    return "Windows";
#elif defined(Q_OS_MACOS)
    return "macOS";
#else
    return "Linux";
#endif
}
QString architecture()
{
    const auto arch = QSysInfo::buildCpuArchitecture();
    return arch == "arm64" || arch == "aarch64" ? "arm64" : arch == "x86_64" ? "x64" : arch;
}
QString manifestName(const QString &os, const QString &arch)
{
    return "update-" + os + '-' + arch + ".json";
}
bool validVersion(const QString &version)
{
    const auto match = semver.match(version);
    if (!match.hasMatch())
        return false;
    for (const auto &part : match.captured(4).split('.'))
        if (numeric(part) && part.size() > 1 && part.startsWith('0'))
            return false;
    return true;
}
int compareVersions(const QString &a, const QString &b)
{
    const auto left = semver.match(a), right = semver.match(b);
    for (int i = 1; i <= 3; ++i)
        if (const auto result = numberCompare(left.captured(i), right.captured(i)); result)
            return result;
    const auto l = left.captured(4), r = right.captured(4);
    if (l.isEmpty() || r.isEmpty())
        return l.isEmpty() == r.isEmpty() ? 0 : l.isEmpty() ? 1 : -1;
    const auto lp = l.split('.'), rp = r.split('.');
    for (qsizetype i = 0; i < qMin(lp.size(), rp.size()); ++i) {
        const bool ln = numeric(lp[i]), rn = numeric(rp[i]);
        const auto result = ln && rn   ? numberCompare(lp[i], rp[i])
                            : ln != rn ? (ln ? -1 : 1)
                                       : QString::compare(lp[i], rp[i], Qt::CaseSensitive);
        if (result)
            return result;
    }
    return lp.size() == rp.size() ? 0 : lp.size() < rp.size() ? -1 : 1;
}
QList<PublishedRelease> releases(const QByteArray &bytes, const QString &os, const QString &arch,
                                 QString *error)
{
    error->clear();
    QJsonParseError parse;
    const auto json = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError || !json.isArray()) {
        *error = QStringLiteral("无法读取 GitHub 发布列表");
        return {};
    }
    QList<PublishedRelease> result;
    for (const auto &value : json.array()) {
        const auto item = value.toObject();
        if (item["draft"].toBool())
            continue;
        const auto tag = item["tag_name"].toString();
        const auto version = tag.startsWith('v') ? tag.mid(1) : QString();
        if (!validVersion(version) ||
            !sameUrl(QUrl(item["html_url"].toString()), releaseRoot + "tag/" + tag))
            continue;
        PublishedRelease release{version,
                                 item["body"].toString(),
                                 QUrl(item["html_url"].toString()),
                                 {},
                                 item["prerelease"].toBool()};
        for (const auto &file : item["assets"].toArray()) {
            const auto asset = file.toObject();
            if (asset["name"].toString() != manifestName(os, arch))
                continue;
            const auto url = QUrl(asset["browser_download_url"].toString());
            if (sameUrl(url, releaseRoot + "download/" + tag + '/' + manifestName(os, arch)))
                release.manifestUrl = url;
        }
        result.append(release);
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return compareVersions(a.version, b.version) > 0;
    });
    return result;
}
QList<UpdateAsset> assets(const QByteArray &bytes, const PublishedRelease &release,
                          const QString &os, const QString &arch, QString *error)
{
    error->clear();
    QJsonParseError parse;
    const auto json = QJsonDocument::fromJson(bytes, &parse);
    const auto item = json.object();
    if (parse.error != QJsonParseError::NoError || !json.isObject() ||
        item["schema"].toInt() != 1 ||
        item["applicationId"] != "io.github.hoowhoami.echomusicnext" ||
        item["version"] != release.version || item["platform"] != os || item["arch"] != arch) {
        *error = QStringLiteral("更新信息与当前应用或平台不匹配");
        return {};
    }
    const QMap<QString, QString> extensions{
        {"nsis", ".exe"},           {"dmg", ".dmg"},   {"zip", ".zip"},
        {"appimage", ".AppImage"},  {"deb", ".deb"},   {"rpm", ".rpm"},
        {"pacman", ".pkg.tar.zst"}, {"tar", ".tar.gz"}};
    const QSet<QString> allowed = os == "Windows" ? QSet<QString>{"nsis"}
                                  : os == "macOS" ? QSet<QString>{"dmg", "zip"}
                                  : os == "Linux"
                                      ? QSet<QString>{"appimage", "deb", "rpm", "pacman", "tar"}
                                      : QSet<QString>{};
    QList<UpdateAsset> result;
    QSet<QString> kinds;
    for (const auto &value : item["assets"].toArray()) {
        const auto asset = value.toObject();
        const auto name = asset["name"].toString(), kind = asset["kind"].toString();
        const auto hash = asset["sha512"].toString().toLatin1();
        const auto size = asset["size"].toDouble(-1);
        const auto expected = "EchoMusicNext-" + release.version + '-' + os + '-' + arch +
                              (kind == "nsis" ? "-Setup" : "") + extensions.value(kind);
        if (!allowed.contains(kind) || kinds.contains(kind) || name != expected || size <= 0 ||
            size > 4.0 * 1024 * 1024 * 1024 || size != qint64(size) ||
            !QRegularExpression("^[0-9a-f]{128}$").match(QString::fromLatin1(hash)).hasMatch()) {
            *error = QStringLiteral("更新文件信息无效");
            return {};
        }
        kinds.insert(kind);
        result.append({name, kind, qint64(size), hash,
                       QUrl(releaseRoot + "download/v" + release.version + '/' + name)});
    }
    if (result.isEmpty())
        *error = QStringLiteral("更新信息中没有可用安装包");
    return result;
}
} // namespace EchoMusic::ReleaseManifest
