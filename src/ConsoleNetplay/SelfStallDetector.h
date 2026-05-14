#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "NetSession.h"

namespace ConsoleNetplay {

class SelfStallDetector
{
public:
    struct Snapshot
    {
        bool active = false;
        bool hosting = false;
        SessionState sessionState = SessionState::Lobby;
        RecoveryInputMode recoveryInputMode = RecoveryInputMode::Normal;
        uint32_t timelineEpoch = 0;
        uint32_t activeResyncId = 0;
        uint32_t pendingResyncAckCount = 0;
        uint32_t connectedRemoteParticipantCount = 0;
        FrameNumber localSimulationFrame = 0;
        FrameNumber confirmedFrame = 0;
        FrameNumber maxRemoteReportedCurrentFrame = 0;
        FrameNumber maxRemoteReportedConfirmedFrame = 0;
        uint32_t inputDelayFrames = 0;
        uint32_t predictFrames = 0;
        uint32_t playbackStopCount = 0;
        uint32_t rollbackScheduledCount = 0;
    };

    struct UpdateResult
    {
        bool shouldResync = false;
        std::string detail;
    };

    void reset();
    UpdateResult update(const Snapshot& snapshot, std::chrono::steady_clock::time_point now);

private:
    struct ProgressSample
    {
        uint32_t timelineEpoch = 0;
        FrameNumber localSimulationFrame = 0;
        FrameNumber confirmedFrame = 0;
        FrameNumber maxRemoteReportedCurrentFrame = 0;
        FrameNumber maxRemoteReportedConfirmedFrame = 0;
        uint32_t inputDelayFrames = 0;
        uint32_t predictFrames = 0;
        uint32_t playbackStopCount = 0;
        uint32_t rollbackScheduledCount = 0;
    };

    static constexpr auto kStallTimeout = std::chrono::milliseconds(2000);
    static constexpr auto kRecoveryCooldown = std::chrono::milliseconds(5000);
    static constexpr uint32_t kChurnThreshold = 3;

    std::optional<ProgressSample> m_progressBaseline;
    std::chrono::steady_clock::time_point m_lastProgressAt = {};
    std::chrono::steady_clock::time_point m_cooldownUntil = {};

    static bool isEligible(const Snapshot& snapshot);
    static ProgressSample makeSample(const Snapshot& snapshot);
    static bool hasForwardProgress(const ProgressSample& baseline, const ProgressSample& current);
    static uint32_t churnSince(const ProgressSample& baseline, const ProgressSample& current);
    static bool hostIsWaitingWithinRemoteTolerance(const Snapshot& snapshot, const ProgressSample& current);
};

} // namespace ConsoleNetplay
