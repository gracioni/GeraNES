#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "GeraNESNetplay/Diagnostics.h"
#include "GeraNESNetplay/NetSession.h"

namespace Netplay {

class NetplayAutoSettings
{
public:
    struct Snapshot
    {
        bool enabled = false;
        uint8_t currentRecommendedDelay = 0;
        uint8_t currentFixedPredict = 0;
        uint32_t stableFrameCount = 0;
        FrameNumber lastAdjustmentFrame = 0;
        std::string lastDecisionReason;
    };

    struct Recommendations
    {
        std::optional<uint8_t> inputDelayFrames;
        std::optional<uint8_t> predictFrames;
    };

#ifdef __EMSCRIPTEN__
public:
    void setEnabled(bool enabled);
    bool enabled() const;

    Recommendations update(const RoomState& room,
                           const RollbackStats& stats,
                           uint32_t unresolvedPredictedRemoteFrameCount,
                           uint32_t fps);

    Snapshot snapshot() const;

private:
    bool m_enabled = true;
    Snapshot m_snapshot;
#else

private:
    bool m_enabled = true;
    uint32_t m_lastSessionId = 0;
    uint32_t m_lastTimelineEpoch = 0;
    SessionState m_lastState = SessionState::Lobby;
    bool m_runningWindowInitialized = false;
    bool m_predictLocked = false;
    uint8_t m_currentRecommendedDelay = 2;
    uint8_t m_currentFixedPredict = 1;
    uint32_t m_stableFrameCount = 0;
    FrameNumber m_lastEvaluationFrame = 0;
    FrameNumber m_lastAdjustmentFrame = 0;
    uint32_t m_lastPredictionMissCount = 0;
    uint32_t m_lastPlaybackStopCount = 0;
    uint32_t m_lastPredictionLimitStopCount = 0;
    uint32_t m_lastMissingInputStopCount = 0;
    uint32_t m_lastRollbackScheduledCount = 0;
    uint32_t m_lastPredictedFrameUseCount = 0;
    std::string m_lastDecisionReason;

    static constexpr uint8_t kMaxAutoDelayFrames = 8;
    static constexpr uint8_t kMaxAutoPredictFrames = 8;
    static constexpr FrameNumber kEvaluationWindowFrames = 120;
    static constexpr FrameNumber kDelayDecreaseStableFrames = 600;
    static constexpr FrameNumber kPredictDecreaseStableFrames = 900;
    static constexpr FrameNumber kAdjustmentCooldownFrames = 180;

    static bool isAssignedActiveParticipant(const ParticipantInfo& participant);
    static uint8_t clampDelay(uint32_t frames);
    static uint8_t clampPredict(uint32_t frames);
    static uint8_t jitterFramesForRoom(const RoomState& room, uint32_t fps);
    static uint8_t predictionBaselineForRoom(const RoomState& room, uint32_t fps);

    void resetRunningWindow(const RollbackStats& stats, FrameNumber frame);
    void resetForSession(uint32_t sessionId, uint32_t timelineEpoch, SessionState state);

public:
    void setEnabled(bool enabled);
    bool enabled() const;

    Recommendations update(const RoomState& room,
                           const RollbackStats& stats,
                           uint32_t unresolvedPredictedRemoteFrameCount,
                           uint32_t fps);

    Snapshot snapshot() const;
#endif
};

} // namespace Netplay
