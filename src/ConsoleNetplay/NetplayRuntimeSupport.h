#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ConsoleNetplay/ConfirmedInputBufferDriver.h"
#include "ConsoleNetplay/INetplayConsole.h"
#include "ConsoleNetplay/NetplayRuntimeTypes.h"
#include "ConsoleNetplay/NetplayAutoTune.h"
#include "ConsoleNetplay/NetplayCoordinator.h"
#include "ConsoleNetplay/SelfStallDetector.h"

namespace ConsoleNetplay {

struct RuntimeRomValidationState
{
    std::string lastSelectedRomKey;
    std::string lastSubmittedValidationKey;
    std::string stickyStatusMessage;
};

struct RuntimeRomValidationResult
{
    bool disconnectedForMismatch = false;
    bool selectedRomChanged = false;
    bool submittedValidationChanged = false;
    std::string stickyStatusMessage;
};

struct RuntimeInputDelaySettings
{
    bool debugMode = false;
    uint32_t gameplayReceiveDelayMs = 0;
    bool autoGameplayTuning = false;
    uint32_t manualInputDelayFrames = 0;
    uint32_t manualPredictFrames = 0;
    uint32_t regionFps = 60;
};

struct RuntimeInputDelayResult
{
    uint32_t inputDelayFrames = 0;
    uint32_t predictFrames = 0;
    size_t inputBufferCapacity = 64;
};

class INetplayStateBridge
{
public:
    virtual ~INetplayStateBridge() = default;

    virtual bool valid() const = 0;
    virtual FrameNumber frameCount() const = 0;
    virtual uint32_t inputTimelineEpoch() const = 0;
    virtual void setInputTimelineEpoch(uint32_t epoch) = 0;
    virtual void discardQueuedInputFramesAfter(FrameNumber frame) = 0;
    virtual bool loadStateFromMemoryOnCleanBoot(const std::vector<uint8_t>& payload) = 0;
    virtual std::vector<uint8_t> saveNetplayStateToMemory() = 0;
    virtual uint32_t canonicalNetplayStateCrc32() = 0;
};

class INetplayStateHostBridge
{
public:
    virtual ~INetplayStateHostBridge() = default;

    virtual void beginPresentationHoldUntilNextFrameReady() = 0;
    virtual void discardQueuedNetplayInputsAfter(FrameNumber frame) = 0;
    virtual FrameNumber lastFrameReadyFrame() const = 0;
    virtual uint32_t lastFrameReadyNetplayCrc32() const = 0;
    virtual std::optional<std::shared_ptr<const std::vector<uint8_t>>> netplaySnapshotForFrame(FrameNumber frame) const = 0;
    virtual std::optional<uint32_t> netplaySnapshotCrc32ForFrame(FrameNumber frame) const = 0;
    virtual bool updateNetplaySnapshotCrc32ForFrame(FrameNumber frame, uint32_t canonicalCrc32) = 0;
    virtual void seedNetplaySnapshot(FrameNumber frame,
                                     const std::vector<uint8_t>& data,
                                     std::optional<uint32_t> canonicalCrc32 = std::nullopt) = 0;
    virtual void setAuthoritativeFrameReadyState(FrameNumber frame, uint32_t canonicalCrc32) = 0;
};

template<typename EmulatorHost>
class NetplayStateBridgeAdapter final : public INetplayStateBridge
{
public:
    explicit NetplayStateBridgeAdapter(EmulatorHost& host) : m_host(host) {}

    bool valid() const override { return m_host.valid(); }
    FrameNumber frameCount() const override { return m_host.frameCount(); }
    uint32_t inputTimelineEpoch() const override { return m_host.inputTimelineEpoch(); }
    void setInputTimelineEpoch(uint32_t epoch) override { m_host.setInputTimelineEpoch(epoch); }
    void discardQueuedInputFramesAfter(FrameNumber frame) override { m_host.discardQueuedInputFramesAfter(frame); }
    bool loadStateFromMemoryOnCleanBoot(const std::vector<uint8_t>& payload) override
    {
        return m_host.loadStateFromMemoryOnCleanBoot(payload);
    }
    std::vector<uint8_t> saveNetplayStateToMemory() override { return m_host.saveNetplayStateToMemory(); }
    uint32_t canonicalNetplayStateCrc32() override { return m_host.canonicalNetplayStateCrc32(); }

private:
    EmulatorHost& m_host;
};

template<typename RuntimeHost>
class NetplayStateHostBridgeAdapter final : public INetplayStateHostBridge
{
public:
    explicit NetplayStateHostBridgeAdapter(RuntimeHost& host) : m_host(host) {}

