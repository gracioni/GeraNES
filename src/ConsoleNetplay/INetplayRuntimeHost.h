#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ConsoleNetplay/NetProtocol.h"
#include "ConsoleNetplay/NetplayInputFrame.h"
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
        uint32_t predictionHitCount = 0;
        uint32_t predictionMissCount = 0;
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

struct NetplayReplayFrameInput
{
    NetplayInputFrame frameOverride;
    bool speculative = false;
    bool hasFrameOverride = false;
};

class INetplayEmulator
{
public:
    enum class StateLoadAudioPolicy
    {
        ResetOutput,
        PreserveContinuousOutput
    };

    virtual ~INetplayEmulator() = default;

    virtual bool valid() const = 0;
    virtual FrameNumber frameCount() const = 0;
    virtual uint32_t regionFps() const = 0;
    virtual uint32_t inputTimelineEpoch() const = 0;
    virtual void setInputTimelineEpoch(uint32_t epoch) = 0;
    virtual bool hasQueuedInputFrame(FrameNumber frame, uint32_t timelineEpoch) const = 0;
    virtual bool queueInputFrame(const NetplayInputFrame& inputFrame) = 0;
    virtual NetplayInputFrame createNeutralInputFrame(FrameNumber frame) const = 0;
    virtual void discardQueuedInputFramesAfter(FrameNumber frame) = 0;
    virtual bool updateUntilFrame(uint32_t frameDtMs, bool resimulating) = 0;
    virtual bool rollbackToFrame(FrameNumber frame) = 0;
    virtual bool resimulateToFrame(FrameNumber frame, const std::function<NetplayReplayFrameInput(FrameNumber)>& resolver) = 0;
    virtual std::vector<uint8_t> saveNetplayStateToMemory() = 0;
    virtual bool loadStateFromMemoryOnCleanBoot(const std::vector<uint8_t>& data) = 0;
    virtual bool loadStateFromMemoryWithAudioPolicy(const std::vector<uint8_t>& data, StateLoadAudioPolicy policy) = 0;
    virtual uint32_t canonicalNetplayStateCrc32() = 0;
    virtual std::optional<NetplayRomSelection> currentRomSelection() const = 0;
    virtual void configureInputBufferCapacity(size_t capacity) = 0;
};

class INetplayRuntimeHost
{
public:
    using ReplayFrameResolver = std::function<bool(FrameNumber, NetplayReplayFrameInput&)>;
    using EmulationCommand = std::function<void(INetplayEmulator&)>;

    virtual ~INetplayRuntimeHost() = default;

    virtual void postNetplayCommand(EmulationCommand command) = 0;
    virtual void beginPresentationHoldUntilNextFrameReady() = 0;
    virtual void discardQueuedNetplayInputsAfter(FrameNumber frame) = 0;
    virtual std::vector<NetplayManualStateChangeRecord> consumeManualStateChanges() = 0;
    virtual void restartAudio() = 0;
    virtual void discardQueuedAudio() = 0;
    virtual void setSimulationSuspended(bool suspended) = 0;
    virtual void setAutoQueuePendingInputOnFrameStart(bool enabled) = 0;
    virtual void setAllowPresenterTimeoutAdvance(bool enabled) = 0;
    virtual void setFrameInputResolver(ReplayFrameResolver resolver) = 0;
    virtual void configureNetplaySnapshots(size_t snapshotCapacity) = 0;
    virtual FrameNumber lastFrameReadyFrame() const = 0;
    virtual uint32_t lastFrameReadyNetplayCrc32() const = 0;
    virtual void setAuthoritativeFrameReadyState(FrameNumber frame, uint32_t canonicalCrc32) = 0;
    virtual std::optional<std::shared_ptr<const std::vector<uint8_t>>> netplaySnapshotForFrame(FrameNumber frame) const = 0;
    virtual std::optional<uint32_t> netplaySnapshotCrc32ForFrame(FrameNumber frame) const = 0;
    virtual bool updateNetplaySnapshotCrc32ForFrame(FrameNumber frame, uint32_t canonicalCrc32) = 0;
    virtual void seedNetplaySnapshot(FrameNumber frame,
                                     const std::vector<uint8_t>& data,
                                     std::optional<uint32_t> canonicalCrc32 = std::nullopt) = 0;
    virtual NetplayRuntimeDiagnostics getNetplayDiagnostics() const = 0;
};

} // namespace ConsoleNetplay
