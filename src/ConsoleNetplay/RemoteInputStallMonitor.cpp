#include "ConsoleNetplay/RemoteInputStallMonitor.h"

namespace ConsoleNetplay {

namespace {

constexpr FrameNumber kTransientStallLogCoalesceFrames = 16u;

} // namespace

void RemoteInputStallMonitor::reset()
{
    m_pending.reset();
    m_lastRecovered.reset();
}

RemoteInputStallMonitor::StallUpdate RemoteInputStallMonitor::noteStall(ParticipantId participantId,
                                                                                  PlayerSlot slot,
                                                                                  FrameNumber frame,
                                                                                  uint32_t observedPeerHealthSerial)
{
    StallUpdate update;
    if(m_pending.has_value() && m_pending->participantId == participantId) {
        const FrameNumber pendingFrame = m_pending->stalledFrame;
        const FrameNumber frameDistance =
            frame > pendingFrame ? (frame - pendingFrame) : (pendingFrame - frame);
        if(frameDistance <= kTransientStallLogCoalesceFrames) {
            if(frame < m_pending->stalledFrame) {
                m_pending->stalledFrame = frame;
                m_pending->playerSlot = slot;
            }
            update.recovery = *m_pending;
            return update;
        }
    }

    const FrameNumber framesSinceRecovery =
        m_lastRecovered.has_value() && frame > m_lastRecovered->recoveredThroughFrame
            ? frame - m_lastRecovered->recoveredThroughFrame
            : 0u;
    const bool coalescedWithRecentRecovery =
        m_lastRecovered.has_value() &&
        m_lastRecovered->participantId == participantId &&
        framesSinceRecovery > 0u &&
        framesSinceRecovery <= kTransientStallLogCoalesceFrames;

    m_pending = PendingRecovery{
        participantId,
        slot,
        frame,
        observedPeerHealthSerial,
        !coalescedWithRecentRecovery
    };
    update.newlyTracked = m_pending->loggable;
    update.recovery = *m_pending;
    return update;
}

RemoteInputStallMonitor::RecoveryUpdate RemoteInputStallMonitor::clearRecovered(ParticipantId participantId,
                                                                                          FrameNumber recoveredThroughFrame)
{
    RecoveryUpdate update;
    if(!m_pending.has_value()) return update;
    if(m_pending->participantId != participantId) return update;
    if(recoveredThroughFrame < m_pending->stalledFrame) return update;

    update.cleared = m_pending->loggable;
    update.recovery = *m_pending;
    m_lastRecovered = LastRecoveredStall{
        m_pending->participantId,
        m_pending->playerSlot,
        recoveredThroughFrame
    };
    m_pending.reset();
    return update;
}

RemoteInputStallMonitor::PeerHealthUpdate RemoteInputStallMonitor::onPeerHealth(ParticipantId participantId,
                                                                                          uint32_t peerHealthSerial)
{
    PeerHealthUpdate update;
    if(!m_pending.has_value()) return update;
    if(m_pending->participantId != participantId) return update;
    if(peerHealthSerial <= m_pending->observedPeerHealthSerial) return update;

    if(!m_pending->loggable) {
        m_pending.reset();
        return update;
    }

    update.shouldScheduleResync = true;
    update.recovery = *m_pending;
    m_pending.reset();
    return update;
}

const std::optional<RemoteInputStallMonitor::PendingRecovery>& RemoteInputStallMonitor::pending() const
{
    return m_pending;
}

} // namespace ConsoleNetplay
