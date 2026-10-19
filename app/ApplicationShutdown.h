#pragma once
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QObject>
#include <functional>
#include <utility>

namespace EchoMusic
{
/// Keep Qt/system quit requests on the same asynchronous service-drain path.
class ApplicationShutdown final : public QObject
{
public:
    ApplicationShutdown(QCoreApplication &application, std::function<void()> begin)
        : m_application(application), m_begin(std::move(begin))
    {
        application.installEventFilter(this);
        connect(&application, &QCoreApplication::aboutToQuit, this, [this] {
            if (m_completed)
                return;
            // exit()/platform session termination can bypass QEvent::Quit.
            // Qt only processes deferred deletes after aboutToQuit returns;
            // let the existing asynchronous drain finish before returning.
            QEventLoop pending;
            m_pending = &pending;
            request();
            while (!m_completed)
                pending.exec();
            m_pending = nullptr;
        });
    }
    void complete()
    {
        if (std::exchange(m_completed, true))
            return;
        if (m_pending)
            m_pending->quit();
        else
            m_application.quit();
    }

protected:
    bool eventFilter(QObject *receiver, QEvent *event) override
    {
        if (receiver != &m_application || event->type() != QEvent::Quit || m_completed)
            return false;
        event->ignore();
        request();
        return true;
    }

private:
    void request()
    {
        if (!std::exchange(m_requested, true))
            m_begin();
    }
    QCoreApplication &m_application;
    std::function<void()> m_begin;
    QEventLoop *m_pending = nullptr;
    bool m_requested = false, m_completed = false;
};
} // namespace EchoMusic
