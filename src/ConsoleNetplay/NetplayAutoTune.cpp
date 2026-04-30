#include "ConsoleNetplay/NetplayAutoTune.h"

#include <algorithm>
#include <string>

namespace ConsoleNetplay {

#ifdef __EMSCRIPTEN__

void NetplayAutoTune::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if(!m_enabled) {
        m_snapshot.lastDecisionReason = "Automatic gameplay tuning unavailable on Emscripten";
    }
}

bool NetplayAutoTune::enabled() const
{
    return m_enabled;
}

NetplayAutoTune::Recommendations NetplayAutoTune::update(const RoomState& room,
                                                         const RollbackStats&,
                                                         uint32_t,
                                                         uint32_t)
{
    constexpr uint8_t kFixedDelayFrames = 3;
    constexpr uint8_t kFixedPredictFrames = 8;

    Recommendations recommendations;
    m_snapshot.enabled = m_enabled;
    m_snapshot.currentRecommendedDelay = kFixedDelayFrames;
    m_snapshot.currentFixedPredict = kFixedPredictFrames;
    if(!m_enabled) {
        m_snapshot.lastDecisionReason = "Automatic gameplay tuning unavailable on Emscripten";
        return recommendations;
    }

    if(room.inputDelayFrames != kFixedDelayFrames) {
        recommendations.inputDelayFrames = kFixedDelayFrames;
    }
    if(room.predictFrames != kFixedPredictFrames) {
        recommendations.predictFrames = kFixedPredictFrames;
    }

    m_snapshot.lastDecisionReason = "Fixed tuning active: input delay 3, predict 8";
    return recommendations;
}

NetplayAutoTune::Recommendations NetplayAutoTune::recommendForImpendingResync(const RoomState&, ResyncReason)
{
    return {};
}

NetplayAutoTune::Snapshot NetplayAutoTune::snapshot() const
{
    return m_snapshot;
}

#else

uint8_t NetplayAutoTune::clampDelay(uint32_t frames)
{
    return static_cast<uint8_t>(std::min<uint32_t>(kMaxAutoDelayFrames, std::max<uint32_t>(1u, frames)));
}

uint8_t NetplayAutoTune::clampPredict(uint32_t frames)
{
    return static_cast<uint8_t>(std::min<uint32_t>(kMaxAutoPredictFrames, std::max<uint32_t>(1u, frames)));
}

void NetplayAutoTune::resetForSession(uint32_t sessionId, SessionState state)
{
    m_lastSessionId = sessionId;
    m_lastState = state;
    m_stableFrameCount = 0;
    m_lastAdjustmentFrame = 0;
    m_lastStableEvaluationFrame = 0;
    m_lastDecisionReason.clear();
}

bool NetplayAutoTune::shouldIncreaseDelayForResync(ResyncReason reason)
{
    switch(reason) {
        case ResyncReason::ConfirmedDesync:
        case ResyncReason::HostStallRecovery:
        case ResyncReason::ClientStallRecovery:
            return true;
        default:
            return false;
    }
}

void NetplayAutoTune::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if(!m_enabled) {
        m_lastDecisionReason = "Automatic gameplay tuning disabled";
    }
}

bool NetplayAutoTune::enabled() const
{
    return m_enabled;
}

