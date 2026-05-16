#include "ConsoleNetplay/NetplayAppRuntime.h"

#include <algorithm>
#include <utility>

#include "ConsoleNetplay/NetplayInputAssignment.h"

namespace ConsoleNetplay {

namespace {

template<typename Result>
void applyRuntimeRecoveryResult(const Result& result,
                                FrameNumber& lastLoadedAuthoritativeFrame,
                                RuntimeRollbackProcessState& rollbackProcessState)
{
    if(result.loadedAuthoritativeFrame != 0) {
        lastLoadedAuthoritativeFrame = result.loadedAuthoritativeFrame;
    }
    if(result.reanchorFrame != 0) {
        rollbackProcessState.lastRecoveryReanchorFrame = result.reanchorFrame;
    }
}

} // namespace

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

void NetplayAppRuntime::enqueueRuntimeCommand(RuntimeCommand command)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_pendingRuntimeCommands.emplace_back(std::move(command));
}

void NetplayAppRuntime::drainRuntimeCommands()
{
    std::deque<RuntimeCommand> commands;
    {
        std::scoped_lock stateLock(m_stateMutex);
        commands.swap(m_pendingRuntimeCommands);
    }

    for(auto& command : commands) {
        command(*this);
    }
}

void NetplayAppRuntime::setLocalReconnectToken(uint64_t token)
{
    {
        std::scoped_lock stateLock(m_stateMutex);
        m_cachedReconnectToken = token;
        m_hasCachedReconnectToken = true;
    }
    enqueueRuntimeCommand([token](NetplayAppRuntime& self) {
        self.m_coordinator.setLocalReconnectToken(token);
    });
}

void NetplayAppRuntime::refreshLocalRomSelectionImmediate()
{
    enqueueRuntimeCommand([](NetplayAppRuntime& self) {
        self.m_romValidationState.lastSelectedRomKey.clear();
        self.m_romValidationState.lastSubmittedValidationKey.clear();
    });
}

void NetplayAppRuntime::recordFramePacing(uint32_t dtMs,
                                          uint32_t framesAdvanced,
                                          uint32_t catchupFrames,
                                          bool netplayOverrideActive,
                                          bool cadenceMatched)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_framePacingDiagnostics.record(
        dtMs,
        framesAdvanced,
        catchupFrames,
        netplayOverrideActive,
        cadenceMatched
    );
    m_uiSnapshot.framePacingDiagnostics = m_framePacingDiagnostics;
}

void NetplayAppRuntime::setRuntimeHostWakeCallback(std::function<void()> callback)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_runtimeHostWakeCallback = std::move(callback);
}

void NetplayAppRuntime::setRepeatedInputFrameTransformer(
    std::function<NetplayInputFrame(const NetplayInputFrame&, FrameNumber)> transformer)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_coordinator.setRepeatedInputFrameTransformer(std::move(transformer));
}

NetplayAppRuntime::UiSnapshot NetplayAppRuntime::uiSnapshot() const
{
    std::scoped_lock stateLock(m_stateMutex);
    return m_uiSnapshot;
}

void NetplayAppRuntime::injectDropNextIncomingMessages(MessageType type, uint32_t count)
{
    enqueueRuntimeCommand([=](NetplayAppRuntime& self) {
        self.m_coordinator.dropNextIncomingMessages(type, count);
    });
}

void NetplayAppRuntime::clearIncomingMessageDrops()
{
    enqueueRuntimeCommand([](NetplayAppRuntime& self) {
        self.m_coordinator.clearIncomingMessageDrops();
    });
}

void NetplayAppRuntime::setReconnectReservationTimeoutForTests(uint32_t seconds)
{
    enqueueRuntimeCommand([=](NetplayAppRuntime& self) {
        self.m_coordinator.setReconnectReservationDurationForTests(seconds);
    });
}

void NetplayAppRuntime::simulateTransportFailureForTests()
{
    {
        std::scoped_lock stateLock(m_stateMutex);
        m_uiSnapshot.active = false;
        m_uiSnapshot.hosting = false;
        m_uiSnapshot.connected = false;
        m_uiSnapshot.reconnecting = false;
        m_uiSnapshot.reconnectSecondsRemaining = 0;
    }
    enqueueRuntimeCommand([](NetplayAppRuntime& self) {
        self.m_coordinator.simulateTransportFailureForTests();
        self.m_inputDriver.reset();
        self.m_runtimeLastTickTime = {};
    });
}

void NetplayAppRuntime::drainRuntimeCommandsForTests()
{
    drainRuntimeCommands();
}

size_t NetplayAppRuntime::pendingInputTopologyChangeCountForTests() const
{
    std::scoped_lock stateLock(m_stateMutex);
    return m_pendingInputTopologyChanges.size();
}

NetplayCoordinator& NetplayAppRuntime::coordinatorForTests()
{
    return m_coordinator;
}

const NetplayCoordinator& NetplayAppRuntime::coordinatorForTests() const
{
    return m_coordinator;
}

void NetplayAppRuntime::processPendingInputTopologyChangesForTests(INetplayConsole& console,
                                                                   INetplayStateBridge& stateBridge,
                                                                   INetplayStateHostBridge& hostBridge)
{
    processPendingInputTopologyChanges(console, stateBridge, hostBridge);
}

void NetplayAppRuntime::setTransportBackend(NetTransportBackend backend)
{
    enqueueRuntimeCommand([backend](NetplayAppRuntime& self) {
        const bool changed = self.m_coordinator.setTransportBackend(backend);
        if(!changed && self.m_coordinator.isActive()) {
            self.m_stickyStatusMessage = "Disconnect before changing the netplay backend.";
        } else if(changed) {
            self.m_stickyStatusMessage.clear();
        }
    });
}

