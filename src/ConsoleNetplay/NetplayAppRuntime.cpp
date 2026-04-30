#include "ConsoleNetplay/NetplayAppRuntime.h"

#include <algorithm>
#include <utility>

#include "ConsoleNetplay/NetplayInputAssignment.h"
#include "ConsoleNetplay/NetplayRuntimeSupport.h"

namespace ConsoleNetplay {

void NetplayAppRuntime::FramePacingDiagnostics::record(uint32_t dtMs,
                                                       uint32_t framesAdvanced,
                                                       uint32_t catchupFrames,
                                                       bool netplayOverrideActive,
                                                       bool cadenceMatched)
{
    ++sampleCount;
    lastDtMs = dtMs;
    maxDtMs = std::max(maxDtMs, dtMs);
    lastFramesAdvanced = framesAdvanced;
    maxFramesAdvanced = std::max(maxFramesAdvanced, framesAdvanced);
    totalFramesAdvanced += framesAdvanced;
    lastCatchupFrames = catchupFrames;
    maxCatchupFrames = std::max(maxCatchupFrames, catchupFrames);
    if(catchupFrames > 0) {
        ++catchupTickCount;
    }
    netplayPacingOverrideActive = netplayOverrideActive;
    presenterCadenceMatched = cadenceMatched;
}

NetplayAppRuntime::NetplayAppRuntime(INetplayRuntimeHost& runtimeHost)
    : m_runtimeHost(runtimeHost)
{
}

NetplayCoordinator& NetplayAppRuntime::coordinator() { return m_coordinator; }
const NetplayCoordinator& NetplayAppRuntime::coordinator() const { return m_coordinator; }
ConfirmedInputBufferDriver& NetplayAppRuntime::inputDriver() { return m_inputDriver; }
const ConfirmedInputBufferDriver& NetplayAppRuntime::inputDriver() const { return m_inputDriver; }

void NetplayAppRuntime::setLocalReconnectToken(uint64_t token)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_cachedReconnectToken = token;
    m_hasCachedReconnectToken = true;
}

void NetplayAppRuntime::refreshLocalRomSelectionImmediate()
{
    m_lastSessionState.reset();
}

void NetplayAppRuntime::updateLatestInputMasks(const std::array<uint64_t, 4>& masks)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_latestInputMasks = masks;
}

void NetplayAppRuntime::setLocalInputBuilder(LocalInputBuilder builder)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_localInputBuilder = std::move(builder);
}

void NetplayAppRuntime::recordFramePacing(uint32_t dtMs,
                                          uint32_t framesAdvanced,
                                          uint32_t catchupFrames,
                                          bool netplayOverrideActive,
                                          bool cadenceMatched)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_framePacingDiagnostics.record(dtMs, framesAdvanced, catchupFrames, netplayOverrideActive, cadenceMatched);
}

NetplayAppRuntime::UiSnapshot NetplayAppRuntime::uiSnapshot() const
{
    std::scoped_lock stateLock(m_stateMutex);
    return m_uiSnapshot;
}

NetplayAppRuntime::MenuSnapshot NetplayAppRuntime::menuSnapshot() const
{
    std::scoped_lock stateLock(m_stateMutex);
    MenuSnapshot snapshot;
    snapshot.hosting = m_uiSnapshot.hosting;
    snapshot.inputManaged = m_uiSnapshot.active && m_uiSnapshot.room.state == SessionState::Running;
    snapshot.transportBackend = m_uiSnapshot.transportBackend;
    snapshot.inputTopology = m_uiSnapshot.room.inputTopology;
    if(m_uiSnapshot.localParticipantId != kInvalidParticipantId) {
        const auto participantIt = std::find_if(
            m_uiSnapshot.room.participants.begin(),
            m_uiSnapshot.room.participants.end(),
            [this](const ParticipantInfo& participant) {
                return participant.id == m_uiSnapshot.localParticipantId;
            }
        );
        if(participantIt != m_uiSnapshot.room.participants.end()) {
            const ParticipantInfo* participant = &*participantIt;
            snapshot.localAssignments = participantAssignments(*participant);
        }
    }
    return snapshot;
}