NetplayAutoTune::Recommendations NetplayAutoTune::update(const RoomState& room,
                                                         const RollbackStats&,
                                                         uint32_t,
                                                         uint32_t)
{
    Recommendations recommendations;

    if(!m_enabled) {
        m_currentRecommendedDelay = static_cast<uint8_t>(room.inputDelayFrames);
        m_currentFixedPredict = static_cast<uint8_t>(room.predictFrames);
        m_stableFrameCount = 0;
        m_lastStableEvaluationFrame = room.currentFrame;
        m_lastDecisionReason = "Automatic gameplay tuning disabled";
        return recommendations;
    }

    if(m_lastSessionId != room.sessionId || m_lastState != room.state) {
        resetForSession(room.sessionId, room.state);
    }

    m_lastSessionId = room.sessionId;
    m_lastState = room.state;
    m_currentRecommendedDelay = clampDelay(room.inputDelayFrames);
    m_currentFixedPredict = 8;

    if(room.predictFrames != m_currentFixedPredict) {
        recommendations.predictFrames = m_currentFixedPredict;
    }

    if(room.state != SessionState::Running ||
       room.recoveryInputMode != RecoveryInputMode::Normal ||
       room.activeResyncId != 0u ||
       room.pendingResyncAckCount != 0u) {
        m_stableFrameCount = 0;
        m_lastStableEvaluationFrame = room.currentFrame;
        m_lastDecisionReason = "Waiting for stable running session";
        return recommendations;
    }

    if(m_lastStableEvaluationFrame == 0u) {
        m_lastStableEvaluationFrame = room.currentFrame;
        if(room.currentFrame == 0u) {
            m_lastDecisionReason = "Initialized reactive delay controller";
            return recommendations;
        }
        m_stableFrameCount += room.currentFrame;
    }
    else if(room.currentFrame <= m_lastStableEvaluationFrame) {
        return recommendations;
    }
    else {
        m_stableFrameCount += room.currentFrame - m_lastStableEvaluationFrame;
        m_lastStableEvaluationFrame = room.currentFrame;
    }

    if(room.inputDelayFrames > 1u &&
       m_stableFrameCount >= kDelayDecayStableFrames) {
        const uint8_t targetDelay = clampDelay(static_cast<uint32_t>(room.inputDelayFrames - 1u));
        if(targetDelay < room.inputDelayFrames) {
            recommendations.inputDelayFrames = targetDelay;
            m_currentRecommendedDelay = targetDelay;
            m_lastAdjustmentFrame = room.currentFrame;
            m_stableFrameCount = 0;
            m_lastDecisionReason =
                "Reduced delay after sustained stable playback to " +
                std::to_string(static_cast<unsigned>(targetDelay));
            return recommendations;
        }
    }

    m_lastDecisionReason = "Stable session; holding current delay";
    return recommendations;
}

NetplayAutoTune::Recommendations NetplayAutoTune::recommendForImpendingResync(const RoomState& room,
                                                                               ResyncReason reason)
{
    Recommendations recommendations;

    if(!m_enabled) {
        return recommendations;
    }

    m_currentFixedPredict = 8;
    if(room.predictFrames != m_currentFixedPredict) {
        recommendations.predictFrames = m_currentFixedPredict;
    }

    if(!shouldIncreaseDelayForResync(reason)) {
        m_lastDecisionReason =
            "Reactive tuning ignored non-pressure resync " + std::to_string(static_cast<unsigned>(reason));
        return recommendations;
    }

    if(room.currentFrame < room.autoTuneDelayIncreaseBlockedUntilFrame) {
        m_lastDecisionReason =
            "Reactive delay increase blocked until frame " +
            std::to_string(room.autoTuneDelayIncreaseBlockedUntilFrame);
        return recommendations;
    }

    const uint8_t currentDelay = clampDelay(room.inputDelayFrames);
    const uint8_t targetDelay = clampDelay(static_cast<uint32_t>(currentDelay) + 1u);
    m_stableFrameCount = 0;
    m_lastStableEvaluationFrame = room.currentFrame;

    if(targetDelay > currentDelay) {
        recommendations.inputDelayFrames = targetDelay;
        m_currentRecommendedDelay = targetDelay;
        m_lastAdjustmentFrame = room.currentFrame;
        m_lastDecisionReason =
            "Raised delay to " + std::to_string(static_cast<unsigned>(targetDelay)) +
            " before reactive resync";
    } else {
        m_currentRecommendedDelay = currentDelay;
        m_lastDecisionReason =
            "Reactive resync pressure detected but delay already at cap " +
            std::to_string(static_cast<unsigned>(currentDelay));
    }

    return recommendations;
}

NetplayAutoTune::Snapshot NetplayAutoTune::snapshot() const
{
    Snapshot snapshot;
    snapshot.enabled = m_enabled;
    snapshot.currentRecommendedDelay = m_currentRecommendedDelay;
    snapshot.currentFixedPredict = m_currentFixedPredict;
    snapshot.stableFrameCount = m_stableFrameCount;
    snapshot.lastAdjustmentFrame = m_lastAdjustmentFrame;
    snapshot.lastDecisionReason = m_lastDecisionReason;
    return snapshot;
}

#endif

} // namespace ConsoleNetplay
