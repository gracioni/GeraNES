#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "ConsoleNetplay/ConfirmedInputBufferDriver.h"
#include "ConsoleNetplay/INetplayRuntimeHost.h"
#include "ConsoleNetplay/NetplayAutoTune.h"
#include "ConsoleNetplay/NetplayConfig.h"
#include "ConsoleNetplay/NetplayCoordinator.h"
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
        FrameNumber latestPredictedRemoteFrame = 0;
        NetplayRuntimeDiagnostics runtimeDiagnostics;
        std::string sessionBlockedReason;
        std::vector<std::string> eventLog;
    };

    struct MenuSnapshot
    {
        bool hosting = false;
        bool inputManaged = false;
        NetTransportBackend transportBackend = defaultNetTransportBackend();
        std::vector<PlayerSlot> localAssignments;
        std::vector<InputSlotDescriptor> inputTopology;
    };

    using WorkerCommand = std::function<void(NetplayAppRuntime&, INetplayEmulator&)>;
    using LocalInputBuilder = ConfirmedInputBufferDriver::LocalInputBuilder;

    explicit NetplayAppRuntime(INetplayRuntimeHost& runtimeHost);

    NetplayCoordinator& coordinator();
    const NetplayCoordinator& coordinator() const;
    ConfirmedInputBufferDriver& inputDriver();
    const ConfirmedInputBufferDriver& inputDriver() const;

    void setLocalReconnectToken(uint64_t token);
    void refreshLocalRomSelectionImmediate();
    void updateLatestInputMasks(const std::array<uint64_t, 4>& masks);
    void setLocalInputBuilder(LocalInputBuilder builder);
    void recordFramePacing(uint32_t dtMs,
                           uint32_t framesAdvanced,
                           uint32_t catchupFrames,
                           bool netplayOverrideActive,
                           bool cadenceMatched);
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
    void setInputTopology(std::vector<InputSlotDescriptor> inputTopology,
                          std::optional<ParticipantId> preservedParticipantId = std::nullopt,
                          PlayerSlot preservedAssignment = kObserverPlayerSlot);
    void kickParticipant(ParticipantId participantId);
    void removeReconnectReservation(ParticipantId participantId);
    void requestForceResync();
    void toggleHostedSessionPause();
    void appendNetplayLog(const std::string& message);
    void shutdown();
    void shutdownForUnload();
    void runOnEmulationThread(INetplayEmulator& emu);

private:
    struct PendingManualStateResync
    {
        ResyncReason reason = ResyncReason::Unspecified;
        FrameNumber eventFrame = 0;
        bool waitForAdvance = true;
    };

    INetplayRuntimeHost& m_runtimeHost;
    NetplayCoordinator m_coordinator;
    ConfirmedInputBufferDriver m_inputDriver;
    SelfStallDetector m_selfStallDetector;
    NetplayAutoTune m_autoSettings;
    FramePacingDiagnostics m_framePacingDiagnostics;

    mutable std::mutex m_stateMutex;
    std::deque<WorkerCommand> m_pendingCommands;
    std::array<uint64_t, 4> m_latestInputMasks = {};
    LocalInputBuilder m_localInputBuilder;
    UiSnapshot m_uiSnapshot;
    uint64_t m_cachedReconnectToken = 0;
    bool m_hasCachedReconnectToken = false;
    std::string m_stickyStatusMessage;
    std::optional<SessionState> m_lastSessionState;
    std::vector<PlayerSlot> m_lastLocalAssignedSlots;
    std::deque<PendingManualStateResync> m_pendingManualStateResyncs;
    FrameNumber m_lastSubmittedLocalCrcFrame = 0;
    FrameNumber m_lastRollbackTargetFrame = 0;
    FrameNumber m_lastLoadedAuthoritativeFrame = 0;
    FrameNumber m_lastRecoveryReanchorFrame = 0;
    std::atomic<bool> m_runtimeActive{false};
    std::atomic<bool> m_runtimeRunning{false};

    template<typename Fn>
    void enqueueCommand(Fn&& fn)
    {
        std::scoped_lock stateLock(m_stateMutex);
        m_pendingCommands.emplace_back(std::forward<Fn>(fn));
    }

    void drainPendingCommands(INetplayEmulator& emu);
    std::vector<PlayerSlot> localAssignedSlots() const;
    void updateUiSnapshot(const std::optional<NetplayRomSelection>& localRom);
    void reanchorInputDriver(FrameNumber anchorFrame);
    void queuePendingFramesToEmu(INetplayEmulator& emu);
};

} // namespace ConsoleNetplay
