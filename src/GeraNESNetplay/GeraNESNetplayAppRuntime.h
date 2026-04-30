#pragma once

#include <algorithm>
#include <array>
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

#include "GeraNESApp/AppSettings.h"
#include "GeraNESApp/IEmulationHost.h"
#include "ConsoleNetplay/ConfirmedInputBufferDriver.h"
#include "ConsoleNetplay/NetplayInputAssignment.h"
#include "ConsoleNetplay/SelfStallDetector.h"
#include "ConsoleNetplay/NetplayAutoTune.h"
#include "ConsoleNetplay/NetplayAppRuntime.h"
#include "ConsoleNetplay/NetplayConfig.h"
#include "ConsoleNetplay/NetplayCoordinator.h"
#include "ConsoleNetplay/NetplayRuntimeSupport.h"

namespace GeraNESNetplay {

using ConsoleNetplay::ConfirmedInputBufferDriver;
using ConsoleNetplay::FrameNumber;
using ConsoleNetplay::InputTimeline;
using ConsoleNetplay::kInvalidParticipantId;
using ConsoleNetplay::kObserverPlayerSlot;
using ConsoleNetplay::MessageType;
using ConsoleNetplay::NetplayAutoTune;
using ConsoleNetplay::NetplayCoordinator;
using ConsoleNetplay::NetplayRomSelection;
using ConsoleNetplay::NetTransportBackend;
using ConsoleNetplay::NetTransportOptions;
using ConsoleNetplay::ParticipantId;
using ConsoleNetplay::PlayerSlot;
using ConsoleNetplay::ResyncReason;
using ConsoleNetplay::RollbackStats;
using ConsoleNetplay::RoomState;
using ConsoleNetplay::RuntimePeriodicCrcState;
using ConsoleNetplay::RuntimeRomValidationState;
using ConsoleNetplay::SelfStallDetector;
using ConsoleNetplay::SessionState;
using ConsoleNetplay::TimelineInputEntry;
using ConsoleNetplay::defaultNetTransportBackend;

class GeraNESNetplayAppRuntime
{
public:
    using FramePacingDiagnostics = ConsoleNetplay::NetplayAppRuntime::FramePacingDiagnostics;

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
        FrameNumber latestPredictedRemoteFrame = 0;
        IEmulationHost::NetplayDiagnosticsSnapshot runtimeDiagnostics;
        std::string sessionBlockedReason;
        std::vector<std::string> eventLog;
    };

    using RomSelection = NetplayRomSelection;

    struct MenuSnapshot
    {
        bool hosting = false;
        bool inputManaged = false;
        NetTransportBackend transportBackend = defaultNetTransportBackend();
        std::vector<PlayerSlot> localAssignments;
        std::optional<Settings::Device> port1Device;
        std::optional<Settings::Device> port2Device;
        Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
        Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
        Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;
    };

private:
    using WorkerCommand = std::function<void(GeraNESNetplayAppRuntime&, GeraNESEmu&)>;

    struct PendingManualStateResync
    {
        ResyncReason reason = ResyncReason::Unspecified;
        FrameNumber eventFrame = 0;
        bool waitForAdvance = true;
    };

    IEmulationHost& m_emuHost;
    NetplayCoordinator m_coordinator;
    ConfirmedInputBufferDriver m_inputDriver;
    SelfStallDetector m_selfStallDetector;
    NetplayAutoTune m_autoSettings;
    FramePacingDiagnostics m_framePacingDiagnostics;

    mutable std::mutex m_stateMutex;
    std::deque<WorkerCommand> m_pendingCommands;
    std::array<uint64_t, 4> m_latestRawMasks = {};
    IEmulationHost::InputState m_latestInputState = {};
    UiSnapshot m_uiSnapshot;
    uint64_t m_cachedReconnectToken = 0;
    bool m_hasCachedReconnectToken = false;
    std::string m_stickyStatusMessage;

