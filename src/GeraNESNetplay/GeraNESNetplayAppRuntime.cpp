#include "GeraNESNetplay/GeraNESNetplayAppRuntime.h"

#include <algorithm>
#include <sstream>

#include "GeraNESNetplay/GeraNESNetplayAdapters.h"
#include "GeraNESNetplay/GeraNESNetplayConsole.h"
#include "ConsoleNetplay/NetplayLog.h"
#include "logger/logger.h"

namespace GeraNESNetplay {

using namespace ConsoleNetplay;

GeraNESNetplayAppRuntime::GeraNESNetplayAppRuntime(IEmulationHost& emuHost)
    : m_emuHost(emuHost)
{
    setNetplayLogCallback([](const std::string& message, NetplayLogLevel level) {
        Logger::Type type = Logger::Type::INFO;
        switch(level) {
            case NetplayLogLevel::Debug: type = Logger::Type::DEBUG; break;
            case NetplayLogLevel::Warning: type = Logger::Type::WARNING; break;
            case NetplayLogLevel::Error: type = Logger::Type::ERROR; break;
            case NetplayLogLevel::User: type = Logger::Type::USER; break;
            case NetplayLogLevel::Info:
            default: type = Logger::Type::INFO; break;
        }
        Logger::instance().log(message, type);
    });
}

std::optional<GeraNESNetplayAppRuntime::RomSelection> GeraNESNetplayAppRuntime::captureCurrentRomSelection(GeraNESEmu& emu)
{
    return GeraNESNetplayConsole::captureRomSelection(emu);
}

void GeraNESNetplayAppRuntime::reanchorInputDriver(FrameNumber anchorFrame)
{
    m_inputDriver.reanchor(anchorFrame);
    m_lastRecoveryReanchorFrame = anchorFrame;
}

std::vector<uint8_t> GeraNESNetplayAppRuntime::buildAuthoritativeStatePayload(FrameNumber authoritativeFrame,
                                                                        bool preferConfirmedSnapshot) const
{
    NetplayStateBridgeAdapter<IEmulationHost> stateBridge(m_emuHost);
    NetplayStateHostBridgeAdapter<IEmulationHost> hostBridge(m_emuHost);
    return runtimeBuildAuthoritativeStatePayload(
        stateBridge,
        hostBridge,
        authoritativeFrame,
        preferConfirmedSnapshot
    );
}

bool GeraNESNetplayAppRuntime::beginAuthoritativeResync(FrameNumber authoritativeFrame,
                                                 const std::vector<uint8_t>& statePayload,
                                                 bool preferConfirmedSnapshot,
                                                 ResyncReason reason,
                                                 ParticipantId targetParticipantId)
{
    NetplayStateBridgeAdapter<IEmulationHost> stateBridge(m_emuHost);
    NetplayStateHostBridgeAdapter<IEmulationHost> hostBridge(m_emuHost);
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
    if(result.loadedAuthoritativeFrame != 0) {
        m_lastLoadedAuthoritativeFrame = result.loadedAuthoritativeFrame;
    }
    if(result.reanchorFrame != 0) {
        m_lastRecoveryReanchorFrame = result.reanchorFrame;
    }
    return result.started;
}

bool GeraNESNetplayAppRuntime::beginAuthoritativeResyncWithoutLocalReload(FrameNumber authoritativeFrame,
                                                                   const std::vector<uint8_t>& statePayload,
                                                                   bool preferConfirmedSnapshot,
                                                                   ResyncReason reason)
{
    NetplayStateBridgeAdapter<IEmulationHost> stateBridge(m_emuHost);
    NetplayStateHostBridgeAdapter<IEmulationHost> hostBridge(m_emuHost);
    const RuntimeAuthoritativeStateResult result = runtimeBeginAuthoritativeResyncWithoutLocalReload(
        m_coordinator,
        m_inputDriver,
        stateBridge,
        hostBridge,
        authoritativeFrame,
        statePayload,
        preferConfirmedSnapshot,
        reason
    );
    if(result.reanchorFrame != 0) {
        m_lastRecoveryReanchorFrame = result.reanchorFrame;
    }
    return result.started;
}

std::vector<PlayerSlot> GeraNESNetplayAppRuntime::localAssignedSlots() const
{
    if(!m_coordinator.isActive()) return {};
    return runtimeLocalAssignedSlots(m_coordinator);
}

std::string GeraNESNetplayAppRuntime::computeSessionBlockedReason(const std::optional<RomSelection>& localRom) const
{
    return runtimeSessionBlockedReason(
        m_coordinator.isActive(),
        m_coordinator.session().roomState(),
        localRom
    );
}

void GeraNESNetplayAppRuntime::drainPendingCommands(GeraNESEmu& emu)
{
    std::deque<WorkerCommand> commands;
    {
        std::scoped_lock stateLock(m_stateMutex);
        commands.swap(m_pendingCommands);
    }

    for(auto& command : commands) {
        command(*this, emu);
    }
}

void GeraNESNetplayAppRuntime::syncRomValidation(const std::optional<RomSelection>& localRom)
{
    m_romValidationState.stickyStatusMessage = m_stickyStatusMessage;
    const RuntimeRomValidationResult result =
        runtimeSyncRomValidation(m_coordinator, m_romValidationState, localRom);
    m_stickyStatusMessage = result.stickyStatusMessage;

    if(!m_coordinator.isActive() && !result.disconnectedForMismatch) {
        m_lastSessionState.reset();
        return;
    }

    if(result.disconnectedForMismatch) {
        m_inputDriver.reset();
        m_runtimeLastTickTime = {};
        m_lastLocalAssignedSlots.clear();
        m_lastAssignmentLayoutKey.clear();
    }
}

void GeraNESNetplayAppRuntime::syncInputDelayFromSettings()
{
    auto& cfg = AppSettings::instance().data.netplay;
    cfg.gameplayReceiveDelayMs = std::max(0, cfg.gameplayReceiveDelayMs);
#ifdef NDEBUG
    cfg.gameplayReceiveDelayMs = 0;
#endif

    RuntimeInputDelaySettings settings;
    settings.debugMode = cfg.showNetplayDebugLog;
    settings.gameplayReceiveDelayMs = static_cast<uint32_t>(cfg.gameplayReceiveDelayMs);
    settings.autoGameplayTuning = cfg.autoGameplayTuning;
    settings.manualInputDelayFrames = static_cast<uint32_t>(std::max(0, cfg.inputDelayFrames));
    settings.manualPredictFrames = static_cast<uint32_t>(std::max(0, cfg.predictFrames));
    settings.regionFps = m_emuHost.getRegionFPS();

    const RuntimeInputDelayResult result =
        runtimeSyncInputDelaySettings(m_coordinator, m_inputDriver, m_autoSettings, settings);
    m_emuHost.configureInputBufferCapacity(result.inputBufferCapacity);

    cfg.inputDelayFrames = static_cast<int>(result.inputDelayFrames);
    cfg.predictFrames = static_cast<int>(result.predictFrames);
}

void GeraNESNetplayAppRuntime::processAutoResumeIfNeeded(const std::optional<RomSelection>& localRom)
{
    (void)runtimeProcessAutoResumeIfNeeded(
        m_coordinator,
        m_webVisibilityManagedPause,
        m_webPageVisible,
        localRom
    );
}

void GeraNESNetplayAppRuntime::processHostManualStateChangeResyncIfNeeded()
{
    const std::vector<IEmulationHost::ManualStateChangeRecord> events =
        m_emuHost.consumeManualStateChanges();
    if(events.empty()) return;

    for(const auto& event : events) {
        if(!m_coordinator.isHosting()) continue;

        const RoomState& room = m_coordinator.session().roomState();
        const SessionState state = room.state;
        if(state != SessionState::Running &&
           state != SessionState::Paused &&
           state != SessionState::Resyncing) {
            continue;
        }
        if(!m_emuHost.valid()) continue;

        const ResyncReason reason =
            event.kind == IEmulationHost::ManualStateChangeKind::Reset
                ? ResyncReason::HostReset
                : ResyncReason::HostLoadedState;
        {
            std::ostringstream oss;
            oss << "Owner manual state change detected"
                << " reason " << NetplayCoordinator::resyncReasonToast(reason)
                << " eventFrame " << event.frame
                << " emuFrame " << m_emuHost.frameCount()
                << " roomEpoch " << room.timelineEpoch;
            m_coordinator.appendNetplayLog(oss.str());
        }
        const std::string toast = NetplayCoordinator::resyncReasonToast(reason);
        if(!toast.empty()) {
            m_coordinator.appendNetplayLog(toast);
        }

        const FrameNumber eventFrame = std::min<FrameNumber>(event.frame, m_emuHost.frameCount());
        const bool resyncBusy =
            state == SessionState::Resyncing ||
            room.activeResyncId != 0 ||
            room.pendingResyncAckCount != 0;
        m_emuHost.discardQueuedInputFramesAfter(eventFrame);
        syncEmuInputTimelineEpoch();
        reanchorInputDriver(eventFrame);

        const bool hasRemotePeers = std::any_of(
            room.participants.begin(),
            room.participants.end(),
            [this](const ParticipantInfo& participant) {
                return participant.id != m_coordinator.localParticipantId();
            }
        );
        if(!hasRemotePeers) {
            continue;
        }

        if(resyncBusy) {
            m_coordinator.appendNetplayLog(
                "Deferring manual host recovery until the active resync/bootstrap finishes"
            );
            m_pendingManualStateResyncs.clear();
            m_pendingManualStateResyncs.push_back(PendingManualStateResync{
                reason,
                eventFrame,
                event.kind == IEmulationHost::ManualStateChangeKind::Reset
            });
            continue;
        }

        m_coordinator.discardTimelineAfter(eventFrame);
        m_coordinator.invalidateLocalCrcHistoryAfter(eventFrame);
        m_coordinator.setLocalSimulationFrame(eventFrame);

        if(event.kind == IEmulationHost::ManualStateChangeKind::LoadState) {
            const std::vector<uint8_t> statePayload =
                buildAuthoritativeStatePayload(eventFrame, false);
            if(statePayload.empty()) {
                continue;
            }

            m_pendingManualStateResyncs.clear();
            beginAuthoritativeResync(eventFrame, statePayload, false, reason);
            continue;
        }

        m_pendingManualStateResyncs.clear();
        m_pendingManualStateResyncs.push_back(PendingManualStateResync{reason, eventFrame, true});
    }
}

void GeraNESNetplayAppRuntime::processPendingManualStateResyncIfNeeded()
{
    if(m_pendingManualStateResyncs.empty()) return;
    if(!m_coordinator.isHosting()) {
        m_pendingManualStateResyncs.clear();
        return;
    }

    const SessionState state = m_coordinator.session().roomState().state;
    if(state != SessionState::Running && state != SessionState::Paused) {
        return;
    }

    while(!m_pendingManualStateResyncs.empty()) {
        const PendingManualStateResync pending = m_pendingManualStateResyncs.front();
        if(pending.waitForAdvance && m_emuHost.frameCount() <= pending.eventFrame) {
            return;
        }

        const FrameNumber authoritativeFrame = m_emuHost.frameCount();
        const std::vector<uint8_t> statePayload =
            buildAuthoritativeStatePayload(authoritativeFrame, false);
        if(statePayload.empty()) {
            return;
        }

        if(beginAuthoritativeResync(authoritativeFrame, statePayload, false, pending.reason)) {
            std::ostringstream oss;
            oss << "Applying deferred manual host recovery"
                << " reason " << NetplayCoordinator::resyncReasonToast(pending.reason)
                << " authoritativeFrame " << authoritativeFrame;
            m_coordinator.appendNetplayLog(oss.str());
            m_pendingManualStateResyncs.pop_front();
        } else {
            return;
        }
    }
}

void GeraNESNetplayAppRuntime::processAutoStartIfNeeded(const std::optional<RomSelection>& localRom)
{
    NetplayStateBridgeAdapter<IEmulationHost> stateBridge(m_emuHost);
    const RuntimeAutoStartResult result =
        runtimeProcessAutoStartIfNeeded(m_coordinator, stateBridge, localRom);
    if(result.initialSyncNeeded) {
        beginInitialSessionSyncOnWorker();
    }
}

uint32_t GeraNESNetplayAppRuntime::consumeWorkerDtMs()
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

void GeraNESNetplayAppRuntime::handleSessionStateTransitionsOnWorker()
{
    if(!m_coordinator.isActive()) {
        m_lastSessionState.reset();
        m_inputDriver.reset();
        m_runtimeLastTickTime = {};
        m_periodicCrcState = RuntimePeriodicCrcState{};
        m_lastRollbackTargetFrame = 0;
        m_lastMissingRollbackSnapshotFrame = 0;
        m_lastMissingRollbackSnapshotLocalFrame = 0;
        m_lastLoadedAuthoritativeFrame = 0;
        m_lastRecoveryReanchorFrame = 0;
        return;
    }

    const SessionState currentState = m_coordinator.session().roomState().state;
    const std::optional<SessionState> previousState = m_lastSessionState;
    if(previousState.has_value() && *previousState == currentState) {
        return;
    }

    const bool enteringResync = currentState == SessionState::Resyncing;
    const bool leavingResync =
        previousState.has_value() &&
        *previousState == SessionState::Resyncing &&
        currentState != SessionState::Resyncing;
    if(enteringResync || leavingResync) {
        m_emuHost.restartAudio();
    }
    if(enteringResync) {
        const uint32_t discardAfterFrame =
            m_coordinator.session().roomState().resyncTargetFrame != 0u
                ? m_coordinator.session().roomState().resyncTargetFrame
                : m_emuHost.frameCount();
        m_emuHost.discardQueuedNetplayInputsAfter(discardAfterFrame);
    }

    if(currentState != SessionState::Running) {
        m_inputDriver.reset();
        m_runtimeLastTickTime = {};
    }

    if(currentState == SessionState::Paused) {
        m_emuHost.setSimulationSuspended(true);
        if(!previousState.has_value() || *previousState != SessionState::Paused) {
            m_emuHost.discardQueuedAudio();
        }
    }

    if(previousState.has_value() &&
       *previousState == SessionState::Paused &&
       currentState != SessionState::Paused &&
       !m_observerVisibilityResyncPending) {
        m_emuHost.setSimulationSuspended(false);
    }

    if(currentState == SessionState::Running &&
       previousState.has_value() &&
       (*previousState == SessionState::Starting || *previousState == SessionState::Resyncing)) {
        const uint32_t anchorFrame = m_coordinator.session().roomState().lastConfirmedFrame;
        reanchorInputDriver(anchorFrame);
        m_periodicCrcState.postRecoveryRapidCrcThroughFrame = anchorFrame + 3u;
        if(m_observerVisibilityResyncPending) {
            m_observerVisibilityResyncPending = false;
            m_emuHost.setSimulationSuspended(false);
        }
    }

    if(currentState == SessionState::Running &&
       (!previousState.has_value() || *previousState != SessionState::Running)) {
        m_periodicCrcState.forceNextConfirmedCrcSubmission = true;
    }

    m_lastSessionState = currentState;
}

void GeraNESNetplayAppRuntime::processPeriodicLocalCrcIfNeeded()
{
    m_periodicCrcState.lastLoadedAuthoritativeFrame = m_lastLoadedAuthoritativeFrame;
    NetplayStateBridgeAdapter<IEmulationHost> stateBridge(m_emuHost);
    NetplayStateHostBridgeAdapter<IEmulationHost> hostBridge(m_emuHost);
    (void)runtimeSubmitPeriodicLocalCrcIfNeeded(
        m_coordinator,
        stateBridge,
        hostBridge,
        m_periodicCrcState
    );
}

bool GeraNESNetplayAppRuntime::beginInitialSessionSyncOnWorker()
{
    if(!m_coordinator.isHosting()) return false;
    if(!m_emuHost.valid()) return false;
    if(m_coordinator.session().roomState().state != SessionState::Starting) return false;

    const FrameNumber authoritativeFrame = m_emuHost.frameCount();
    const std::vector<uint8_t> statePayload =
        buildAuthoritativeStatePayload(authoritativeFrame, false);
    if(!beginAuthoritativeResync(authoritativeFrame, statePayload, false)) {
        return false;
    }

    m_coordinator.appendNetplayLog("Netplay initial session sync started");
    return true;
}

void GeraNESNetplayAppRuntime::processHostResyncIfNeededOnWorker()
{
    NetplayStateBridgeAdapter<IEmulationHost> stateBridge(m_emuHost);
    NetplayStateHostBridgeAdapter<IEmulationHost> hostBridge(m_emuHost);
    const RuntimeHostResyncProcessResult result =
        runtimeProcessHostResyncIfNeeded(
            m_coordinator,
            m_inputDriver,
            m_autoSettings,
            stateBridge,
            hostBridge,
            AppSettings::instance().data.netplay.autoGameplayTuning
        );
    if(result.loadedAuthoritativeFrame != 0) {
        m_lastLoadedAuthoritativeFrame = result.loadedAuthoritativeFrame;
    }
    if(result.reanchorFrame != 0) {
        m_lastRecoveryReanchorFrame = result.reanchorFrame;
    }
}

void GeraNESNetplayAppRuntime::processHostLateJoinResyncIfNeededOnWorker()
{
    NetplayStateBridgeAdapter<IEmulationHost> stateBridge(m_emuHost);
    NetplayStateHostBridgeAdapter<IEmulationHost> hostBridge(m_emuHost);
    const RuntimeHostResyncProcessResult result =
        runtimeProcessHostLateJoinResyncIfNeeded(
            m_coordinator,
            m_inputDriver,
            stateBridge,
            hostBridge
        );
    if(result.loadedAuthoritativeFrame != 0) {
        m_lastLoadedAuthoritativeFrame = result.loadedAuthoritativeFrame;
    }
    if(result.reanchorFrame != 0) {
        m_lastRecoveryReanchorFrame = result.reanchorFrame;
    }
}

void GeraNESNetplayAppRuntime::processHostStallIfNeededOnWorker()
{
    NetplayStateBridgeAdapter<IEmulationHost> stateBridge(m_emuHost);
    NetplayStateHostBridgeAdapter<IEmulationHost> hostBridge(m_emuHost);
    const RuntimeHostResyncProcessResult result =
        runtimeProcessSelfStallRecoveryIfNeeded(
            m_coordinator,
            m_inputDriver,
            m_selfStallDetector,
            stateBridge,
            hostBridge,
            std::chrono::steady_clock::now()
        );
    if(result.loadedAuthoritativeFrame != 0) {
        m_lastLoadedAuthoritativeFrame = result.loadedAuthoritativeFrame;
    }
    if(result.reanchorFrame != 0) {
        m_lastRecoveryReanchorFrame = result.reanchorFrame;
    }
}

void GeraNESNetplayAppRuntime::processResyncIfNeededOnWorker(GeraNESEmu& emu)
{
    std::optional<NetplayCoordinator::PendingResyncApply> pending = m_coordinator.consumePendingResyncApply();
    if(!pending.has_value()) return;

    m_emuHost.beginPresentationHoldUntilNextFrameReady();
    const bool loaded = m_emuHost.loadStateFromMemoryOnCleanBoot(pending->payload);
    const uint32_t loadedFrame = m_emuHost.frameCount();
    const bool loadedExpectedFrame =
        loaded &&
        (pending->targetFrame == 0u || loadedFrame == pending->targetFrame);
    const uint32_t loadedCrc32 = loadedExpectedFrame ? m_emuHost.canonicalNetplayStateCrc32() : 0;
    if(loadedExpectedFrame) {
        m_emuHost.discardQueuedInputFramesAfter(pending->targetFrame);
        syncEmuInputTimelineEpoch();
        m_coordinator.setLocalSimulationFrame(pending->targetFrame);
        m_emuHost.discardQueuedNetplayInputsAfter(pending->targetFrame);
        m_emuHost.seedNetplaySnapshot(pending->targetFrame, pending->payload, loadedCrc32);
        m_lastLoadedAuthoritativeFrame = pending->targetFrame;
        m_emuHost.setAuthoritativeFrameReadyState(
            pending->frameReadyFrame != 0u ? pending->frameReadyFrame : pending->targetFrame,
            pending->frameReadyCrc32 != 0u ? pending->frameReadyCrc32 : loadedCrc32
        );
        reanchorInputDriver(pending->targetFrame);
        alignResyncPlaybackToSharedClockOnWorker(emu, pending->targetFrame);
        std::ostringstream oss;
        oss << "Netplay resync post-load validation accepted"
            << " targetFrame " << pending->targetFrame
            << " loadedCrc32 " << loadedCrc32
            << " frameReadyFrame "
            << (pending->frameReadyFrame != 0u ? pending->frameReadyFrame : pending->targetFrame);
        m_coordinator.appendNetplayLog(oss.str());
    }
    m_coordinator.acknowledgeResync(pending->resyncId, pending->targetFrame, loadedCrc32, loadedExpectedFrame);

    if(!loadedExpectedFrame) {
        if(loaded && loadedFrame != pending->targetFrame) {
            std::ostringstream oss;
            oss << "Netplay resync load frame mismatch: expected "
                << pending->targetFrame
                << ", got "
                << loadedFrame
                << " after clean-boot state load";
            m_coordinator.appendNetplayLog(oss.str());
        }
        m_coordinator.appendNetplayLog("Netplay resync post-load validation rejected");
        m_coordinator.appendNetplayLog("Netplay resync failed");
    }
}

void GeraNESNetplayAppRuntime::alignResyncPlaybackToSharedClockOnWorker(GeraNESEmu& emu, FrameNumber loadedFrame)
{
    if(!m_coordinator.isActive() || m_coordinator.session().roomState().state != SessionState::Running) {
        return;
    }

    if(!emu.valid()) {
        return;
    }

    constexpr uint32_t kMaxSilentCatchupFrames = 120u;
    const uint32_t advancedFrames = advanceToSharedClockIfNeededOnWorker(
        emu,
        kMaxSilentCatchupFrames,
        false
    );

    if(advancedFrames > 0u) {
        std::ostringstream oss;
        oss << "Netplay resync shared-clock alignment advanced "
            << advancedFrames
            << " frame(s) from "
            << loadedFrame
            << " to "
            << emu.frameCount()
            << " (audio muted)";
        m_coordinator.appendNetplayLog(oss.str());
    }
}

uint32_t GeraNESNetplayAppRuntime::advanceToSharedClockIfNeededOnWorker(GeraNESEmu& emu,
                                                                 uint32_t maxFrames,
                                                                 bool requireLagTrigger)
{
    if(maxFrames == 0u) {
        return 0u;
    }
    if(m_coordinator.isHosting()) {
        return 0u;
    }
    if(!m_coordinator.isActive() || m_coordinator.session().roomState().state != SessionState::Running) {
        return 0u;
    }
    if(!emu.valid()) {
        return 0u;
    }

    const uint32_t frameDt = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));
    const uint64_t frameDtMicros =
        std::max<uint64_t>(1u, 1000000ull / std::max<uint64_t>(1u, static_cast<uint64_t>(emu.getRegionFPS())));
    const FrameNumber confirmedThroughFrame = m_inputDriver.confirmedThroughFrame(m_coordinator);
    bool lagTriggerActivated = false;
    FrameNumber estimatedHostFrameFromClock = 0u;
    FrameNumber estimatedLagFrames = 0u;

    constexpr FrameNumber kContinuousCatchupLagTriggerFrames = 2u;
    const RoomState& room = m_coordinator.session().roomState();
    if(m_sharedClockResyncRequestPending &&
       room.timelineEpoch != m_sharedClockResyncRequestEpoch) {
        m_sharedClockResyncRequestPending = false;
        m_sharedClockLagOverBudgetSince = {};
        m_sharedClockLagOverBudgetSinceFrame = 0;
    }
    if(room.lastAuthoritativeClockFrame != 0u &&
       room.lastAuthoritativeClockMicros != 0u &&
       room.sharedClockSynchronized) {
        const uint64_t nowSharedClockMicros = m_coordinator.sharedClockNowMicros();
        if(nowSharedClockMicros != 0u && nowSharedClockMicros > room.lastAuthoritativeClockMicros) {
            const uint64_t elapsedSinceAuthoritativeMicros =
                nowSharedClockMicros - room.lastAuthoritativeClockMicros;
            estimatedHostFrameFromClock =
                room.lastAuthoritativeClockFrame +
                static_cast<FrameNumber>(elapsedSinceAuthoritativeMicros / frameDtMicros);
            const FrameNumber localFrame = emu.frameCount();
            estimatedLagFrames =
                estimatedHostFrameFromClock > localFrame ? (estimatedHostFrameFromClock - localFrame) : 0u;
        }
    }

    if(requireLagTrigger) {
        if(estimatedLagFrames < kContinuousCatchupLagTriggerFrames) {
            return 0u;
        }
        lagTriggerActivated = true;
        if(emu.frameCount() >= confirmedThroughFrame) {
            return 0u;
        }
    }

    if(estimatedLagFrames > maxFrames) {
        constexpr auto kLargeLagPersistence = std::chrono::milliseconds(250);
        constexpr auto kSharedClockResyncRequestCooldown = std::chrono::milliseconds(1500);
        const auto now = std::chrono::steady_clock::now();
        const FrameNumber localFrame = emu.frameCount();
        const FrameNumber confirmedLagFrames =
            confirmedThroughFrame > localFrame ? (confirmedThroughFrame - localFrame) : 0u;

        if(m_sharedClockLagOverBudgetSince.time_since_epoch().count() == 0 ||
           estimatedLagFrames <= maxFrames) {
            m_sharedClockLagOverBudgetSince = now;
            m_sharedClockLagOverBudgetSinceFrame = localFrame;
        }

        if(confirmedLagFrames <= maxFrames) {
            m_sharedClockLagOverBudgetSince = {};
            m_sharedClockLagOverBudgetSinceFrame = 0;
            if(localFrame >= m_lastSharedClockConfirmedLagWaitLogFrame + 300u) {
                m_lastSharedClockConfirmedLagWaitLogFrame = localFrame;
                std::ostringstream oss;
                oss << "Netplay shared-clock catchup over budget but confirmed input is not far enough ahead"
                    << " lag " << estimatedLagFrames
                    << " confirmedLag " << confirmedLagFrames
                    << " budget " << maxFrames
                    << "; waiting instead of requesting resync";
                m_coordinator.appendNetplayLog(oss.str());
            }
            return 0u;
        }

        const bool lagPersisted =
            now - m_sharedClockLagOverBudgetSince >= kLargeLagPersistence;
        if(!lagPersisted) {
            return 0u;
        }

        if(m_lastSharedClockResyncRequestAt.time_since_epoch().count() != 0 &&
           now - m_lastSharedClockResyncRequestAt < kSharedClockResyncRequestCooldown) {
            return 0u;
        }
        m_sharedClockResyncRequestPending = false;

        ResyncRequestData request;
        request.reason = ResyncReason::ConfirmedDesync;
        request.localFrame = localFrame;
        request.estimatedHostFrame = estimatedHostFrameFromClock;
        request.confirmedThroughFrame = confirmedThroughFrame;
        request.lagFrames =
            static_cast<uint16_t>(std::min<FrameNumber>(estimatedLagFrames, 0xffffu));
        request.catchupBudgetFrames =
            static_cast<uint16_t>(std::min<uint32_t>(maxFrames, 0xffffu));
        request.source = 1u; // shared-clock catchup over budget

        if(m_coordinator.requestHostResync(request)) {
            m_sharedClockResyncRequestPending = true;
            m_sharedClockResyncRequestEpoch = room.timelineEpoch;
            m_lastSharedClockResyncRequestAt = now;
            m_lastSharedClockResyncRequestFrame = localFrame;
            std::ostringstream oss;
            oss << "Netplay shared-clock catchup skipped; lag "
                << estimatedLagFrames
                << " frame(s) exceeds catchup budget "
                << maxFrames
                << " since local frame "
                << m_sharedClockLagOverBudgetSinceFrame
                << ", requested authoritative resync";
            m_coordinator.appendNetplayLog(oss.str());
        }
        return 0u;
    }

    m_sharedClockLagOverBudgetSince = {};
    m_sharedClockLagOverBudgetSinceFrame = 0;

    uint32_t advancedFrames = 0u;

    while(advancedFrames < maxFrames) {
        const FrameNumber nextFrame = emu.frameCount() + 1u;
        if(nextFrame > confirmedThroughFrame) {
            break;
        }
        const uint64_t nextFrameClockMicros = m_coordinator.authoritativeFrameStartClockMicros(nextFrame);
        if(nextFrameClockMicros == 0u) {
            break;
        }

        const uint64_t nowSharedClockMicros = m_coordinator.sharedClockNowMicros();
        if(nowSharedClockMicros == 0u || nowSharedClockMicros < nextFrameClockMicros) {
            break;
        }

        NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
        if(!m_coordinator.tryBuildPlaybackFrame(nextFrame, false, playbackFrame, false)) {
            break;
        }
        if(playbackFrame.predicted) {
            break;
        }

        GeraNESNetplayConsole console(m_emuHost, emu, m_latestInputState);
        if(!console.queuePlaybackInputFrame(playbackFrame)) {
            break;
        }

        if(!emu.updateUntilFrame(frameDt, false)) {
            break;
        }

        ++advancedFrames;
        m_coordinator.setLocalSimulationFrame(emu.frameCount());
    }

    if(advancedFrames > 0u) {
        std::ostringstream oss;
        oss << "Netplay continuous shared-clock catchup advanced "
            << advancedFrames
            << " frame(s)"
            << " localFrame="
            << emu.frameCount()
            << " confirmedThrough="
            << confirmedThroughFrame;
        if(lagTriggerActivated) {
            oss << " triggerLagFrames="
                << estimatedLagFrames
                << " estimatedHostFrame="
                << estimatedHostFrameFromClock;
        }
        oss << " (audio muted)";
        m_coordinator.appendNetplayLog(oss.str());
    }

    return advancedFrames;
}

