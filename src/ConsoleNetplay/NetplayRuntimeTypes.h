#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "ConsoleNetplay/NetProtocol.h"
#include "ConsoleNetplay/NetplayTypes.h"

namespace ConsoleNetplay {

struct NetplayRomSelection
{
    bool loaded = false;
    std::string gameName;
    RomValidationData validation = {};
};

enum class NetplayManualStateChangeKind
{
    Reset,
    LoadState
};

struct NetplayManualStateChangeRecord
{
    NetplayManualStateChangeKind kind = NetplayManualStateChangeKind::Reset;
    FrameNumber frame = 0;
};

struct NetplayRuntimeDiagnostics
{
    struct TimingStats
    {
        uint64_t count = 0;
        uint64_t totalUs = 0;
        uint64_t maxUs = 0;
        uint64_t lastUs = 0;
        uint64_t recentAverageUs = 0;

        void record(uint64_t elapsedUs)
        {
            lastUs = elapsedUs;
            totalUs += elapsedUs;
            maxUs = std::max(maxUs, elapsedUs);
            recentAverageUs = count == 0
                ? elapsedUs
                : ((recentAverageUs * 7u) + elapsedUs) / 8u;
            ++count;
        }
    };

    struct ByteStats
    {
        uint64_t count = 0;
        uint64_t totalBytes = 0;
        uint64_t maxBytes = 0;
        uint64_t lastBytes = 0;

        void record(size_t bytes)
        {
            const uint64_t value = static_cast<uint64_t>(bytes);
            lastBytes = value;
            totalBytes += value;
            maxBytes = std::max(maxBytes, value);
            ++count;
        }
    };

    struct RollbackStats
    {
        uint32_t rollbackCount = 0;
        uint32_t maxRollbackDistance = 0;
        uint32_t hardResyncCount = 0;
        uint32_t lastRollbackFromFrame = 0;
        uint32_t lastRollbackToFrame = 0;
    };

    bool enabled = false;
    FrameNumber currentFrame = 0;
    size_t snapshotCapacity = 0;
    size_t storedSnapshots = 0;
    uint32_t latestSnapshotCrc32 = 0;
    TimingStats netplayStateSaveTiming;
    TimingStats netplayRollbackSnapshotSaveTiming;
    TimingStats netplayCrcTiming;
    TimingStats rollbackLoadTiming;
    ByteStats netplayStateSerializedBytes;
    ByteStats netplayRollbackSnapshotSerializedBytes;
    ByteStats snapshotLookupCopyBytes;
    ByteStats rollbackSnapshotCopyBytes;
    ByteStats seededSnapshotCopyBytes;
    RollbackStats rollbackStats;
};

} // namespace ConsoleNetplay
