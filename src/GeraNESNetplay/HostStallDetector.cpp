#include "GeraNESNetplay/HostStallDetector.h"

#include <algorithm>
#include <sstream>

namespace Netplay {

void HostStallDetector::reset()
{
    m_progressBaseline.reset();
    m_lastProgressAt = {};
    m_cooldownUntil = {};
}

HostStallDetector::UpdateResult HostStallDetector::update(const Snapshot& snapshot,
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
    oss << "host_progress_stall"
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

bool HostStallDetector::isEligible(const Snapshot& snapshot)
{
    return snapshot.active &&
           snapshot.hosting &&
           snapshot.sessionState == SessionState::Running &&
           snapshot.recoveryInputMode == RecoveryInputMode::Normal &&
           snapshot.activeResyncId == 0 &&
           snapshot.pendingResyncAckCount == 0 &&
           snapshot.connectedRemoteParticipantCount > 0;
}

HostStallDetector::ProgressSample HostStallDetector::makeSample(const Snapshot& snapshot)
{
    return ProgressSample{
        snapshot.timelineEpoch,
        snapshot.localSimulationFrame,
        snapshot.confirmedFrame,
        snapshot.maxRemoteReportedCurrentFrame,
        snapshot.maxRemoteReportedConfirmedFrame,
        snapshot.playbackStopCount,
        snapshot.rollbackScheduledCount
    };
}

bool HostStallDetector::hasForwardProgress(const ProgressSample& baseline, const ProgressSample& current)
{
    return current.localSimulationFrame > baseline.localSimulationFrame ||
           current.confirmedFrame > baseline.confirmedFrame ||
           current.maxRemoteReportedCurrentFrame > baseline.maxRemoteReportedCurrentFrame ||
           current.maxRemoteReportedConfirmedFrame > baseline.maxRemoteReportedConfirmedFrame;
}

uint32_t HostStallDetector::churnSince(const ProgressSample& baseline, const ProgressSample& current)
{
    const uint32_t playbackStopDelta = current.playbackStopCount - baseline.playbackStopCount;
    const uint32_t rollbackScheduledDelta =
        current.rollbackScheduledCount - baseline.rollbackScheduledCount;
    return std::max(playbackStopDelta, rollbackScheduledDelta);
}

} // namespace Netplay
