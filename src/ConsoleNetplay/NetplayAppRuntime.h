#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ConsoleNetplay/ConfirmedInputBufferDriver.h"
#include "ConsoleNetplay/NetplayRuntimeTypes.h"
#include "ConsoleNetplay/NetplayAutoTune.h"
#include "ConsoleNetplay/NetplayCoordinator.h"
#include "ConsoleNetplay/NetplayRuntimeSupport.h"
#include "ConsoleNetplay/SelfStallDetector.h"

namespace ConsoleNetplay {

class NetplayAppRuntime
{
public:
    struct FramePacingDiagnostics
    {
        uint64_t sampleCount = 0;
        uint32_t lastDtMs = 0;
        uint32_t maxDtMs = 0;
        uint32_t lastFramesAdvanced = 0;
        uint32_t maxFramesAdvanced = 0;
        uint64_t totalFramesAdvanced = 0;
        uint32_t lastCatchupFrames = 0;
        uint32_t maxCatchupFrames = 0;
        uint64_t catchupTickCount = 0;
        bool netplayPacingOverrideActive = false;
        bool presenterCadenceMatched = false;

        void record(uint32_t dtMs,
                    uint32_t framesAdvanced,
                    uint32_t catchupFrames,
                    bool netplayOverrideActive,
                    bool cadenceMatched);
    };

    struct UiSnapshot
    {
        bool valid = false;
        bool active = false;
        bool hosting = false;
        bool connected = false;
        bool reconnecting = false;
        uint16_t reconnectSecondsRemaining = 0;
        bool localRomLoaded = false;
        std::string localRomGameName;
        uint32_t localRomCrc32 = 0;
        NetTransportBackend transportBackend = defaultNetTransportBackend();
        ParticipantId localParticipantId = kInvalidParticipantId;
        std::string lastError;
        RoomState room;
        size_t localInputCount = 0;
        size_t remoteInputCount = 0;
        InputTimeline::LookupStats localInputLookupStats;
        InputTimeline::LookupStats remoteInputLookupStats;
        NetplayCoordinator::PerformanceDiagnostics coordinatorPerformanceDiagnostics;
        ConfirmedInputBufferDriver::PlaybackQueueStats playbackQueueStats;
        std::optional<TimelineInputEntry> latestLocalInput;
        std::optional<TimelineInputEntry> latestRemoteInput;
        RollbackStats predictionStats;
        FrameNumber localSimulationFrame = 0;
        FrameNumber publishedConfirmedFrame = 0;
        FrameNumber lastSubmittedLocalCrcFrame = 0;
        FrameNumber lastRollbackTargetFrame = 0;
        FrameNumber lastLoadedAuthoritativeFrame = 0;
        FrameNumber lastRecoveryReanchorFrame = 0;
        NetplayAutoTune::Snapshot autoSettings;
        FramePacingDiagnostics framePacingDiagnostics;
        uint32_t unresolvedPredictedRemoteFrameCount = 0;
        std::vector<TimelineInputEntry> unresolvedPredictedRemoteInputs;
        FrameNumber latestPredictedRemoteFrame = 0;
        NetplayRuntimeDiagnostics runtimeDiagnostics;
        std::string sessionBlockedReason;
        std::vector<std::string> eventLog;
    };

    using RomSelection = NetplayRomSelection;
    using InputTopologyConfigurer = std::function<void(NetplayCoordinator&,
                                                       std::optional<ParticipantId>,
                                                       PlayerSlot)>;

    struct RuntimeFrameSettings
    {
        bool autoGameplayTuning = false;
        bool showDebugLog = false;
        NetplayRuntimeDiagnostics diagnostics;
        std::function<void(FrameNumber)> discardQueuedNetplayInputsAfter;
    };

    struct RuntimeFrameResult
    {
        bool running = false;
        bool paused = false;
    };