bool NetplayAppRuntime::runtimeActive() const { return m_runtimeActive.load(); }
bool NetplayAppRuntime::runtimeRunning() const { return m_runtimeRunning.load(); }

void NetplayAppRuntime::injectDropNextIncomingMessages(MessageType type, uint32_t count)
{
    enqueueCommand([=](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.dropNextIncomingMessages(type, count);
    });
}

void NetplayAppRuntime::clearIncomingMessageDrops()
{
    enqueueCommand([](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.clearIncomingMessageDrops();
    });
}

void NetplayAppRuntime::setReconnectReservationTimeoutForTests(uint32_t seconds)
{
    enqueueCommand([=](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.setReconnectReservationDurationForTests(seconds);
    });
}

void NetplayAppRuntime::simulateTransportFailureForTests()
{
    enqueueCommand([](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.simulateTransportFailureForTests();
    });
}

void NetplayAppRuntime::setTransportBackend(NetTransportBackend backend)
{
    enqueueCommand([backend](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.setTransportBackend(backend);
    });
}

void NetplayAppRuntime::setTransportOptions(const NetTransportOptions& options)
{
    enqueueCommand([options](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.setTransportOptions(options);
    });
}

void NetplayAppRuntime::configureRollbackWindow(size_t snapshotCapacity)
{
    enqueueCommand([this, snapshotCapacity](NetplayAppRuntime&, INetplayEmulator&) {
        m_runtimeHost.configureNetplaySnapshots(snapshotCapacity);
    });
}

NetTransportOptions NetplayAppRuntime::transportOptions() const { return m_coordinator.transportOptions(); }
NetTransportBackend NetplayAppRuntime::transportBackend() const { return m_coordinator.transportBackend(); }

void NetplayAppRuntime::host(uint16_t port, size_t maxPeers, const std::string& displayName)
{
    enqueueCommand([=](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.host(port, maxPeers, displayName);
    });
}

void NetplayAppRuntime::join(const std::string& hostName, uint16_t port, const std::string& displayName)
{
    enqueueCommand([=](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.join(hostName, port, displayName);
    });
}

void NetplayAppRuntime::disconnect()
{
    enqueueCommand([](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.disconnect();
        self.m_inputDriver.reset();
        self.m_runtimeActive.store(false);
        self.m_runtimeRunning.store(false);
    });
}

void NetplayAppRuntime::assignController(ParticipantId participantId, PlayerSlot slot)
{
    enqueueCommand([=](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.assignController(participantId, slot);
    });
}

void NetplayAppRuntime::addControllerAssignment(ParticipantId participantId, PlayerSlot slot)
{
    enqueueCommand([=](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.addControllerAssignment(participantId, slot);
    });
}

void NetplayAppRuntime::removeControllerAssignment(ParticipantId participantId, PlayerSlot slot)
{
    enqueueCommand([=](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.removeControllerAssignment(participantId, slot);
    });
}

void NetplayAppRuntime::clearControllerAssignments(ParticipantId participantId)
{
    enqueueCommand([=](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.clearControllerAssignments(participantId);
    });
}

void NetplayAppRuntime::setInputTopology(std::vector<InputSlotDescriptor> inputTopology,
                                         std::optional<ParticipantId> preservedParticipantId,
                                         PlayerSlot preservedAssignment)
{
    enqueueCommand([inputTopology = std::move(inputTopology), preservedParticipantId, preservedAssignment](NetplayAppRuntime& self, INetplayEmulator&) mutable {
        self.m_coordinator.setRoomInputTopology(std::move(inputTopology), preservedParticipantId, preservedAssignment);
    });
}

void NetplayAppRuntime::kickParticipant(ParticipantId participantId)
{
    enqueueCommand([=](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.kickParticipant(participantId);
    });
}

