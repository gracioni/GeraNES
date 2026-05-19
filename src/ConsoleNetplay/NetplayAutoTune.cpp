#include "ConsoleNetplay/NetplayAutoTune.h"

#include <algorithm>
#include <cmath>
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
                                                         const NetplayRecoveryStats&,
                                                         uint32_t)
{
    constexpr uint8_t kFixedDelayFrames = 3;

    Recommendations recommendations;
    m_snapshot.enabled = m_enabled;
    m_snapshot.currentRecommendedDelay = kFixedDelayFrames;
    if(!m_enabled) {
        m_snapshot.lastDecisionReason = "Automatic gameplay tuning unavailable on Emscripten";
        return recommendations;
    }

    if(room.inputDelayFrames != kFixedDelayFrames) {
        recommendations.inputDelayFrames = kFixedDelayFrames;
    }

    m_snapshot.lastDecisionReason = "Fixed tuning active: input delay 3";
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

uint8_t NetplayAutoTune::delayForPing(uint32_t fps, double pingMs)
{
    const double pingSeconds = std::max(0.0, pingMs) / 1000.0;
    const uint32_t frames = static_cast<uint32_t>(
        std::lround(static_cast<double>(std::max<uint32_t>(1u, fps)) * pingSeconds)
    );
    return clampDelay(frames);
}

uint32_t NetplayAutoTune::highestPeerPingMs(const RoomState& room) const
{
    uint32_t highestPingMs = 0;
    for(const ParticipantInfo& participant : room.participants) {
        if(!participant.connected || participant.pingMs == 0u) continue;
        highestPingMs = std::max<uint32_t>(highestPingMs, participant.pingMs);
    }
    return highestPingMs;
}

uint8_t NetplayAutoTune::updateSmoothedPingFloor(const RoomState& room, uint32_t fps)
{
    const uint32_t highestPingMs = highestPeerPingMs(room);
    if(highestPingMs == 0u) {
        if(m_smoothedHighestPingMs == 0.0) {
            m_pingFloorDelay = 1u;
        }
        m_highestPingMs = 0u;
        return m_pingFloorDelay;
    }

    if(m_smoothedHighestPingMs <= 0.0) {
        m_smoothedHighestPingMs = static_cast<double>(highestPingMs);
    } else {
        m_smoothedHighestPingMs =
            (m_smoothedHighestPingMs * (1.0 - kHighestPingSmoothingAlpha)) +
            (static_cast<double>(highestPingMs) * kHighestPingSmoothingAlpha);
    }
    m_highestPingMs = highestPingMs;
    m_pingFloorDelay = delayForPing(fps, m_smoothedHighestPingMs);
    return m_pingFloorDelay;
}

void NetplayAutoTune::resetForSession(uint32_t sessionId, SessionState state)
{
    m_lastSessionId = sessionId;
    m_lastState = state;
    m_stableFrameCount = 0;
    m_lastAdjustmentFrame = 0;
    m_lastStableEvaluationFrame = 0;
    m_highestPingMs = 0;
    m_smoothedHighestPingMs = 0.0;
    m_pingFloorDelay = 1;
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
                                                         const NetplayRecoveryStats&,
                                                         uint32_t fps)
{
    Recommendations recommendations;

    if(!m_enabled) {
        m_currentRecommendedDelay = static_cast<uint8_t>(room.inputDelayFrames);
        m_stableFrameCount = 0;
        m_lastStableEvaluationFrame = room.currentFrame;
        m_lastDecisionReason = "Automatic gameplay tuning disabled";
        return recommendations;
    }

    if(m_lastSessionId != room.sessionId || m_lastState != room.state) {
        resetForSession(room.sessionId, room.state);
    }

    const uint8_t pingFloorDelay = updateSmoothedPingFloor(room, fps);

    m_lastSessionId = room.sessionId;
    m_lastState = room.state;
    m_currentRecommendedDelay = clampDelay(room.inputDelayFrames);

    if(room.inputDelayFrames < pingFloorDelay) {
        recommendations.inputDelayFrames = pingFloorDelay;
        m_currentRecommendedDelay = pingFloorDelay;
        m_lastAdjustmentFrame = room.currentFrame;
        m_stableFrameCount = 0;
        m_lastStableEvaluationFrame = room.currentFrame;
        m_lastDecisionReason =
            "Raised delay to ping floor " + std::to_string(static_cast<unsigned>(pingFloorDelay)) +
            " from smoothed highest ping " +
            std::to_string(static_cast<unsigned>(std::lround(m_smoothedHighestPingMs))) + "ms";
        return recommendations;
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

    if(room.inputDelayFrames > pingFloorDelay &&
       m_stableFrameCount >= kDelayDecayStableFrames) {
        const uint8_t targetDelay = clampDelay(static_cast<uint32_t>(room.inputDelayFrames - 1u));
        if(targetDelay >= pingFloorDelay && targetDelay < room.inputDelayFrames) {
            recommendations.inputDelayFrames = targetDelay;
            m_currentRecommendedDelay = targetDelay;
            m_lastAdjustmentFrame = room.currentFrame;
            m_stableFrameCount = 0;
            m_lastDecisionReason =
                "Reduced delay after sustained stable playback to " +
                std::to_string(static_cast<unsigned>(targetDelay)) +
                " (ping floor " + std::to_string(static_cast<unsigned>(pingFloorDelay)) + ")";
            return recommendations;
        }
    }

    m_lastDecisionReason =
        "Stable session; holding current delay, ping floor " +
        std::to_string(static_cast<unsigned>(pingFloorDelay));
    return recommendations;
}

NetplayAutoTune::Recommendations NetplayAutoTune::recommendForImpendingResync(const RoomState& room,
                                                                               ResyncReason reason)
{
    Recommendations recommendations;

    if(!m_enabled) {
        return recommendations;
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
    const uint8_t targetDelay = clampDelay(
        std::max<uint32_t>(static_cast<uint32_t>(m_pingFloorDelay),
                           static_cast<uint32_t>(currentDelay) + 1u)
    );
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
    snapshot.pingFloorDelay = m_pingFloorDelay;
    snapshot.highestPingMs = m_highestPingMs;
    snapshot.smoothedHighestPingMs = m_smoothedHighestPingMs;
    snapshot.stableFrameCount = m_stableFrameCount;
    snapshot.lastAdjustmentFrame = m_lastAdjustmentFrame;
    snapshot.lastDecisionReason = m_lastDecisionReason;
    return snapshot;
}

#endif

} // namespace ConsoleNetplay
