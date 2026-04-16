#include "GeraNESNetplay/NetplayAppRuntime.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "GeraNES/util/Crc32.h"

namespace Netplay {

NetplayAppRuntime::NetplayAppRuntime(IEmulationHost& emuHost)
    : m_emuHost(emuHost)
{
}

std::string NetplayAppRuntime::buildRomKey(const std::optional<RomSelection>& selection)
{
    if(!selection.has_value() || !selection->loaded) return "none";

    const auto& v = selection->validation;
    return selection->gameName + "|" +
           std::to_string(v.romCrc32) + "|" +
           std::to_string(v.mapperId) + "|" +
           std::to_string(v.subMapperId) + "|" +
           std::to_string(v.prgRomSize) + "|" +
           std::to_string(v.chrRomSize) + "|" +
           std::to_string(v.chrRamSize) + "|" +
           std::to_string(v.fileSize);
}

std::optional<NetplayAppRuntime::RomSelection> NetplayAppRuntime::captureCurrentRomSelection(GeraNESEmu& emu)
{
    if(!emu.valid()) return std::nullopt;

    Cartridge& cart = emu.getConsole().cartridge();
    RomSelection selection;
    selection.loaded = true;
    selection.gameName = cart.romFile().fileName();
    selection.validation.romCrc32 = cart.romFile().fileCrc32();
    selection.validation.mapperId = static_cast<uint16_t>(std::max(0, cart.mapperId()));
    selection.validation.subMapperId = static_cast<uint16_t>(std::max(0, cart.subMapperId()));
    selection.validation.prgRomSize = static_cast<uint32_t>(std::max(0, cart.prgSize()));
    selection.validation.chrRomSize = static_cast<uint32_t>(std::max(0, cart.chrSize()));
    selection.validation.chrRamSize = static_cast<uint32_t>(std::max(0, cart.chrRamSize()));
    selection.validation.fileSize = static_cast<uint32_t>(cart.romFile().size());
    return selection;
}

std::string NetplayAppRuntime::buildAssignmentLayoutKey() const
{
    if(!m_coordinator.isActive()) return {};

    std::string key;
    for(const auto& participant : m_coordinator.session().roomState().participants) {
        key += std::to_string(participant.id);
        key += ":";
        bool first = true;
        for(PlayerSlot slot : participantAssignments(participant)) {
            if(!first) key += ",";
            key += std::to_string(static_cast<int>(slot));
            first = false;
        }
        key += ";";
    }
    return key;
}

void NetplayAppRuntime::reanchorInputDriver(FrameNumber anchorFrame, const std::vector<PlayerSlot>& localSlots)
{
    (void)localSlots;
    m_inputDriver.reanchor(anchorFrame);
    m_lastRecoveryReanchorFrame = anchorFrame;
}

bool NetplayAppRuntime::applyAuthoritativeStateLocally(GeraNESEmu& emu,
                                                       FrameNumber targetFrame,
                                                       const std::vector<uint8_t>& payload)
{
    if(payload.empty()) return false;
    m_emuHost.beginPresentationHoldUntilNextFrameReady();
    if(!emu.loadStateFromMemoryOnCleanBoot(payload)) return false;

    const uint32_t loadedCrc32 = emu.canonicalNetplayStateCrc32();
    emu.discardQueuedInputFramesAfter(targetFrame);
    syncEmuInputTimelineEpoch(emu);
    m_coordinator.setLocalSimulationFrame(targetFrame);
    m_emuHost.discardQueuedNetplayInputsAfter(targetFrame);
    m_emuHost.seedNetplaySnapshot(targetFrame, payload, loadedCrc32);
    m_emuHost.setAuthoritativeFrameReadyState(targetFrame, loadedCrc32);
    m_lastLoadedAuthoritativeFrame = targetFrame;
    reanchorInputDriver(targetFrame, localAssignedSlots());
    return true;
}

std::vector<uint8_t> NetplayAppRuntime::buildAuthoritativeStatePayload(GeraNESEmu& emu,
                                                                        FrameNumber authoritativeFrame,
                                                                        bool preferConfirmedSnapshot) const
{
    if(preferConfirmedSnapshot) {
        if(const std::optional<std::vector<uint8_t>> snapshot =
               m_emuHost.netplaySnapshotForFrame(authoritativeFrame);
           snapshot.has_value()) {
            return *snapshot;
        }
    }

    return emu.saveNetplayStateToMemory();
}

uint32_t NetplayAppRuntime::computeAuthoritativeStateCrc32(GeraNESEmu& emu,
                                                           FrameNumber authoritativeFrame,
                                                           bool preferConfirmedSnapshot) const
{
    if(preferConfirmedSnapshot) {
        if(const std::optional<uint32_t> snapshotCrc32 =
               m_emuHost.netplaySnapshotCrc32ForFrame(authoritativeFrame);
           snapshotCrc32.has_value()) {
            return *snapshotCrc32;
        }
    }

    if(!preferConfirmedSnapshot &&
       emu.valid() &&
       emu.frameCount() == authoritativeFrame) {
        return emu.canonicalNetplayStateCrc32();
    }

    return 0;
}

bool NetplayAppRuntime::beginAuthoritativeResync(GeraNESEmu& emu,
                                                 FrameNumber authoritativeFrame,
                                                 const std::vector<uint8_t>& statePayload,
                                                 bool preferConfirmedSnapshot,
                                                 ResyncReason reason,
                                                 ParticipantId targetParticipantId)
{
    if(statePayload.empty()) return false;

    const uint32_t payloadCrc32 =
        Crc32::calc(reinterpret_cast<const char*>(statePayload.data()), statePayload.size());
    const uint32_t stateCrc32 =
        computeAuthoritativeStateCrc32(emu, authoritativeFrame, preferConfirmedSnapshot);
    if(!m_coordinator.beginResync(
           authoritativeFrame,
           statePayload,
           payloadCrc32,
           stateCrc32,
           reason,
           targetParticipantId
       )) {
        return false;
    }

    if(targetParticipantId == kInvalidParticipantId) {
        applyAuthoritativeStateLocally(emu, authoritativeFrame, statePayload);
    }
    return true;
}

bool NetplayAppRuntime::beginAuthoritativeResyncWithoutLocalReload(GeraNESEmu& emu,
                                                                   FrameNumber authoritativeFrame,
                                                                   const std::vector<uint8_t>& statePayload,
                                                                   bool preferConfirmedSnapshot,
                                                                   ResyncReason reason)
{
    if(statePayload.empty()) return false;

    const uint32_t payloadCrc32 =
        Crc32::calc(reinterpret_cast<const char*>(statePayload.data()), statePayload.size());
    const uint32_t stateCrc32 =
        computeAuthoritativeStateCrc32(emu, authoritativeFrame, preferConfirmedSnapshot);
    if(!m_coordinator.beginResync(authoritativeFrame, statePayload, payloadCrc32, stateCrc32, reason)) {
        return false;
    }

    syncEmuInputTimelineEpoch(emu);
    m_coordinator.invalidateLocalCrcHistoryAfter(authoritativeFrame);
    m_coordinator.setLocalSimulationFrame(authoritativeFrame);
    if(stateCrc32 != 0) {
        m_emuHost.seedNetplaySnapshot(authoritativeFrame, statePayload, stateCrc32);
        m_emuHost.setAuthoritativeFrameReadyState(authoritativeFrame, stateCrc32);
    } else {
        m_emuHost.seedNetplaySnapshot(authoritativeFrame, statePayload);
    }
    reanchorInputDriver(authoritativeFrame, localAssignedSlots());
    return true;
}

std::vector<PlayerSlot> NetplayAppRuntime::localAssignedSlots() const
{
    if(!m_coordinator.isActive()) return {};

    const ParticipantId localParticipantId = m_coordinator.localParticipantId();
    for(const auto& participant : m_coordinator.session().roomState().participants) {
        if(participant.id == localParticipantId) {
            return participant.controllerAssignments;
        }
    }

    return {};
}

std::string NetplayAppRuntime::computeSessionBlockedReason(const std::optional<RomSelection>& localRom) const
{
    if(!m_coordinator.isActive()) return "Netplay is not active.";

    const auto& room = m_coordinator.session().roomState();
    if(!localRom.has_value() || !localRom->loaded) {
        return "Load a ROM locally to populate room validation.";
    }
    if(room.selectedGameName.empty()) {
        return "Load a ROM locally to populate room validation.";
    }

    bool anyMissingRom = false;
    bool anyIncompatibleRom = false;

    for(const auto& participant : room.participants) {
        if(participantIsObserver(participant)) continue;
        if(!participant.connected) continue;
        if(!participant.romLoaded) anyMissingRom = true;
        else if(!participant.romCompatible) anyIncompatibleRom = true;
    }

    if(anyMissingRom) return "Waiting for assigned participants to load the selected ROM.";
    if(anyIncompatibleRom) return "One or more assigned participants have an incompatible ROM.";
    return "";
}

void NetplayAppRuntime::drainPendingCommands(GeraNESEmu& emu)
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

void NetplayAppRuntime::syncRomValidation(const std::optional<RomSelection>& localRom)
{
    const std::string localRomKey = buildRomKey(localRom);

    if(!m_coordinator.isActive()) {
        m_lastSelectedRomKey.clear();
        m_lastSubmittedValidationKey.clear();
        m_lastSessionState.reset();
        return;
    }

    if(m_coordinator.isHosting() && localRomKey != m_lastSelectedRomKey && localRom.has_value()) {
        m_coordinator.selectRom(localRom->gameName, localRom->validation);
        m_lastSelectedRomKey = localRomKey;
        m_lastSubmittedValidationKey.clear();
    }

    const auto& room = m_coordinator.session().roomState();
    const bool hostRomSelected = !room.selectedGameName.empty();
    const bool romLoaded = localRom.has_value() && localRom->loaded;
    const bool romCompatible =
        hostRomSelected &&
        romLoaded &&
        NetplayCoordinator::romValidationMatches(localRom->validation, room.romValidation);

    const std::string validationKey =
        std::to_string(room.sessionId) + "|" +
        std::to_string(room.romValidation.romCrc32) + "|" +
        localRomKey + "|" +
        (romLoaded ? "1" : "0") + "|" +
        (romCompatible ? "1" : "0");

    if(validationKey != m_lastSubmittedValidationKey) {
        m_coordinator.submitLocalRomValidation(
            romLoaded,
            romCompatible,
            localRom.has_value() ? localRom->validation : RomValidationData{}
        );
        m_lastSubmittedValidationKey = validationKey;
    }

    if(!m_coordinator.isHosting() &&
       !room.selectedGameName.empty() &&
       (!romLoaded || !romCompatible)) {
        std::ostringstream oss;
        oss << "Room requires ROM \"" << room.selectedGameName
            << "\" (CRC " << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
            << room.romValidation.romCrc32 << ").";
        m_stickyStatusMessage = oss.str();
        m_coordinator.disconnect();
        m_inputDriver.reset();
        m_runtimeLastTickTime = {};
        m_lastLocalAssignedSlots.clear();
        m_lastAssignmentLayoutKey.clear();
        return;
    }

    if(!m_coordinator.isHosting() && romLoaded && romCompatible && !room.selectedGameName.empty()) {
        m_stickyStatusMessage.clear();
    }
}

void NetplayAppRuntime::syncInputDelayFromSettings(GeraNESEmu& emu)
{
    auto& cfg = AppSettings::instance().data.netplay;
    cfg.gameplayReceiveDelayMs = std::max(0, cfg.gameplayReceiveDelayMs);
#ifdef NDEBUG
    cfg.gameplayReceiveDelayMs = 0;
#endif
    m_coordinator.setGameplayReceiveDelayMs(static_cast<uint32_t>(cfg.gameplayReceiveDelayMs));
    m_autoSettings.setEnabled(cfg.autoGameplayTuning);

    if(!m_coordinator.isActive()) {
        m_inputDriver.setPrebufferFrames(static_cast<uint32_t>(std::max(0, cfg.inputDelayFrames)));
        m_inputDriver.setPredictFrames(static_cast<uint32_t>(std::max(0, cfg.predictFrames)));
        return;
    }

    const auto& room = m_coordinator.session().roomState();
    if(m_coordinator.isHosting()) {
        if(cfg.autoGameplayTuning) {
            const auto recommendations = m_autoSettings.update(
                room,
                m_coordinator.predictionStats(),
                m_coordinator.unresolvedPredictedRemoteFrameCount(),
                emu.getRegionFPS()
            );
            if(recommendations.inputDelayFrames.has_value() &&
               room.inputDelayFrames != *recommendations.inputDelayFrames) {
                m_coordinator.setInputDelayFrames(*recommendations.inputDelayFrames);
            }
            if(recommendations.predictFrames.has_value() &&
               room.predictFrames != *recommendations.predictFrames) {
                m_coordinator.setPredictFrames(*recommendations.predictFrames);
            }
        } else {
            const uint8_t manualDelay = static_cast<uint8_t>(std::max(0, cfg.inputDelayFrames));
            const uint8_t manualPredict = static_cast<uint8_t>(std::max(0, cfg.predictFrames));
            if(room.inputDelayFrames != manualDelay) {
                m_coordinator.setInputDelayFrames(manualDelay);
            }
            if(room.predictFrames != manualPredict) {
                m_coordinator.setPredictFrames(manualPredict);
            }
        }
    }

    const auto& effectiveRoom = m_coordinator.session().roomState();
    m_inputDriver.setPrebufferFrames(static_cast<uint32_t>(effectiveRoom.inputDelayFrames));
    m_inputDriver.setPredictFrames(static_cast<uint32_t>(effectiveRoom.predictFrames));

    cfg.inputDelayFrames = static_cast<int>(effectiveRoom.inputDelayFrames);
    cfg.predictFrames = static_cast<int>(effectiveRoom.predictFrames);
}

void NetplayAppRuntime::processAutoResumeIfNeeded(const std::optional<RomSelection>& localRom)
{
    if(!m_coordinator.isActive() || !m_coordinator.isHosting()) return;

    const auto& room = m_coordinator.session().roomState();
    if(room.state != SessionState::Paused) return;
    if(room.activeResyncId != 0 || room.pendingResyncAckCount != 0) return;
    if(!computeSessionBlockedReason(localRom).empty()) return;

    if(!m_webVisibilityManagedPause) return;
    if(!m_webPageVisible) return;

    if(m_coordinator.resumeSession()) {
        m_webVisibilityManagedPause = false;
    }
}

void NetplayAppRuntime::processHostManualStateChangeResyncIfNeeded(GeraNESEmu& emu)
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
        if(!emu.valid()) continue;