    std::chrono::steady_clock::time_point m_runtimeLastTickTime = {};
    RuntimeRomValidationState m_romValidationState;
    std::optional<SessionState> m_lastSessionState;
    std::vector<PlayerSlot> m_lastLocalAssignedSlots;
    std::string m_lastAssignmentLayoutKey;
    std::deque<PendingManualStateResync> m_pendingManualStateResyncs;
    RuntimePeriodicCrcState m_periodicCrcState;
    FrameNumber m_lastRollbackTargetFrame = 0;
    FrameNumber m_lastMissingRollbackSnapshotFrame = 0;
    FrameNumber m_lastMissingRollbackSnapshotLocalFrame = 0;
    FrameNumber m_lastLoadedAuthoritativeFrame = 0;
    FrameNumber m_lastRecoveryReanchorFrame = 0;
    FrameNumber m_sharedClockLagOverBudgetSinceFrame = 0;
    FrameNumber m_lastSharedClockResyncRequestFrame = 0;
    FrameNumber m_lastSharedClockConfirmedLagWaitLogFrame = 0;
    uint32_t m_sharedClockResyncRequestEpoch = 0;
    std::chrono::steady_clock::time_point m_sharedClockLagOverBudgetSince = {};
    std::chrono::steady_clock::time_point m_lastSharedClockResyncRequestAt = {};
    bool m_sharedClockResyncRequestPending = false;
    bool m_observerVisibilityResyncPending = false;
    bool m_webVisibilityManagedPause = false;
    bool m_webPageVisible = true;
    std::atomic<bool> m_runtimeActive{false};
    std::atomic<bool> m_runtimeRunning{false};

    // Recovery entry points and ownership:
    // - `processRollbackIfNeededOnWorker`: local rollback correction after
    //   prediction mismatch; preserves the current epoch and resimulates.
    // - `processHostResyncIfNeededOnWorker`: host-authoritative hard resync for
    //   confirmed divergence or explicit recovery requests.
    // - `processHostLateJoinResyncIfNeededOnWorker`: host-authoritative sync for
    //   reconnect/late-join style bootstrap.
    // - `processHostManualStateChangeResyncIfNeeded` +
    //   `processPendingManualStateResyncIfNeeded`: host reset/load-state
    //   bootstrap; these replace the old causal timeline and reanchor runtime
    //   producers/consumers to the loaded frame.
    // - `processResyncIfNeededOnWorker`: client-side application/acknowledgement
    //   of authoritative state after a host-directed resync transfer.

    static std::optional<RomSelection> captureCurrentRomSelection(GeraNESEmu& emu);

    void reanchorInputDriver(FrameNumber anchorFrame);

    std::vector<uint8_t> buildAuthoritativeStatePayload(FrameNumber authoritativeFrame,
                                                        bool preferConfirmedSnapshot) const;

    bool beginAuthoritativeResync(FrameNumber authoritativeFrame,
                                  const std::vector<uint8_t>& statePayload,
                                  bool preferConfirmedSnapshot,
                                  ResyncReason reason = ResyncReason::Unspecified,
                                  ParticipantId targetParticipantId = kInvalidParticipantId);

    bool beginAuthoritativeResyncWithoutLocalReload(FrameNumber authoritativeFrame,
                                                    const std::vector<uint8_t>& statePayload,
                                                    bool preferConfirmedSnapshot,
                                                    ResyncReason reason);

    std::vector<PlayerSlot> localAssignedSlots() const;

    std::string computeSessionBlockedReason(const std::optional<RomSelection>& localRom) const;

    void syncRomValidation(const std::optional<RomSelection>& localRom);
    void syncInputDelayFromSettings();
    void processAutoStartIfNeeded(const std::optional<RomSelection>& localRom);
    void processAutoResumeIfNeeded(const std::optional<RomSelection>& localRom);
    void processHostManualStateChangeResyncIfNeeded();
    void processPendingManualStateResyncIfNeeded();
    void processPeriodicLocalCrcIfNeeded();
    uint32_t consumeWorkerDtMs();
    void handleSessionStateTransitionsOnWorker();
    bool beginInitialSessionSyncOnWorker();
    void processHostResyncIfNeededOnWorker();
    void processHostLateJoinResyncIfNeededOnWorker();
    void processHostStallIfNeededOnWorker();
    void processResyncIfNeededOnWorker(GeraNESEmu& emu);
    uint32_t advanceToSharedClockIfNeededOnWorker(GeraNESEmu& emu,
                                                  uint32_t maxFrames,
                                                  bool requireLagTrigger = true);
    void alignResyncPlaybackToSharedClockOnWorker(GeraNESEmu& emu, FrameNumber loadedFrame);
    void processRollbackIfNeededOnWorker(GeraNESEmu& emu);
    void ensureStandaloneInputBootstrapFrame(GeraNESEmu& emu);
    bool tryBuildPlaybackConfirmedFrame(uint32_t frame, NetplayCoordinator::ConfirmedFrameInputs& outFrame);
    bool tryBuildPlaybackReplayFrame(uint32_t frame, IEmulationHost::ReplayFrameInput& outFrame);
    void updateUiSnapshot(const std::optional<RomSelection>& localRom);
    void syncEmuInputTimelineEpoch();
    bool tryQueuePlaybackFrameToEmu(GeraNESEmu& emu, uint32_t frame);
    bool shouldAllowPredictionForFrame(FrameNumber frame) const;
    void recordPlaybackStop(FrameNumber frame);