void NetplayAppRuntime::setTransportOptions(const NetTransportOptions& options)
{
    enqueueRuntimeCommand([options](NetplayAppRuntime& self) {
        self.m_coordinator.setTransportOptions(options);
    });
}

void NetplayAppRuntime::configureRollbackWindow(size_t snapshotCapacity)
{
    enqueueRuntimeCommand([snapshotCapacity](NetplayAppRuntime& self) {
        self.m_pendingSnapshotCapacity = snapshotCapacity;
    });
}

void NetplayAppRuntime::notifyWebVisibilityChanged(bool visible)
{
    enqueueRuntimeCommand([visible](NetplayAppRuntime& self) {
        self.m_pendingWebVisibilityChange = visible;
    });
}

NetTransportOptions NetplayAppRuntime::transportOptions() const
{
    return m_coordinator.transportOptions();
}

void NetplayAppRuntime::host(uint16_t port, size_t maxPeers, const std::string& displayName)
{
    enqueueRuntimeCommand([=](NetplayAppRuntime& self) {
        self.m_stickyStatusMessage.clear();
        if(!self.m_latestLocalRom.has_value() || !self.m_latestLocalRom->loaded) {
            self.m_stickyStatusMessage = "Load a ROM before creating a room.";
            return;
        }
        self.m_coordinator.host(port, maxPeers, displayName);
    });
}

void NetplayAppRuntime::join(const std::string& hostName, uint16_t port, const std::string& displayName)
{
    enqueueRuntimeCommand([=](NetplayAppRuntime& self) {
        self.m_stickyStatusMessage.clear();
        if(self.m_hasCachedReconnectToken) {
            self.m_coordinator.setLocalReconnectToken(self.m_cachedReconnectToken);
        }
        self.m_coordinator.setPendingJoinRomValidation(
            self.m_latestLocalRom.has_value() && self.m_latestLocalRom->loaded,
            self.m_latestLocalRom.has_value() ? self.m_latestLocalRom->validation : RomValidationData{}
        );
        self.m_coordinator.join(hostName, port, displayName);
    });
}

void NetplayAppRuntime::disconnect()
{
    wakeRuntimeHost();
    {
        std::scoped_lock stateLock(m_stateMutex);
        const NetTransportBackend backend = m_uiSnapshot.transportBackend;
        m_uiSnapshot = UiSnapshot{};
        m_uiSnapshot.transportBackend = backend;
    }
    enqueueRuntimeCommand([](NetplayAppRuntime& self) {
        self.m_coordinator.disconnect();
        self.resetInactiveRuntimeState();
    });
}

void NetplayAppRuntime::assignController(ParticipantId participantId, PlayerSlot slot)
{
    wakeRuntimeHost();
    enqueueRuntimeCommand([=](NetplayAppRuntime& self) {
        self.m_coordinator.assignController(participantId, slot);
    });
}

void NetplayAppRuntime::addControllerAssignment(ParticipantId participantId, PlayerSlot slot)
{
    wakeRuntimeHost();
    enqueueRuntimeCommand([=](NetplayAppRuntime& self) {
        self.m_coordinator.addControllerAssignment(participantId, slot);
    });
}

void NetplayAppRuntime::removeControllerAssignment(ParticipantId participantId, PlayerSlot slot)
{
    wakeRuntimeHost();
    enqueueRuntimeCommand([=](NetplayAppRuntime& self) {
        self.m_coordinator.removeControllerAssignment(participantId, slot);
    });
}

void NetplayAppRuntime::clearControllerAssignments(ParticipantId participantId)
{
    wakeRuntimeHost();
    enqueueRuntimeCommand([=](NetplayAppRuntime& self) {
        self.m_coordinator.clearControllerAssignments(participantId);
    });
}

void NetplayAppRuntime::configureInputAssignments(ParticipantId participantId,
                                                  std::vector<PlayerSlot> slots,
                                                  InputTopologyConfigurer configureTopology)
{
    wakeRuntimeHost();
    enqueueRuntimeCommand([=, configureTopology = std::move(configureTopology)](NetplayAppRuntime& self) mutable {
        PendingInputTopologyChange change;
        change.participantId = participantId;
        change.slots = std::move(slots);
        change.configureTopology = std::move(configureTopology);
        self.m_pendingInputTopologyChanges.emplace_back(std::move(change));
    });
}

void NetplayAppRuntime::kickParticipant(ParticipantId participantId)
{
    enqueueRuntimeCommand([=](NetplayAppRuntime& self) {
        self.m_coordinator.kickParticipant(participantId);
    });
}

void NetplayAppRuntime::removeReconnectReservation(ParticipantId participantId)
{
    enqueueRuntimeCommand([=](NetplayAppRuntime& self) {
        self.m_coordinator.removeReconnectReservation(participantId);
    });
}

void NetplayAppRuntime::requestForceResync()
{
    enqueueRuntimeCommand([](NetplayAppRuntime& self) {
        self.m_forceResyncRequested = true;
    });
}

void NetplayAppRuntime::toggleHostedSessionPause()
{
    const UiSnapshot snapshot = uiSnapshot();
    if(snapshot.active && snapshot.hosting && snapshot.room.state == SessionState::Paused) {
        wakeRuntimeHost();
    }
    enqueueRuntimeCommand([](NetplayAppRuntime& self) {
        self.m_hostedPauseToggleRequested = true;
    });
}

