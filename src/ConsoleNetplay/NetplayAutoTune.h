#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ConsoleNetplay/Diagnostics.h"
#include "ConsoleNetplay/NetSession.h"

namespace ConsoleNetplay {

class NetplayAutoTune
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
    Recommendations recommendForImpendingResync(const RoomState& room, ResyncReason reason);

    Snapshot snapshot() const;

private:
    bool m_enabled = true;
    Snapshot m_snapshot;
#else

private:
    bool m_enabled = true;
    uint32_t m_lastSessionId = 0;
    SessionState m_lastState = SessionState::Lobby;
    uint8_t m_currentRecommendedDelay = 1;
    uint8_t m_currentFixedPredict = 8;
    uint32_t m_stableFrameCount = 0;
    FrameNumber m_lastAdjustmentFrame = 0;
    FrameNumber m_lastStableEvaluationFrame = 0;
    std::string m_lastDecisionReason;

    static constexpr uint8_t kMaxAutoDelayFrames = 8;
    static constexpr uint8_t kMaxAutoPredictFrames = 16;
    static constexpr FrameNumber kDelayDecayStableFrames = 1200;

    static uint8_t clampDelay(uint32_t frames);
    static uint8_t clampPredict(uint32_t frames);
    void resetForSession(uint32_t sessionId, SessionState state);
    static bool shouldIncreaseDelayForResync(ResyncReason reason);

public:
    void setEnabled(bool enabled);
    bool enabled() const;

    Recommendations update(const RoomState& room,
                           const RollbackStats& stats,
                           uint32_t unresolvedPredictedRemoteFrameCount,
                           uint32_t fps);
    Recommendations recommendForImpendingResync(const RoomState& room, ResyncReason reason);

    Snapshot snapshot() const;
#endif
};

} // namespace ConsoleNetplay
