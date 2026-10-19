#include "LogService.h"
#include "echo_ffi.h"
#include <QStandardPaths>
#include <QDebug>
#include <QScopedValueRollback>
#include <QLoggingCategory>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonValue>
#include <atomic>
#include <deque>
#include <cstdio>

namespace EchoMusic
{
namespace
{
constexpr size_t capacity = 512;
constexpr qint64 fileLimit = 5 * 1024 * 1024;
constexpr int retainedFiles = 7;
const QStringList levels{QStringLiteral("error"), QStringLiteral("warn"), QStringLiteral("info"),
                         QStringLiteral("debug"), QStringLiteral("verbose")};
std::unique_ptr<QSettings> accessor(const QSettings &settings)
{
    auto result =
        settings.organizationName().isEmpty() && settings.applicationName().isEmpty()
            ? std::make_unique<QSettings>(settings.fileName(), settings.format())
            : std::make_unique<QSettings>(settings.format(), settings.scope(),
                                          settings.organizationName(), settings.applicationName());
    result->setFallbacksEnabled(settings.fallbacksEnabled());
    result->beginGroup(settings.group());
    return result;
}
bool sensitiveKey(const QString &key)
{
    static const QStringList fragments{
        "token",      "cookie",     "authorization", "password",  "passwd",  "secret",
        "ticket",     "randstr",    "proof",         "secu",      "ssecode", "qrsig",
        "access_key", "signature",  "mobile",        "phone",     "email",   "key",
        "pwd",        "verifycode", "qrcode",        "login_sig", "h5sig",   "fingerprint",
        "dev",        "username",   "dfid",          "mid",       "uuid",    "guid",
        "mac"};
    const auto lower = key.toLower();
    for (const auto &fragment : fragments)
        if (lower.contains(fragment))
            return true;
    return QStringList{"t1", "mid", "dfid", "uuid", "guid", "mac", "sid", "edt", "dev", "serverdev"}
        .contains(lower);
}
QJsonValue safeJson(const QJsonValue &value, int depth = 0)
{
    if (depth >= 8)
        return QStringLiteral("[truncated]");
    if (value.isObject()) {
        QJsonObject result;
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            result.insert(it.key(), sensitiveKey(it.key()) ? QJsonValue("[redacted]")
                                                           : safeJson(it.value(), depth + 1));
        return result;
    }
    if (value.isArray()) {
        QJsonArray result;
        for (const auto &entry : value.toArray())
            result.append(safeJson(entry, depth + 1));
        return result;
    }
    if (value.isString() && (value.toString().contains("://") ||
                             value.toString().startsWith("Bearer ", Qt::CaseInsensitive)))
        return QStringLiteral("[redacted address or credential]");
    return value;
}
QString safeMessage(const QString &message)
{
    // Native API diagnostics redact and bound bodies before enqueueing. Apply
    // the same field protection to structured Qt/QML logs, and reject labelled
    // credentials/signed URLs in free text. Unlabelled secrets cannot be inferred;
    // callers must never log raw credentials in the first place.
    if (message.toUtf8().size() > 8192)
        return QStringLiteral("[oversized log omitted]");
    const auto structured = QJsonDocument::fromJson(message.toUtf8());
    if (structured.isObject())
        return QString::fromUtf8(
            QJsonDocument(safeJson(structured.object()).toObject()).toJson(QJsonDocument::Compact));
    if (structured.isArray())
        return QString::fromUtf8(
            QJsonDocument(safeJson(structured.array()).toArray()).toJson(QJsonDocument::Compact));
    static const QStringList keys{
        QStringLiteral("token"),       QStringLiteral("cookie"),  QStringLiteral("authorization"),
        QStringLiteral("password"),    QStringLiteral("passwd"),  QStringLiteral("secret"),
        QStringLiteral("ticket"),      QStringLiteral("randstr"), QStringLiteral("proof"),
        QStringLiteral("secu_params"), QStringLiteral("ssecode"), QStringLiteral("qrsig"),
        QStringLiteral("access_key"),  QStringLiteral("dfid"),    QStringLiteral("device_id"),
        QStringLiteral("username"),    QStringLiteral("mobile"),  QStringLiteral("email")};
    for (const auto &key : keys)
        if (message.contains(key, Qt::CaseInsensitive))
            return QStringLiteral("[redacted sensitive log]");
    if ((message.contains("https://", Qt::CaseInsensitive) ||
         message.contains("http://", Qt::CaseInsensitive)) &&
        message.contains('?'))
        return QStringLiteral("[redacted sensitive log]");
    return message;
}
uint32_t logLevel(QtMsgType type)
{
    return type == QtDebugMsg                              ? 3
           : type == QtWarningMsg                          ? 1
           : (type == QtCriticalMsg || type == QtFatalMsg) ? 0
                                                           : 2;
}
const char *levelName(uint32_t level)
{
    return level == 0 ? "error" : level == 1 ? "warn" : level == 3 ? "debug" : "info";
}
struct Record
{
    qint64 time;
    uint32_t level;
    QString target;
    QString message;
};
} // namespace
// The QThread object is host-owned, not used as a worker QObject. Only run()
// touches QFile/console; producers use a bounded try-lock queue and wake it.
class LogSink final : public QThread
{
public:
    LogSink(QString directory, int level, bool console)
        : m_directory(std::move(directory)), m_level(level), m_console(console)
    {
    }
    void setLevel(int level, qint64 diagnosticUntil)
    {
        m_diagnosticUntil.store(diagnosticUntil, std::memory_order_relaxed);
        m_level.store(level, std::memory_order_relaxed);
    }
    void enqueue(uint32_t level, const QString &target, const QString &message, qint64 time = 0)
    {
        const auto now = QDateTime::currentMSecsSinceEpoch();
        const auto base = m_level.load(std::memory_order_relaxed);
        // The GUI's countdown timer may be delayed by a busy window. Enforce
        // expiry at ingestion as well, so diagnostics never extend past the
        // saved deadline merely because its next UI timer tick is pending.
        const auto threshold =
            now < m_diagnosticUntil.load(std::memory_order_relaxed) ? qMax(3, base) : base;
        if (int(level) > threshold)
            return;
        if (!m_mutex.tryLock()) {
            m_dropped.fetch_add(1);
            return;
        }
        if (!m_stopping && m_queue.size() < capacity) {
            m_queue.push_back(
                {time ? time : QDateTime::currentMSecsSinceEpoch(), level, target.left(80),
                 message.size() > 8192 ? QStringLiteral("[oversized log omitted]") : message});
            m_ready.wakeOne();
        } else {
            m_dropped.fetch_add(1);
        }
        m_mutex.unlock();
    }
    void finish()
    {
        {
            QMutexLocker lock(&m_mutex);
            m_stopping = true;
            m_ready.wakeOne();
        }
        wait(); // only after the application event loop/services have stopped
    }

protected:
    void run() override
    {
        QDir directory(m_directory);
        bool fileAlive = directory.mkpath(QStringLiteral("."));
        if (fileAlive) {
            // Seven-day retention, matching the reference. Each instance owns
            // its PID-named active file so concurrent launches do not rotate it.
            const auto files =
                directory.entryInfoList({QStringLiteral("echomusicnext-*.log")}, QDir::Files);
            for (const auto &file : files)
                if (file.lastModified().secsTo(QDateTime::currentDateTime()) > 7 * 86400)
                    QFile::remove(file.absoluteFilePath());
        }
        const QString path = directory.filePath(
            QStringLiteral("echomusicnext-%1.log").arg(QCoreApplication::applicationPid()));
        QFile file(path);
        fileAlive = fileAlive && file.open(QIODevice::WriteOnly | QIODevice::Append);
        bool consoleAlive = m_console;
        const auto failure = [&](const char *text) {
            if (consoleAlive) {
                if (std::fputs(text, stderr) < 0)
                    consoleAlive = false;
                std::fflush(stderr);
            }
        };
        if (!fileAlive)
            failure("EchoMusicNext: log file unavailable; console logging continues\n");
        const auto write = [&](const Record &record) {
            const auto stamp =
                QDateTime::fromMSecsSinceEpoch(record.time).toUTC().toString(Qt::ISODateWithMs);
            auto bytes =
                QJsonDocument(QJsonObject{{"timestamp", stamp},
                                          {"pid", QCoreApplication::applicationPid()},
                                          {"level", QString::fromLatin1(levelName(record.level))},
                                          {"target", record.target},
                                          {"message", safeMessage(record.message)}})
                    .toJson(QJsonDocument::Compact);
            bytes.append('\n');
            if (fileAlive && file.size() > 0 && file.size() + bytes.size() > fileLimit) {
                file.flush();
                // Backups are closed. Copy/truncate the active file on this
                // thread rather than rename an open handle on Windows.
                const auto segment = [&](int n) {
                    return path.left(path.size() - 4) + QStringLiteral(".%1.log").arg(n);
                };
                QFile::remove(segment(retainedFiles - 1));
                for (int n = retainedFiles - 2; n >= 1; --n) {
                    if (QFile::exists(segment(n)) && !QFile::rename(segment(n), segment(n + 1)))
                        fileAlive = false;
                }
                if (fileAlive &&
                    (!QFile::copy(path, segment(1)) || !file.resize(0) || !file.seek(0)))
                    fileAlive = false;
                if (!fileAlive)
                    failure("EchoMusicNext: log rotation failed; console logging continues\n");
            }
            if (fileAlive && file.write(bytes) != bytes.size()) {
                fileAlive = false;
                failure("EchoMusicNext: log write failed; console logging continues\n");
            }
            if (consoleAlive && std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stderr) !=
                                    size_t(bytes.size()))
                consoleAlive = false;
            if (consoleAlive)
                std::fflush(stderr);
        };
        for (;;) {
            std::deque<Record> batch;
            bool stopping;
            {
                QMutexLocker lock(&m_mutex);
                if (m_queue.empty() && !m_stopping)
                    m_ready.wait(&m_mutex, 100);
                batch.swap(m_queue);
                stopping = m_stopping;
            }
            const auto dropped = m_dropped.exchange(0);
            if (dropped)
                write(
                    {QDateTime::currentMSecsSinceEpoch(), 1, QStringLiteral("echo.logging"),
                     QStringLiteral("dropped %1 entries: bounded Qt log queue full").arg(dropped)});
            for (const auto &record : batch)
                write(record);
            if (fileAlive)
                file.flush();
            if (stopping)
                break;
        }
    }

private:
    QString m_directory;
    std::atomic_int m_level;
    std::atomic<qint64> m_diagnosticUntil{0};
    bool m_console;
    QMutex m_mutex;
    QWaitCondition m_ready;
    std::deque<Record> m_queue;
    std::atomic<quint64> m_dropped{0};
    bool m_stopping = false;
};
namespace
{
thread_local qint64 rustEmissionTime = 0;
QMutex sinkMutex;
std::shared_ptr<LogSink> sink;
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (!sinkMutex.tryLock())
        return;
    auto writer = sink;
    sinkMutex.unlock();
    if (writer)
        writer->enqueue(logLevel(type),
                        QString::fromLatin1(context.category ? context.category : "qt"), message,
                        rustEmissionTime);
}
void rustRecord(void *, const EchoLogEntry *entry)
{
    const auto target =
        QByteArray(reinterpret_cast<const char *>(entry->target), qsizetype(entry->target_len));
    const auto message = QString::fromUtf8(reinterpret_cast<const char *>(entry->message),
                                           qsizetype(entry->message_len));
    // Enter Qt's category/message-handler path, never from a Rust producer.
    // Keep the Rust emission time; GUI polling must not shift the recorded
    // event time when the window is busy. No pointer/context survives this call.
    QScopedValueRollback<qint64> emission(rustEmissionTime, qint64(entry->timestamp_ms));
    QLoggingCategory category(target.constData());
    switch (entry->level) {
    case 0:
        qCCritical(category).noquote() << message;
        break;
    case 1:
        qCWarning(category).noquote() << message;
        break;
    case 3:
    case 4:
        qCDebug(category).noquote() << message;
        break;
    default:
        qCInfo(category).noquote() << message;
        break;
    }
}
} // namespace
LogService::LogService(const QString &directory, int level, bool console, QSettings *preferences,
                       bool commandLineLevel)
    : m_directory(directory.isEmpty()
                      ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                            QStringLiteral("/logs")
                      : directory)
{
    m_level = qBound(0, level, 4);
    if (preferences) {
        m_preferences = accessor(*preferences);
        const auto savedLevel =
            levels.indexOf(m_preferences->value("logging/level", "info").toString());
        if (!commandLineLevel && savedLevel >= 0)
            m_level = savedLevel;
        m_responseBody = m_preferences->value("logging/apiResponseBody", false).toBool();
        const auto now = QDateTime::currentMSecsSinceEpoch();
        const auto savedUntil = m_preferences->value("logging/diagnosticUntil", 0).toLongLong();
        if (savedUntil > now)
            m_diagnosticUntil = qMin(savedUntil, now + 10 * 60000);
    }
    const auto effective = diagnosticActive() ? qMax(3, m_level) : m_level;
    if (!echo_log_start(uint32_t(effective))) {
        qWarning("Unable to start EchoMusicNext logging");
        return;
    }
    m_sink = std::make_shared<LogSink>(m_directory, m_level, console);
    m_sink->setLevel(m_level, m_diagnosticUntil);
    {
        QMutexLocker lock(&sinkMutex);
        sink = m_sink;
    }
    m_sink->start();
    m_previous = qInstallMessageHandler(messageHandler);
    m_active = true;
    echo_log_api_responses(m_responseBody);
    QLoggingCategory::setFilterRules(
        QStringLiteral("echo.*.debug=true\nqml.debug=true\nqml.*.debug=true"));
    connect(&m_poll, &QTimer::timeout, this, [] { echo_log_drain(rustRecord, nullptr); });
    m_poll.start(50);
    m_diagnostics.setInterval(1000);
    connect(&m_diagnostics, &QTimer::timeout, this, [this] {
        if (!diagnosticActive()) {
            m_diagnosticUntil = 0;
            persist("logging/diagnosticUntil", 0);
            m_diagnostics.stop();
            applyThreshold();
        }
        emit changed();
    });
    if (diagnosticActive())
        m_diagnostics.start();
    qInfo().noquote() << "EchoMusicNext Qt logging started; directory=" << m_directory;
}
QString LogService::level() const
{
    return levels.at(m_level);
}
bool LogService::diagnosticActive() const
{
    return m_diagnosticUntil > QDateTime::currentMSecsSinceEpoch();
}
int LogService::diagnosticMinutes() const
{
    return int(qMax<qint64>(0, m_diagnosticUntil - QDateTime::currentMSecsSinceEpoch() + 59999) /
               60000);
}
bool LogService::persist(const QString &key, const QVariant &value)
{
    if (!m_preferences)
        return true;
    auto writer = accessor(*m_preferences);
    const auto previous = writer->value(key);
    writer->setValue(key, value);
    writer->sync();
    if (writer->status() == QSettings::NoError)
        return true;
    if (previous.isValid())
        writer->setValue(key, previous);
    else
        writer->remove(key);
    writer->sync();
    emit notice(tr("日志偏好保存失败"));
    return false;
}
void LogService::applyThreshold()
{
    const auto effective = diagnosticActive() ? qMax(3, m_level) : m_level;
    if (m_sink)
        m_sink->setLevel(m_level, m_diagnosticUntil);
    echo_log_set_level(uint32_t(effective));
    echo_log_api_responses(m_responseBody);
}
void LogService::setLevel(const QString &value)
{
    const auto next = levels.indexOf(value);
    if (next < 0 || next == m_level || !persist("logging/level", value))
        return;
    m_level = next;
    applyThreshold();
    emit changed();
}
void LogService::setApiResponseBody(bool enabled)
{
    if (enabled == m_responseBody || !persist("logging/apiResponseBody", enabled))
        return;
    m_responseBody = enabled;
    applyThreshold();
    emit changed();
}
bool LogService::enableTemporaryDiagnostics()
{
    const auto until = QDateTime::currentMSecsSinceEpoch() + 10 * 60000;
    if (!persist("logging/diagnosticUntil", until))
        return false;
    m_diagnosticUntil = until;
    m_diagnostics.start();
    applyThreshold();
    emit changed();
    return true;
}
void LogService::disableTemporaryDiagnostics()
{
    if (!persist("logging/diagnosticUntil", 0))
        return;
    m_diagnosticUntil = 0;
    m_diagnostics.stop();
    applyThreshold();
    emit changed();
}
bool LogService::openDirectory()
{
    const bool opened =
        QDir().mkpath(m_directory) && QDesktopServices::openUrl(QUrl::fromLocalFile(m_directory));
    if (!opened)
        emit notice(tr("无法打开运行日志目录"));
    return opened;
}
LogService::~LogService()
{
    if (!m_active)
        return;
    m_poll.stop();
    m_diagnostics.stop();
    echo_log_api_responses(false);
    while (echo_log_drain(rustRecord, nullptr) > 0) {
    }
    echo_log_stop();
    qInfo("EchoMusicNext Qt logging stopped; draining accepted entries");
    qInstallMessageHandler(m_previous);
    {
        QMutexLocker lock(&sinkMutex);
        sink.reset();
    }
    m_sink->finish();
}
} // namespace EchoMusic
