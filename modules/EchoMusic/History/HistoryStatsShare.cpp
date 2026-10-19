#include "HistoryStatsShare.h"
#include "HistoryService.h"
#include "PlaybackService.h"
#include "SharingCommands.h"
#include <QClipboard>
#include <QLineF>
#include <QDateTime>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <algorithm>
#include <utility>
#include <QtMath>

namespace EchoMusic
{
HistoryStatsShare::HistoryStatsShare(HistoryService &history, PlaybackService &playback,
                                     QObject *parent)
    : QObject(parent), m_history(history), m_playback(playback)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    m_imageDeadline.setSingleShot(true);
    m_imageDeadline.setInterval(800); // EchoMusic's bounded image load deadline.
    connect(&m_imageDeadline, &QTimer::timeout, this, [this] {
        m_imageExpired = true;
        tryCapture();
    });
}
HistoryStatsShare::~HistoryStatsShare()
{
    cancel();
}
void HistoryStatsShare::share(QQuickItem *page)
{
    if (busy() || !page || !page->window() || !page->window()->isVisible() ||
        page->window()->visibility() == QWindow::Minimized)
        return;
    auto *context = QQmlEngine::contextForObject(page);
    if (!context)
        return;
    m_window = page->window();
    m_uiScale = QLineF(page->mapToScene(QPointF{}), page->mapToScene(QPointF(1, 0))).length();
    if (m_uiScale <= 0)
        m_uiScale = 1;
    QQmlComponent component(context->engine(),
                            QUrl("qrc:/qt/qml/EchoMusic/History/HistoryStatsCapture.qml"));
    const auto sharer =
        m_playback.sharing() ? m_playback.sharing()->sharerName() : QStringLiteral("EchoMusic");
    auto *stage = qobject_cast<QQuickItem *>(component.createWithInitialProperties(
        {{"stats", m_history.stats()},
         {"viewportWidth", m_window->width() / m_uiScale},
         {"shareTitle", tr("%1 的听歌统计").arg(sharer)},
         {"updatedAt", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm")}},
        context));
    if (!stage) {
        emit notice(tr("复制听歌统计失败"), true);
        m_window.clear();
        return;
    }
    QQmlEngine::setObjectOwnership(stage, QQmlEngine::CppOwnership);
    stage->setParent(this);
    stage->setParentItem(m_window->contentItem());
    stage->setWidth(std::clamp<qreal>(m_window->width() / m_uiScale - 32, 760, 1180));
    stage->setX(m_window->width() + 32);
    m_stage = stage;
    m_prepared = false;
    m_imageExpired = false;
    ++m_generation;
    m_connections.append(connect(stage, SIGNAL(pendingImagesChanged()), this, SLOT(tryCapture())));
    m_connections.append(connect(page, &QObject::destroyed, this, &HistoryStatsShare::cancel));
    m_connections.append(connect(m_window, &QWindow::visibilityChanged, this, [this] {
        if (m_window && (!m_window->isVisible() || m_window->visibility() == QWindow::Minimized))
            cancel();
    }));
    // Wait for an actual completed Qt frame so nested layouts are polished.
    // The timer only bounds remote artwork, never corrects layout repeatedly.
    m_connections.append(connect(
        m_window, &QQuickWindow::frameSwapped, this,
        [this] {
            if (!m_stage || m_prepared)
                return;
            m_prepared = true;
            tryCapture();
        },
        Qt::QueuedConnection));
    m_imageDeadline.start();
    emit busyChanged();
    m_window->update();
}
void HistoryStatsShare::tryCapture()
{
    if (!m_stage || !m_window || !m_prepared || m_grab ||
        (!m_imageExpired && m_stage->property("pendingImages").toInt() > 0))
        return;
    m_imageDeadline.stop();
    const auto width = m_stage->width(), height = m_stage->height();
    if (width <= 0 || height <= 0) {
        cancel();
        emit notice(tr("复制听歌统计失败"), true);
        return;
    }
    const qreal fit = std::min({1., (m_window->width() - 32. * m_uiScale) / (width * m_uiScale),
                                (m_window->height() - 32. * m_uiScale) / (height * m_uiScale)});
    // Qt Quick's grab result already uses the window's effective device pixel
    // ratio. Passing device pixels here would apply Retina scaling twice.
    const QSize output(std::max(1, qFloor(width * m_uiScale * fit)),
                       std::max(1, qFloor(height * m_uiScale * fit)));
    m_grab = m_stage->grabToImage(output);
    if (!m_grab) {
        cancel();
        emit notice(tr("复制听歌统计失败"), true);
        return;
    }
    const auto generation = m_generation;
    connect(m_grab.data(), &QQuickItemGrabResult::ready, this, [this, generation] {
        if (generation != m_generation || !m_grab || !m_stage)
            return;
        const auto image = m_grab->image();
        // Release our capture cycle before clipboard/consumer callbacks can
        // reenter navigation or start a later share.
        cancel();
        if (!image.isNull()) {
            QGuiApplication::clipboard()->setImage(image);
            emit captured(image);
        }
        emit notice(image.isNull() ? tr("复制听歌统计失败") : tr("听歌统计已复制"), image.isNull());
    });
}
void HistoryStatsShare::cancel()
{
    const bool wasBusy = busy();
    ++m_generation;
    m_imageDeadline.stop();
    for (const auto &connection : std::exchange(m_connections, {}))
        disconnect(connection);
    m_grab.reset();
    if (m_stage) {
        m_stage->setParentItem(nullptr);
        m_stage->deleteLater();
        m_stage.clear();
    }
    m_window.clear();
    m_prepared = false;
    if (wasBusy)
        emit busyChanged();
}
} // namespace EchoMusic