void GeraNESNetplayAppRuntime::processRollbackIfNeededOnWorker(GeraNESEmu& emu)
{
    std::optional<FrameNumber> rollbackFrame = m_coordinator.consumePendingRollbackFrame();
    if(!rollbackFrame.has_value()) return;
    m_lastRollbackTargetFrame = *rollbackFrame;

    const uint32_t currentFrame = emu.frameCount();
    if(currentFrame == 0) return;

    const FrameNumber confirmedFrame = m_coordinator.session().roomState().lastConfirmedFrame;
    const FrameNumber latestSafeRollbackFrame = currentFrame - 1u;
    const FrameNumber earliestConfirmedReplayFrame =
        confirmedFrame > 0 ? (confirmedFrame - 1u) : 0u;
    const FrameNumber rollbackFloor = std::min(earliestConfirmedReplayFrame, latestSafeRollbackFrame);
    if(*rollbackFrame < rollbackFloor) {
        rollbackFrame = rollbackFloor;
    }

    if(*rollbackFrame >= currentFrame) {
        m_coordinator.rescheduleRollbackFrame(*rollbackFrame);
        return;
    }

    const auto requestRollbackRecoveryResync = [&](const std::string& message, uint16_t requestFlags = 0u) {
        m_coordinator.appendNetplayLog(message);
        if(m_coordinator.isHosting()) {
            return;
        }

        ResyncRequestData request;
        request.reason = ResyncReason::ConfirmedDesync;
        request.localFrame = emu.frameCount();
        request.estimatedHostFrame = 0;
        request.confirmedThroughFrame = m_inputDriver.confirmedThroughFrame(m_coordinator);
        request.lagFrames = 0;
        request.catchupBudgetFrames = 0;
        request.source = 2u; // rollback resimulation failed
        request.flags = requestFlags;
        (void)m_coordinator.requestHostResync(request);
    };

    const std::optional<std::shared_ptr<const std::vector<uint8_t>>> snapshotData =
        m_emuHost.netplaySnapshotForFrame(*rollbackFrame);
    if(!snapshotData.has_value()) {
        const bool shouldLog =
            m_lastMissingRollbackSnapshotFrame != *rollbackFrame ||
            m_lastMissingRollbackSnapshotLocalFrame != currentFrame;
        m_lastMissingRollbackSnapshotFrame = *rollbackFrame;
        m_lastMissingRollbackSnapshotLocalFrame = currentFrame;

        if(m_coordinator.isHosting()) {
            const std::vector<uint8_t> statePayload =
                buildAuthoritativeStatePayload(currentFrame, false);
            if(beginAuthoritativeResyncWithoutLocalReload(
                   currentFrame,
                   statePayload,
                   false,
                   ResyncReason::ConfirmedDesync
               )) {
                if(shouldLog) {
                    m_coordinator.appendNetplayLog(
                        "Netplay rollback snapshot unavailable; started authoritative resync at frame " +
                        std::to_string(currentFrame) +
                        " instead of rollback to frame " +
                        std::to_string(*rollbackFrame)
                    );
                }
            } else if(shouldLog) {
                m_coordinator.appendNetplayLog(
                    "Netplay rollback failed: snapshot unavailable for frame " +
                    std::to_string(*rollbackFrame)
                );
            }
            return;
        }

        if(shouldLog) {
            requestRollbackRecoveryResync(
                "Netplay rollback failed: snapshot unavailable for frame " +
                std::to_string(*rollbackFrame),
                kResyncRequestFlagRollbackReplayBuildFailure
            );
        } else {
            ResyncRequestData request;
            request.reason = ResyncReason::ConfirmedDesync;
            request.localFrame = emu.frameCount();
            request.estimatedHostFrame = 0;
            request.confirmedThroughFrame = m_inputDriver.confirmedThroughFrame(m_coordinator);
            request.lagFrames = 0;
            request.catchupBudgetFrames = 0;
            request.source = 2u;
            request.flags = kResyncRequestFlagRollbackReplayBuildFailure;
            (void)m_coordinator.requestHostResync(request);
        }
        return;
    }

    const uint32_t rollbackFromFrame = currentFrame;
    emu.loadStateFromMemoryWithAudioPolicy(
        **snapshotData,
        GeraNESEmu::StateLoadAudioPolicy::PreserveContinuousOutput);
    if(!emu.valid()) {
        m_coordinator.appendNetplayLog("Netplay rollback failed: snapshot load failed");
        return;
    }

    const uint32_t rollbackCanonicalCrc32 = emu.canonicalNetplayStateCrc32();
    (void)m_emuHost.updateNetplaySnapshotCrc32ForFrame(*rollbackFrame, rollbackCanonicalCrc32);
    m_coordinator.setLocalSimulationFrame(*rollbackFrame);
    m_coordinator.discardTimelineAfter(*rollbackFrame, true);
    m_coordinator.invalidateLocalCrcHistoryAfter(*rollbackFrame);

    FrameNumber inputDriverAnchorFrame = *rollbackFrame;
    const ParticipantId localParticipantId = m_coordinator.localParticipantId();
    for(auto it = m_coordinator.localInputs().entries().rbegin();
        it != m_coordinator.localInputs().entries().rend();
        ++it) {
        if(it->participantId != localParticipantId) continue;
        inputDriverAnchorFrame = std::max(inputDriverAnchorFrame, it->frame);
        break;
    }
    reanchorInputDriver(inputDriverAnchorFrame);

    const uint32_t frameDt =
        std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));
    while(emu.frameCount() < currentFrame) {
        const uint32_t nextFrame = emu.frameCount() + 1u;
        NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
        const FrameNumber confirmedThroughFrame = m_inputDriver.confirmedThroughFrame(m_coordinator);
        // Rollback replay must be able to rebuild the speculative tail that was
        // already simulated beyond the confirmed frontier. The normal forward
        // playback gate is stricter because it protects the live runtime from
        // predicting too early inside the delay window, but replaying from a
        // confirmed checkpoint back up to the current frame needs those
        // predicted frames to exist again or playback-frame construction fails
        // immediately after a resync/rollback.
        const bool allowPrediction =
            nextFrame > confirmedThroughFrame &&
            nextFrame <= currentFrame &&
            shouldAllowPredictionForFrame(
                confirmedThroughFrame + static_cast<FrameNumber>(m_inputDriver.prebufferFrames()) + 1u
            );
        const FrameNumber predictionCapFrame =
            confirmedThroughFrame +
            static_cast<FrameNumber>(m_inputDriver.prebufferFrames()) +
            static_cast<FrameNumber>(m_inputDriver.predictFrames());
        const bool allowHostFallback = nextFrame > predictionCapFrame;
        if(!m_coordinator.tryBuildPlaybackFrame(
               nextFrame,
               allowPrediction,
               playbackFrame,
               allowHostFallback
           )) {
            requestRollbackRecoveryResync(
                "Netplay resimulation failed: could not build playback frame " +
                std::to_string(nextFrame),
                kResyncRequestFlagRollbackReplayBuildFailure
            );
            return;
        }

        GeraNESNetplayConsole console(m_emuHost, emu, m_latestInputState);
        if(!console.queuePlaybackInputFrame(playbackFrame)) {
            requestRollbackRecoveryResync(
                "Netplay resimulation failed: rejected playback enqueue at frame " +
                std::to_string(nextFrame),
                kResyncRequestFlagRollbackReplayEnqueueFailure
            );
            return;
        }
        if(!emu.updateUntilFrame(frameDt, true)) {
            requestRollbackRecoveryResync(
                "Netplay resimulation failed: emulator did not advance at frame " +
                std::to_string(nextFrame),
                kResyncRequestFlagRollbackReplayAdvanceFailure
            );
            return;
        }
    }
    m_coordinator.setLocalSimulationFrame(emu.frameCount());
    m_emuHost.discardQueuedNetplayInputsAfter(*rollbackFrame);

    const uint32_t recoveredConfirmedFrame = m_coordinator.session().roomState().lastConfirmedFrame;
    const bool showNetplayDebugLog = AppSettings::instance().data.netplay.showNetplayDebugLog;
    if(showNetplayDebugLog && recoveredConfirmedFrame != 0u && recoveredConfirmedFrame <= emu.frameCount()) {
        const uint32_t recoveredConfirmedCrc32 = emu.canonicalNetplayStateCrc32();
        std::ostringstream validate;
        validate << "Rollback recovery reanchored"
                 << " targetFrame " << *rollbackFrame
                 << " confirmedFrame " << recoveredConfirmedFrame
                 << " localSimulationFrame " << emu.frameCount()
                 << " canonicalCrc32 " << recoveredConfirmedCrc32;
        m_coordinator.appendNetplayLog(validate.str());
    }

    if(showNetplayDebugLog) {
        m_coordinator.appendNetplayLog(
            "Netplay rollback applied (" + std::to_string(rollbackFromFrame) +
            " -> " + std::to_string(*rollbackFrame) + ")"
        );
    }
}