        const ResyncReason reason =
            event.kind == IEmulationHost::ManualStateChangeKind::Reset
                ? ResyncReason::HostReset
                : ResyncReason::HostLoadedState;
        {
            std::ostringstream oss;
            oss << "Owner manual state change detected"
                << " reason " << NetplayCoordinator::resyncReasonToast(reason)
                << " eventFrame " << event.frame
                << " emuFrame " << emu.frameCount()
                << " roomEpoch " << room.timelineEpoch;
            m_coordinator.appendNetplayLog(oss.str());
        }
        const std::string toast = NetplayCoordinator::resyncReasonToast(reason);
        if(!toast.empty()) {
            m_coordinator.appendNetplayLog(toast);
        }

        const FrameNumber eventFrame = std::min<FrameNumber>(event.frame, emu.frameCount());
        const bool resyncBusy =
            state == SessionState::Resyncing ||
            room.activeResyncId != 0 ||
            room.pendingResyncAckCount != 0;
        emu.discardQueuedInputFramesAfter(eventFrame);
        syncEmuInputTimelineEpoch(emu);
        reanchorInputDriver(eventFrame, localAssignedSlots());

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
                buildAuthoritativeStatePayload(emu, eventFrame, false);
            if(statePayload.empty()) {
                continue;
            }

