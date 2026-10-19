#include "SystemSleepInhibitor.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QCoreApplication>
#include <QPointer>
#include <QUuid>
namespace EchoMusic
{
namespace
{
constexpr auto portal = "org.freedesktop.portal.Desktop";
void closeRequest(const QString &path)
{
    if (path.isEmpty())
        return;
    const auto message =
        QDBusMessage::createMethodCall(portal, path, "org.freedesktop.portal.Request", "Close");
    QDBusConnection::sessionBus().asyncCall(message);
}
} // namespace
namespace
{
void disconnectRequest(const QString &path, QObject *owner)
{
    if (path.isEmpty())
        return;
    QDBusConnection::sessionBus().disconnect(portal, path, "org.freedesktop.portal.Request",
                                             "Response", owner,
                                             SLOT(portalResponse(uint, QVariantMap, QDBusMessage)));
}
} // namespace
struct SystemSleepInhibitor::Native
{
    QString handle;
    quint64 revision = 0;
    bool pending = false;
};
SystemSleepInhibitor::SystemSleepInhibitor(QObject *parent)
    : QObject(parent), m_native(std::make_unique<Native>())
{
    QDBusConnection::systemBus().connect("org.freedesktop.login1", "/org/freedesktop/login1",
                                         "org.freedesktop.login1.Manager", "PrepareForSleep", this,
                                         SLOT(setSystemSuspended(bool)));
    auto *watcher = new QDBusServiceWatcher(portal, QDBusConnection::sessionBus(),
                                            QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &owner) {
                ++m_native->revision;
                m_native->pending = false;
                disconnectRequest(m_native->handle, this);
                m_native->handle.clear();
                publish(false);
                if (!owner.isEmpty())
                    reconcile();
            });
}
SystemSleepInhibitor::~SystemSleepInhibitor()
{
    closeRequest(m_native->handle);
}
void SystemSleepInhibitor::reconcile()
{
    if (!m_required || m_suspended) {
        ++m_native->revision;
        disconnectRequest(m_native->handle, this);
        closeRequest(m_native->handle);
        m_native->handle.clear();
        m_native->pending = false;
        publish(false);
        return;
    }
    if (m_active || m_native->pending)
        return;
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        publish(false, tr("系统会话总线不可用，无法阻止休眠"));
        return;
    }
    const auto token = QString("echo_sleep_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    auto sender = bus.baseService().mid(1).replace('.', '_');
    m_native->handle = "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;
    // Subscribe before Inhibit: a fast portal can respond before its method reply.
    bus.connect(portal, m_native->handle, "org.freedesktop.portal.Request", "Response", this,
                SLOT(portalResponse(uint, QVariantMap, QDBusMessage)));
    auto message = QDBusMessage::createMethodCall(portal, "/org/freedesktop/portal/desktop",
                                                  "org.freedesktop.portal.Inhibit", "Inhibit");
    message.setArguments(
        {QString(), quint32(4),
         QVariantMap{{"handle_token", token}, {"reason", tr("EchoMusicNext 正在播放音乐")}}});
    const auto revision = ++m_native->revision;
    m_native->pending = true;
    const QPointer<SystemSleepInhibitor> guard(this);
    // The watcher outlives a deleted owner until the reply so a late acquired
    // handle is always closed, instead of leaving a session lease behind.
    auto *watcher =
        new QDBusPendingCallWatcher(bus.asyncCall(message, 10000), QCoreApplication::instance());
    connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
            [guard, revision](QDBusPendingCallWatcher *watcher) {
                const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
                watcher->deleteLater();
                if (!guard || revision != guard->m_native->revision) {
                    if (!reply.isError())
                        closeRequest(reply.value().path());
                    return;
                }
                guard->m_native->pending = false;
                if (reply.isError()) {
                    disconnectRequest(guard->m_native->handle, guard);
                    guard->m_native->handle.clear();
                    guard->publish(false,
                                   guard->tr("未能阻止系统休眠：%1").arg(reply.error().message()));
                } else {
                    guard->m_native->handle = reply.value().path();
                    guard->publish(true);
                }
            });
}
void SystemSleepInhibitor::portalResponse(uint response, const QVariantMap &,
                                          const QDBusMessage &message)
{
    if (!response || message.path() != m_native->handle)
        return;
    ++m_native->revision;
    m_native->pending = false;
    disconnectRequest(m_native->handle, this);
    m_native->handle.clear();
    publish(false, tr("系统未允许阻止休眠"));
}
} // namespace EchoMusic