void NetplayAppRuntime::appendNetplayLog(const std::string& message)
{
    enqueueRuntimeCommand([message](NetplayAppRuntime& self) {
        self.m_coordinator.appendNetplayLog(message);
    });
}

void NetplayAppRuntime::clearNetplayLog()
{
    enqueueRuntimeCommand([](NetplayAppRuntime& self) {
        self.m_coordinator.clearEventLog();
    });
}

void NetplayAppRuntime::shutdown()
{
    wakeRuntimeHost();
    std::scoped_lock stateLock(m_stateMutex);
    m_pendingRuntimeCommands.clear();
    m_pendingInputTopologyChanges.clear();
    m_runtimeActive.store(false, std::memory_order_release);
    m_runtimeRunning.store(false, std::memory_order_release);
    m_uiSnapshot = UiSnapshot{};
    m_webVisibilityManagedPause = false;
    m_webPageVisible = true;
    m_selfStallDetector.reset();
    m_coordinator.disconnect();
}

void NetplayAppRuntime::shutdownForUnload()
{
    wakeRuntimeHost();
    std::scoped_lock stateLock(m_stateMutex);
    m_pendingRuntimeCommands.clear();
    m_pendingInputTopologyChanges.clear();
    m_runtimeActive.store(false, std::memory_order_release);
    m_runtimeRunning.store(false, std::memory_order_release);
    m_uiSnapshot = UiSnapshot{};
    m_webVisibilityManagedPause = false;
    m_webPageVisible = true;
    m_selfStallDetector.reset();
    m_coordinator.shutdownForUnload();
}

bool NetplayAppRuntime::tryBuildPlaybackFrame(FrameNumber frame,
                                              NetplayCoordinator::ConfirmedFrameInputs& outFrame)
{
    m_coordinator.recordLocalAuthoritativeFrameStart(frame);
    return runtimeTryBuildPlaybackConfirmedFrame(m_coordinator, m_inputDriver, frame, outFrame);
}