            m_pendingManualStateResyncs.clear();
            beginAuthoritativeResync(emu, eventFrame, statePayload, false, reason);
            continue;
        }

        m_pendingManualStateResyncs.clear();
        m_pendingManualStateResyncs.push_back(PendingManualStateResync{reason, eventFrame, true});
    }
}

void NetplayAppRuntime::processPendingManualStateResyncIfNeeded(GeraNESEmu& emu)
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
        if(pending.waitForAdvance && emu.frameCount() <= pending.eventFrame) {
            return;
        }

        const FrameNumber authoritativeFrame = emu.frameCount();
        const std::vector<uint8_t> statePayload =
            buildAuthoritativeStatePayload(emu, authoritativeFrame, false);
        if(statePayload.empty()) {
            return;
        }

        if(beginAuthoritativeResync(emu, authoritativeFrame, statePayload, false, pending.reason)) {
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

void NetplayAppRuntime::processAutoStartIfNeeded(GeraNESEmu& emu, const std::optional<RomSelection>& localRom)
{
    if(!m_coordinator.isActive() || !m_coordinator.isHosting()) return;
    if(!localRom.has_value() || !localRom->loaded) return;

    const auto& room = m_coordinator.session().roomState();
    if(room.selectedGameName.empty()) return;
    if(room.state == SessionState::Starting ||
       room.state == SessionState::Running ||
       room.state == SessionState::Resyncing ||
       room.state == SessionState::Paused ||
       room.state == SessionState::Ended) {
        return;
    }
    if(!computeSessionBlockedReason(localRom).empty()) return;

    m_coordinator.setLocalSimulationFrame(emu.frameCount());
    if(m_coordinator.startSession() && emu.frameCount() > 0) {
        beginInitialSessionSyncOnWorker(emu);
    }
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

void NetplayAppRuntime::handleSessionStateTransitionsOnWorker(GeraNESEmu& emu)
{
    if(!m_coordinator.isActive()) {
        m_lastSessionState.reset();
        m_inputDriver.reset();
        m_runtimeLastTickTime = {};
        m_lastSubmittedLocalCrcFrame = 0;
        m_nextScheduledLocalCrcFrame = kDesyncCrcIntervalFrames;
        m_postRecoveryRapidCrcThroughFrame = 0;
        m_lastRollbackTargetFrame = 0;
        m_lastLoadedAuthoritativeFrame = 0;
        m_lastRecoveryReanchorFrame = 0;
        m_forceNextConfirmedCrcSubmission = false;
        m_waitingPostResyncDelayBuffer = false;
        m_waitingPostResyncDelayBufferSince = {};
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
                : emu.frameCount();
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
        reanchorInputDriver(anchorFrame, localAssignedSlots());
        m_waitingPostResyncDelayBuffer = true;
        m_waitingPostResyncDelayBufferSince = std::chrono::steady_clock::now();
        m_postRecoveryRapidCrcThroughFrame = anchorFrame + 3u;
        if(m_observerVisibilityResyncPending) {
            m_observerVisibilityResyncPending = false;
            m_emuHost.setSimulationSuspended(false);
        }
    }

    if(currentState == SessionState::Running &&
       (!previousState.has_value() || *previousState != SessionState::Running)) {
        m_forceNextConfirmedCrcSubmission = true;
    }

    m_lastSessionState = currentState;
}

void NetplayAppRuntime::processPeriodicLocalCrcIfNeeded(GeraNESEmu& emu)
{
    // Desync monitoring compares confirmed or freshly recovered authoritative
    // checkpoints only. Predicted-frame divergence is expected and should be
    // corrected by rollback before it becomes CRC-visible.
    if(!kDesyncMonitorEnabled) return;
    if(!m_coordinator.isActive()) return;
    if(m_coordinator.session().roomState().state != SessionState::Running) return;
    if(!emu.valid()) return;

    const FrameNumber confirmedFrame = m_coordinator.latestConfirmedFrame();
    const FrameNumber lastFrameReadyFrame = m_emuHost.lastFrameReadyFrame();
    const FrameNumber safeConfirmedFrame =
        confirmedFrame == 0u ? 0u : std::min(confirmedFrame, lastFrameReadyFrame);
    const FrameNumber authoritativeCheckpointFrame =
        (m_lastLoadedAuthoritativeFrame != 0u &&
         lastFrameReadyFrame == m_lastLoadedAuthoritativeFrame)
            ? m_lastLoadedAuthoritativeFrame
            : 0u;
    const FrameNumber crcCheckpointFrame = std::max(safeConfirmedFrame, authoritativeCheckpointFrame);
    if(crcCheckpointFrame == 0) return;

    const bool periodicDue = crcCheckpointFrame >= m_nextScheduledLocalCrcFrame;
    const bool postRecoveryRapidDue =
        crcCheckpointFrame > m_lastSubmittedLocalCrcFrame &&
        crcCheckpointFrame <= std::max(m_postRecoveryRapidCrcThroughFrame, authoritativeCheckpointFrame);
    const bool forcedDue =
        m_forceNextConfirmedCrcSubmission &&
        crcCheckpointFrame != m_lastSubmittedLocalCrcFrame;
    if(!periodicDue && !forcedDue && !postRecoveryRapidDue) return;

    std::optional<uint32_t> crc32;
    if(crcCheckpointFrame == authoritativeCheckpointFrame &&
       m_emuHost.lastFrameReadyNetplayCrc32() != 0u) {
        crc32 = m_emuHost.lastFrameReadyNetplayCrc32();
    }
    if(!crc32.has_value()) {
        crc32 = m_emuHost.netplaySnapshotCrc32ForFrame(crcCheckpointFrame);
    }
    if(!crc32.has_value() && emu.frameCount() == crcCheckpointFrame) {
        crc32 = emu.canonicalNetplayStateCrc32();
    }
    if(!crc32.has_value()) return;

    m_coordinator.submitLocalCrc(crcCheckpointFrame, *crc32);
    m_lastSubmittedLocalCrcFrame = crcCheckpointFrame;
    m_forceNextConfirmedCrcSubmission = false;
    if(crcCheckpointFrame >= m_postRecoveryRapidCrcThroughFrame) {
        m_postRecoveryRapidCrcThroughFrame = 0;
    }
    m_nextScheduledLocalCrcFrame =
        ((crcCheckpointFrame / kDesyncCrcIntervalFrames) + 1u) * kDesyncCrcIntervalFrames;
}

bool NetplayAppRuntime::beginInitialSessionSyncOnWorker(GeraNESEmu& emu)
{
    if(!m_coordinator.isHosting()) return false;
    if(!emu.valid()) return false;
    if(m_coordinator.session().roomState().state != SessionState::Starting) return false;

    const FrameNumber authoritativeFrame = emu.frameCount();
    const std::vector<uint8_t> statePayload =
        buildAuthoritativeStatePayload(emu, authoritativeFrame, false);
    if(!beginAuthoritativeResync(emu, authoritativeFrame, statePayload, false)) {
        return false;
    }

    m_coordinator.appendNetplayLog("Netplay initial session sync started");
    return true;
}

void NetplayAppRuntime::processHostResyncIfNeededOnWorker(GeraNESEmu& emu)
{
    if(!m_coordinator.isHosting()) return;

    std::optional<NetplayCoordinator::PendingHostResyncRequest> pending = m_coordinator.consumePendingHostResyncFrame();
    if(!pending.has_value()) return;
    if(!emu.valid()) return;

    const bool initialSessionSync =
        m_coordinator.session().roomState().state == SessionState::Starting;

    const FrameNumber requestedFrame =
        initialSessionSync
            ? emu.frameCount()
            : pending->frame;
    const FrameNumber authoritativeFrame =
        std::min<FrameNumber>(requestedFrame, emu.frameCount());

    const std::vector<uint8_t> statePayload =
        buildAuthoritativeStatePayload(emu, authoritativeFrame, !initialSessionSync);
    if(statePayload.empty()) return;

    const ResyncReason reason =
        initialSessionSync ? ResyncReason::InitialSessionSync : pending->reason;
    if(beginAuthoritativeResync(
           emu,
           authoritativeFrame,
           statePayload,
           !initialSessionSync,
           reason,
           pending->participantId
       )) {
        if(initialSessionSync) {
            m_coordinator.appendNetplayLog("Netplay initial session sync started");
        } else {
            m_coordinator.appendNetplayLog(
                "Netplay hard resync started after reason " + std::to_string(static_cast<int>(reason)) +
                " at frame " + std::to_string(pending->frame) +
                ", using authoritative frame " + std::to_string(authoritativeFrame)
            );
        }
    }
}

void NetplayAppRuntime::processHostLateJoinResyncIfNeededOnWorker(GeraNESEmu& emu)
{
    if(!m_coordinator.isHosting()) return;

    std::optional<ParticipantId> participantId = m_coordinator.consumePendingHostLateJoinResyncParticipant();
    if(!participantId.has_value()) return;
    if(!emu.valid()) return;

    const FrameNumber authoritativeFrame =
        std::min<FrameNumber>(m_coordinator.session().roomState().lastConfirmedFrame, emu.frameCount());
    const std::vector<uint8_t> statePayload =
        buildAuthoritativeStatePayload(emu, authoritativeFrame, true);
    if(statePayload.empty()) return;

    if(beginAuthoritativeResync(
           emu,
           authoritativeFrame,
           statePayload,
           true,
           ResyncReason::InitialSessionSync,
           *participantId
       )) {
        m_coordinator.appendNetplayLog(
            "Netplay late-join resync started for participant " +
            std::to_string(static_cast<int>(*participantId))
        );
    }
}

void NetplayAppRuntime::processResyncIfNeededOnWorker(GeraNESEmu& emu)
{
    std::optional<NetplayCoordinator::PendingResyncApply> pending = m_coordinator.consumePendingResyncApply();
    if(!pending.has_value()) return;

    m_emuHost.beginPresentationHoldUntilNextFrameReady();
    const bool loaded = emu.loadStateFromMemoryOnCleanBoot(pending->payload);
    const uint32_t loadedFrame = emu.frameCount();
    const bool loadedExpectedFrame =
        loaded &&
        (pending->targetFrame == 0u || loadedFrame == pending->targetFrame);
    const uint32_t loadedCrc32 = loadedExpectedFrame ? emu.canonicalNetplayStateCrc32() : 0;
    if(loadedExpectedFrame) {
        emu.discardQueuedInputFramesAfter(pending->targetFrame);
        syncEmuInputTimelineEpoch(emu);
        m_coordinator.setLocalSimulationFrame(pending->targetFrame);
        m_emuHost.discardQueuedNetplayInputsAfter(pending->targetFrame);
        m_emuHost.seedNetplaySnapshot(pending->targetFrame, pending->payload, loadedCrc32);
        m_lastLoadedAuthoritativeFrame = pending->targetFrame;
        m_emuHost.setAuthoritativeFrameReadyState(
            pending->frameReadyFrame != 0u ? pending->frameReadyFrame : pending->targetFrame,
            pending->frameReadyCrc32 != 0u ? pending->frameReadyCrc32 : loadedCrc32
        );
        reanchorInputDriver(pending->targetFrame, localAssignedSlots());
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

void NetplayAppRuntime::alignResyncPlaybackToSharedClockOnWorker(GeraNESEmu& emu, FrameNumber loadedFrame)
{
    if(!m_coordinator.isActive() || m_coordinator.session().roomState().state != SessionState::Running) {
        return;
    }

    if(!emu.valid()) {
        return;
    }

    constexpr uint32_t kMaxSilentCatchupFrames = 120u;
    const uint32_t advancedFrames = advanceToSharedClockIfNeededOnWorker(emu, kMaxSilentCatchupFrames);

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

uint32_t NetplayAppRuntime::advanceToSharedClockIfNeededOnWorker(GeraNESEmu& emu, uint32_t maxFrames)
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
    uint32_t advancedFrames = 0u;

    while(advancedFrames < maxFrames) {
        const FrameNumber nextFrame = emu.frameCount() + 1u;
        const uint64_t nextFrameClockMicros = m_coordinator.authoritativeFrameStartClockMicros(nextFrame);
        if(nextFrameClockMicros == 0u) {
            break;
        }

        const uint64_t nowSharedClockMicros = m_coordinator.sharedClockNowMicros();
        if(nowSharedClockMicros == 0u || nowSharedClockMicros < nextFrameClockMicros) {
            break;
        }

        if(!tryQueuePlaybackFrameToEmu(emu, nextFrame)) {
            break;
        }

        if(!emu.updateUntilFrame(frameDt, true)) {
            break;
        }

        ++advancedFrames;
        m_coordinator.setLocalSimulationFrame(emu.frameCount());
    }

    return advancedFrames;
}

void NetplayAppRuntime::processRollbackIfNeededOnWorker(GeraNESEmu& emu)
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

    const std::optional<std::vector<uint8_t>> snapshotData = m_emuHost.netplaySnapshotForFrame(*rollbackFrame);
    if(!snapshotData.has_value() || snapshotData->empty()) {
        m_coordinator.appendNetplayLog("Netplay rollback failed: snapshot unavailable");
        return;
    }

    const uint32_t rollbackFromFrame = currentFrame;
    m_emuHost.beginPresentationHoldUntilNextFrameReady();
    emu.loadStateFromMemoryWithAudioPolicy(
        *snapshotData,
        GeraNESEmu::StateLoadAudioPolicy::PreserveContinuousOutput);
    if(!emu.valid()) {
        m_coordinator.appendNetplayLog("Netplay rollback failed");
        return;
    }

    const uint32_t rollbackCanonicalCrc32 = emu.canonicalNetplayStateCrc32();
    m_emuHost.seedNetplaySnapshot(*rollbackFrame, *snapshotData, rollbackCanonicalCrc32);
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
    reanchorInputDriver(inputDriverAnchorFrame, localAssignedSlots());

    const uint32_t frameDt =
        std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));
    while(emu.frameCount() < currentFrame) {
        const uint32_t nextFrame = emu.frameCount() + 1u;
        NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
        const bool allowPrediction = shouldAllowPredictionForFrame(nextFrame);
        if(!m_coordinator.tryBuildPlaybackFrame(nextFrame, allowPrediction, playbackFrame)) {
            m_coordinator.appendNetplayLog("Netplay resimulation failed");
            return;
        }

        InputFrame inputFrame = playbackFrame.inputFrame;
        inputFrame.speculative = playbackFrame.predicted;
        inputFrame.timelineEpoch = emu.inputTimelineEpoch();
        const InputBuffer::EnqueueResult enqueueResult = emu.queueInputFrame(inputFrame);
        if(enqueueResult != InputBuffer::EnqueueResult::Inserted &&
           enqueueResult != InputBuffer::EnqueueResult::UpdatedPending) {
            m_coordinator.appendNetplayLog(
                "Netplay resimulation failed: rejected playback enqueue at frame " +
                std::to_string(nextFrame)
            );
            return;
        }
        if(!emu.updateUntilFrame(frameDt, false)) {
            m_coordinator.appendNetplayLog(
                "Netplay resimulation failed: emulator did not advance at frame " +
                std::to_string(nextFrame)
            );
            return;
        }
    }
    m_coordinator.setLocalSimulationFrame(emu.frameCount());
    m_emuHost.discardQueuedNetplayInputsAfter(*rollbackFrame);

    const uint32_t recoveredConfirmedFrame = m_coordinator.session().roomState().lastConfirmedFrame;
    if(recoveredConfirmedFrame != 0u && recoveredConfirmedFrame <= emu.frameCount()) {
        const uint32_t recoveredConfirmedCrc32 = emu.canonicalNetplayStateCrc32();
        std::ostringstream validate;
        validate << "Rollback recovery reanchored"
                 << " targetFrame " << *rollbackFrame
                 << " confirmedFrame " << recoveredConfirmedFrame
                 << " localSimulationFrame " << emu.frameCount()
                 << " canonicalCrc32 " << recoveredConfirmedCrc32;
        m_coordinator.appendNetplayLog(validate.str());
    }

    m_coordinator.appendNetplayLog(
        "Netplay rollback applied (" + std::to_string(rollbackFromFrame) +
        " -> " + std::to_string(*rollbackFrame) + ")"
    );
}

void NetplayAppRuntime::updateUiSnapshot(const std::optional<RomSelection>& localRom)
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
    if(const auto* latestLocal = m_coordinator.localInputs().latest()) {
    snapshot.latestLocalInput = *latestLocal;
    }
    if(const auto* latestRemote = m_coordinator.remoteInputs().latest()) {
        snapshot.latestRemoteInput = *latestRemote;
    }
    snapshot.predictionStats = m_coordinator.predictionStats();
    snapshot.localSimulationFrame = m_coordinator.localSimulationFrame();
    snapshot.publishedConfirmedFrame = m_coordinator.latestPublishedConfirmedFrame();
    snapshot.lastSubmittedLocalCrcFrame = m_lastSubmittedLocalCrcFrame;
    snapshot.lastRollbackTargetFrame = m_lastRollbackTargetFrame;
    snapshot.lastLoadedAuthoritativeFrame = m_lastLoadedAuthoritativeFrame;
    snapshot.lastRecoveryReanchorFrame = m_lastRecoveryReanchorFrame;
    snapshot.autoSettings = m_autoSettings.snapshot();
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

void NetplayAppRuntime::syncEmuInputTimelineEpoch(GeraNESEmu& emu)
{
    const uint32_t timelineEpoch =
        m_coordinator.isActive() ? m_coordinator.session().roomState().timelineEpoch : 0u;
    if(emu.inputTimelineEpoch() != timelineEpoch) {
        emu.setInputTimelineEpoch(timelineEpoch);
    }
}

bool NetplayAppRuntime::tryBuildPlaybackConfirmedFrame(uint32_t frame,
                                                              NetplayCoordinator::ConfirmedFrameInputs& outFrame)
{
    const bool allowPrediction = shouldAllowPredictionForFrame(frame);
    if(!m_coordinator.tryBuildPlaybackFrame(frame, allowPrediction, outFrame)) {
        recordPlaybackStop(frame);
        return false;
    }
    return true;
}

bool NetplayAppRuntime::shouldAllowPredictionForFrame(FrameNumber frame) const
{
    const FrameNumber confirmedThroughFrame = m_inputDriver.confirmedThroughFrame(m_coordinator);
    const FrameNumber delaySlackFrame =
        confirmedThroughFrame + static_cast<FrameNumber>(m_inputDriver.prebufferFrames());
    return frame > delaySlackFrame;
}

bool NetplayAppRuntime::tryBuildPlaybackReplayFrame(uint32_t frame, IEmulationHost::ReplayFrameInput& outFrame)
{
    m_coordinator.recordLocalAuthoritativeFrameStart(frame);
    NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
    if(!tryBuildPlaybackConfirmedFrame(frame, playbackFrame)) {
        return false;
    }
    outFrame = {};
    outFrame.speculative = playbackFrame.predicted;
    outFrame.hasFrameOverride = true;
    outFrame.frameOverride = playbackFrame.inputFrame;
    outFrame.frameOverride.frame = frame;
    ConfirmedInputBufferDriver::applyInputFrameToInputState(outFrame.state, playbackFrame.inputFrame);
    return true;
}

bool NetplayAppRuntime::shouldRecoverStandaloneInputWhileNetplayActive() const
{
    if(!m_coordinator.isActive()) {
        return false;
    }

    if(!m_coordinator.isConnected()) {
        return true;
    }

    const RoomState& room = m_coordinator.session().roomState();
    if(room.state == SessionState::Ended) {
        return true;
    }

    if(!m_coordinator.isHosting() || room.state != SessionState::Paused) {
        return false;
    }

    const ParticipantId localParticipantId = m_coordinator.localParticipantId();
    const bool hasConnectedRemotePlayer = std::any_of(
        room.participants.begin(),
        room.participants.end(),
        [localParticipantId](const ParticipantInfo& participant) {
            return participant.id != localParticipantId &&
                   participant.connected &&
                   !participantIsObserver(participant);
        }
    );
    return !hasConnectedRemotePlayer;
}

void NetplayAppRuntime::ensureStandaloneInputBootstrapFrame(GeraNESEmu& emu)
{
    syncEmuInputTimelineEpoch(emu);

    const uint32_t currentFrame = emu.frameCount();
    if(emu.inputBuffer().findByFrame(currentFrame, emu.inputTimelineEpoch()) != nullptr) {
        return;
    }

    InputFrame bootstrapFrame = emu.createInputFrame(currentFrame);
    (void)emu.queueInputFrame(bootstrapFrame);
}

bool NetplayAppRuntime::tryQueuePlaybackFrameToEmu(GeraNESEmu& emu, uint32_t frame)
{
    if(emu.inputBuffer().findByFrame(frame, emu.inputTimelineEpoch()) != nullptr) {
        return true;
    }

    NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
    if(!tryBuildPlaybackConfirmedFrame(frame, playbackFrame)) {
        return false;
    }

    InputFrame inputFrame = playbackFrame.inputFrame;
    inputFrame.speculative = playbackFrame.predicted;
    inputFrame.timelineEpoch = emu.inputTimelineEpoch();
    const InputBuffer::EnqueueResult enqueueResult = emu.queueInputFrame(inputFrame);
    return enqueueResult == InputBuffer::EnqueueResult::Inserted ||
           enqueueResult == InputBuffer::EnqueueResult::UpdatedPending;
}

void NetplayAppRuntime::recordPlaybackStop(FrameNumber frame)
{
    const FrameNumber confirmedThroughFrame = m_inputDriver.confirmedThroughFrame(m_coordinator);
    const FrameNumber predictedThroughFrame =
        confirmedThroughFrame + static_cast<FrameNumber>(m_inputDriver.predictFrames());
    const bool predictionLimitReached = frame > predictedThroughFrame;
    m_coordinator.recordPlaybackStop(frame, predictionLimitReached);
}

void NetplayAppRuntime::setLocalReconnectToken(uint64_t token)
{
    {
        std::scoped_lock stateLock(m_stateMutex);
        m_cachedReconnectToken = token;
        m_hasCachedReconnectToken = true;
    }
    enqueueCommand([token](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.setLocalReconnectToken(token);
    });
}

void NetplayAppRuntime::refreshLocalRomSelectionImmediate()
{
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_lastSelectedRomKey.clear();
        self.m_lastSubmittedValidationKey.clear();
    });
}