    struct UpdateContext
    {
        INetplayConsole& console;
        INetplayStateBridge& stateBridge;
        INetplayStateHostBridge& hostBridge;
        INetplayRuntimeSessionControls& sessionControls;
        std::vector<NetplayManualStateChangeRecord> manualEvents;
        RuntimeInputDelaySettings inputDelaySettings;
        RuntimeFrameSettings frameSettings;
        std::function<void(bool netplayOwnsEmulationInput, bool allowPresenterTimeoutAdvance)> applyHostInputOwnership;
    };

    struct UpdateResult
    {
        bool active = false;
        bool running = false;
        bool paused = false;
        bool netplayOwnsEmulationInput = false;
        bool autoQueuePendingInputOnFrameStart = true;
        bool allowPresenterTimeoutAdvance = true;
        bool simulationSuspended = false;
        bool discardQueuedAudio = false;
        size_t inputBufferCapacity = 64;
        std::optional<size_t> snapshotCapacity;
        uint32_t inputDelayFrames = 0;
        uint32_t predictFrames = 0;
    };

    virtual ~NetplayAppRuntime() = default;

    UpdateResult update(UpdateContext context);
    template<typename RuntimeHost>
    UpdateResult update(INetplayConsole& console,
                        RuntimeHost& host,
                        std::vector<NetplayManualStateChangeRecord> manualEvents,
                        RuntimeInputDelaySettings inputDelaySettings,
                        RuntimeFrameSettings frameSettings);
    template<typename RuntimeHost>
    static void applyUpdateResultToHost(RuntimeHost& host,
                                        const UpdateResult& result,
                                        bool applyInputOwnership = true);
    template<typename RuntimeHost, typename Converter>
    void applyPlaybackResolverToHost(RuntimeHost& host,
                                     bool enabled,
                                     Converter converter);
    template<typename RuntimeHost, typename Converter>
    UpdateResult updateAndApply(INetplayConsole& console,
                                RuntimeHost& host,
                                std::vector<NetplayManualStateChangeRecord> manualEvents,
                                RuntimeInputDelaySettings inputDelaySettings,
                                RuntimeFrameSettings frameSettings,
                                Converter playbackConverter);
    void setLocalReconnectToken(uint64_t token);
    void refreshLocalRomSelectionImmediate();
    void recordFramePacing(uint32_t dtMs,
                           uint32_t framesAdvanced,
                           uint32_t catchupFrames,
                           bool netplayOverrideActive,
                           bool cadenceMatched);
    void setRuntimeHostWakeCallback(std::function<void()> callback);
    void setRepeatedInputFrameTransformer(
        std::function<NetplayInputFrame(const NetplayInputFrame&, FrameNumber)> transformer);
    UiSnapshot uiSnapshot() const;
    void injectDropNextIncomingMessages(MessageType type, uint32_t count);
    void clearIncomingMessageDrops();
    void setReconnectReservationTimeoutForTests(uint32_t seconds);
    void simulateTransportFailureForTests();
    void drainRuntimeCommandsForTests();
    size_t pendingInputTopologyChangeCountForTests() const;
    NetplayCoordinator& coordinatorForTests();
    const NetplayCoordinator& coordinatorForTests() const;
    void processPendingInputTopologyChangesForTests(INetplayConsole& console,
                                                    INetplayStateBridge& stateBridge,
                                                    INetplayStateHostBridge& hostBridge);
    void setTransportBackend(NetTransportBackend backend);
    void setTransportOptions(const NetTransportOptions& options);
    void configureRollbackWindow(size_t snapshotCapacity);
    void notifyWebVisibilityChanged(bool visible);
    NetTransportOptions transportOptions() const;
    void host(uint16_t port, size_t maxPeers, const std::string& displayName);
    void join(const std::string& hostName, uint16_t port, const std::string& displayName);
    void disconnect();
    void assignController(ParticipantId participantId, PlayerSlot slot);
    void addControllerAssignment(ParticipantId participantId, PlayerSlot slot);
    void removeControllerAssignment(ParticipantId participantId, PlayerSlot slot);
    void clearControllerAssignments(ParticipantId participantId);
    void configureInputAssignments(ParticipantId participantId,
                                   std::vector<PlayerSlot> slots,
                                   InputTopologyConfigurer configureTopology);
    void kickParticipant(ParticipantId participantId);
    void removeReconnectReservation(ParticipantId participantId);
    void requestForceResync();
    void toggleHostedSessionPause();
    void appendNetplayLog(const std::string& message);
    void clearNetplayLog();
    void shutdown();
    void shutdownForUnload();
    bool tryBuildPlaybackFrame(FrameNumber frame,
                               NetplayCoordinator::ConfirmedFrameInputs& outFrame);

private:
    using RuntimeCommand = std::function<void(NetplayAppRuntime&)>;