NetplayAppRuntime::UpdateResult NetplayAppRuntime::update(UpdateContext context)
{
    UpdateResult result;
    const std::optional<RomSelection> localRom = context.console.currentRomSelection();
    m_latestLocalRom = localRom;
    drainRuntimeCommands();
    processPendingInputTopologyChanges(context.console, context.stateBridge, context.hostBridge);

    if(m_hasCachedReconnectToken) {
        m_coordinator.setLocalReconnectToken(m_cachedReconnectToken);
    }

    const RuntimeInputDelayResult inputDelay =
        syncInputDelayFromSettings(context.inputDelaySettings);
    result.inputBufferCapacity = inputDelay.inputBufferCapacity;
    result.inputDelayFrames = inputDelay.inputDelayFrames;
    result.predictFrames = inputDelay.predictFrames;
    if(m_pendingSnapshotCapacity.has_value()) {
        result.snapshotCapacity = m_pendingSnapshotCapacity;
        m_pendingSnapshotCapacity.reset();
    }

    if(!m_coordinator.isActive() && m_coordinator.reconnectPending()) {
        m_coordinator.update(0);
    }

    if(!m_coordinator.isActive()) {
        result.active = false;
        result.netplayOwnsEmulationInput = false;
        result.autoQueuePendingInputOnFrameStart = true;
        result.allowPresenterTimeoutAdvance = true;
        result.simulationSuspended = false;
        if(context.applyHostInputOwnership) {
            context.applyHostInputOwnership(result.netplayOwnsEmulationInput, result.allowPresenterTimeoutAdvance);
        }
        ensureStandaloneInputBootstrapFrame(context.console, context.stateBridge);
        resetInactiveRuntimeState();
        updateUiSnapshot(localRom, context.frameSettings.diagnostics);
        return result;
    }

    result.active = true;
    m_runtimeActive.store(true, std::memory_order_release);
    result.allowPresenterTimeoutAdvance = false;
    const auto applyHostInputOwnershipForCurrentState = [&]() {
        result.netplayOwnsEmulationInput = runtimeShouldNetplayOwnEmulationInput(m_coordinator);
        result.autoQueuePendingInputOnFrameStart = !result.netplayOwnsEmulationInput;
        context.applyHostInputOwnership(result.netplayOwnsEmulationInput, result.allowPresenterTimeoutAdvance);
    };

    if(m_pendingWebVisibilityChange.has_value()) {
        const bool visible = *m_pendingWebVisibilityChange;
        m_pendingWebVisibilityChange.reset();
        m_runtimeLastTickTime = {};
        m_webPageVisible = visible;

        if(!visible) {
            if(m_coordinator.isActive() &&
               m_coordinator.isConnected() &&
               m_coordinator.isHosting() &&
               m_coordinator.session().roomState().state == SessionState::Running &&
               m_coordinator.pauseSession()) {
                m_webVisibilityManagedPause = true;
                result.discardQueuedAudio = true;
            }

            m_sessionTransitionState.observerVisibilityResyncPending =
                m_coordinator.isActive() &&
                m_coordinator.isConnected() &&
                !m_coordinator.isHosting() &&
                m_coordinator.session().roomState().state == SessionState::Running &&
                runtimeLocalAssignedSlots(m_coordinator).empty();
        } else {
            if(m_webVisibilityManagedPause) {
                const SessionState state = m_coordinator.session().roomState().state;
                if(state == SessionState::Paused) {
                    if(m_coordinator.resumeSession()) {
                        m_webVisibilityManagedPause = false;
                    }
                } else {
                    m_webVisibilityManagedPause = false;
                }
            }

            if(m_sessionTransitionState.observerVisibilityResyncPending) {
                m_inputDriver.reset();
                context.stateBridge.discardQueuedInputFramesAfter(context.console.frameCount());
                if(context.frameSettings.discardQueuedNetplayInputsAfter) {
                    context.frameSettings.discardQueuedNetplayInputsAfter(context.console.frameCount());
                }
                result.simulationSuspended = true;
                if(!m_coordinator.requestHostResync(ResyncReason::ObserverVisibilityRestore)) {
                    m_sessionTransitionState.observerVisibilityResyncPending = false;
                    result.simulationSuspended = false;
                }
                if(context.applyHostInputOwnership) {
                    applyHostInputOwnershipForCurrentState();
                }
                return result;
            }

            if(context.frameSettings.discardQueuedNetplayInputsAfter) {
                context.frameSettings.discardQueuedNetplayInputsAfter(context.console.frameCount());
            }
            const SessionState state = m_coordinator.session().roomState().state;
            if(state == SessionState::Running) {
                reanchorInputDriver(context.console.frameCount());
            } else {
                m_inputDriver.reset();
            }
            result.simulationSuspended = state == SessionState::Paused;
        }
    }

    if(m_hostedPauseToggleRequested) {
        m_hostedPauseToggleRequested = false;
        if(m_coordinator.isActive() && m_coordinator.isHosting()) {
            const SessionState state = m_coordinator.session().roomState().state;
            if(state == SessionState::Running) {
                if(m_coordinator.pauseSession()) {
                    m_webVisibilityManagedPause = false;
                    m_runtimeLastTickTime = {};
                    m_coordinator.appendNetplayLog("Paused");
                    result.discardQueuedAudio = true;
                }
            } else if(state == SessionState::Paused) {
                if(!computeSessionBlockedReason(localRom).empty()) {
                    result.simulationSuspended = true;
                    if(context.applyHostInputOwnership) {
                        applyHostInputOwnershipForCurrentState();
                    }
                    return result;
                }
                if(m_coordinator.resumeSession()) {
                    m_webVisibilityManagedPause = false;
                    m_runtimeLastTickTime = {};
                    m_coordinator.appendNetplayLog("Unpaused");
                } else {
                    result.simulationSuspended = true;
                    if(context.applyHostInputOwnership) {
                        applyHostInputOwnershipForCurrentState();
                    }
                    return result;
                }
            }
        }
    }

    if(m_forceResyncRequested) {
        m_forceResyncRequested = false;
        const auto state = m_coordinator.session().roomState().state;
        if(m_coordinator.isHosting() &&
           context.console.valid() &&
           (state == SessionState::Running || state == SessionState::Paused)) {
            const FrameNumber authoritativeFrame = context.console.frameCount();
            const std::vector<uint8_t> statePayload =
                buildAuthoritativeStatePayload(context.stateBridge, context.hostBridge, authoritativeFrame, false);
            beginAuthoritativeResync(
                context.stateBridge,
                context.hostBridge,
                authoritativeFrame,
                statePayload,
                false,
                ResyncReason::ManualForce
           );
        }
    }

    if(context.applyHostInputOwnership) {
        applyHostInputOwnershipForCurrentState();
    } else {
        result.netplayOwnsEmulationInput = runtimeShouldNetplayOwnEmulationInput(m_coordinator);
        result.autoQueuePendingInputOnFrameStart = !result.netplayOwnsEmulationInput;
    }
    if(!result.netplayOwnsEmulationInput) {
        ensureStandaloneInputBootstrapFrame(context.console, context.stateBridge);
    }

    const RuntimeFrameResult frameResult = runActiveConsoleFrame(
        context.console,
        context.stateBridge,
        context.hostBridge,
        context.sessionControls,
        localRom,
        context.manualEvents,
        context.frameSettings
    );

    result.running = frameResult.running;
    result.paused = frameResult.paused;
    result.simulationSuspended = frameResult.paused;
    return result;
}

void NetplayAppRuntime::wakeRuntimeHost()
{
    std::function<void()> callback;
    {
        std::scoped_lock stateLock(m_stateMutex);
        callback = m_runtimeHostWakeCallback;
    }
    if(callback) {
        callback();
    }
}

void NetplayAppRuntime::processPendingInputTopologyChanges(INetplayConsole& console,
                                                           INetplayStateBridge& stateBridge,
                                                           INetplayStateHostBridge& hostBridge)
{
    while(true) {
        PendingInputTopologyChange change;
        {
            std::scoped_lock stateLock(m_stateMutex);
            if(m_pendingInputTopologyChanges.empty()) {
                return;
            }
            change = std::move(m_pendingInputTopologyChanges.front());
            m_pendingInputTopologyChanges.pop_front();
        }

        if(!change.configureTopology) {
            continue;
        }

        const RoomState& room = m_coordinator.session().roomState();
        if(room.state == SessionState::Resyncing ||
           room.activeResyncId != 0 ||
           room.pendingResyncAckCount != 0 ||
           room.recoveryInputMode != RecoveryInputMode::Normal) {
            std::scoped_lock stateLock(m_stateMutex);
            m_pendingInputTopologyChanges.emplace_front(std::move(change));
            return;
        }

        const FrameNumber rebuildFromFrame = console.frameCount() > 0 ? (console.frameCount() - 1u) : 0u;
        const PlayerSlot preservedSlot = change.slots.empty() ? kObserverPlayerSlot : change.slots.front();
        m_coordinator.setLocalSimulationFrame(rebuildFromFrame);
        change.configureTopology(m_coordinator, change.participantId, preservedSlot);
        m_coordinator.clearControllerAssignments(change.participantId);
        for(PlayerSlot slot : change.slots) {
            if(slot == kObserverPlayerSlot) continue;
            m_coordinator.addControllerAssignment(change.participantId, slot);
        }
        console.applyRemoteInputTopology(m_coordinator.session().roomState());
        console.discardQueuedInputFramesAfter(rebuildFromFrame);
        reanchorInputDriver(rebuildFromFrame);
        m_lastAssignmentLayoutKey.clear();
        m_lastLocalAssignedSlots.clear();

        const SessionState state = m_coordinator.session().roomState().state;
        const bool shouldResyncImmediately =
            m_coordinator.isHosting() &&
            console.valid() &&
            (state == SessionState::Running || state == SessionState::Paused);
        if(!shouldResyncImmediately) {
            continue;
        }

        (void)m_coordinator.consumePendingHostResyncFrame();

        const FrameNumber authoritativeFrame = console.frameCount();
        const std::vector<uint8_t> statePayload =
            buildAuthoritativeStatePayload(stateBridge, hostBridge, authoritativeFrame, false);
        beginAuthoritativeResync(
            stateBridge,
            hostBridge,
            authoritativeFrame,
            statePayload,
            false,
            ResyncReason::AssignmentChanged
        );
    }
}