void NetplayAppRuntime::updateLatestRawMasks(const std::array<uint64_t, 4>& masks)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_latestRawMasks = masks;
}

void NetplayAppRuntime::updateLatestInputState(const IEmulationHost::InputState& inputState)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_latestInputState = inputState;
}

void NetplayAppRuntime::notifyWebVisibilityChanged(bool visible)
{
    m_emuHost.postCommand([this, visible](GeraNESEmu& emu) {
        notifyWebVisibilityChangedImmediate(emu, visible);
    });
}

void NetplayAppRuntime::notifyWebVisibilityChangedImmediate(GeraNESEmu& emu, bool visible)
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
        reanchorInputDriver(emu.frameCount(), localAssignedSlots());
    } else {
        m_inputDriver.reset();
    }
    m_emuHost.setSimulationSuspended(state == SessionState::Paused);
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
    snapshot.inputManaged = m_uiSnapshot.active && m_uiSnapshot.connected;
    snapshot.transportBackend = m_uiSnapshot.transportBackend;
    snapshot.port1Device = m_uiSnapshot.room.port1Device;
    snapshot.port2Device = m_uiSnapshot.room.port2Device;
    snapshot.expansionDevice = m_uiSnapshot.room.expansionDevice;
    snapshot.nesMultitapDevice = m_uiSnapshot.room.nesMultitapDevice;
    snapshot.famicomMultitapDevice = m_uiSnapshot.room.famicomMultitapDevice;
    if(snapshot.inputManaged) {
        for(const auto& participant : m_uiSnapshot.room.participants) {
            if(participant.id != m_uiSnapshot.localParticipantId) continue;
            snapshot.localAssignments = participant.controllerAssignments;
            break;
        }
    }
    return snapshot;
}

