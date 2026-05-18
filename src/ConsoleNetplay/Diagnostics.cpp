#include "ConsoleNetplay/Diagnostics.h"

namespace ConsoleNetplay {

void RollbackStats::recordPrediction(bool hit)
{
    if(hit) ++predictionHitCount;
    else ++predictionMissCount;
}

void RollbackStats::recordPredictedFrameUse(FrameNumber frame, PlayerSlot slot)
{
    if(lastDecision == "Prediction used" &&
       lastPredictedFrame == frame &&
       lastDecisionSlot == slot) {
        return;
    }
    ++predictedFrameUseCount;
    lastPredictedFrame = frame;
    lastDecisionFrame = frame;
    lastDecisionSlot = slot;
    lastDecision = "Prediction used";
}

void RollbackStats::recordPlaybackStop(FrameNumber frame, bool predictionLimitReached)
{
    const char* reason =
        predictionLimitReached
            ? "Playback stopped: prediction limit reached"
            : "Playback stopped: missing input";
    if(lastStopFrame == frame && lastStopReason == reason) {
        return;
    }

    ++playbackStopCount;
    if(predictionLimitReached) {
        ++stopDueToPredictionLimitCount;
    } else {
        ++stopDueToMissingInputCount;
    }

    lastStopFrame = frame;
    lastStopReason = reason;
    lastDecisionFrame = frame;
    lastDecisionSlot = kObserverPlayerSlot;
    lastDecision = reason;
}

void RollbackStats::recordRollback(FrameNumber fromFrame, FrameNumber toFrame)
{
    ++rollbackCount;
    lastRollbackFromFrame = fromFrame;
    lastRollbackToFrame = toFrame;

    if(toFrame >= fromFrame) {
        const uint32_t distance = toFrame - fromFrame;
        if(distance > maxRollbackDistance) {
            maxRollbackDistance = distance;
        }
    }
}

void RollbackStats::recordHardResync()
{
    ++hardResyncCount;
}

void RollbackStats::recordRollbackScheduled(FrameNumber frame, PlayerSlot slot)
{
    ++rollbackScheduledCount;
    lastDecisionFrame = frame;
    lastDecisionSlot = slot;
    lastDecision = "Rollback scheduled";
}

void RollbackStats::recordConfirmedReplayScheduled(FrameNumber frame, PlayerSlot slot)
{
    ++confirmedReplayScheduledCount;
    lastDecisionFrame = frame;
    lastDecisionSlot = slot;
    lastDecision = "Confirmed replay scheduled";
}

void RollbackStats::recordPredictionMismatchRollbackScheduled(FrameNumber frame, PlayerSlot slot)
{
    ++predictionMismatchRollbackScheduledCount;
    lastDecisionFrame = frame;
    lastDecisionSlot = slot;
    lastDecision = "Prediction mismatch rollback scheduled";
}

void RollbackStats::recordMissingInputGap(FrameNumber frame, PlayerSlot slot)
{
    ++missingInputGapCount;
    lastDecisionFrame = frame;
    lastDecisionSlot = slot;
    lastDecision = "Missing input gap, waiting";
}

void RollbackStats::recordFutureFrameMismatch(FrameNumber frame, PlayerSlot slot)
{
    ++futureFrameMismatchCount;
    lastDecisionFrame = frame;
    lastDecisionSlot = slot;
    lastDecision = "Future-frame mismatch, no rollback";
}

void RollbackStats::recordConfirmedFrameConflict(FrameNumber frame, PlayerSlot slot)
{
    ++confirmedFrameConflictCount;
    lastDecisionFrame = frame;
    lastDecisionSlot = slot;
    lastDecision = "Confirmed-frame conflict, hard resync required";
}

} // namespace ConsoleNetplay