void GeraNESNetplayAppRuntime::updateUiSnapshot(const std::optional<RomSelection>& localRom)
{
    UiSnapshot snapshot;
    snapshot.valid = true;
    snapshot.active = m_coordinator.isActive();
    snapshot.hosting = m_coordinator.isHosting();
    snapshot.connected = m_coordinator.isConnected();
    snapshot.reconnecting = m_coordinator.reconnectPending();
    snapshot.reconnectSecondsRemaining = m_coordinator.reconnectSecondsRemaining();
    snapshot.localRomLoaded = localRom.has_value() && localRom->loaded;
    snapshot.localRomGameName = localRom.has_value() ? localRom->gameName : "";
    snapshot.localRomCrc32 = localRom.has_value() ? localRom->validation.romCrc32 : 0;
    snapshot.transportBackend = m_coordinator.transportBackend();
    snapshot.localParticipantId = m_coordinator.localParticipantId();
    snapshot.lastError = m_coordinator.lastError().empty() ? m_stickyStatusMessage : m_coordinator.lastError();
    snapshot.room = m_coordinator.session().roomState();
    snapshot.localInputCount = m_coordinator.localInputs().size();
    snapshot.remoteInputCount = m_coordinator.remoteInputs().size();
    snapshot.localInputLookupStats = m_coordinator.localInputs().lookupStats();
    snapshot.remoteInputLookupStats = m_coordinator.remoteInputs().lookupStats();
    snapshot.coordinatorPerformanceDiagnostics = m_coordinator.performanceDiagnostics();
    snapshot.playbackQueueStats = m_inputDriver.playbackQueueStats();
    if(const auto* latestLocal = m_coordinator.localInputs().latest()) {
    snapshot.latestLocalInput = *latestLocal;
    }
    if(const auto* latestRemote = m_coordinator.remoteInputs().latest()) {
        snapshot.latestRemoteInput = *latestRemote;
    }
    snapshot.predictionStats = m_coordinator.predictionStats();
    snapshot.localSimulationFrame = m_coordinator.localSimulationFrame();
    snapshot.publishedConfirmedFrame = m_coordinator.latestPublishedConfirmedFrame();
    snapshot.lastSubmittedLocalCrcFrame = m_periodicCrcState.lastSubmittedLocalCrcFrame;
    snapshot.lastRollbackTargetFrame = m_lastRollbackTargetFrame;
    snapshot.lastLoadedAuthoritativeFrame = m_lastLoadedAuthoritativeFrame;
    snapshot.lastRecoveryReanchorFrame = m_lastRecoveryReanchorFrame;
    snapshot.autoSettings = m_autoSettings.snapshot();
    snapshot.framePacingDiagnostics = m_framePacingDiagnostics;
    snapshot.unresolvedPredictedRemoteFrameCount = m_coordinator.unresolvedPredictedRemoteFrameCount();
    snapshot.latestPredictedRemoteFrame = m_coordinator.latestPredictedRemoteFrame();
    snapshot.runtimeDiagnostics = m_emuHost.getNetplayDiagnostics();
    snapshot.sessionBlockedReason = computeSessionBlockedReason(localRom);
    snapshot.eventLog = m_coordinator.eventLog();

    std::scoped_lock stateLock(m_stateMutex);
    if(m_coordinator.localReconnectToken() != 0) {
        m_cachedReconnectToken = m_coordinator.localReconnectToken();
        m_hasCachedReconnectToken = true;
    }
    m_uiSnapshot = std::move(snapshot);
}