bool NetplayAppRuntime::runtimeActive() const
{
    return m_runtimeActive.load(std::memory_order_acquire);
}

bool NetplayAppRuntime::runtimeRunning() const
{
    return m_runtimeRunning.load(std::memory_order_acquire);
}

void NetplayAppRuntime::injectDropNextIncomingMessages(MessageType type, uint32_t count)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.dropNextIncomingMessages(type, count);
    });
}

void NetplayAppRuntime::clearIncomingMessageDrops()
{
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.clearIncomingMessageDrops();
    });
}

void NetplayAppRuntime::setReconnectReservationTimeoutForTests(uint32_t seconds)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
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
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_coordinator.simulateTransportFailureForTests();
        self.m_inputDriver.reset();
        self.m_runtimeLastTickTime = {};
        self.updateUiSnapshot(captureCurrentRomSelection(emu));
    });
}

void NetplayAppRuntime::setTransportBackend(NetTransportBackend backend)
{
    enqueueCommand([backend](NetplayAppRuntime& self, GeraNESEmu& emu) {
        const bool changed = self.m_coordinator.setTransportBackend(backend);
        if(!changed && self.m_coordinator.isActive()) {
            self.m_stickyStatusMessage = "Disconnect before changing the netplay backend.";
        } else if(changed) {
            self.m_stickyStatusMessage.clear();
        }
        self.updateUiSnapshot(captureCurrentRomSelection(emu));
    });
}