void NetplayAppRuntime::resetInactiveRuntimeState()
{
    m_runtimeActive.store(false, std::memory_order_release);
    m_runtimeRunning.store(false, std::memory_order_release);
    m_inputDriver.reset();
    m_selfStallDetector.reset();
    m_runtimeLastTickTime = {};
    m_romValidationState.lastSelectedRomKey.clear();
    m_romValidationState.lastSubmittedValidationKey.clear();
    m_sessionTransitionState.lastSessionState.reset();
    m_lastLocalAssignedSlots.clear();
    m_lastAssignmentLayoutKey.clear();
    m_pendingManualStateResyncs.clear();
    m_periodicCrcState = RuntimePeriodicCrcState{};
    m_rollbackProcessState = RuntimeRollbackProcessState{};
    m_lastLoadedAuthoritativeFrame = 0;
    m_sharedClockCatchupState = RuntimeSharedClockCatchupState{};
    m_sessionTransitionState.observerVisibilityResyncPending = false;
    m_webVisibilityManagedPause = false;
    m_webPageVisible = true;
}

void NetplayAppRuntime::reanchorInputDriver(FrameNumber anchorFrame)
{
    m_inputDriver.reanchor(anchorFrame);
    m_rollbackProcessState.lastRecoveryReanchorFrame = anchorFrame;
}

std::vector<uint8_t> NetplayAppRuntime::buildAuthoritativeStatePayload(
    INetplayStateBridge& stateBridge,
    const INetplayStateHostBridge& hostBridge,
    FrameNumber authoritativeFrame,
    bool preferConfirmedSnapshot) const
{
    return runtimeBuildAuthoritativeStatePayload(
        stateBridge,
        hostBridge,
        authoritativeFrame,
        preferConfirmedSnapshot
    );
}

bool NetplayAppRuntime::beginAuthoritativeResync(INetplayStateBridge& stateBridge,
                                                 INetplayStateHostBridge& hostBridge,
                                                 FrameNumber authoritativeFrame,
                                                 const std::vector<uint8_t>& statePayload,
                                                 bool preferConfirmedSnapshot,
                                                 ResyncReason reason,
                                                 ParticipantId targetParticipantId)
{
    const RuntimeAuthoritativeStateResult result = runtimeBeginAuthoritativeResync(
        m_coordinator,
        m_inputDriver,
        stateBridge,
        hostBridge,
        authoritativeFrame,
        statePayload,
        preferConfirmedSnapshot,
        reason,
        targetParticipantId
    );
    applyRuntimeRecoveryResult(result, m_lastLoadedAuthoritativeFrame, m_rollbackProcessState);
    return result.started;
}

std::string NetplayAppRuntime::computeSessionBlockedReason(const std::optional<RomSelection>& localRom) const
{
    return runtimeSessionBlockedReason(
        m_coordinator.isActive(),
        m_coordinator.session().roomState(),
        localRom
    );
}

void NetplayAppRuntime::syncRomValidation(const std::optional<RomSelection>& localRom)
{
    m_romValidationState.stickyStatusMessage = m_stickyStatusMessage;
    const RuntimeRomValidationResult result =
        runtimeSyncRomValidation(m_coordinator, m_romValidationState, localRom);
    m_stickyStatusMessage = result.stickyStatusMessage;

    if(!m_coordinator.isActive() && !result.disconnectedForMismatch) {
        m_sessionTransitionState.lastSessionState.reset();
        return;
    }

    if(result.disconnectedForMismatch) {
        m_inputDriver.reset();
        m_runtimeLastTickTime = {};
        m_lastLocalAssignedSlots.clear();
        m_lastAssignmentLayoutKey.clear();
    }
}

RuntimeInputDelayResult NetplayAppRuntime::syncInputDelayFromSettings(const RuntimeInputDelaySettings& settings)
{
    return runtimeSyncInputDelaySettings(m_coordinator, m_inputDriver, m_autoSettings, settings);
}

bool NetplayAppRuntime::processAutoStartIfNeeded(INetplayStateBridge& stateBridge,
                                                 INetplayStateHostBridge& hostBridge,
                                                 const std::optional<RomSelection>& localRom)
{
    const RuntimeAutoStartResult result =
        runtimeProcessAutoStartIfNeeded(m_coordinator, stateBridge, localRom);
    if(result.initialSyncNeeded) {
        return beginInitialSessionSyncOnWorker(stateBridge, hostBridge);
    }
    return result.started;
}

