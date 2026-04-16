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
    m_snapshot.enabled = m_enabled;
    m_snapshot.currentRecommendedDelay = static_cast<uint8_t>(room.inputDelayFrames);
    m_snapshot.currentFixedPredict = static_cast<uint8_t>(room.predictFrames);
    if(m_enabled) {
        m_snapshot.lastDecisionReason.clear();
    }
    return {};
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
    Recommendations recommendations;

    if(room.sessionId != m_lastSessionId ||
       room.timelineEpoch != m_lastTimelineEpoch) {
        resetForSession(room.sessionId, room.timelineEpoch, room.state);
    }

    if(!m_enabled) {
        m_lastState = room.state;
        return recommendations;
    }

    const uint8_t worstJitterFrames = jitterFramesForRoom(room, fps);
    const uint8_t preSessionDelay = clampDelay(std::max<uint32_t>(room.inputDelayFrames, worstJitterFrames + 1u));
    const uint8_t preSessionPredict =
        std::max<uint8_t>(
            clampPredict(std::max<uint32_t>(1u, preSessionDelay + 1u)),
            predictionBaselineForRoom(room, fps)
        );

    if(room.state == SessionState::Lobby ||
       room.state == SessionState::ValidatingRom ||
       room.state == SessionState::ReadyCheck ||
       room.state == SessionState::Starting) {
        m_currentRecommendedDelay = preSessionDelay;
        if(!m_predictLocked) {
            m_currentFixedPredict = preSessionPredict;
        }
        if(room.inputDelayFrames != m_currentRecommendedDelay) {
            recommendations.inputDelayFrames = m_currentRecommendedDelay;
            m_lastDecisionReason =
                "Auto delay set to " + std::to_string(static_cast<unsigned>(m_currentRecommendedDelay)) +
                " from connection jitter";
            m_lastAdjustmentFrame = room.currentFrame;
        }
        if(room.predictFrames != m_currentFixedPredict) {
            recommendations.predictFrames = m_currentFixedPredict;
            m_lastDecisionReason =
                "Auto predict fixed at " + std::to_string(static_cast<unsigned>(m_currentFixedPredict)) +
                " for this session";
        }
        m_lastState = room.state;
        return recommendations;
    }

    if(room.state == SessionState::Paused ||
       room.state == SessionState::Resyncing) {
        m_currentRecommendedDelay = room.inputDelayFrames;
        if(!m_predictLocked) {
            m_currentFixedPredict = room.predictFrames > 0 ? room.predictFrames : preSessionPredict;
            m_predictLocked = true;
        }
        m_runningWindowInitialized = false;
        m_lastState = room.state;
        return recommendations;
    }

    if(room.state == SessionState::Running) {
        if(!m_predictLocked) {
            m_currentFixedPredict = room.predictFrames > 0 ? room.predictFrames : preSessionPredict;
            m_predictLocked = true;
        }

        m_currentRecommendedDelay = room.inputDelayFrames;

        if(!m_runningWindowInitialized) {
            resetRunningWindow(stats, room.currentFrame);
            m_lastState = room.state;
            return recommendations;
        }

        const FrameNumber framesSinceEval = room.currentFrame - m_lastEvaluationFrame;
        if(framesSinceEval < kEvaluationWindowFrames) {
            m_lastState = room.state;
            return recommendations;
        }

        const uint32_t deltaMisses = stats.predictionMissCount - m_lastPredictionMissCount;
        const uint32_t deltaStops = stats.playbackStopCount - m_lastPlaybackStopCount;
        const uint32_t deltaPredictionLimitStops =
            stats.stopDueToPredictionLimitCount - m_lastPredictionLimitStopCount;
        const uint32_t deltaMissingInputStops =
            stats.stopDueToMissingInputCount - m_lastMissingInputStopCount;
        const uint32_t deltaRollbacks = stats.rollbackScheduledCount - m_lastRollbackScheduledCount;
        const uint32_t deltaPredictedUses = stats.predictedFrameUseCount - m_lastPredictedFrameUseCount;

        m_lastPredictionMissCount = stats.predictionMissCount;
        m_lastPlaybackStopCount = stats.playbackStopCount;
        m_lastPredictionLimitStopCount = stats.stopDueToPredictionLimitCount;
        m_lastMissingInputStopCount = stats.stopDueToMissingInputCount;
        m_lastRollbackScheduledCount = stats.rollbackScheduledCount;
        m_lastPredictedFrameUseCount = stats.predictedFrameUseCount;
        m_lastEvaluationFrame = room.currentFrame;

        const bool severeStop = deltaStops > 0;
        const bool severeMiss =
            deltaPredictedUses >= 4u &&
            (deltaMisses * 100u) >= (deltaPredictedUses * 10u);
        const bool severeRollback = deltaRollbacks >= 8u;
        const bool severeJitter = (worstJitterFrames + 1u) > room.inputDelayFrames;
        const bool unresolvedPressure = unresolvedPredictedRemoteFrameCount >= room.predictFrames && room.predictFrames > 0;
        const bool canAdjustNow =
            room.currentFrame >= m_lastAdjustmentFrame + kAdjustmentCooldownFrames;
        const uint8_t desiredPredictBaseline = predictionBaselineForRoom(room, fps);
        const bool predictPressure =
            deltaPredictionLimitStops > 0u ||
            (unresolvedPressure && !severeMiss && !severeRollback && !severeJitter);

        if(canAdjustNow &&
           predictPressure &&
           room.predictFrames < kMaxAutoPredictFrames &&
           deltaMissingInputStops == 0u) {
            const uint8_t raisedPredict = clampPredict(std::max<uint32_t>(
                room.predictFrames + 1u,
                desiredPredictBaseline
            ));
            if(raisedPredict != room.predictFrames) {
                recommendations.predictFrames = raisedPredict;
                m_currentFixedPredict = raisedPredict;
                m_lastAdjustmentFrame = room.currentFrame;
                m_stableFrameCount = 0;
                if(deltaPredictionLimitStops > 0u) {
                    m_lastDecisionReason = "Auto predict increased after prediction-limit stops";
                } else {
                    m_lastDecisionReason = "Auto predict increased from prediction window pressure";
                }
                m_lastState = room.state;
                return recommendations;
            }
        }

        if(canAdjustNow && (severeStop || severeMiss || severeRollback || severeJitter || unresolvedPressure)) {
            const uint8_t raisedDelay = clampDelay(std::max<uint32_t>(
                room.inputDelayFrames + 1u,
                static_cast<uint32_t>(worstJitterFrames) + 1u
            ));
            if(raisedDelay != room.inputDelayFrames) {
                recommendations.inputDelayFrames = raisedDelay;
                m_currentRecommendedDelay = raisedDelay;
                m_lastAdjustmentFrame = room.currentFrame;
                m_stableFrameCount = 0;
                if(severeStop) {
                    if(deltaMissingInputStops > 0u) {
                        m_lastDecisionReason = "Auto delay increased after missing-input stops";
                    } else if(deltaPredictionLimitStops > 0u) {
                        m_lastDecisionReason = "Auto delay increased after persistent prediction pressure";
                    } else {
                        m_lastDecisionReason = "Auto delay increased after playback stops";
                    }
                } else if(severeMiss) {
                    m_lastDecisionReason = "Auto delay increased after prediction misses";
                } else if(severeRollback) {
                    m_lastDecisionReason = "Auto delay increased after frequent rollback corrections";
                } else if(unresolvedPressure) {
                    m_lastDecisionReason = "Auto delay increased after prediction window pressure";
                } else {
                    m_lastDecisionReason = "Auto delay increased from connection jitter";
                }
            }
            m_lastState = room.state;
            return recommendations;
        }

        const bool stableWindow =
            deltaStops == 0u &&
            deltaMisses == 0u &&
            deltaRollbacks <= 1u &&
            unresolvedPredictedRemoteFrameCount == 0u &&
            (worstJitterFrames + 1u) < room.inputDelayFrames;

        if(stableWindow) {
            m_stableFrameCount += framesSinceEval;
        } else {
            m_stableFrameCount = 0;
        }

        if(canAdjustNow &&
           room.predictFrames > desiredPredictBaseline &&
           m_stableFrameCount >= kPredictDecreaseStableFrames) {
            const uint8_t loweredPredict =
                std::max<uint8_t>(desiredPredictBaseline, static_cast<uint8_t>(room.predictFrames - 1u));
            if(loweredPredict != room.predictFrames) {
                recommendations.predictFrames = loweredPredict;
                m_currentFixedPredict = loweredPredict;
                m_lastAdjustmentFrame = room.currentFrame;
                m_stableFrameCount = 0;
                m_lastDecisionReason = "Auto predict reduced after stable session period";
                m_lastState = room.state;
                return recommendations;
            }
        }

        if(canAdjustNow &&
           room.inputDelayFrames > 0 &&
           m_stableFrameCount >= kDelayDecreaseStableFrames) {
            const uint8_t loweredDelay = static_cast<uint8_t>(room.inputDelayFrames - 1u);
            recommendations.inputDelayFrames = loweredDelay;
            m_currentRecommendedDelay = loweredDelay;
            m_lastAdjustmentFrame = room.currentFrame;
            m_stableFrameCount = 0;
            m_lastDecisionReason = "Auto delay reduced after stable session period";
        }
    }

    m_lastState = room.state;
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