    void beginPresentationHoldUntilNextFrameReady() override
    {
        m_host.beginPresentationHoldUntilNextFrameReady();
    }

    void discardQueuedNetplayInputsAfter(FrameNumber frame) override
    {
        m_host.discardQueuedNetplayInputsAfter(frame);
    }

    FrameNumber lastFrameReadyFrame() const override
    {
        return m_host.lastFrameReadyFrame();
    }

    uint32_t lastFrameReadyNetplayCrc32() const override
    {
        return m_host.lastFrameReadyNetplayCrc32();
    }

    std::optional<std::shared_ptr<const std::vector<uint8_t>>> netplaySnapshotForFrame(FrameNumber frame) const override
    {
        return m_host.netplaySnapshotForFrame(frame);
    }

    std::optional<uint32_t> netplaySnapshotCrc32ForFrame(FrameNumber frame) const override
    {
        return m_host.netplaySnapshotCrc32ForFrame(frame);
    }

    bool updateNetplaySnapshotCrc32ForFrame(FrameNumber frame, uint32_t canonicalCrc32) override
    {
        return m_host.updateNetplaySnapshotCrc32ForFrame(frame, canonicalCrc32);
    }

    void seedNetplaySnapshot(FrameNumber frame,
                             const std::vector<uint8_t>& data,
                             std::optional<uint32_t> canonicalCrc32 = std::nullopt) override
    {
        m_host.seedNetplaySnapshot(frame, data, canonicalCrc32);
    }

    void setAuthoritativeFrameReadyState(FrameNumber frame, uint32_t canonicalCrc32) override
    {
        m_host.setAuthoritativeFrameReadyState(frame, canonicalCrc32);
    }

private:
    RuntimeHost& m_host;
};

struct RuntimeAuthoritativeStateResult
{
    bool started = false;
    bool localStateApplied = false;
    FrameNumber loadedAuthoritativeFrame = 0;
    FrameNumber reanchorFrame = 0;
    uint32_t stateCrc32 = 0;
};

struct RuntimePeriodicCrcState
{
    FrameNumber lastSubmittedLocalCrcFrame = 0;
    FrameNumber nextScheduledLocalCrcFrame = kDesyncCrcIntervalFrames;
    FrameNumber lastLoadedAuthoritativeFrame = 0;
    FrameNumber postRecoveryRapidCrcThroughFrame = 0;
    bool forceNextConfirmedCrcSubmission = false;
};

struct RuntimePeriodicCrcResult
{
    bool submitted = false;
    FrameNumber submittedFrame = 0;
    uint32_t submittedCrc32 = 0;
};

struct RuntimeAutoStartResult
{
    bool started = false;
    bool initialSyncNeeded = false;
};

struct RuntimeHostResyncProcessResult
{
    bool started = false;
    bool localStateApplied = false;
    bool initialSessionSync = false;
    bool lateJoinSync = false;
    FrameNumber requestedFrame = 0;
    FrameNumber authoritativeFrame = 0;
    FrameNumber loadedAuthoritativeFrame = 0;
    FrameNumber reanchorFrame = 0;
    ResyncReason reason = ResyncReason::Unspecified;
    ParticipantId targetParticipantId = kInvalidParticipantId;
};

struct RuntimePendingResyncApplyResult
{
    bool consumed = false;
    bool loadedExpectedFrame = false;
    uint32_t resyncId = 0;
    FrameNumber targetFrame = 0;
    FrameNumber loadedFrame = 0;
    FrameNumber loadedAuthoritativeFrame = 0;
    FrameNumber reanchorFrame = 0;
    uint32_t loadedCrc32 = 0;
};

struct RuntimeRollbackProcessState
{
    FrameNumber lastRollbackTargetFrame = 0;
    FrameNumber lastMissingRollbackSnapshotFrame = 0;
    FrameNumber lastMissingRollbackSnapshotLocalFrame = 0;
    FrameNumber lastRecoveryReanchorFrame = 0;
};

struct RuntimeRollbackProcessSettings
{
    bool showDebugLog = false;
};

struct RuntimeRollbackProcessResult
{
    bool consumed = false;
    bool applied = false;
    bool requestedResync = false;
    bool startedAuthoritativeResync = false;
    FrameNumber rollbackFromFrame = 0;
    FrameNumber rollbackTargetFrame = 0;
    FrameNumber reanchorFrame = 0;
};

struct RuntimePendingManualStateResync
{
    ResyncReason reason = ResyncReason::Unspecified;
    FrameNumber eventFrame = 0;
    bool waitForAdvance = true;
};

struct RuntimeManualStateResyncProcessResult
{
    bool startedResync = false;
    bool deferredResync = false;
    FrameNumber loadedAuthoritativeFrame = 0;
    FrameNumber reanchorFrame = 0;
};

struct RuntimeSharedClockCatchupState
{
    FrameNumber lagOverBudgetSinceFrame = 0;
    FrameNumber lastResyncRequestFrame = 0;
    FrameNumber lastConfirmedLagWaitLogFrame = 0;
    uint32_t resyncRequestEpoch = 0;
    std::chrono::steady_clock::time_point lagOverBudgetSince = {};
    std::chrono::steady_clock::time_point lastResyncRequestAt = {};
    bool resyncRequestPending = false;
};

struct RuntimeSessionTransitionState
{
    std::optional<SessionState> lastSessionState;
    bool observerVisibilityResyncPending = false;
};

struct RuntimeSessionTransitionResult
{
    bool inactive = false;
    bool resetWorkerTick = false;
};

class INetplayRuntimeSessionControls
{
public:
    virtual ~INetplayRuntimeSessionControls() = default;