void NetplayAppRuntime::processHostManualStateChangeResyncIfNeeded(
    INetplayStateBridge& stateBridge,
    INetplayStateHostBridge& hostBridge,
    const std::vector<NetplayManualStateChangeRecord>& events)
{
    const RuntimeManualStateResyncProcessResult result =
        runtimeProcessHostManualStateChangesIfNeeded(
            m_coordinator,
            m_inputDriver,
            stateBridge,
            hostBridge,
            m_pendingManualStateResyncs,
            events
        );
    applyRuntimeRecoveryResult(result, m_lastLoadedAuthoritativeFrame, m_rollbackProcessState);
}

void NetplayAppRuntime::processPendingManualStateResyncIfNeeded(INetplayStateBridge& stateBridge,
                                                                INetplayStateHostBridge& hostBridge)
{
    const RuntimeManualStateResyncProcessResult result =
        runtimeProcessPendingManualStateResyncIfNeeded(
            m_coordinator,
            m_inputDriver,
            stateBridge,
            hostBridge,
            m_pendingManualStateResyncs
        );
    applyRuntimeRecoveryResult(result, m_lastLoadedAuthoritativeFrame, m_rollbackProcessState);
}

void NetplayAppRuntime::processPeriodicLocalCrcIfNeeded(INetplayStateBridge& stateBridge,
                                                        const INetplayStateHostBridge& hostBridge)
{
    m_periodicCrcState.lastLoadedAuthoritativeFrame = m_lastLoadedAuthoritativeFrame;
    (void)runtimeSubmitPeriodicLocalCrcIfNeeded(
        m_coordinator,
        stateBridge,
        hostBridge,
        m_periodicCrcState
    );
}

uint32_t NetplayAppRuntime::consumeWorkerDtMs()
{
    const auto now = std::chrono::steady_clock::now();
    uint32_t dtMs = 0;
    if(m_runtimeLastTickTime.time_since_epoch().count() != 0) {
        dtMs = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - m_runtimeLastTickTime).count()
        );
    }
    m_runtimeLastTickTime = now;
    return std::min<uint32_t>(dtMs, 100u);
}

void NetplayAppRuntime::handleSessionStateTransitionsOnWorker(INetplayRuntimeSessionControls& controls)
{
    const RuntimeSessionTransitionResult result =
        runtimeHandleSessionStateTransitions(
            m_coordinator,
            m_inputDriver,
            controls,
            m_sessionTransitionState,
            m_periodicCrcState,
            m_rollbackProcessState,
            m_sharedClockCatchupState,
            m_lastLoadedAuthoritativeFrame
        );
    if(result.resetWorkerTick) {
        m_runtimeLastTickTime = {};
    }
}

bool NetplayAppRuntime::beginInitialSessionSyncOnWorker(INetplayStateBridge& stateBridge,
                                                        INetplayStateHostBridge& hostBridge)
{
    const RuntimeHostResyncProcessResult result =
        runtimeBeginInitialSessionSyncIfNeeded(
            m_coordinator,
            m_inputDriver,
            stateBridge,
            hostBridge
        );
    applyRuntimeRecoveryResult(result, m_lastLoadedAuthoritativeFrame, m_rollbackProcessState);
    return result.started;
}

void NetplayAppRuntime::processHostResyncIfNeededOnWorker(INetplayStateBridge& stateBridge,
                                                          INetplayStateHostBridge& hostBridge,
                                                          bool autoGameplayTuning)
{
    const RuntimeHostResyncProcessResult result =
        runtimeProcessHostResyncIfNeeded(
            m_coordinator,
            m_inputDriver,
            m_autoSettings,
            stateBridge,
            hostBridge,
            autoGameplayTuning
        );
    applyRuntimeRecoveryResult(result, m_lastLoadedAuthoritativeFrame, m_rollbackProcessState);
}

void NetplayAppRuntime::processHostLateJoinResyncIfNeededOnWorker(INetplayStateBridge& stateBridge,
                                                                  INetplayStateHostBridge& hostBridge)
{
    const RuntimeHostResyncProcessResult result =
        runtimeProcessHostLateJoinResyncIfNeeded(
            m_coordinator,
            m_inputDriver,
            stateBridge,
            hostBridge
        );
    applyRuntimeRecoveryResult(result, m_lastLoadedAuthoritativeFrame, m_rollbackProcessState);
}

void NetplayAppRuntime::processHostStallIfNeededOnWorker(INetplayStateBridge& stateBridge,
                                                         INetplayStateHostBridge& hostBridge,
                                                         std::chrono::steady_clock::time_point now)
{
    const RuntimeHostResyncProcessResult result =
        runtimeProcessSelfStallRecoveryIfNeeded(
            m_coordinator,
            m_inputDriver,
            m_selfStallDetector,
            stateBridge,
            hostBridge,
            now
        );
    applyRuntimeRecoveryResult(result, m_lastLoadedAuthoritativeFrame, m_rollbackProcessState);
}

RuntimePendingResyncApplyResult NetplayAppRuntime::processResyncIfNeededOnWorker(
    INetplayStateBridge& stateBridge,
    INetplayStateHostBridge& hostBridge)
{
    const RuntimePendingResyncApplyResult result =
        runtimeProcessPendingResyncApplyIfNeeded(
            m_coordinator,
            m_inputDriver,
            stateBridge,
            hostBridge
        );
    if(result.consumed) {
        applyRuntimeRecoveryResult(result, m_lastLoadedAuthoritativeFrame, m_rollbackProcessState);
    }
    return result;
}

