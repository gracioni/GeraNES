#include "ConsoleNetplay/Diagnostics.h"

namespace ConsoleNetplay {

void NetplayRecoveryStats::recordPlaybackStop(FrameNumber frame)
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

void NetplayRecoveryStats::recordHardResync()
{
    ++hardResyncCount;
}

void NetplayRecoveryStats::recordMissingInputGap(FrameNumber frame, PlayerSlot slot)
{
    ++missingInputGapCount;
    lastDecisionFrame = frame;
    lastDecisionSlot = slot;
    lastDecision = "Missing input gap, waiting";
}

} // namespace ConsoleNetplay