void NetplayAppRuntime::setTransportOptions(const NetTransportOptions& options)
{
    enqueueCommand([options](NetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_coordinator.setTransportOptions(options);
        self.updateUiSnapshot(captureCurrentRomSelection(emu));
    });
}

void NetplayAppRuntime::configureRollbackWindow(size_t snapshotCapacity)
{
    enqueueCommand([snapshotCapacity](NetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_emuHost.configureNetplaySnapshots(snapshotCapacity);
        self.updateUiSnapshot(captureCurrentRomSelection(emu));
    });
}

NetTransportOptions NetplayAppRuntime::transportOptions() const
{
    return m_coordinator.transportOptions();
}

NetTransportBackend NetplayAppRuntime::transportBackend() const
{
    std::scoped_lock stateLock(m_stateMutex);
    return m_uiSnapshot.transportBackend;
}

void NetplayAppRuntime::host(uint16_t port, size_t maxPeers, const std::string& displayName)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_stickyStatusMessage.clear();
        if(!emu.valid()) {
            self.m_stickyStatusMessage = "Load a ROM before creating a room.";
            return;
        }
        self.m_coordinator.host(port, maxPeers, displayName);
    });
}

void NetplayAppRuntime::join(const std::string& hostName, uint16_t port, const std::string& displayName)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu& emu) {
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

void NetplayAppRuntime::disconnect()
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
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_coordinator.disconnect();
        self.m_inputDriver.reset();
        self.ensureStandaloneInputBootstrapFrame(emu);
        self.m_runtimeLastTickTime = {};
        self.m_webVisibilityManagedPause = false;
        self.m_webPageVisible = true;
        self.m_lastSelectedRomKey.clear();
        self.m_lastSubmittedValidationKey.clear();
        self.m_lastSessionState.reset();
        self.m_lastLocalAssignedSlots.clear();
        self.m_lastAssignmentLayoutKey.clear();
        self.m_pendingManualStateResyncs.clear();
        self.updateUiSnapshot(captureCurrentRomSelection(emu));
    });
}