void GeraNESNetplayAppRuntime::syncEmuInputTimelineEpoch()
{
    NetplayStateBridgeAdapter<IEmulationHost> stateBridge(m_emuHost);
    runtimeSyncEmulatorInputTimelineEpoch(m_coordinator, stateBridge);
}

bool GeraNESNetplayAppRuntime::tryBuildPlaybackConfirmedFrame(uint32_t frame,
                                                              NetplayCoordinator::ConfirmedFrameInputs& outFrame)
{
    return runtimeTryBuildPlaybackConfirmedFrame(m_coordinator, m_inputDriver, frame, outFrame);
}

bool GeraNESNetplayAppRuntime::shouldAllowPredictionForFrame(FrameNumber frame) const
{
    return runtimeShouldAllowPredictionForFrame(m_coordinator, m_inputDriver, frame);
}

bool GeraNESNetplayAppRuntime::tryBuildPlaybackReplayFrame(uint32_t frame, IEmulationHost::ReplayFrameInput& outFrame)
{
    m_coordinator.recordLocalAuthoritativeFrameStart(frame);
    NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
    if(!tryBuildPlaybackConfirmedFrame(frame, playbackFrame)) {
        return false;
    }
    return GeraNESNetplayConsole::buildReplayFrameInput(playbackFrame, frame, outFrame);
}