void NetplayAppRuntime::removeReconnectReservation(ParticipantId participantId)
{
    enqueueCommand([=](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.removeReconnectReservation(participantId);
    });
}

void NetplayAppRuntime::requestForceResync()
{
    enqueueCommand([](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.requestHostResync(ResyncReason::ManualForce);
    });
}

void NetplayAppRuntime::toggleHostedSessionPause()
{
    enqueueCommand([](NetplayAppRuntime& self, INetplayEmulator&) {
        const RoomState& room = self.m_coordinator.session().roomState();
        if(room.state == SessionState::Paused) {
            self.m_coordinator.resumeSession();
        } else if(room.state == SessionState::Running) {
            self.m_coordinator.pauseSession();
        }
    });
}

void NetplayAppRuntime::appendNetplayLog(const std::string& message)
{
    enqueueCommand([message](NetplayAppRuntime& self, INetplayEmulator&) {
        self.m_coordinator.appendNetplayLog(message);
    });
}

void NetplayAppRuntime::shutdown()
{
    m_coordinator.disconnect();
    m_inputDriver.reset();
    m_runtimeActive.store(false);
    m_runtimeRunning.store(false);
}

void NetplayAppRuntime::shutdownForUnload()
{
    shutdown();
}

void NetplayAppRuntime::drainPendingCommands(INetplayEmulator& emu)
{
    std::deque<WorkerCommand> commands;
    {
        std::scoped_lock stateLock(m_stateMutex);
        commands.swap(m_pendingCommands);
    }
    for(WorkerCommand& command : commands) {
        command(*this, emu);
    }
}

std::vector<PlayerSlot> NetplayAppRuntime::localAssignedSlots() const
{
    return runtimeLocalAssignedSlots(m_coordinator);
}

void NetplayAppRuntime::updateUiSnapshot(const std::optional<NetplayRomSelection>& localRom)
{
    UiSnapshot snapshot;
    snapshot.valid = true;
    snapshot.active = m_coordinator.isActive();
    snapshot.hosting = m_coordinator.isHosting();
    snapshot.connected = m_coordinator.isConnected();
    snapshot.reconnecting = m_coordinator.reconnectPending();
    snapshot.reconnectSecondsRemaining = m_coordinator.reconnectSecondsRemaining();
    snapshot.localRomLoaded = localRom.has_value() && localRom->loaded;
    snapshot.localRomGameName = localRom.has_value() ? localRom->gameName : std::string{};
    snapshot.localRomCrc32 = localRom.has_value() ? localRom->validation.romCrc32 : 0;
    snapshot.transportBackend = m_coordinator.transportBackend();
    snapshot.localParticipantId = m_coordinator.localParticipantId();
    snapshot.lastError = m_coordinator.lastError();
    snapshot.room = m_coordinator.session().roomState();
    snapshot.localInputCount = m_coordinator.localInputs().size();
    snapshot.remoteInputCount = m_coordinator.remoteInputs().size();
    snapshot.localInputLookupStats = m_coordinator.localInputs().lookupStats();
    snapshot.remoteInputLookupStats = m_coordinator.remoteInputs().lookupStats();
    snapshot.coordinatorPerformanceDiagnostics = m_coordinator.performanceDiagnostics();
    snapshot.playbackQueueStats = m_inputDriver.playbackQueueStats();
    if(const TimelineInputEntry* latestLocalInput = m_coordinator.localInputs().latest()) {
        snapshot.latestLocalInput = *latestLocalInput;
    }
    if(const TimelineInputEntry* latestRemoteInput = m_coordinator.remoteInputs().latest()) {
        snapshot.latestRemoteInput = *latestRemoteInput;
    }
    snapshot.predictionStats = m_coordinator.predictionStats();
    snapshot.localSimulationFrame = m_coordinator.localSimulationFrame();
    snapshot.publishedConfirmedFrame = m_coordinator.latestConfirmedFrame();
    snapshot.lastSubmittedLocalCrcFrame = m_lastSubmittedLocalCrcFrame;
    snapshot.lastRollbackTargetFrame = m_lastRollbackTargetFrame;
    snapshot.lastLoadedAuthoritativeFrame = m_lastLoadedAuthoritativeFrame;
    snapshot.lastRecoveryReanchorFrame = m_lastRecoveryReanchorFrame;
    snapshot.autoSettings = m_autoSettings.snapshot();
    snapshot.framePacingDiagnostics = m_framePacingDiagnostics;
    snapshot.unresolvedPredictedRemoteFrameCount = m_coordinator.unresolvedPredictedRemoteFrameCount();
    snapshot.latestPredictedRemoteFrame = m_coordinator.latestPredictedRemoteFrame();
    snapshot.runtimeDiagnostics = m_runtimeHost.getNetplayDiagnostics();
    snapshot.eventLog = m_coordinator.eventLog();

    std::scoped_lock stateLock(m_stateMutex);
    m_uiSnapshot = std::move(snapshot);
}