void NetplayAppRuntime::assignController(ParticipantId participantId, PlayerSlot slot)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.assignController(participantId, slot);
    });
}

void NetplayAppRuntime::addControllerAssignment(ParticipantId participantId, PlayerSlot slot)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.addControllerAssignment(participantId, slot);
    });
}

void NetplayAppRuntime::removeControllerAssignment(ParticipantId participantId, PlayerSlot slot)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.removeControllerAssignment(participantId, slot);
    });
}

void NetplayAppRuntime::clearControllerAssignments(ParticipantId participantId)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.clearControllerAssignments(participantId);
    });
}

void NetplayAppRuntime::configureInputAssignment(ParticipantId participantId,
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

void NetplayAppRuntime::configureInputAssignments(ParticipantId participantId,
                                                         std::optional<Settings::Device> port1Device,
                                                         std::optional<Settings::Device> port2Device,
                                                         Settings::ExpansionDevice expansionDevice,
                                                         Settings::NesMultitapDevice nesMultitapDevice,
                                                         Settings::FamicomMultitapDevice famicomMultitapDevice,
                                                         const std::vector<PlayerSlot>& slots)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu& emu) {
        const FrameNumber rebuildFromFrame = emu.frameCount() > 0 ? (emu.frameCount() - 1u) : 0u;
        const PlayerSlot preservedSlot = slots.empty() ? kObserverPlayerSlot : slots.front();
        self.m_coordinator.setLocalSimulationFrame(rebuildFromFrame);
        emu.setNesMultitapDevice(nesMultitapDevice);
        emu.setFamicomMultitapDevice(famicomMultitapDevice);
        emu.setPortDevice(Settings::Port::P_1, port1Device.value_or(Settings::Device::NONE));
        emu.setPortDevice(Settings::Port::P_2, port2Device.value_or(Settings::Device::NONE));
        emu.setExpansionDevice(expansionDevice);
        self.m_coordinator.setRoomInputTopology(
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
        self.reanchorInputDriver(rebuildFromFrame, self.localAssignedSlots());
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
            self.buildAuthoritativeStatePayload(emu, authoritativeFrame, false);
        self.beginAuthoritativeResync(emu, authoritativeFrame, statePayload, false);
    });
}

void NetplayAppRuntime::kickParticipant(ParticipantId participantId)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.kickParticipant(participantId);
    });
}

void NetplayAppRuntime::removeReconnectReservation(ParticipantId participantId)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.removeReconnectReservation(participantId);
    });
}

void NetplayAppRuntime::requestForceResync()
{
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu& emu) {
        if(!self.m_coordinator.isHosting()) return;
        const auto state = self.m_coordinator.session().roomState().state;
        if(!emu.valid()) return;
        if(state != SessionState::Running && state != SessionState::Paused) return;

        const FrameNumber requestedFrame = self.m_coordinator.session().roomState().lastConfirmedFrame;
        const FrameNumber authoritativeFrame =
            std::min<FrameNumber>(requestedFrame, emu.frameCount());
        const std::vector<uint8_t> statePayload =
            self.buildAuthoritativeStatePayload(emu, authoritativeFrame, true);
        self.beginAuthoritativeResync(emu, authoritativeFrame, statePayload, true);
    });
}

