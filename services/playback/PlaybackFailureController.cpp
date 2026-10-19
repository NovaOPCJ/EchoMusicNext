#include "PlaybackFailureController.h"
#include "PlaybackService.h"
#include <QRandomGenerator>

namespace EchoMusic
{
PlaybackFailureController::PlaybackFailureController(PlaybackService &playback)
    : QObject(&playback), m_playback(playback)
{
    m_timer.setSingleShot(true);
    connect(&playback.m_preferences, &PlaybackPreferences::failurePolicyChanged, this, [this] {
        // Changing policy cancels an already scheduled action. It does not
        // retroactively turn a paused/error track into a playback request.
        cancel(true);
    });
    connect(&m_timer, &QTimer::timeout, this, [this] {
        auto &p = m_playback;
        if (p.m_closing || m_sequence != p.m_sequence || m_revision != p.m_api.sessionRevision() ||
            m_trackKey != p.m_track.key() || p.m_playing || p.m_loading || !p.m_autoplay ||
            !p.m_preferences.autoNext() || m_attempts >= p.m_preferences.autoNextMaxAttempts())
            return;
        if (p.m_personalFm->active()) {
            ++m_attempts;
            p.m_personalFm->advance(false, false, true);
            return;
        }
        if (p.sourceKey() == DiscoverQueueController::queueKey()) {
            ++m_attempts;
            const auto next = p.m_transition->nextIndex(false);
            if (next >= 0) {
                p.m_transition->consumeNext(next, false);
                p.startIndex(next, true);
            } else
                p.m_discoverQueue->advance(false, true);
            return;
        }
        auto *queue = p.queue();
        const auto count = queue->rowCount();
        if (count <= 1)
            return;
        const auto current = queue->currentIndex();
        int next = -1;
        if (p.m_mode == PlaybackService::Shuffle) {
            next = p.m_transition->failureIndex();
        } else {
            // EchoMusic's error traversal wraps even for sequential/single-loop
            // modes. Natural EOF and queued-next priority are separate policies.
            for (int offset = 1; offset < count; ++offset) {
                const auto index = (qMax(0, current) + offset) % count;
                if (queue->at(index)->playable()) {
                    next = index;
                    break;
                }
            }
        }
        if (next < 0)
            return;
        ++m_attempts;
        p.startIndex(next, true);
    });
}
void PlaybackFailureController::cancel(bool resetAttempts)
{
    m_timer.stop();
    if (resetAttempts)
        m_attempts = 0;
}
void PlaybackFailureController::started(bool preserveFailureChain)
{
    cancel(!preserveFailureChain);
}
void PlaybackFailureController::succeeded()
{
    cancel(true);
}
void PlaybackFailureController::failed()
{
    auto &p = m_playback;
    if (p.m_closing || !p.m_autoplay || !p.hasTrack() || !p.m_preferences.autoNext() ||
        (!p.m_personalFm->active() && p.sourceKey() != DiscoverQueueController::queueKey() &&
         p.queue()->rowCount() <= 1) ||
        m_attempts >= p.m_preferences.autoNextMaxAttempts())
        return;
    m_sequence = p.m_sequence;
    m_revision = p.m_api.sessionRevision();
    m_trackKey = p.m_track.key();
    m_timer.start(p.m_preferences.autoNextDelaySeconds() * 1000);
}
} // namespace EchoMusic
