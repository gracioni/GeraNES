#pragma once

#include <cstdint>
#include <string>

#include "NetplayTypes.h"

namespace ConsoleNetplay {

struct NetplayRecoveryStats
{
    uint32_t playbackStopCount = 0;
    uint32_t stopDueToMissingInputCount = 0;
    uint32_t hardResyncCount = 0;
    uint32_t missingInputGapCount = 0;
    FrameNumber lastStopFrame = 0;
    FrameNumber lastDecisionFrame = 0;
    PlayerSlot lastDecisionSlot = kObserverPlayerSlot;
    std::string lastStopReason;
    std::string lastDecision;

    void recordPlaybackStop(FrameNumber frame);
    void recordHardResync();
    void recordMissingInputGap(FrameNumber frame, PlayerSlot slot);
};

} // namespace ConsoleNetplay
