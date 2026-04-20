#pragma once

#include <chrono>
#include <optional>

#include "NetplayTypes.h"

namespace Netplay {

class ImplicitStallRecoveryMonitor
{
public:
    struct PendingRecovery
    {
        ParticipantId participantId = kInvalidParticipantId;
        PlayerSlot playerSlot = kObserverPlayerSlot;
        FrameNumber stalledFrame = 0;
        uint32_t observedPeerHealthSerial = 0;
        std::chrono::steady_clock::time_point trackedAt = {};
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

    struct TimeoutUpdate
    {
        bool shouldScheduleResync = false;
        PendingRecovery recovery = {};
    };

    void reset();
    StallUpdate noteStall(ParticipantId participantId, PlayerSlot slot, FrameNumber frame, uint32_t observedPeerHealthSerial);
    RecoveryUpdate clearRecovered(ParticipantId participantId, FrameNumber recoveredThroughFrame);
    PeerHealthUpdate onPeerHealth(ParticipantId participantId, uint32_t peerHealthSerial);
    TimeoutUpdate onTimeout(ParticipantId participantId,
                            const std::chrono::steady_clock::time_point& now,
                            const std::chrono::milliseconds& timeout);
    const std::optional<PendingRecovery>& pending() const;

private:
    std::optional<PendingRecovery> m_pending;
};

} // namespace Netplay
