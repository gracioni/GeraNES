#include "ConsoleNetplay/SelfStallDetector.h"

#include <algorithm>
#include <sstream>

namespace ConsoleNetplay {

void SelfStallDetector::reset()
{
    m_progressBaseline.reset();
    m_lastProgressAt = {};
    m_cooldownUntil = {};
}

SelfStallDetector::UpdateResult SelfStallDetector::update(const Snapshot& snapshot,
                                                          std::chrono::steady_clock::time_point now)
{
    UpdateResult result;
    if(!isEligible(snapshot)) {
        reset();
        return result;
    }

    const ProgressSample current = makeSample(snapshot);
    if(!m_progressBaseline.has_value() ||
       m_progressBaseline->timelineEpoch != current.timelineEpoch) {
        m_progressBaseline = current;
        m_lastProgressAt = now;
        return result;
    }

    if(hasForwardProgress(*m_progressBaseline, current)) {
        m_progressBaseline = current;
        m_lastProgressAt = now;
        return result;
    }

    if(current.confirmedFrame >= current.localSimulationFrame &&
       current.maxRemoteReportedConfirmedFrame >= current.confirmedFrame) {
        m_progressBaseline = current;
        m_lastProgressAt = now;
        return result;
    }

    if(hostIsWaitingWithinRemoteTolerance(snapshot, current)) {
        m_progressBaseline = current;
        m_lastProgressAt = now;
        return result;
    }

    if(now < m_cooldownUntil) {
        return result;
    }

    const auto stalledFor =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastProgressAt);
    const uint32_t churn = churnSince(*m_progressBaseline, current);
    if(stalledFor < kStallTimeout && churn < kChurnThreshold) {
        return result;
    }

    std::ostringstream oss;
    oss << "self_progress_stall"
        << " stalledMs=" << stalledFor.count()
        << " localFrame=" << current.localSimulationFrame
        << " confirmedFrame=" << current.confirmedFrame
        << " remoteCurrent=" << current.maxRemoteReportedCurrentFrame
        << " remoteConfirmed=" << current.maxRemoteReportedConfirmedFrame
        << " playbackStopsDelta="
        << (current.playbackStopCount - m_progressBaseline->playbackStopCount)
        << " rollbackScheduledDelta="
        << (current.rollbackScheduledCount - m_progressBaseline->rollbackScheduledCount)
        << " connectedRemotes=" << snapshot.connectedRemoteParticipantCount;
    result.shouldResync = true;
    result.detail = oss.str();

    m_progressBaseline = current;
    m_lastProgressAt = now;
    m_cooldownUntil = now + kRecoveryCooldown;
    return result;
}

bool SelfStallDetector::isEligible(const Snapshot& snapshot)
{
    return snapshot.active &&
           snapshot.sessionState == SessionState::Running &&
           snapshot.recoveryInputMode == RecoveryInputMode::Normal &&
           snapshot.activeResyncId == 0 &&
           snapshot.pendingResyncAckCount == 0 &&
           snapshot.connectedRemoteParticipantCount > 0;
}

SelfStallDetector::ProgressSample SelfStallDetector::makeSample(const Snapshot& snapshot)
{
    return ProgressSample{
        snapshot.timelineEpoch,
        snapshot.localSimulationFrame,
        snapshot.confirmedFrame,
        snapshot.maxRemoteReportedCurrentFrame,
        snapshot.maxRemoteReportedConfirmedFrame,
        snapshot.inputDelayFrames,
        snapshot.predictFrames,
        snapshot.playbackStopCount,
        snapshot.rollbackScheduledCount
    };
}

bool SelfStallDetector::hasForwardProgress(const ProgressSample& baseline, const ProgressSample& current)
{
    return current.localSimulationFrame > baseline.localSimulationFrame ||
           current.confirmedFrame > baseline.confirmedFrame;
}

uint32_t SelfStallDetector::churnSince(const ProgressSample& baseline, const ProgressSample& current)
{
    const uint32_t playbackStopDelta = current.playbackStopCount - baseline.playbackStopCount;
    const uint32_t rollbackScheduledDelta =
        current.rollbackScheduledCount - baseline.rollbackScheduledCount;
    return std::max(playbackStopDelta, rollbackScheduledDelta);
}

bool SelfStallDetector::hostIsWaitingWithinRemoteTolerance(const Snapshot& snapshot,
                                                           const ProgressSample& current)
{
    if(!snapshot.hosting || snapshot.connectedRemoteParticipantCount == 0u) {
        return false;
    }

    const FrameNumber toleranceFrames =
        std::max<FrameNumber>(6u, snapshot.inputDelayFrames + snapshot.predictFrames + 2u);
    const FrameNumber remoteCurrentLag =
        current.localSimulationFrame > current.maxRemoteReportedCurrentFrame
            ? (current.localSimulationFrame - current.maxRemoteReportedCurrentFrame)
            : 0u;
    const FrameNumber remoteConfirmedLag =
        current.confirmedFrame > current.maxRemoteReportedConfirmedFrame
            ? (current.confirmedFrame - current.maxRemoteReportedConfirmedFrame)
            : 0u;

    return remoteCurrentLag <= toleranceFrames &&
           remoteConfirmedLag <= toleranceFrames;
}

} // namespace ConsoleNetplay
