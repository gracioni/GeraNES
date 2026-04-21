#include "GeraNESNetplay/NetplayAutoSettings.h"
#include "GeraNESNetplay/NetplayInputAssignment.h"

#include <algorithm>
#include <cmath>
#include <string>

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

uint64_t NetplayAutoSettings::activeParticipantSignature(const RoomState& room)
{
    uint64_t hash = 1469598103934665603ull;
    auto mix = [&hash](uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };

    for(const ParticipantInfo& participant : room.participants) {
        mix(static_cast<uint64_t>(participant.id));
        mix(static_cast<uint64_t>(participant.connected ? 1u : 0u));
        mix(static_cast<uint64_t>(participant.reconnectReserved ? 1u : 0u));
        mix(static_cast<uint64_t>(participant.role));
        mix(static_cast<uint64_t>(participant.controllerAssignments.size()));
        for(PlayerSlot slot : participant.controllerAssignments) {
            mix(static_cast<uint64_t>(slot) + 17ull);
        }
    }

    return hash;
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
    m_lastRecoveryModeTransitionCount = 0;
    m_lastActiveParticipantSignature = 0;
    m_delayRetuneBlockedUntilFrame = 0;
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
    const FrameNumber currentFrame = room.currentFrame;
    constexpr uint8_t kFixedPredictFrames = 8;

    Recommendations recommendations;
    if(!m_enabled) {
        m_currentRecommendedDelay = static_cast<uint8_t>(room.inputDelayFrames);
        m_currentFixedPredict = static_cast<uint8_t>(room.predictFrames);
        m_predictLocked = false;
        m_runningWindowInitialized = false;
        m_stableFrameCount = 0;
        m_delayRetuneBlockedUntilFrame = 0;
        m_lastDecisionReason = "Automatic gameplay tuning disabled";
        return recommendations;
    }

    if(m_lastSessionId != room.sessionId ||
       m_lastTimelineEpoch != room.timelineEpoch ||
       m_lastState != room.state) {
        resetForSession(room.sessionId, room.timelineEpoch, room.state);
    }

    m_lastSessionId = room.sessionId;
    m_lastTimelineEpoch = room.timelineEpoch;
    m_lastState = room.state;

    m_currentRecommendedDelay = static_cast<uint8_t>(room.inputDelayFrames);
    m_currentFixedPredict = kFixedPredictFrames;
    m_predictLocked = true;

    if(room.predictFrames != kFixedPredictFrames) {
        recommendations.predictFrames = kFixedPredictFrames;
    }

    const uint64_t participantSignature = activeParticipantSignature(room);
    if(m_lastActiveParticipantSignature != 0u &&
       participantSignature != m_lastActiveParticipantSignature) {
        m_runningWindowInitialized = false;
        m_stableFrameCount = 0;
        m_delayRetuneBlockedUntilFrame = 0;
        const uint8_t rebasedDelay = std::max<uint8_t>(1u, jitterFramesForRoom(room, fps));
        if(room.inputDelayFrames != rebasedDelay) {
            recommendations.inputDelayFrames = rebasedDelay;
            m_currentRecommendedDelay = rebasedDelay;
            m_lastAdjustmentFrame = currentFrame;
            m_lastDecisionReason =
                "Participant topology changed; rebased delay to " +
                std::to_string(static_cast<unsigned>(rebasedDelay));
        } else {
            m_lastDecisionReason = "Participant topology changed; reset adaptive window";
        }
        m_lastActiveParticipantSignature = participantSignature;
        return recommendations;
    }
    m_lastActiveParticipantSignature = participantSignature;

    if(room.state != SessionState::Running) {
        m_runningWindowInitialized = false;
        m_stableFrameCount = 0;
        m_lastDecisionReason = "Waiting for running session";
        return recommendations;
    }

    if(room.recoveryInputMode != RecoveryInputMode::Normal ||
       room.activeResyncId != 0u ||
       room.pendingResyncAckCount != 0u) {
        m_runningWindowInitialized = false;
        m_stableFrameCount = 0;
        m_lastDecisionReason = "Recovery/resync active; delaying auto-tune";
        return recommendations;
    }

    if(room.recoveryModeTransitionCount != m_lastRecoveryModeTransitionCount) {
        m_lastRecoveryModeTransitionCount = room.recoveryModeTransitionCount;
        m_delayRetuneBlockedUntilFrame = currentFrame + kPostRecoveryRetuneDelayFrames;
        m_runningWindowInitialized = false;
        m_stableFrameCount = 0;
        m_lastDecisionReason = "Recovery transition detected; waiting for stability before retune";
        return recommendations;
    }

    const bool inPostRecoverySettleWindow = currentFrame < m_delayRetuneBlockedUntilFrame;

    const uint8_t baselineDelay = std::max<uint8_t>(1u, jitterFramesForRoom(room, fps));

    if(!m_runningWindowInitialized) {
        resetRunningWindow(stats, currentFrame);
        m_currentRecommendedDelay = static_cast<uint8_t>(room.inputDelayFrames);
        m_lastDecisionReason = "Initialized adaptive delay controller";
        return recommendations;
    }

    if(currentFrame <= m_lastEvaluationFrame) {
        return recommendations;
    }

    const auto safeDelta = [](uint32_t current, uint32_t previous) {
        return current >= previous ? (current - previous) : current;
    };

    const uint32_t predictionMissDelta = safeDelta(stats.predictionMissCount, m_lastPredictionMissCount);
    const uint32_t playbackStopDelta = safeDelta(stats.playbackStopCount, m_lastPlaybackStopCount);
    const uint32_t predictionLimitStopDelta =
        safeDelta(stats.stopDueToPredictionLimitCount, m_lastPredictionLimitStopCount);
    const uint32_t missingInputStopDelta =
        safeDelta(stats.stopDueToMissingInputCount, m_lastMissingInputStopCount);
    const uint32_t rollbackScheduledDelta =
        safeDelta(stats.rollbackScheduledCount, m_lastRollbackScheduledCount);
    m_lastPredictionMissCount = stats.predictionMissCount;
    m_lastPlaybackStopCount = stats.playbackStopCount;
    m_lastPredictionLimitStopCount = stats.stopDueToPredictionLimitCount;
    m_lastMissingInputStopCount = stats.stopDueToMissingInputCount;
    m_lastRollbackScheduledCount = stats.rollbackScheduledCount;
    m_lastPredictedFrameUseCount = stats.predictedFrameUseCount;

    const bool evaluationWindowReached =
        (currentFrame - m_lastEvaluationFrame) >= kEvaluationWindowFrames;
    const bool severePressureNow =
        missingInputStopDelta > 0u ||
        predictionLimitStopDelta > 0u ||
        unresolvedPredictedRemoteFrameCount >= 4u;
    if(!evaluationWindowReached && !severePressureNow) {
        return recommendations;
    }
    m_lastEvaluationFrame = currentFrame;

    bool recoveringAssignedPeer = false;
    for(const ParticipantInfo& participant : room.participants) {
        if(participant.id == kInvalidParticipantId || !participant.connected) continue;
        if(participantIsObserver(participant)) continue;
        if(participant.inputSuspended || participant.inputResumeAwaitingResync) {
            recoveringAssignedPeer = true;
            break;
        }
    }

    const uint32_t stressScore =
        (predictionMissDelta * 2u) +
        (playbackStopDelta * 2u) +
        (predictionLimitStopDelta * 4u) +
        (missingInputStopDelta * 5u) +
        (rollbackScheduledDelta * 2u) +
        (unresolvedPredictedRemoteFrameCount > 0u ? 1u + (unresolvedPredictedRemoteFrameCount / 2u) : 0u) +
        (recoveringAssignedPeer ? 4u : 0u);

    const FrameNumber framesSinceAdjustment = currentFrame - m_lastAdjustmentFrame;
    const bool cooldownActive = framesSinceAdjustment < kAdjustmentCooldownFrames;
    const bool shouldIncrease = stressScore >= 4u;

    if(shouldIncrease && !cooldownActive) {
        const uint8_t increaseStep =
            (stressScore >= 10u || recoveringAssignedPeer || missingInputStopDelta > 0u) ? 2u : 1u;
        const uint8_t targetDelay =
            clampDelay(static_cast<uint32_t>(std::max<uint8_t>(room.inputDelayFrames, m_currentRecommendedDelay)) +
                       static_cast<uint32_t>(increaseStep));
        if(targetDelay > room.inputDelayFrames) {
            recommendations.inputDelayFrames = targetDelay;
            m_currentRecommendedDelay = targetDelay;
            m_lastAdjustmentFrame = currentFrame;
            m_stableFrameCount = 0;
            m_lastDecisionReason =
                "Increased delay to absorb prediction/rollback pressure (" +
                std::to_string(static_cast<unsigned>(stressScore)) + ")";
            return recommendations;
        }
    }

    if(inPostRecoverySettleWindow) {
        m_runningWindowInitialized = false;
        m_stableFrameCount = 0;
        m_lastDecisionReason = "Post-recovery settle window active";
        return recommendations;
    }

    const bool healthyWindow =
        stressScore == 0u &&
        unresolvedPredictedRemoteFrameCount == 0u &&
        predictionMissDelta == 0u &&
        playbackStopDelta == 0u &&
        rollbackScheduledDelta == 0u &&
        !recoveringAssignedPeer;

    if(healthyWindow) {
        m_stableFrameCount += (currentFrame - m_lastEvaluationFrame + kEvaluationWindowFrames);
    } else {
        m_stableFrameCount = 0u;
    }

    const FrameNumber requiredStableFramesForDelayDecrease =
        room.inputDelayFrames >= 6u
            ? FrameNumber{240}
            : (room.inputDelayFrames >= 4u ? FrameNumber{360} : kDelayDecreaseStableFrames);

    if(!cooldownActive &&
       m_stableFrameCount >= requiredStableFramesForDelayDecrease &&
       room.inputDelayFrames > 1u) {
        // After suspend/resume transitions, jitter telemetry can remain high for
        // a while. Let large delay spikes recover faster while keeping low-delay
        // reductions conservative.
        const uint8_t decreaseStep = room.inputDelayFrames >= 6u ? 2u : 1u;
        const uint8_t loweredDelay = static_cast<uint8_t>(
            room.inputDelayFrames > decreaseStep ? room.inputDelayFrames - decreaseStep : 1u
        );
        const uint8_t targetDelay = std::max<uint8_t>(1u, std::min<uint8_t>(loweredDelay, room.inputDelayFrames));
        recommendations.inputDelayFrames = targetDelay;
        m_currentRecommendedDelay = targetDelay;
        m_lastAdjustmentFrame = currentFrame;
        m_stableFrameCount = 0u;
        m_lastDecisionReason =
            "Decreased delay after sustained stable playback to " +
            std::to_string(static_cast<unsigned>(targetDelay)) +
            " (jitter floor " + std::to_string(static_cast<unsigned>(baselineDelay)) + ")";
        return recommendations;
    }

    if(healthyWindow) {
        m_lastDecisionReason = "Stable window; monitoring for gradual delay reduction";
    } else {
        m_lastDecisionReason = "Within tolerance; keeping current delay";
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