uint32_t NetplayAppRuntime::advanceToSharedClockIfNeededOnWorker(INetplayConsole& console,
                                                                 uint32_t maxFrames,
                                                                 bool requireLagTrigger)
{
    return runtimeAdvanceToSharedClockIfNeeded(
        m_coordinator,
        m_inputDriver,
        console,
        m_sharedClockCatchupState,
        maxFrames,
        requireLagTrigger
    );
}

void NetplayAppRuntime::processRollbackIfNeededOnWorker(INetplayConsole& console,
                                                        INetplayStateBridge& stateBridge,
                                                        INetplayStateHostBridge& hostBridge,
                                                        const RuntimeRollbackProcessSettings& settings)
{
    const RuntimeRollbackProcessResult result =
        runtimeProcessRollbackIfNeeded(
            m_coordinator,
            m_inputDriver,
            console,
            stateBridge,
            hostBridge,
            m_rollbackProcessState,
            settings
        );

    if(result.reanchorFrame != 0) {
        m_rollbackProcessState.lastRecoveryReanchorFrame = result.reanchorFrame;
    }
}

NetplayAppRuntime::RuntimeFrameResult NetplayAppRuntime::runActiveConsoleFrame(
    INetplayConsole& console,
    INetplayStateBridge& stateBridge,
    INetplayStateHostBridge& hostBridge,
    INetplayRuntimeSessionControls& sessionControls,
    const std::optional<RomSelection>& localRom,
    const std::vector<NetplayManualStateChangeRecord>& manualEvents,
    const RuntimeFrameSettings& settings)
{
    const RoomState roomState = m_coordinator.session().roomState();
    if(!m_coordinator.isHosting()) {
        console.applyRemoteInputTopology(roomState);
    }
    console.publishCurrentInputTopology(m_coordinator);
    syncRomValidation(localRom);
    syncEmuInputTimelineEpoch(stateBridge);
    m_coordinator.setLocalSimulationFrame(console.frameCount());
    m_coordinator.update(0);
    syncEmuInputTimelineEpoch(stateBridge);
    handleSessionStateTransitionsOnWorker(sessionControls);
    processAutoStartIfNeeded(stateBridge, hostBridge, localRom);
    processHostManualStateChangeResyncIfNeeded(stateBridge, hostBridge, manualEvents);
    processPendingManualStateResyncIfNeeded(stateBridge, hostBridge);

    if(m_coordinator.isHosting() &&
       m_coordinator.session().roomState().state == SessionState::Starting &&
       m_coordinator.session().roomState().activeResyncId == 0) {
        beginInitialSessionSyncOnWorker(stateBridge, hostBridge);
    }

    processHostResyncIfNeededOnWorker(stateBridge, hostBridge, settings.autoGameplayTuning);
    processHostLateJoinResyncIfNeededOnWorker(stateBridge, hostBridge);

    const RuntimePendingResyncApplyResult resyncResult =
        processResyncIfNeededOnWorker(stateBridge, hostBridge);
    if(resyncResult.loadedExpectedFrame &&
       m_coordinator.isActive() &&
       m_coordinator.session().roomState().state == SessionState::Running &&
       console.valid()) {
        constexpr uint32_t kMaxSilentCatchupFrames = 120u;
        const uint32_t advancedFrames = advanceToSharedClockIfNeededOnWorker(
            console,
            kMaxSilentCatchupFrames,
            false
        );

        if(advancedFrames > 0u) {
            m_coordinator.appendNetplayLog(
                "Netplay resync shared-clock alignment advanced " +
                std::to_string(advancedFrames) +
                " frame(s) from " +
                std::to_string(resyncResult.targetFrame) +
                " to " +
                std::to_string(console.frameCount()) +
                " (audio muted)"
            );
        }
    }

    (void)runtimeProcessAutoResumeIfNeeded(
        m_coordinator,
        m_webVisibilityManagedPause,
        m_webPageVisible,
        localRom
    );

    RuntimeRollbackProcessSettings rollbackSettings;
    rollbackSettings.showDebugLog = settings.showDebugLog;
    processRollbackIfNeededOnWorker(console, stateBridge, hostBridge, rollbackSettings);
    processHostStallIfNeededOnWorker(stateBridge, hostBridge, std::chrono::steady_clock::now());

    RuntimeFrameResult result;
    result.paused = m_coordinator.session().roomState().state == SessionState::Paused;
    result.running = m_coordinator.session().roomState().state == SessionState::Running;
    m_runtimeRunning.store(result.running, std::memory_order_release);

    RuntimeAssignmentLayoutResult assignmentLayout = runtimeSyncAssignmentLayout(
        m_coordinator,
        m_inputDriver,
        m_lastAssignmentLayoutKey,
        m_lastLocalAssignedSlots,
        console.frameCount(),
        result.running
    );
    if(assignmentLayout.layoutChanged || assignmentLayout.localSlotsChanged) {
        console.discardQueuedInputFramesAfter(console.frameCount());
        if(settings.discardQueuedNetplayInputsAfter) {
            settings.discardQueuedNetplayInputsAfter(console.frameCount());
        }
    }
    if(assignmentLayout.reanchorInputDriver) {
        m_rollbackProcessState.lastRecoveryReanchorFrame = console.frameCount();
    }

    const uint32_t workerDtMs = consumeWorkerDtMs();
    runtimeProduceLocalBufferedInputs(
        m_coordinator,
        m_inputDriver,
        console,
        assignmentLayout.localSlots,
        workerDtMs
    );

    if(result.running) {
        constexpr uint32_t kMaxObserverPeerCatchupFrames = 120u;
        (void)runtimeAdvanceObserverPeerIfNeeded(
            m_coordinator,
            m_inputDriver,
            console,
            kMaxObserverPeerCatchupFrames,
            settings.showDebugLog
        );

        constexpr uint32_t kMaxContinuousClockCatchupFrames = 120u;
        (void)advanceToSharedClockIfNeededOnWorker(console, kMaxContinuousClockCatchupFrames);

        runtimePreparePlaybackFrames(m_coordinator, m_inputDriver, console);
        (void)tryQueuePlaybackFrameToConsole(console, console.frameCount());
    }

    processPeriodicLocalCrcIfNeeded(stateBridge, hostBridge);
    updateUiSnapshot(localRom, settings.diagnostics);
    return result;
}