void NetplayAppRuntime::toggleHostedSessionPause()
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

    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu& emu) {
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

void NetplayAppRuntime::appendNetplayLog(const std::string& message)
{
    enqueueCommand([message](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.appendNetplayLog(message);
    });
}

void NetplayAppRuntime::shutdown()
{
    std::scoped_lock stateLock(m_stateMutex);
    m_pendingCommands.clear();
    m_runtimeActive.store(false, std::memory_order_release);
    m_runtimeRunning.store(false, std::memory_order_release);
    m_uiSnapshot = UiSnapshot{};
    m_webVisibilityManagedPause = false;
    m_webPageVisible = true;
    m_emuHost.setSimulationSuspended(false);
    m_coordinator.disconnect();
}

void NetplayAppRuntime::shutdownForUnload()
{
    std::scoped_lock stateLock(m_stateMutex);
    m_pendingCommands.clear();
    m_runtimeActive.store(false, std::memory_order_release);
    m_runtimeRunning.store(false, std::memory_order_release);
    m_uiSnapshot = UiSnapshot{};
    m_webVisibilityManagedPause = false;
    m_webPageVisible = true;
    m_emuHost.setSimulationSuspended(false);
    m_coordinator.shutdownForUnload();
}

void NetplayAppRuntime::runOnEmulationThread(GeraNESEmu& emu)
{
    drainPendingCommands(emu);

    if(m_hasCachedReconnectToken) {
        m_coordinator.setLocalReconnectToken(m_cachedReconnectToken);
    }

    syncInputDelayFromSettings(emu);

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
        m_runtimeLastTickTime = {};
        m_lastSelectedRomKey.clear();
        m_lastSubmittedValidationKey.clear();
        m_lastSessionState.reset();
        m_lastLocalAssignedSlots.clear();
        m_lastAssignmentLayoutKey.clear();
        m_pendingManualStateResyncs.clear();
        m_lastSubmittedLocalCrcFrame = 0;
        m_nextScheduledLocalCrcFrame = kDesyncCrcIntervalFrames;
        m_postRecoveryRapidCrcThroughFrame = 0;
        m_lastRollbackTargetFrame = 0;
        m_lastLoadedAuthoritativeFrame = 0;
        m_lastRecoveryReanchorFrame = 0;
        m_forceNextConfirmedCrcSubmission = false;
        m_waitingPostResyncDelayBuffer = false;
        m_waitingPostResyncDelayBufferSince = {};
        m_observerVisibilityResyncPending = false;
        m_webVisibilityManagedPause = false;
        m_webPageVisible = true;
        m_emuHost.setSimulationSuspended(false);
        updateUiSnapshot(captureCurrentRomSelection(emu));
        return;
    }

    m_runtimeActive.store(true, std::memory_order_release);
    const SessionState currentRoomState = m_coordinator.session().roomState().state;
    const bool standaloneInputRecovery = shouldRecoverStandaloneInputWhileNetplayActive();
    const bool netplayOwnsEmulationInput =
        !standaloneInputRecovery &&
        (currentRoomState == SessionState::Starting ||
         currentRoomState == SessionState::Running ||
         currentRoomState == SessionState::Resyncing ||
         currentRoomState == SessionState::Paused);
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
    m_coordinator.setRoomInputTopology(
        emu.getPortDevice(Settings::Port::P_1),
        emu.getPortDevice(Settings::Port::P_2),
        emu.getExpansionDevice(),
        emu.getNesMultitapDevice(),
        emu.getFamicomMultitapDevice()
    );
    syncRomValidation(localRom);
    syncEmuInputTimelineEpoch(emu);
    m_coordinator.setLocalSimulationFrame(emu.frameCount());
    m_coordinator.update(0);
    syncEmuInputTimelineEpoch(emu);
    handleSessionStateTransitionsOnWorker(emu);
    processAutoStartIfNeeded(emu, localRom);
    processHostManualStateChangeResyncIfNeeded(emu);
    processPendingManualStateResyncIfNeeded(emu);

    if(m_coordinator.isHosting() &&
       m_coordinator.session().roomState().state == SessionState::Starting &&
       m_coordinator.session().roomState().activeResyncId == 0) {
        beginInitialSessionSyncOnWorker(emu);
    }

    processHostResyncIfNeededOnWorker(emu);
    processHostLateJoinResyncIfNeededOnWorker(emu);
    processResyncIfNeededOnWorker(emu);
    processAutoResumeIfNeeded(localRom);
    processRollbackIfNeededOnWorker(emu);

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

    const std::vector<PlayerSlot> localSlots = localAssignedSlots();
    const std::string assignmentLayoutKey = buildAssignmentLayoutKey();
    if(!m_lastAssignmentLayoutKey.empty() && assignmentLayoutKey != m_lastAssignmentLayoutKey) {
        m_emuHost.discardQueuedNetplayInputsAfter(emu.frameCount());
        if(running) {
            reanchorInputDriver(emu.frameCount(), localSlots);
        } else {
            m_inputDriver.reset();
        }
    }
    m_lastAssignmentLayoutKey = assignmentLayoutKey;
    if(localSlots != m_lastLocalAssignedSlots) {
        if(running) {
            reanchorInputDriver(emu.frameCount(), localSlots);
        } else {
            m_inputDriver.reset();
        }
        m_lastLocalAssignedSlots = localSlots;
    }
    const uint32_t workerDtMs = consumeWorkerDtMs();

    m_inputDriver.produceLocalBufferedInputs(
        m_coordinator,
        m_coordinator.isActive(),
        false,
        m_coordinator.session().roomState().state,
        localSlots,
        workerDtMs,
        latestInputState,
        m_coordinator.session().roomState(),
        emu.getRegionFPS(),
        emu.frameCount(),
        m_inputDriver.confirmedThroughFrame(m_coordinator)
    );

    bool holdForPostResyncDelayBuffer = false;
    if(running && m_waitingPostResyncDelayBuffer) {
        constexpr auto kMaxPostResyncDelayBufferWait = std::chrono::milliseconds(1500);
        const FrameNumber confirmedThroughFrame = m_inputDriver.confirmedThroughFrame(m_coordinator);
        const FrameNumber requiredConfirmedFrame =
            emu.frameCount() + static_cast<FrameNumber>(m_inputDriver.prebufferFrames());
        holdForPostResyncDelayBuffer = confirmedThroughFrame < requiredConfirmedFrame;
        if(holdForPostResyncDelayBuffer && m_waitingPostResyncDelayBufferSince.time_since_epoch().count() != 0) {
            const auto waited = std::chrono::steady_clock::now() - m_waitingPostResyncDelayBufferSince;
            if(waited >= kMaxPostResyncDelayBufferWait) {
                holdForPostResyncDelayBuffer = false;
                m_waitingPostResyncDelayBuffer = false;
                m_waitingPostResyncDelayBufferSince = {};
                if(!m_observerVisibilityResyncPending) {
                    m_emuHost.setSimulationSuspended(false);
                }
                std::ostringstream warning;
                warning << "Post-resync delay buffer wait timed out after "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(waited).count()
                        << " ms; resuming simulation (confirmed="
                        << static_cast<unsigned>(confirmedThroughFrame)
                        << " required="
                        << static_cast<unsigned>(requiredConfirmedFrame)
                        << ")";
                m_coordinator.appendNetplayLog(warning.str());
            }
        }
        if(holdForPostResyncDelayBuffer) {
            m_emuHost.setSimulationSuspended(true);
        } else {
            m_waitingPostResyncDelayBuffer = false;
            m_waitingPostResyncDelayBufferSince = {};
            if(!m_observerVisibilityResyncPending) {
                m_emuHost.setSimulationSuspended(false);
            }
        }
    }

    if(running) {
        if(!holdForPostResyncDelayBuffer) {
            constexpr uint32_t kMaxContinuousClockCatchupFrames = 120u;
            (void)advanceToSharedClockIfNeededOnWorker(emu, kMaxContinuousClockCatchupFrames);

            m_inputDriver.preparePlaybackFramesForEmulationThread(
                m_coordinator,
                m_coordinator.isActive(),
                false,
                m_coordinator.session().roomState().state,
                emu.frameCount()
            );
            m_inputDriver.queuePendingFramesToEmu(emu);

            const uint32_t currentFrame = emu.frameCount();
            if(emu.inputBuffer().findByFrame(currentFrame, emu.inputTimelineEpoch()) == nullptr) {
                tryQueuePlaybackFrameToEmu(emu, currentFrame);
            }
        }
    }

    processPeriodicLocalCrcIfNeeded(emu);

    updateUiSnapshot(localRom);
}
} // namespace Netplay
