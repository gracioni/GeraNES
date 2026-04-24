#pragma once

#include <optional>

#include "NetplayTypes.h"

namespace Netplay {

class RemoteInputStallMonitor
{
public:
    struct PendingRecovery
    {
        ParticipantId participantId = kInvalidParticipantId;
        PlayerSlot playerSlot = kObserverPlayerSlot;
        FrameNumber stalledFrame = 0;
        uint32_t observedPeerHealthSerial = 0;
    };

    struct StallUpdate
    {
        bool newlyTracked = false;
        PendingRecovery recovery = {};
    };

    struct RecoveryUpdate
    {
        bool cleared = false;
        PendingRecovery recovery = {};
    };

    struct PeerHealthUpdate
    {
        bool shouldScheduleResync = false;
        PendingRecovery recovery = {};
    };

    void reset();
    StallUpdate noteStall(ParticipantId participantId, PlayerSlot slot, FrameNumber frame, uint32_t observedPeerHealthSerial);
    RecoveryUpdate clearRecovered(ParticipantId participantId, FrameNumber recoveredThroughFrame);
    PeerHealthUpdate peekPeerHealth(ParticipantId participantId, uint32_t peerHealthSerial) const;
    void clearPendingRecovery(ParticipantId participantId);
    const std::optional<PendingRecovery>& pending() const;

private:
    std::optional<PendingRecovery> m_pending;
};

} // namespace Netplay