    void enqueueRuntimeCommand(RuntimeCommand command);
    void drainRuntimeCommands();
    void wakeRuntimeHost();
    void processPendingInputTopologyChanges(INetplayConsole& console,
                                            INetplayStateBridge& stateBridge,
                                            INetplayStateHostBridge& hostBridge);

    void resetInactiveRuntimeState();
    void reanchorInputDriver(FrameNumber anchorFrame);
    std::vector<uint8_t> buildAuthoritativeStatePayload(INetplayStateBridge& stateBridge,
                                                        const INetplayStateHostBridge& hostBridge,
                                                        FrameNumber authoritativeFrame,
                                                        bool preferConfirmedSnapshot) const;
    bool beginAuthoritativeResync(INetplayStateBridge& stateBridge,
                                  INetplayStateHostBridge& hostBridge,
                                  FrameNumber authoritativeFrame,
                                  const std::vector<uint8_t>& statePayload,
                                  bool preferConfirmedSnapshot,
                                  ResyncReason reason = ResyncReason::Unspecified,
                                  ParticipantId targetParticipantId = kInvalidParticipantId);
    std::string computeSessionBlockedReason(const std::optional<RomSelection>& localRom) const;
    void syncRomValidation(const std::optional<RomSelection>& localRom);
    RuntimeInputDelayResult syncInputDelayFromSettings(const RuntimeInputDelaySettings& settings);
    bool processAutoStartIfNeeded(INetplayStateBridge& stateBridge,
                                  INetplayStateHostBridge& hostBridge,
                                  const std::optional<RomSelection>& localRom);
    void processHostManualStateChangeResyncIfNeeded(INetplayStateBridge& stateBridge,
                                                    INetplayStateHostBridge& hostBridge,
                                                    const std::vector<NetplayManualStateChangeRecord>& events);
    void processPendingManualStateResyncIfNeeded(INetplayStateBridge& stateBridge,
                                                 INetplayStateHostBridge& hostBridge);
    void processPeriodicLocalCrcIfNeeded(INetplayStateBridge& stateBridge,
                                         const INetplayStateHostBridge& hostBridge,
                                         bool showDebugLog);
    uint32_t consumeWorkerDtMs();
    void handleSessionStateTransitionsOnWorker(INetplayRuntimeSessionControls& controls);
    bool beginInitialSessionSyncOnWorker(INetplayStateBridge& stateBridge,
                                         INetplayStateHostBridge& hostBridge);
    void processHostResyncIfNeededOnWorker(INetplayStateBridge& stateBridge,
                                           INetplayStateHostBridge& hostBridge,
                                           bool autoGameplayTuning,
                                           INetplayRuntimeSessionControls* sessionControls = nullptr);
    void processHostLateJoinResyncIfNeededOnWorker(INetplayStateBridge& stateBridge,
                                                   INetplayStateHostBridge& hostBridge);
    void processHostStallIfNeededOnWorker(INetplayStateBridge& stateBridge,
                                          INetplayStateHostBridge& hostBridge,
                                          std::chrono::steady_clock::time_point now);
    RuntimePendingResyncApplyResult processResyncIfNeededOnWorker(INetplayStateBridge& stateBridge,
                                                                  INetplayStateHostBridge& hostBridge);
    uint32_t advanceToSharedClockIfNeededOnWorker(INetplayConsole& console,
                                                  uint32_t maxFrames,
                                                  bool requireLagTrigger = true);
    void processRollbackIfNeededOnWorker(INetplayConsole& console,
                                         INetplayStateBridge& stateBridge,
                                         INetplayStateHostBridge& hostBridge,
                                         const RuntimeRollbackProcessSettings& settings);
    RuntimeFrameResult runActiveConsoleFrame(INetplayConsole& console,
                                             INetplayStateBridge& stateBridge,
                                             INetplayStateHostBridge& hostBridge,
                                             INetplayRuntimeSessionControls& sessionControls,
                                             const std::optional<RomSelection>& localRom,
                                             const std::vector<NetplayManualStateChangeRecord>& manualEvents,
                                             const RuntimeFrameSettings& settings);
    void updateUiSnapshot(const std::optional<RomSelection>& localRom,
                          const NetplayRuntimeDiagnostics& runtimeDiagnostics);
    void syncEmuInputTimelineEpoch(INetplayStateBridge& stateBridge);
    void ensureStandaloneInputBootstrapFrame(INetplayConsole& console,
                                             INetplayStateBridge& stateBridge);
    bool tryQueuePlaybackFrameToConsole(INetplayConsole& console, FrameNumber frame);

