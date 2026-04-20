#include "GeraNESNetplay/ImplicitStallRecoveryMonitor.h"

namespace Netplay {

void ImplicitStallRecoveryMonitor::reset()
{
    m_pending.reset();
}

ImplicitStallRecoveryMonitor::StallUpdate ImplicitStallRecoveryMonitor::noteStall(ParticipantId participantId,
                                                                                  PlayerSlot slot,
                                                                                  FrameNumber frame,
                                                                                  uint32_t observedPeerHealthSerial)
{
    StallUpdate update;
    if(m_pending.has_value() &&
       m_pending->participantId == participantId &&
       m_pending->playerSlot == slot) {
        // Keep tracking window anchored to the first observed stall for this
        // participant/slot. Do not reset timeout just because newer missing
        // frames are observed while the same stall condition persists.
        if(frame < m_pending->stalledFrame) {
            m_pending->stalledFrame = frame;
        }
        if(observedPeerHealthSerial < m_pending->observedPeerHealthSerial) {
            m_pending->observedPeerHealthSerial = observedPeerHealthSerial;
        }
        update.recovery = *m_pending;
        return update;
    }

    m_pending = PendingRecovery{
        participantId,
        slot,
        frame,
        observedPeerHealthSerial,
        std::chrono::steady_clock::now()
    };
    update.newlyTracked = true;
    update.recovery = *m_pending;
    return update;
}

ImplicitStallRecoveryMonitor::RecoveryUpdate ImplicitStallRecoveryMonitor::clearRecovered(ParticipantId participantId,
                                                                                          FrameNumber recoveredThroughFrame)
{
    RecoveryUpdate update;
    if(!m_pending.has_value()) return update;
    if(m_pending->participantId != participantId) return update;
    if(recoveredThroughFrame < m_pending->stalledFrame) return update;

    update.cleared = true;
    update.recovery = *m_pending;
    m_pending.reset();
    return update;
}

ImplicitStallRecoveryMonitor::PeerHealthUpdate ImplicitStallRecoveryMonitor::onPeerHealth(ParticipantId participantId,
                                                                                          uint32_t peerHealthSerial)
{
    PeerHealthUpdate update;
    if(!m_pending.has_value()) return update;
    if(m_pending->participantId != participantId) return update;
    if(peerHealthSerial <= m_pending->observedPeerHealthSerial) return update;

    update.shouldScheduleResync = true;
    update.recovery = *m_pending;
    m_pending.reset();
    return update;
}

ImplicitStallRecoveryMonitor::TimeoutUpdate ImplicitStallRecoveryMonitor::onTimeout(
    ParticipantId participantId,
    const std::chrono::steady_clock::time_point& now,
    const std::chrono::milliseconds& timeout)
{
    TimeoutUpdate update;
    if(!m_pending.has_value()) return update;
    if(m_pending->participantId != participantId) return update;
    if(m_pending->trackedAt == std::chrono::steady_clock::time_point{}) return update;
    if(now - m_pending->trackedAt < timeout) return update;

    update.shouldScheduleResync = true;
    update.recovery = *m_pending;
    m_pending.reset();
    return update;
}

const std::optional<ImplicitStallRecoveryMonitor::PendingRecovery>& ImplicitStallRecoveryMonitor::pending() const
{
    return m_pending;
}

} // namespace Netplay
