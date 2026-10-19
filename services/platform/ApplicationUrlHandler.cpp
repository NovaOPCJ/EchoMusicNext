#include "ApplicationUrlHandler.h"
#include <QCoreApplication>
#include <QFileOpenEvent>
namespace EchoMusic
{
ApplicationUrlHandler::ApplicationUrlHandler(QObject *parent) : QObject(parent)
{
    QCoreApplication::instance()->installEventFilter(this);
}
QList<QUrl> ApplicationUrlHandler::takePending(qsizetype maximum)
{
    const auto pending = m_pending.mid(0, qBound(qsizetype(0), maximum, m_pending.size()));
    m_pending.remove(0, pending.size());
    return pending;
}
void ApplicationUrlHandler::beginDelivery()
{
    m_ready = true;
    for (const auto &url : takePending())
        emit urlRequested(url);
}
bool ApplicationUrlHandler::eventFilter(QObject *, QEvent *event)
{
    if (event->type() != QEvent::FileOpen)
        return false;
    const auto url = static_cast<QFileOpenEvent *>(event)->url();
    const bool mod = url.isLocalFile() && url.path().endsWith(".echomod", Qt::CaseInsensitive) &&
                     !url.hasQuery() && !url.hasFragment();
    if ((!mod && url.scheme() != "echomusicnext") || url.toEncoded().size() > 8192)
        return false;
    if (m_ready)
        emit urlRequested(url);
    else if (m_pending.size() < 16)
        m_pending.append(url);
    event->accept();
    return true;
}
} // namespace EchoMusic