    NetplayCoordinator m_coordinator;
    ConfirmedInputBufferDriver m_inputDriver;
    SelfStallDetector m_selfStallDetector;
    NetplayAutoTune m_autoSettings;
    FramePacingDiagnostics m_framePacingDiagnostics;

    mutable std::mutex m_stateMutex;
    std::deque<RuntimeCommand> m_pendingRuntimeCommands;
    struct PendingInputTopologyChange
    {
        ParticipantId participantId = kInvalidParticipantId;
        std::vector<PlayerSlot> slots;
        InputTopologyConfigurer configureTopology;
    };
    std::deque<PendingInputTopologyChange> m_pendingInputTopologyChanges;
    UiSnapshot m_uiSnapshot;
    std::function<void()> m_runtimeHostWakeCallback;
    uint64_t m_cachedReconnectToken = 0;
    bool m_hasCachedReconnectToken = false;
    std::string m_stickyStatusMessage;

    std::chrono::steady_clock::time_point m_runtimeLastTickTime = {};
    RuntimeRomValidationState m_romValidationState;
    RuntimeSessionTransitionState m_sessionTransitionState;
    std::vector<PlayerSlot> m_lastLocalAssignedSlots;
    std::string m_lastAssignmentLayoutKey;
    std::deque<RuntimePendingManualStateResync> m_pendingManualStateResyncs;
    RuntimePeriodicCrcState m_periodicCrcState;
    RuntimeRollbackProcessState m_rollbackProcessState;
    FrameNumber m_lastLoadedAuthoritativeFrame = 0;
    RuntimeSharedClockCatchupState m_sharedClockCatchupState;
    bool m_webVisibilityManagedPause = false;
    bool m_webPageVisible = true;
    std::optional<RomSelection> m_latestLocalRom;
    bool m_forceResyncRequested = false;
    bool m_hostedPauseToggleRequested = false;
    std::optional<bool> m_pendingWebVisibilityChange;
    std::optional<size_t> m_pendingSnapshotCapacity;
    std::atomic<bool> m_runtimeActive{false};
    std::atomic<bool> m_runtimeRunning{false};
};

NetplayAppRuntime::UiSnapshot buildNetplayUiSnapshot(
    const NetplayCoordinator& coordinator,
    const ConfirmedInputBufferDriver& inputDriver,
    const std::optional<NetplayRomSelection>& localRom,
    const std::string& stickyStatusMessage,
    FrameNumber lastSubmittedLocalCrcFrame,
    FrameNumber lastRollbackTargetFrame,
    FrameNumber lastLoadedAuthoritativeFrame,
    FrameNumber lastRecoveryReanchorFrame,
    const NetplayAutoTune::Snapshot& autoSettings,
    const NetplayAppRuntime::FramePacingDiagnostics& framePacingDiagnostics,
    const NetplayRuntimeDiagnostics& runtimeDiagnostics,
    const std::string& sessionBlockedReason);

} // namespace ConsoleNetplay
