#include "AboutService.h"
#include <QDesktopServices>
#include <QFile>
#include <QRegularExpression>
#include <QTextDocument>
#include <QVersionNumber>
#include <QXmlStreamReader>
namespace EchoMusic
{
namespace
{
const QUrl repository(QStringLiteral("https://github.com/hoowhoami/EchoMusicNext"));
const QUrl feed(repository.toString() + QStringLiteral("/releases.atom"));
constexpr qsizetype feedLimit = 1024 * 1024;
} // namespace
AboutService::AboutService(QSettings &settings, NetworkPreferences &network, QObject *parent)
    : QObject(parent), m_settings(settings), m_policy(network), m_network(this),
      m_autoCheck(settings.value("updates/autoCheck", true).toBool()),
      m_prerelease(settings.value("updates/prerelease", false).toBool()),
      m_silent(settings.value("updates/silent", true).toBool())
{
}
QString AboutService::version() const
{
    return QStringLiteral(ECHO_APP_VERSION);
}
QString AboutService::changelog() const
{
    // Current-version notes are a bundled asset, independent of connectivity or
    // the newer release being checked. Never substitute the remote feed here.
    QFile file(QStringLiteral(":/settings/CHANGELOG.md"));
    if (!file.open(QIODevice::ReadOnly))
        return tr("无法读取更新日志");
    if (file.size() > 1024 * 1024)
        return tr("更新日志超过大小限制");
    const auto text = QString::fromUtf8(file.readAll()).trimmed();
    return text.isEmpty() ? tr("暂无更新日志") : text;
}
AboutService::~AboutService()
{
    shutdown();
}
void AboutService::setAutoCheck(bool enabled)
{
    if (m_autoCheck == enabled)
        return;
    m_autoCheck = enabled;
    m_settings.setValue("updates/autoCheck", enabled);
    emit changed();
}
void AboutService::setPrerelease(bool enabled)
{
    if (m_prerelease == enabled)
        return;
    m_prerelease = enabled;
    m_settings.setValue("updates/prerelease", enabled);
    ++m_revision;
    if (m_reply)
        m_reply->abort();
    m_reply = nullptr;
    m_checking = false;
    m_release = {};
    m_status.clear();
    m_resultOpen = false;
    m_resultState = NoResult;
    emit changed();
}
void AboutService::setSilentUpdate(bool enabled)
{
    if (m_silent == enabled)
        return;
    m_silent = enabled;
    m_settings.setValue("updates/silent", enabled);
    emit changed();
}
void AboutService::check(bool automatic)
{
    if (m_closed || checking())
        return;
    m_checking = true;
    m_status.clear();
    const auto target = m_policy.acceleratedGithubUrl(feed);
    fetch(target, target != feed, ++m_revision, automatic);
    emit changed();
}
QList<PublishedRelease> AboutService::parseFeed(const QByteArray &xml, QString *error)
{
    QList<PublishedRelease> releases;
    if (xml.size() > feedLimit) {
        *error = tr("更新响应超过大小限制");
        return {};
    }
    QXmlStreamReader reader(xml);
    bool atom = false;
    static const QRegularExpression tag(
        "^v?([0-9]+\\.[0-9]+\\.[0-9]+(?:-[0-9A-Za-z.-]+)?(?:\\+[0-9A-Za-z.-]+)?)$");
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isDTD()) {
            *error = tr("更新响应包含不支持的文档声明");
            return {};
        }
        if (!reader.isStartElement())
            continue;
        if (reader.name() == "feed") {
            atom = reader.namespaceUri() == "http://www.w3.org/2005/Atom";
            if (!atom)
                break;
        } else if (atom && reader.name() == "entry") {
            if (releases.size() >= 100) {
                *error = tr("更新列表超过数量限制");
                return {};
            }
            PublishedRelease release;
            while (reader.readNextStartElement()) {
                if (reader.name() == "link" && reader.attributes().value("rel") == "alternate") {
                    release.url = QUrl(reader.attributes().value("href").toString());
                    reader.skipCurrentElement();
                } else if (reader.name() == "content") {
                    QTextDocument notes;
                    notes.setHtml(
                        reader.readElementText(QXmlStreamReader::ErrorOnUnexpectedElement));
                    release.notes = notes.toMarkdown().trimmed();
                } else
                    reader.skipCurrentElement();
            }
            const auto url = release.url;
            const QString prefix = "/hoowhoami/EchoMusicNext/releases/tag/";
            if (url.scheme() != "https" || url.host() != "github.com" ||
                !url.userInfo().isEmpty() || url.port() != -1 || !url.path().startsWith(prefix) ||
                url.hasQuery() || url.hasFragment())
                continue;
            const auto match = tag.match(url.path().mid(prefix.size()));
            if (match.hasMatch()) {
                release.version = match.captured(1);
                releases.append(release);
            }
        }
    }
    if (reader.hasError() || !atom) {
        *error = tr("无法读取 GitHub 发布列表");
        return {};
    }
    error->clear();
    return releases;
}
void AboutService::fetch(const QUrl &url, bool fallback, quint64 revision, bool automatic)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(20000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", "EchoMusicNext/" ECHO_APP_VERSION);
    request.setRawHeader("Accept", "application/atom+xml,application/xml");
    auto reply = m_network.get(request);
    m_reply = reply;
    reply->setReadBufferSize(64 * 1024);
    auto bytes = std::make_shared<QByteArray>();
    connect(reply, &QIODevice::readyRead, this, [reply, bytes] {
        bytes->append(reply->read(feedLimit - bytes->size() + 1));
        if (bytes->size() > feedLimit)
            reply->abort();
    });
    connect(
        reply, &QNetworkReply::finished, this, [this, reply, bytes, fallback, revision, automatic] {
            if (reply->isOpen())
                bytes->append(reply->read(feedLimit - bytes->size() + 1));
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool ok = reply->error() == QNetworkReply::NoError && status == 200 &&
                            bytes->size() <= feedLimit;
            reply->deleteLater();
            if (revision != m_revision || m_closed)
                return;
            m_reply = nullptr;
            if (!ok && fallback) {
                fetch(feed, false, revision, automatic);
                return;
            }
            QString error;
            const auto releases = ok ? parseFeed(*bytes, &error) : QList<PublishedRelease>{};
            if (!ok)
                error = bytes->size() > feedLimit ? tr("更新响应超过大小限制")
                                                  : tr("更新检查失败（HTTP %1）").arg(status);
            m_release = {};
            if (error.isEmpty()) {
                for (const auto &release : releases) {
                    if (!m_prerelease && release.version.section('+', 0, 0).contains('-'))
                        continue;
                    m_release = release;
                    break;
                }
                if (m_release.version.isEmpty()) {
                    m_status =
                        tr("当前还没有已发布的%1版本").arg(m_prerelease ? QString() : tr("正式"));
                    m_resultState = NoPublishedRelease;
                } else if (QVersionNumber::fromString(m_release.version) >
                           QVersionNumber::fromString(version())) {
                    m_status = tr("发现新版本 v%1").arg(m_release.version);
                    m_resultState = UpdateAvailable;
                    emit updateAvailable();
                } else {
                    m_status = tr("当前已是最新版本");
                    m_resultState = AlreadyLatest;
                }
            } else {
                m_status = error;
                m_resultState = CheckFailed;
            }
            m_checking = false;
            m_resultOpen = !automatic || m_resultState == UpdateAvailable;
            emit changed();
        });
}
void AboutService::openRepository()
{
    QDesktopServices::openUrl(repository);
}
void AboutService::openRelease()
{
    if (!m_release.url.isEmpty())
        QDesktopServices::openUrl(m_release.url);
}
void AboutService::shutdown()
{
    m_closed = true;
    ++m_revision;
    if (m_reply)
        m_reply->abort();
    m_reply = nullptr;
    m_checking = false;
    m_resultOpen = false;
}
} // namespace EchoMusic
