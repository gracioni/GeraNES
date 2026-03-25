#pragma once

#include <cstdint>
#include <string>

#include "NetplayTypes.h"

namespace Netplay {

struct RollbackStats
{
    uint32_t rollbackCount = 0;
    uint32_t maxRollbackDistance = 0;
    uint32_t predictionHitCount = 0;
    uint32_t predictionMissCount = 0;
    uint32_t hardResyncCount = 0;
    uint32_t rollbackScheduledCount = 0;
    uint32_t missingInputGapCount = 0;
    uint32_t futureFrameMismatchCount = 0;
    uint32_t confirmedFrameConflictCount = 0;
    FrameNumber lastRollbackFromFrame = 0;
    FrameNumber lastRollbackToFrame = 0;
    FrameNumber lastDecisionFrame = 0;
    PlayerSlot lastDecisionSlot = kObserverPlayerSlot;
    std::string lastDecision;

    void recordPrediction(bool hit)
    {
        if(hit) ++predictionHitCount;
        else ++predictionMissCount;
    }

    void recordRollback(FrameNumber fromFrame, FrameNumber toFrame)
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

    void recordHardResync()
    {
        ++hardResyncCount;
    }

    void recordRollbackScheduled(FrameNumber frame, PlayerSlot slot)
    {
        ++rollbackScheduledCount;
        lastDecisionFrame = frame;
        lastDecisionSlot = slot;
        lastDecision = "Rollback scheduled";
    }

    void recordMissingInputGap(FrameNumber frame, PlayerSlot slot)
    {
        ++missingInputGapCount;
        lastDecisionFrame = frame;
        lastDecisionSlot = slot;
        lastDecision = "Missing input gap, waiting";
    }

    void recordFutureFrameMismatch(FrameNumber frame, PlayerSlot slot)
    {
        ++futureFrameMismatchCount;
        lastDecisionFrame = frame;
        lastDecisionSlot = slot;
        lastDecision = "Future-frame mismatch, no rollback";
    }

    void recordConfirmedFrameConflict(FrameNumber frame, PlayerSlot slot)
    {
        ++confirmedFrameConflictCount;
        lastDecisionFrame = frame;
        lastDecisionSlot = slot;
        lastDecision = "Confirmed-frame conflict, hard resync required";
    }
};

} // namespace Netplay