    virtual FrameNumber frameCount() const = 0;
    virtual void restartAudio() = 0;
    virtual void discardQueuedNetplayInputsAfter(FrameNumber frame) = 0;
    virtual void discardQueuedAudio() = 0;
    virtual void setSimulationSuspended(bool suspended) = 0;
};

template<typename RuntimeHost>
class NetplayRuntimeSessionControlsAdapter final : public INetplayRuntimeSessionControls
{
public:
    explicit NetplayRuntimeSessionControlsAdapter(RuntimeHost& host) : m_host(host) {}

    FrameNumber frameCount() const override { return m_host.frameCount(); }
    void restartAudio() override { m_host.restartAudio(); }
    void discardQueuedNetplayInputsAfter(FrameNumber frame) override
    {
        m_host.discardQueuedNetplayInputsAfter(frame);
    }
    void discardQueuedAudio() override { m_host.discardQueuedAudio(); }
    void setSimulationSuspended(bool suspended) override { m_host.setSimulationSuspended(suspended); }

private:
    RuntimeHost& m_host;
};

size_t runtimeInputBufferCapacity(uint32_t prebufferFrames, uint32_t predictFrames);

RuntimeInputDelayResult runtimeSyncInputDelaySettings(NetplayCoordinator& coordinator,
                                                      ConfirmedInputBufferDriver& inputDriver,
                                                      NetplayAutoTune& autoTune,
                                                      const RuntimeInputDelaySettings& settings);

std::string runtimeRomKey(const std::optional<NetplayRomSelection>& selection);

std::string runtimeSessionBlockedReason(bool active,
                                        const RoomState& room,
                                        const std::optional<NetplayRomSelection>& localRom);

RuntimeRomValidationResult runtimeSyncRomValidation(NetplayCoordinator& coordinator,
                                                    RuntimeRomValidationState& state,
                                                    const std::optional<NetplayRomSelection>& localRom);

void runtimeSyncEmulatorInputTimelineEpoch(const NetplayCoordinator& coordinator,
                                           INetplayStateBridge& emu);

std::vector<uint8_t> runtimeBuildAuthoritativeStatePayload(INetplayStateBridge& emu,
                                                           const INetplayStateHostBridge& runtimeHost,
                                                           FrameNumber authoritativeFrame,
                                                           bool preferConfirmedSnapshot);

RuntimeAuthoritativeStateResult runtimeBeginAuthoritativeResync(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    FrameNumber authoritativeFrame,
    const std::vector<uint8_t>& statePayload,
    bool preferConfirmedSnapshot,
    ResyncReason reason = ResyncReason::Unspecified,
    ParticipantId targetParticipantId = kInvalidParticipantId);

RuntimePeriodicCrcResult runtimeSubmitPeriodicLocalCrcIfNeeded(
    NetplayCoordinator& coordinator,
    INetplayStateBridge& emu,
    const INetplayStateHostBridge& runtimeHost,
    RuntimePeriodicCrcState& state);

RuntimeAutoStartResult runtimeProcessAutoStartIfNeeded(NetplayCoordinator& coordinator,
                                                       const INetplayStateBridge& emu,
                                                       const std::optional<NetplayRomSelection>& localRom);

RuntimeHostResyncProcessResult runtimeBeginInitialSessionSyncIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost);