    template<typename Fn>
    void enqueueCommand(Fn&& fn)
    {
        std::scoped_lock stateLock(m_stateMutex);
        m_pendingCommands.emplace_back(std::forward<Fn>(fn));
    }

    void drainPendingCommands(GeraNESEmu& emu);

public:
    explicit GeraNESNetplayAppRuntime(IEmulationHost& emuHost);

    void setLocalReconnectToken(uint64_t token);
    void refreshLocalRomSelectionImmediate();
    void updateLatestInputState(const IEmulationHost::InputState& inputState);
    void updateLatestRawMasks(const std::array<uint64_t, 4>& masks);
    void recordFramePacing(uint32_t dtMs,
                           uint32_t framesAdvanced,
                           uint32_t catchupFrames,
                           bool netplayOverrideActive,
                           bool cadenceMatched);
    void notifyWebVisibilityChanged(bool visible);
    void notifyWebVisibilityChangedImmediate(GeraNESEmu& emu, bool visible);
    UiSnapshot uiSnapshot() const;
    MenuSnapshot menuSnapshot() const;
    bool runtimeActive() const;
    bool runtimeRunning() const;
    void injectDropNextIncomingMessages(MessageType type, uint32_t count);
    void clearIncomingMessageDrops();
    void setReconnectReservationTimeoutForTests(uint32_t seconds);
    void simulateTransportFailureForTests();
    void setTransportBackend(NetTransportBackend backend);
    void setTransportOptions(const NetTransportOptions& options);
    void configureRollbackWindow(size_t snapshotCapacity);
    NetTransportOptions transportOptions() const;
    NetTransportBackend transportBackend() const;

    void host(uint16_t port, size_t maxPeers, const std::string& displayName);
    void join(const std::string& hostName, uint16_t port, const std::string& displayName);
    void disconnect();
    void assignController(ParticipantId participantId, PlayerSlot slot);
    void addControllerAssignment(ParticipantId participantId, PlayerSlot slot);
    void removeControllerAssignment(ParticipantId participantId, PlayerSlot slot);
    void clearControllerAssignments(ParticipantId participantId);
    void configureInputAssignment(ParticipantId participantId,
                                  std::optional<Settings::Device> port1Device,
                                  std::optional<Settings::Device> port2Device,
                                  Settings::ExpansionDevice expansionDevice,
                                  Settings::NesMultitapDevice nesMultitapDevice,
                                  Settings::FamicomMultitapDevice famicomMultitapDevice,
                                  PlayerSlot slot);
    void configureInputAssignments(ParticipantId participantId,
                                   std::optional<Settings::Device> port1Device,
                                   std::optional<Settings::Device> port2Device,
                                   Settings::ExpansionDevice expansionDevice,
                                   Settings::NesMultitapDevice nesMultitapDevice,
                                   Settings::FamicomMultitapDevice famicomMultitapDevice,
                                   const std::vector<PlayerSlot>& slots);
    void kickParticipant(ParticipantId participantId);
    void removeReconnectReservation(ParticipantId participantId);
    void requestForceResync();
    void toggleHostedSessionPause();
    void appendNetplayLog(const std::string& message);
    void shutdown();
    void shutdownForUnload();
    void runOnEmulationThread(GeraNESEmu& emu);
};

} // namespace GeraNESNetplay

