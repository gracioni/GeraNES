#include "GeraNESNetplay/RemoteInputStallMonitor.h"

namespace Netplay {

void RemoteInputStallMonitor::reset()
{
    m_pending.reset();
}

RemoteInputStallMonitor::StallUpdate RemoteInputStallMonitor::noteStall(ParticipantId participantId,
                                                                                  PlayerSlot slot,
                                                                                  FrameNumber frame,
                                                                                  uint32_t observedPeerHealthSerial)
{
    StallUpdate update;
    if(m_pending.has_value() &&
       m_pending->participantId == participantId &&
       m_pending->playerSlot == slot) {
        if(m_pending->observedPeerHealthSerial == observedPeerHealthSerial) {
            if(frame < m_pending->stalledFrame) {
                m_pending->stalledFrame = frame;
            }
            update.recovery = *m_pending;
            return update;
        }
        if(m_pending->stalledFrame == frame) {
            update.recovery = *m_pending;
            return update;
        }
    }

    if(m_pending.has_value() &&
       m_pending->participantId == participantId &&
       m_pending->playerSlot == slot &&
       m_pending->stalledFrame == frame) {
        update.recovery = *m_pending;
        return update;
    }

    m_pending = PendingRecovery{participantId, slot, frame, observedPeerHealthSerial};
    update.newlyTracked = true;
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

    update.cleared = true;
    update.recovery = *m_pending;
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

    update.shouldScheduleResync = true;
    update.recovery = *m_pending;
    m_pending.reset();
    return update;
}

const std::optional<RemoteInputStallMonitor::PendingRecovery>& RemoteInputStallMonitor::pending() const
{
    return m_pending;
}

} // namespace Netplay