void GeraNESNetplayAppRuntime::ensureStandaloneInputBootstrapFrame(GeraNESEmu& emu)
{
    syncEmuInputTimelineEpoch();
    GeraNESNetplayConsole console(m_emuHost, emu, m_latestInputState);
    console.queueStandaloneBootstrapInputFrame();
}

bool GeraNESNetplayAppRuntime::tryQueuePlaybackFrameToEmu(GeraNESEmu& emu, uint32_t frame)
{
    GeraNESNetplayConsole console(m_emuHost, emu, m_latestInputState);
    if(console.hasStableQueuedInputFrame(frame)) {
        return true;
    }

    NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
    if(!tryBuildPlaybackConfirmedFrame(frame, playbackFrame)) {
        return false;
    }

    return console.queuePlaybackInputFrame(playbackFrame);
}

void GeraNESNetplayAppRuntime::recordPlaybackStop(FrameNumber frame)
{
    runtimeRecordPlaybackStop(m_coordinator, m_inputDriver, frame);
}

void GeraNESNetplayAppRuntime::setLocalReconnectToken(uint64_t token)
{
    {
        std::scoped_lock stateLock(m_stateMutex);
        m_cachedReconnectToken = token;
        m_hasCachedReconnectToken = true;
    }
    enqueueCommand([token](GeraNESNetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.setLocalReconnectToken(token);
    });
}