RuntimeHostResyncProcessResult runtimeProcessHostResyncIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    NetplayAutoTune& autoTune,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    bool autoGameplayTuning);

RuntimeHostResyncProcessResult runtimeProcessHostLateJoinResyncIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost);

RuntimeHostResyncProcessResult runtimeProcessSelfStallRecoveryIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    SelfStallDetector& selfStallDetector,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    std::chrono::steady_clock::time_point now);

RuntimePendingResyncApplyResult runtimeProcessPendingResyncApplyIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost);

RuntimeManualStateResyncProcessResult runtimeProcessHostManualStateChangesIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    std::deque<RuntimePendingManualStateResync>& pendingManualStateResyncs,
    const std::vector<NetplayManualStateChangeRecord>& events);

RuntimeManualStateResyncProcessResult runtimeProcessPendingManualStateResyncIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    std::deque<RuntimePendingManualStateResync>& pendingManualStateResyncs);

RuntimeSessionTransitionResult runtimeHandleSessionStateTransitions(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayRuntimeSessionControls& controls,
    RuntimeSessionTransitionState& sessionState,
    RuntimePeriodicCrcState& periodicCrcState,
    RuntimeRollbackProcessState& rollbackState,
    RuntimeSharedClockCatchupState& sharedClockState,
    FrameNumber& lastLoadedAuthoritativeFrame);

RuntimeRollbackProcessResult runtimeProcessRollbackIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayConsole& console,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    RuntimeRollbackProcessState& state,
    const RuntimeRollbackProcessSettings& settings);

uint32_t runtimeAdvanceToSharedClockIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayConsole& console,
    RuntimeSharedClockCatchupState& state,
    uint32_t maxFrames,
    bool requireLagTrigger);

uint32_t runtimeAdvanceObserverPeerIfNeeded(NetplayCoordinator& coordinator,
                                            ConfirmedInputBufferDriver& inputDriver,
                                            INetplayConsole& console,
                                            uint32_t maxFrames);

bool runtimeProcessAutoResumeIfNeeded(NetplayCoordinator& coordinator,
                                      bool& webVisibilityManagedPause,
                                      bool webPageVisible,
                                      const std::optional<NetplayRomSelection>& localRom);

bool runtimeShouldRecoverStandaloneInputWhileNetplayActive(const NetplayCoordinator& coordinator);

bool runtimeShouldNetplayOwnEmulationInput(const NetplayCoordinator& coordinator);

std::vector<PlayerSlot> runtimeLocalAssignedSlots(const NetplayCoordinator& coordinator);

struct RuntimeAssignmentLayoutResult
{
    std::string layoutKey;
    std::vector<PlayerSlot> localSlots;
    bool layoutChanged = false;
    bool localSlotsChanged = false;
    bool reanchorInputDriver = false;
    bool resetInputDriver = false;
};

RuntimeAssignmentLayoutResult runtimeSyncAssignmentLayout(NetplayCoordinator& coordinator,
                                                          ConfirmedInputBufferDriver& inputDriver,
                                                          std::string& lastAssignmentLayoutKey,
                                                          std::vector<PlayerSlot>& lastLocalAssignedSlots,
                                                          FrameNumber localFrame,
                                                          bool running);

void runtimeProduceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                       ConfirmedInputBufferDriver& inputDriver,
                                       INetplayConsole& console,
                                       const std::vector<PlayerSlot>& localSlots,
                                       uint32_t workerDtMs);

void runtimePreparePlaybackFrames(NetplayCoordinator& coordinator,
                                  ConfirmedInputBufferDriver& inputDriver,
                                  INetplayConsole& console);

bool runtimeTryBuildPlaybackConfirmedFrame(NetplayCoordinator& coordinator,
                                           const ConfirmedInputBufferDriver& inputDriver,
                                           FrameNumber frame,
                                           NetplayCoordinator::ConfirmedFrameInputs& outFrame);

} // namespace ConsoleNetplay