void NetplayAppRuntime::reanchorInputDriver(FrameNumber anchorFrame)
{
    m_inputDriver.reanchor(anchorFrame);
    m_lastRecoveryReanchorFrame = anchorFrame;
}

void NetplayAppRuntime::queuePendingFramesToEmu(INetplayEmulator& emu)
{
    m_inputDriver.consumePendingFrames(
        emu.frameCount(),
        emu.frameCount() + m_inputDriver.prebufferFrames() + m_inputDriver.predictFrames(),
        [&emu](const NetplayCoordinator::ConfirmedFrameInputs& confirmed) {
            NetplayInputFrame inputFrame = confirmed.netplayFrame;
            inputFrame.speculative = confirmed.predicted;
            inputFrame.timelineEpoch = emu.inputTimelineEpoch();
            (void)emu.queueInputFrame(inputFrame);
        }
    );
}

void NetplayAppRuntime::runOnEmulationThread(INetplayEmulator& emu)
{
    drainPendingCommands(emu);
    m_coordinator.update(0);

    const std::optional<NetplayRomSelection> localRom = emu.currentRomSelection();
    m_runtimeActive.store(m_coordinator.isActive());
    m_runtimeRunning.store(m_coordinator.isActive() && m_coordinator.session().roomState().state == SessionState::Running);

    const RoomState room = m_coordinator.session().roomState();
    emu.setInputTimelineEpoch(room.timelineEpoch);
    m_coordinator.setLocalSimulationFrame(emu.frameCount());

    const std::vector<PlayerSlot> localSlots = localAssignedSlots();
    LocalInputBuilder builder;
    std::array<uint64_t, 4> masks = {};
    {
        std::scoped_lock stateLock(m_stateMutex);
        builder = m_localInputBuilder;
        masks = m_latestInputMasks;
    }

    if(!builder) {
        builder = [masks](PlayerSlot slot, FrameNumber frame, const RoomState& inputRoom) {
            const size_t maskIndex = slot <= 3 ? static_cast<size_t>(slot) : 0u;
            return ConfirmedInputBufferDriver::buildMaskContribution(slot, frame, inputRoom.timelineEpoch, masks[maskIndex]);
        };
    }

    m_inputDriver.produceLocalBufferedInputs(
        m_coordinator,
        m_coordinator.isActive(),
        false,
        room.state,
        localSlots,
        0,
        room,
        builder,
        emu.regionFps(),
        emu.frameCount(),
        m_inputDriver.confirmedThroughFrame(m_coordinator)
    );

    if(m_coordinator.isActive() && room.state == SessionState::Running) {
        m_inputDriver.preparePlaybackFramesForEmulationThread(m_coordinator, true, false, room.state, emu.frameCount());
        queuePendingFramesToEmu(emu);
    }

    updateUiSnapshot(localRom);
}

} // namespace ConsoleNetplay