void GeraNESNetplayAppRuntime::refreshLocalRomSelectionImmediate()
{
    enqueueCommand([](GeraNESNetplayAppRuntime& self, GeraNESEmu&) {
        self.m_romValidationState.lastSelectedRomKey.clear();
        self.m_romValidationState.lastSubmittedValidationKey.clear();
    });
}

void GeraNESNetplayAppRuntime::updateLatestRawMasks(const std::array<uint64_t, 4>& masks)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_latestRawMasks = masks;
}

void GeraNESNetplayAppRuntime::updateLatestInputState(const IEmulationHost::InputState& inputState)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_latestInputState = inputState;
}

void GeraNESNetplayAppRuntime::recordFramePacing(uint32_t dtMs,
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

void GeraNESNetplayAppRuntime::notifyWebVisibilityChanged(bool visible)
{
    m_emuHost.postCommand([this, visible](GeraNESEmu& emu) {
        notifyWebVisibilityChangedImmediate(emu, visible);
    });
}

void GeraNESNetplayAppRuntime::notifyWebVisibilityChangedImmediate(GeraNESEmu& emu, bool visible)
{
    m_runtimeLastTickTime = {};
    m_webPageVisible = visible;
    if(!visible) {
        if(m_coordinator.isActive() &&
           m_coordinator.isConnected() &&
           m_coordinator.isHosting() &&
           m_coordinator.session().roomState().state == SessionState::Running &&
           m_coordinator.pauseSession()) {
            m_webVisibilityManagedPause = true;
        }

        const bool observerNeedsVisibilityResync =
            m_coordinator.isActive() &&
            m_coordinator.isConnected() &&
            !m_coordinator.isHosting() &&
            m_coordinator.session().roomState().state == SessionState::Running &&
            localAssignedSlots().empty();
        m_observerVisibilityResyncPending = observerNeedsVisibilityResync;
        return;
    }

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

    if(m_observerVisibilityResyncPending) {
        m_inputDriver.reset();
        emu.discardQueuedInputFramesAfter(emu.frameCount());
        m_emuHost.discardQueuedNetplayInputsAfter(emu.frameCount());
        m_emuHost.setSimulationSuspended(true);
        if(!m_coordinator.requestHostResync(ResyncReason::ObserverVisibilityRestore)) {
            m_observerVisibilityResyncPending = false;
            m_emuHost.setSimulationSuspended(false);
        }
        return;
    }

    m_emuHost.discardQueuedNetplayInputsAfter(emu.frameCount());
    const SessionState state = m_coordinator.session().roomState().state;
    if(state == SessionState::Running) {
        reanchorInputDriver(emu.frameCount());
    } else {
        m_inputDriver.reset();
    }
    m_emuHost.setSimulationSuspended(state == SessionState::Paused);
}

GeraNESNetplayAppRuntime::UiSnapshot GeraNESNetplayAppRuntime::uiSnapshot() const
{
    std::scoped_lock stateLock(m_stateMutex);
    return m_uiSnapshot;
}

GeraNESNetplayAppRuntime::MenuSnapshot GeraNESNetplayAppRuntime::menuSnapshot() const
{
    std::scoped_lock stateLock(m_stateMutex);
    MenuSnapshot snapshot;
    snapshot.hosting = m_uiSnapshot.hosting;
    snapshot.inputManaged = m_uiSnapshot.active && m_uiSnapshot.connected;
    snapshot.transportBackend = m_uiSnapshot.transportBackend;
    snapshot.port1Device = geraNESPortDeviceFromTopology(m_uiSnapshot.room, kPort1PlayerSlot);
    snapshot.port2Device = geraNESPortDeviceFromTopology(m_uiSnapshot.room, kPort2PlayerSlot);
    snapshot.expansionDevice = geraNESExpansionDeviceFromTopology(m_uiSnapshot.room);
    snapshot.nesMultitapDevice = geraNESNesMultitapDeviceFromTopology(m_uiSnapshot.room);
    snapshot.famicomMultitapDevice = geraNESFamicomMultitapDeviceFromTopology(m_uiSnapshot.room);
    if(snapshot.inputManaged) {
        for(const auto& participant : m_uiSnapshot.room.participants) {
            if(participant.id != m_uiSnapshot.localParticipantId) continue;
            snapshot.localAssignments = participantAssignments(participant);
            break;
        }
    }
    return snapshot;
}

bool GeraNESNetplayAppRuntime::runtimeActive() const
{
    return m_runtimeActive.load(std::memory_order_acquire);
}

bool GeraNESNetplayAppRuntime::runtimeRunning() const
{
    return m_runtimeRunning.load(std::memory_order_acquire);
}

void GeraNESNetplayAppRuntime::injectDropNextIncomingMessages(MessageType type, uint32_t count)
{
    enqueueCommand([=](GeraNESNetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.dropNextIncomingMessages(type, count);
    });
}

void GeraNESNetplayAppRuntime::clearIncomingMessageDrops()
{
    enqueueCommand([](GeraNESNetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.clearIncomingMessageDrops();
    });
}

void GeraNESNetplayAppRuntime::setReconnectReservationTimeoutForTests(uint32_t seconds)
{
    enqueueCommand([=](GeraNESNetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.setReconnectReservationDurationForTests(seconds);
    });
}

void GeraNESNetplayAppRuntime::simulateTransportFailureForTests()
{
    {
        std::scoped_lock stateLock(m_stateMutex);
        m_uiSnapshot.active = false;
        m_uiSnapshot.hosting = false;
        m_uiSnapshot.connected = false;
        m_uiSnapshot.reconnecting = false;
        m_uiSnapshot.reconnectSecondsRemaining = 0;
    }
    enqueueCommand([](GeraNESNetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_coordinator.simulateTransportFailureForTests();
        self.m_inputDriver.reset();
        self.m_runtimeLastTickTime = {};
        self.updateUiSnapshot(captureCurrentRomSelection(emu));
    });
}

void GeraNESNetplayAppRuntime::setTransportBackend(NetTransportBackend backend)
{
    enqueueCommand([backend](GeraNESNetplayAppRuntime& self, GeraNESEmu& emu) {
        const bool changed = self.m_coordinator.setTransportBackend(backend);
        if(!changed && self.m_coordinator.isActive()) {
            self.m_stickyStatusMessage = "Disconnect before changing the netplay backend.";
        } else if(changed) {
            self.m_stickyStatusMessage.clear();
        }
        self.updateUiSnapshot(captureCurrentRomSelection(emu));
    });
}

void GeraNESNetplayAppRuntime::setTransportOptions(const NetTransportOptions& options)
{
    enqueueCommand([options](GeraNESNetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_coordinator.setTransportOptions(options);
        self.updateUiSnapshot(captureCurrentRomSelection(emu));
    });
}

void GeraNESNetplayAppRuntime::configureRollbackWindow(size_t snapshotCapacity)
{
    enqueueCommand([snapshotCapacity](GeraNESNetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_emuHost.configureNetplaySnapshots(snapshotCapacity);
        self.updateUiSnapshot(captureCurrentRomSelection(emu));
    });
}

NetTransportOptions GeraNESNetplayAppRuntime::transportOptions() const
{
    return m_coordinator.transportOptions();
}

NetTransportBackend GeraNESNetplayAppRuntime::transportBackend() const
{
    std::scoped_lock stateLock(m_stateMutex);
    return m_uiSnapshot.transportBackend;
}

void GeraNESNetplayAppRuntime::host(uint16_t port, size_t maxPeers, const std::string& displayName)
{
    enqueueCommand([=](GeraNESNetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_stickyStatusMessage.clear();
        if(!emu.valid()) {
            self.m_stickyStatusMessage = "Load a ROM before creating a room.";
            return;
        }
        self.m_coordinator.host(port, maxPeers, displayName);
    });
}

void GeraNESNetplayAppRuntime::join(const std::string& hostName, uint16_t port, const std::string& displayName)
{
    enqueueCommand([=](GeraNESNetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_stickyStatusMessage.clear();
        const auto localRom = captureCurrentRomSelection(emu);
        if(self.m_hasCachedReconnectToken) {
            self.m_coordinator.setLocalReconnectToken(self.m_cachedReconnectToken);
        }
        self.m_coordinator.setPendingJoinRomValidation(
            localRom.has_value() && localRom->loaded,
            localRom.has_value() ? localRom->validation : RomValidationData{}
        );
        self.m_coordinator.join(hostName, port, displayName);
    });
}

void GeraNESNetplayAppRuntime::disconnect()
{
    // Netplay pause suspends the emulation worker. Wake it before queuing the
    // disconnect command so teardown is not delayed behind a paused worker.
    m_emuHost.setSimulationSuspended(false);

    {
        std::scoped_lock stateLock(m_stateMutex);
        const NetTransportBackend backend = m_uiSnapshot.transportBackend;
        m_uiSnapshot = UiSnapshot{};
        m_uiSnapshot.transportBackend = backend;
    }
    enqueueCommand([](GeraNESNetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_coordinator.disconnect();
        self.m_inputDriver.reset();
        self.m_selfStallDetector.reset();
        self.ensureStandaloneInputBootstrapFrame(emu);
        self.m_runtimeLastTickTime = {};
        self.m_webVisibilityManagedPause = false;
        self.m_webPageVisible = true;
        self.m_romValidationState.lastSelectedRomKey.clear();
        self.m_romValidationState.lastSubmittedValidationKey.clear();
        self.m_lastSessionState.reset();
        self.m_lastLocalAssignedSlots.clear();
        self.m_lastAssignmentLayoutKey.clear();
        self.m_pendingManualStateResyncs.clear();
        self.updateUiSnapshot(captureCurrentRomSelection(emu));
    });
}

void GeraNESNetplayAppRuntime::assignController(ParticipantId participantId, PlayerSlot slot)
{
    enqueueCommand([=](GeraNESNetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.assignController(participantId, slot);
    });
}

void GeraNESNetplayAppRuntime::addControllerAssignment(ParticipantId participantId, PlayerSlot slot)
{
    enqueueCommand([=](GeraNESNetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.addControllerAssignment(participantId, slot);
    });
}

void GeraNESNetplayAppRuntime::removeControllerAssignment(ParticipantId participantId, PlayerSlot slot)
{
    enqueueCommand([=](GeraNESNetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.removeControllerAssignment(participantId, slot);
    });
}

void GeraNESNetplayAppRuntime::clearControllerAssignments(ParticipantId participantId)
{
    enqueueCommand([=](GeraNESNetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.clearControllerAssignments(participantId);
    });
}

void GeraNESNetplayAppRuntime::configureInputAssignment(ParticipantId participantId,
                                                        std::optional<Settings::Device> port1Device,
                                                        std::optional<Settings::Device> port2Device,
                                                        Settings::ExpansionDevice expansionDevice,
                                                        Settings::NesMultitapDevice nesMultitapDevice,
                                                        Settings::FamicomMultitapDevice famicomMultitapDevice,
                                                        PlayerSlot slot)
{
    configureInputAssignments(
        participantId,
        port1Device,
        port2Device,
        expansionDevice,
        nesMultitapDevice,
        famicomMultitapDevice,
        std::vector<PlayerSlot>{slot}
    );
}

void GeraNESNetplayAppRuntime::configureInputAssignments(ParticipantId participantId,
                                                         std::optional<Settings::Device> port1Device,
                                                         std::optional<Settings::Device> port2Device,
                                                         Settings::ExpansionDevice expansionDevice,
                                                         Settings::NesMultitapDevice nesMultitapDevice,
                                                         Settings::FamicomMultitapDevice famicomMultitapDevice,
                                                         const std::vector<PlayerSlot>& slots)
{
    enqueueCommand([=](GeraNESNetplayAppRuntime& self, GeraNESEmu& emu) {
        const FrameNumber rebuildFromFrame = emu.frameCount() > 0 ? (emu.frameCount() - 1u) : 0u;
        const PlayerSlot preservedSlot = slots.empty() ? kObserverPlayerSlot : slots.front();
        self.m_coordinator.setLocalSimulationFrame(rebuildFromFrame);
        GeraNESNetplayConsole console(self.m_emuHost, emu, self.m_latestInputState);
        console.configureInputTopology(
            self.m_coordinator,
            port1Device,
            port2Device,
            expansionDevice,
            nesMultitapDevice,
            famicomMultitapDevice,
            participantId,
            preservedSlot
        );
        self.m_coordinator.clearControllerAssignments(participantId);
        for(PlayerSlot slot : slots) {
            if(slot == kObserverPlayerSlot) continue;
            self.m_coordinator.addControllerAssignment(participantId, slot);
        }
        emu.discardQueuedInputFramesAfter(rebuildFromFrame);
        self.reanchorInputDriver(rebuildFromFrame);
        self.m_lastAssignmentLayoutKey.clear();
        self.m_lastLocalAssignedSlots.clear();

        const SessionState state = self.m_coordinator.session().roomState().state;
        const bool shouldResyncImmediately =
            self.m_coordinator.isHosting() &&
            emu.valid() &&
            (state == SessionState::Running || state == SessionState::Paused);
        if(!shouldResyncImmediately) {
            return;
        }

        (void)self.m_coordinator.consumePendingHostResyncFrame();

        const FrameNumber authoritativeFrame = emu.frameCount();
        const std::vector<uint8_t> statePayload =
            self.buildAuthoritativeStatePayload(authoritativeFrame, false);
        self.beginAuthoritativeResync(
            authoritativeFrame,
            statePayload,
            false,
            ResyncReason::AssignmentChanged
        );
    });
}

void GeraNESNetplayAppRuntime::kickParticipant(ParticipantId participantId)
{
    enqueueCommand([=](GeraNESNetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.kickParticipant(participantId);
    });
}

void GeraNESNetplayAppRuntime::removeReconnectReservation(ParticipantId participantId)
{
    enqueueCommand([=](GeraNESNetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.removeReconnectReservation(participantId);
    });
}

void GeraNESNetplayAppRuntime::requestForceResync()
{
    enqueueCommand([](GeraNESNetplayAppRuntime& self, GeraNESEmu& emu) {
        if(!self.m_coordinator.isHosting()) return;
        const auto state = self.m_coordinator.session().roomState().state;
        if(!emu.valid()) return;
        if(state != SessionState::Running && state != SessionState::Paused) return;

        const FrameNumber authoritativeFrame = emu.frameCount();
        const std::vector<uint8_t> statePayload =
            self.buildAuthoritativeStatePayload(authoritativeFrame, false);
        self.beginAuthoritativeResync(
            authoritativeFrame,
            statePayload,
            false,
            ResyncReason::ManualForce
        );
    });
}

void GeraNESNetplayAppRuntime::toggleHostedSessionPause()
{
    const UiSnapshot snapshot = uiSnapshot();
    const bool attemptingResume =
        snapshot.active &&
        snapshot.hosting &&
        snapshot.room.state == SessionState::Paused;
    if(attemptingResume) {
        // Resume is processed on the emulation worker. If the worker is still
        // suspended by netplay pause, wake it so the resume command can run.
        m_emuHost.setSimulationSuspended(false);
    }

    enqueueCommand([](GeraNESNetplayAppRuntime& self, GeraNESEmu& emu) {
        if(!self.m_coordinator.isActive() || !self.m_coordinator.isHosting()) return;

        const SessionState state = self.m_coordinator.session().roomState().state;
        if(state == SessionState::Running) {
            if(self.m_coordinator.pauseSession()) {
                self.m_emuHost.setSimulationSuspended(true);
                self.m_emuHost.discardQueuedAudio();
                self.m_runtimeLastTickTime = {};
                self.m_coordinator.appendNetplayLog("Paused");
            }
        } else if(state == SessionState::Paused) {
            const auto localRom = captureCurrentRomSelection(emu);
            if(!self.computeSessionBlockedReason(localRom).empty()) {
                self.m_emuHost.setSimulationSuspended(true);
                return;
            }
            if(self.m_coordinator.resumeSession()) {
                self.m_webVisibilityManagedPause = false;
                self.m_emuHost.setSimulationSuspended(false);
                self.m_runtimeLastTickTime = {};
                self.m_coordinator.appendNetplayLog("Unpaused");
            } else {
                self.m_emuHost.setSimulationSuspended(true);
            }
        }
    });
}

void GeraNESNetplayAppRuntime::appendNetplayLog(const std::string& message)
{
    enqueueCommand([message](GeraNESNetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.appendNetplayLog(message);
    });
}

void GeraNESNetplayAppRuntime::shutdown()
{
    std::scoped_lock stateLock(m_stateMutex);
    m_pendingCommands.clear();
    m_runtimeActive.store(false, std::memory_order_release);
    m_runtimeRunning.store(false, std::memory_order_release);
    m_uiSnapshot = UiSnapshot{};
    m_webVisibilityManagedPause = false;
    m_webPageVisible = true;
    m_emuHost.setSimulationSuspended(false);
    m_selfStallDetector.reset();
    m_coordinator.disconnect();
}

void GeraNESNetplayAppRuntime::shutdownForUnload()
{
    std::scoped_lock stateLock(m_stateMutex);
    m_pendingCommands.clear();
    m_runtimeActive.store(false, std::memory_order_release);
    m_runtimeRunning.store(false, std::memory_order_release);
    m_uiSnapshot = UiSnapshot{};
    m_webVisibilityManagedPause = false;
    m_webPageVisible = true;
    m_emuHost.setSimulationSuspended(false);
    m_selfStallDetector.reset();
    m_coordinator.shutdownForUnload();
}

void GeraNESNetplayAppRuntime::runOnEmulationThread(GeraNESEmu& emu)
{
    drainPendingCommands(emu);

    if(m_hasCachedReconnectToken) {
        m_coordinator.setLocalReconnectToken(m_cachedReconnectToken);
    }

    syncInputDelayFromSettings();

    if(!m_coordinator.isActive() && m_coordinator.reconnectPending()) {
        m_coordinator.update(0);
    }

    if(!m_coordinator.isActive()) {
        (void)m_emuHost.consumeManualStateChanges();
        m_emuHost.setAutoQueuePendingInputOnFrameStart(true);
        m_emuHost.setFrameInputResolver({});
        m_emuHost.setAllowPresenterTimeoutAdvance(true);
        ensureStandaloneInputBootstrapFrame(emu);
        m_runtimeActive.store(false, std::memory_order_release);
        m_runtimeRunning.store(false, std::memory_order_release);
        m_inputDriver.reset();
        m_selfStallDetector.reset();
        m_runtimeLastTickTime = {};
        m_romValidationState.lastSelectedRomKey.clear();
        m_romValidationState.lastSubmittedValidationKey.clear();
        m_lastSessionState.reset();
        m_lastLocalAssignedSlots.clear();
        m_lastAssignmentLayoutKey.clear();
        m_pendingManualStateResyncs.clear();
        m_periodicCrcState = RuntimePeriodicCrcState{};
        m_lastRollbackTargetFrame = 0;
        m_lastMissingRollbackSnapshotFrame = 0;
        m_lastMissingRollbackSnapshotLocalFrame = 0;
        m_lastLoadedAuthoritativeFrame = 0;
        m_lastRecoveryReanchorFrame = 0;
        m_observerVisibilityResyncPending = false;
        m_webVisibilityManagedPause = false;
        m_webPageVisible = true;
        m_emuHost.setSimulationSuspended(false);
        updateUiSnapshot(captureCurrentRomSelection(emu));
        return;
    }

    m_runtimeActive.store(true, std::memory_order_release);
    const bool netplayOwnsEmulationInput = runtimeShouldNetplayOwnEmulationInput(m_coordinator);
    m_emuHost.setAutoQueuePendingInputOnFrameStart(!netplayOwnsEmulationInput);
    if(netplayOwnsEmulationInput) {
        m_emuHost.setFrameInputResolver([this](uint32_t frame, IEmulationHost::ReplayFrameInput& outFrame) {
            return tryBuildPlaybackReplayFrame(frame, outFrame);
        });
    } else {
        m_emuHost.setFrameInputResolver({});
        ensureStandaloneInputBootstrapFrame(emu);
    }
    m_emuHost.setAllowPresenterTimeoutAdvance(false);

    const std::optional<RomSelection> localRom = captureCurrentRomSelection(emu);
    const RoomState roomState = m_coordinator.session().roomState();
    GeraNESNetplayConsole console(m_emuHost, emu, m_latestInputState);
    if(!m_coordinator.isHosting()) {
        console.applyRemoteInputTopology(roomState);
    }
    console.publishCurrentInputTopology(m_coordinator);
    syncRomValidation(localRom);
    syncEmuInputTimelineEpoch();
    m_coordinator.setLocalSimulationFrame(emu.frameCount());
    m_coordinator.update(0);
    syncEmuInputTimelineEpoch();
    handleSessionStateTransitionsOnWorker();
    processAutoStartIfNeeded(localRom);
    processHostManualStateChangeResyncIfNeeded();
    processPendingManualStateResyncIfNeeded();

    if(m_coordinator.isHosting() &&
       m_coordinator.session().roomState().state == SessionState::Starting &&
       m_coordinator.session().roomState().activeResyncId == 0) {
        beginInitialSessionSyncOnWorker();
    }

    processHostResyncIfNeededOnWorker();
    processHostLateJoinResyncIfNeededOnWorker();
    processResyncIfNeededOnWorker(emu);
    processAutoResumeIfNeeded(localRom);
    processRollbackIfNeededOnWorker(emu);
    processHostStallIfNeededOnWorker();

    // Keep pause authoritative even if another runtime path touched the host
    // suspension flag earlier in the frame.
    if(m_coordinator.session().roomState().state == SessionState::Paused) {
        m_emuHost.setSimulationSuspended(true);
    }

    const bool running = m_coordinator.session().roomState().state == SessionState::Running;
    m_runtimeRunning.store(running, std::memory_order_release);

    IEmulationHost::InputState latestInputState{};
    {
        std::scoped_lock stateLock(m_stateMutex);
        latestInputState = m_latestInputState;
    }

    RuntimeAssignmentLayoutResult assignmentLayout = runtimeSyncAssignmentLayout(
        m_coordinator,
        m_inputDriver,
        m_lastAssignmentLayoutKey,
        m_lastLocalAssignedSlots,
        emu.frameCount(),
        running
    );
    if(assignmentLayout.layoutChanged) {
        m_emuHost.discardQueuedNetplayInputsAfter(emu.frameCount());
    }
    if(assignmentLayout.reanchorInputDriver) {
        m_lastRecoveryReanchorFrame = emu.frameCount();
    }
    const uint32_t workerDtMs = consumeWorkerDtMs();

    GeraNESNetplayConsole inputConsole(m_emuHost, emu, latestInputState);
    runtimeProduceLocalBufferedInputs(
        m_coordinator,
        m_inputDriver,
        inputConsole,
        assignmentLayout.localSlots,
        workerDtMs
    );

    if(running) {
        constexpr uint32_t kMaxContinuousClockCatchupFrames = 120u;
        (void)advanceToSharedClockIfNeededOnWorker(emu, kMaxContinuousClockCatchupFrames);

        runtimePreparePlaybackFrames(m_coordinator, m_inputDriver, inputConsole);

        const uint32_t currentFrame = emu.frameCount();
        tryQueuePlaybackFrameToEmu(emu, currentFrame);
    }

    processPeriodicLocalCrcIfNeeded();

    updateUiSnapshot(localRom);
}
} // namespace GeraNESNetplay