void NetplayAppRuntime::updateUiSnapshot(const std::optional<RomSelection>& localRom,
                                         const NetplayRuntimeDiagnostics& runtimeDiagnostics)
{
    UiSnapshot snapshot = buildNetplayUiSnapshot(
        m_coordinator,
        m_inputDriver,
        localRom,
        m_stickyStatusMessage,
        m_periodicCrcState.lastSubmittedLocalCrcFrame,
        m_rollbackProcessState.lastRollbackTargetFrame,
        m_lastLoadedAuthoritativeFrame,
        m_rollbackProcessState.lastRecoveryReanchorFrame,
        m_autoSettings.snapshot(),
        m_framePacingDiagnostics,
        runtimeDiagnostics,
        computeSessionBlockedReason(localRom)
    );

    std::scoped_lock stateLock(m_stateMutex);
    if(m_coordinator.localReconnectToken() != 0) {
        m_cachedReconnectToken = m_coordinator.localReconnectToken();
        m_hasCachedReconnectToken = true;
    }
    m_uiSnapshot = std::move(snapshot);
}

void NetplayAppRuntime::syncEmuInputTimelineEpoch(INetplayStateBridge& stateBridge)
{
    runtimeSyncEmulatorInputTimelineEpoch(m_coordinator, stateBridge);
}

void NetplayAppRuntime::ensureStandaloneInputBootstrapFrame(INetplayConsole& console,
                                                            INetplayStateBridge& stateBridge)
{
    syncEmuInputTimelineEpoch(stateBridge);
    console.queueStandaloneBootstrapInputFrame();
}

bool NetplayAppRuntime::tryQueuePlaybackFrameToConsole(INetplayConsole& console, FrameNumber frame)
{
    if(console.hasStableQueuedInputFrame(frame)) {
        return true;
    }

    NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
    if(!tryBuildPlaybackFrame(frame, playbackFrame)) {
        return false;
    }

    return console.queuePlaybackInputFrame(playbackFrame);
}

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
    const std::string& sessionBlockedReason)
{
    NetplayAppRuntime::UiSnapshot snapshot;
    snapshot.valid = true;
    snapshot.active = coordinator.isActive();
    snapshot.hosting = coordinator.isHosting();
    snapshot.connected = coordinator.isConnected();
    snapshot.reconnecting = coordinator.reconnectPending();
    snapshot.reconnectSecondsRemaining = coordinator.reconnectSecondsRemaining();
    snapshot.localRomLoaded = localRom.has_value() && localRom->loaded;
    snapshot.localRomGameName = localRom.has_value() ? localRom->gameName : std::string{};
    snapshot.localRomCrc32 = localRom.has_value() ? localRom->validation.romCrc32 : 0;
    snapshot.transportBackend = coordinator.transportBackend();
    snapshot.localParticipantId = coordinator.localParticipantId();
    snapshot.lastError = coordinator.lastError().empty() ? stickyStatusMessage : coordinator.lastError();
    snapshot.room = coordinator.session().roomState();
    snapshot.localInputCount = coordinator.localInputs().size();
    snapshot.remoteInputCount = coordinator.remoteInputs().size();
    snapshot.localInputLookupStats = coordinator.localInputs().lookupStats();
    snapshot.remoteInputLookupStats = coordinator.remoteInputs().lookupStats();
    snapshot.coordinatorPerformanceDiagnostics = coordinator.performanceDiagnostics();
    snapshot.playbackQueueStats = inputDriver.playbackQueueStats();
    if(const TimelineInputEntry* latestLocal = coordinator.localInputs().latest()) {
        snapshot.latestLocalInput = *latestLocal;
    }
    if(const TimelineInputEntry* latestRemote = coordinator.remoteInputs().latest()) {
        snapshot.latestRemoteInput = *latestRemote;
    }
    snapshot.predictionStats = coordinator.predictionStats();
    snapshot.localSimulationFrame = coordinator.localSimulationFrame();
    snapshot.publishedConfirmedFrame = coordinator.latestPublishedConfirmedFrame();
    snapshot.lastSubmittedLocalCrcFrame = lastSubmittedLocalCrcFrame;
    snapshot.lastRollbackTargetFrame = lastRollbackTargetFrame;
    snapshot.lastLoadedAuthoritativeFrame = lastLoadedAuthoritativeFrame;
    snapshot.lastRecoveryReanchorFrame = lastRecoveryReanchorFrame;
    snapshot.autoSettings = autoSettings;
    snapshot.framePacingDiagnostics = framePacingDiagnostics;
    snapshot.unresolvedPredictedRemoteFrameCount = coordinator.unresolvedPredictedRemoteFrameCount();
    snapshot.latestPredictedRemoteFrame = coordinator.latestPredictedRemoteFrame();
    snapshot.runtimeDiagnostics = runtimeDiagnostics;
    snapshot.sessionBlockedReason = sessionBlockedReason;
    snapshot.eventLog = coordinator.eventLog();
    return snapshot;
}

} // namespace ConsoleNetplay
