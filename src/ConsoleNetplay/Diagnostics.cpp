#include "ConsoleNetplay/Diagnostics.h"

namespace ConsoleNetplay {

void RollbackStats::recordPlaybackStop(FrameNumber frame)
{
    const char* reason = "Playback stopped: missing input";
    if(lastStopFrame == frame && lastStopReason == reason) {
        return;
    }

    ++playbackStopCount;
    ++stopDueToMissingInputCount;

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
