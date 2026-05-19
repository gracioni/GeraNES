#pragma once

#include <cstdint>
#include <string>

#include "NetplayTypes.h"

namespace ConsoleNetplay {

struct RollbackStats
{
    uint32_t rollbackCount = 0;
    uint32_t maxRollbackDistance = 0;
    uint32_t playbackStopCount = 0;
    uint32_t stopDueToMissingInputCount = 0;
    uint32_t hardResyncCount = 0;
    uint32_t rollbackScheduledCount = 0;
    uint32_t missingInputGapCount = 0;
    uint32_t futureFrameMismatchCount = 0;
    uint32_t confirmedFrameConflictCount = 0;
    FrameNumber lastRollbackFromFrame = 0;
    FrameNumber lastRollbackToFrame = 0;
    FrameNumber lastStopFrame = 0;
    FrameNumber lastDecisionFrame = 0;
    PlayerSlot lastDecisionSlot = kObserverPlayerSlot;
    std::string lastStopReason;
    std::string lastDecision;

    void recordPlaybackStop(FrameNumber frame);
    void recordRollback(FrameNumber fromFrame, FrameNumber toFrame);
    void recordHardResync();
    void recordRollbackScheduled(FrameNumber frame, PlayerSlot slot);
    void recordMissingInputGap(FrameNumber frame, PlayerSlot slot);
    void recordFutureFrameMismatch(FrameNumber frame, PlayerSlot slot);
    void recordConfirmedFrameConflict(FrameNumber frame, PlayerSlot slot);
};

} // namespace ConsoleNetplay
