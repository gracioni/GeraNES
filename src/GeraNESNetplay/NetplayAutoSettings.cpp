#include "GeraNESNetplay/NetplayAutoSettings.h"

#include <algorithm>
#include <cmath>

namespace Netplay {

#ifdef __EMSCRIPTEN__

void NetplayAutoSettings::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if(!m_enabled) {
        m_snapshot.lastDecisionReason = "Automatic gameplay tuning unavailable on Emscripten";
    }
}

bool NetplayAutoSettings::enabled() const
{
    return m_enabled;
}

NetplayAutoSettings::Recommendations NetplayAutoSettings::update(const RoomState& room,
                                                                 const RollbackStats&,
                                                                 uint32_t,
                                                                 uint32_t)
{
    constexpr uint8_t kFixedDelayFrames = 1;
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

    m_snapshot.lastDecisionReason = "Fixed tuning active: input delay 1, predict 8";
    return recommendations;
}

NetplayAutoSettings::Snapshot NetplayAutoSettings::snapshot() const
{
    return m_snapshot;
}

#else

bool NetplayAutoSettings::isAssignedActiveParticipant(const ParticipantInfo& participant)
{
    return participant.connected &&
           !participant.inputSuspended &&
           !participant.inputResumeAwaitingResync &&
           (!participant.controllerAssignments.empty() ||
            participant.controllerAssignment != kObserverPlayerSlot);
}

uint8_t NetplayAutoSettings::clampDelay(uint32_t frames)
{
    return static_cast<uint8_t>(std::min<uint32_t>(kMaxAutoDelayFrames, frames));
}

uint8_t NetplayAutoSettings::clampPredict(uint32_t frames)
{
    return static_cast<uint8_t>(std::min<uint32_t>(kMaxAutoPredictFrames, frames));
}

uint8_t NetplayAutoSettings::jitterFramesForRoom(const RoomState& room, uint32_t fps)
{
    const double fpsDouble = static_cast<double>(std::max<uint32_t>(1u, fps));
    const double frameDurationMs = 1000.0 / fpsDouble;

    uint8_t worstJitterFrames = 0;
    for(const ParticipantInfo& participant : room.participants) {
        if(!isAssignedActiveParticipant(participant)) continue;
        const uint32_t jitterMs = participant.jitterMs;
        const uint32_t frames = static_cast<uint32_t>(std::ceil(static_cast<double>(jitterMs) / frameDurationMs));
        worstJitterFrames = std::max<uint8_t>(worstJitterFrames, clampDelay(frames));
    }
    return worstJitterFrames;
}

uint8_t NetplayAutoSettings::predictionBaselineForRoom(const RoomState& room, uint32_t fps)
{
    const double fpsDouble = static_cast<double>(std::max<uint32_t>(1u, fps));
    const double frameDurationMs = 1000.0 / fpsDouble;

    uint8_t worstTransitFrames = 0;
    for(const ParticipantInfo& participant : room.participants) {
        if(!isAssignedActiveParticipant(participant)) continue;

        const double oneWayMs = static_cast<double>(participant.pingMs) * 0.5;
        const double jitterMs = static_cast<double>(participant.jitterMs);
        const uint32_t frames = static_cast<uint32_t>(
            std::ceil((oneWayMs + jitterMs) / frameDurationMs)
        );
        worstTransitFrames = std::max<uint8_t>(worstTransitFrames, clampPredict(frames));
    }

    const uint32_t baseline =
        std::max<uint32_t>(
            1u,
            std::max<uint32_t>(
                static_cast<uint32_t>(room.inputDelayFrames) + 1u,
                static_cast<uint32_t>(worstTransitFrames) + 1u
            )
        );
    return clampPredict(baseline);
}

void NetplayAutoSettings::resetRunningWindow(const RollbackStats& stats, FrameNumber frame)
{
    m_runningWindowInitialized = true;
    m_lastEvaluationFrame = frame;
    m_lastPredictionMissCount = stats.predictionMissCount;
    m_lastPlaybackStopCount = stats.playbackStopCount;
    m_lastPredictionLimitStopCount = stats.stopDueToPredictionLimitCount;
    m_lastMissingInputStopCount = stats.stopDueToMissingInputCount;
    m_lastRollbackScheduledCount = stats.rollbackScheduledCount;
    m_lastPredictedFrameUseCount = stats.predictedFrameUseCount;
}

void NetplayAutoSettings::resetForSession(uint32_t sessionId, uint32_t timelineEpoch, SessionState state)
{
    m_lastSessionId = sessionId;
    m_lastTimelineEpoch = timelineEpoch;
    m_lastState = state;
    m_runningWindowInitialized = false;
    m_predictLocked = false;
    m_stableFrameCount = 0;
    m_lastEvaluationFrame = 0;
    m_lastAdjustmentFrame = 0;
    m_lastPredictionMissCount = 0;
    m_lastPlaybackStopCount = 0;
    m_lastPredictionLimitStopCount = 0;
    m_lastMissingInputStopCount = 0;
    m_lastRollbackScheduledCount = 0;
    m_lastPredictedFrameUseCount = 0;
    m_lastDecisionReason.clear();
}

void NetplayAutoSettings::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if(!m_enabled) {
        m_lastDecisionReason = "Automatic gameplay tuning disabled";
    }
}

bool NetplayAutoSettings::enabled() const
{
    return m_enabled;
}

NetplayAutoSettings::Recommendations NetplayAutoSettings::update(const RoomState& room,
                                                                 const RollbackStats& stats,
                                                                 uint32_t unresolvedPredictedRemoteFrameCount,
                                                                 uint32_t fps)
{
    (void)stats;
    (void)unresolvedPredictedRemoteFrameCount;
    (void)fps;

    constexpr uint8_t kFixedDelayFrames = 1;
    constexpr uint8_t kFixedPredictFrames = 8;

    Recommendations recommendations;

    m_lastSessionId = room.sessionId;
    m_lastTimelineEpoch = room.timelineEpoch;
    m_lastState = room.state;
    if(!m_enabled) {
        m_currentRecommendedDelay = static_cast<uint8_t>(room.inputDelayFrames);
        m_currentFixedPredict = static_cast<uint8_t>(room.predictFrames);
        m_lastDecisionReason = "Automatic gameplay tuning disabled";
        return recommendations;
    }

    m_currentRecommendedDelay = kFixedDelayFrames;
    m_currentFixedPredict = kFixedPredictFrames;
    m_predictLocked = true;
    m_runningWindowInitialized = false;
    m_stableFrameCount = 0;
    m_lastEvaluationFrame = room.currentFrame;
    m_lastAdjustmentFrame = room.currentFrame;
    m_lastPredictionMissCount = 0;
    m_lastPlaybackStopCount = 0;
    m_lastPredictionLimitStopCount = 0;
    m_lastMissingInputStopCount = 0;
    m_lastRollbackScheduledCount = 0;
    m_lastPredictedFrameUseCount = 0;
    m_lastDecisionReason = "Fixed tuning active: input delay 1, predict 8";

    if(room.inputDelayFrames != kFixedDelayFrames) {
        recommendations.inputDelayFrames = kFixedDelayFrames;
    }
    if(room.predictFrames != kFixedPredictFrames) {
        recommendations.predictFrames = kFixedPredictFrames;
    }

    return recommendations;
}

NetplayAutoSettings::Snapshot NetplayAutoSettings::snapshot() const
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

} // namespace Netplay
