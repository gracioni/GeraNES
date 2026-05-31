#include "ConsoleNetplay/NetplayRuntimeSupport.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "ConsoleNetplay/NetplayCrc32.h"
#include "ConsoleNetplay/NetplayInputAssignment.h"

namespace ConsoleNetplay {

namespace {

std::string runtimeContentHashKey(const RomValidationData& validation)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for(uint8_t byte : validation.contentHash) {
        oss << std::setw(2) << static_cast<unsigned>(byte);
    }
    return oss.str();
}

std::optional<FrameNumber> runtimeClientHostPlaybackCapFrame(const NetplayCoordinator& coordinator)
{
    if(!coordinator.isActive() || coordinator.isHosting()) {
        return std::nullopt;
    }

    const RoomState& room = coordinator.session().roomState();
    return std::max(room.currentFrame, room.lastConfirmedFrame);
}

const char* runtimeResyncReasonLabel(ResyncReason reason)
{
    switch(reason) {
        case ResyncReason::InitialSessionSync: return "InitialSessionSync";
        case ResyncReason::ConfirmedDesync: return "ConfirmedDesync";
        case ResyncReason::HostStallRecovery: return "HostStallRecovery";
        case ResyncReason::ClientStallRecovery: return "ClientStallRecovery";
        case ResyncReason::AssignmentChanged: return "AssignmentChanged";
        case ResyncReason::ManualForce: return "ManualForce";
        case ResyncReason::HostReset: return "HostReset";
        case ResyncReason::HostLoadedState: return "HostLoadedState";
        case ResyncReason::ObserverVisibilityRestore: return "ObserverVisibilityRestore";
        default: return "Unspecified";
    }
}

uint32_t runtimeStateCrc32(const std::vector<uint8_t>& payload)
{
    return payload.empty() ? 0u : crc32(payload.data(), payload.size());
}

} // namespace

SelfStallDetector::Snapshot runtimeBuildSelfStallSnapshot(const NetplayCoordinator& coordinator,
                                                          FrameNumber localSimulationFrame);
size_t runtimeInputBufferCapacity(uint32_t prebufferFrames)
{
    const size_t horizon = static_cast<size_t>(prebufferFrames);
    const size_t capacity = (horizon * 4u) + 16u;
    return std::max<size_t>(64u, capacity);
}

RuntimeInputDelayResult runtimeSyncInputDelaySettings(NetplayCoordinator& coordinator,
                                                      ConfirmedInputBufferDriver& inputDriver,
                                                      NetplayAutoTune& autoTune,
                                                      const RuntimeInputDelaySettings& settings)
{
    coordinator.setDebugMode(settings.debugMode);
    coordinator.setGameplayReceiveDelayMs(settings.gameplayReceiveDelayMs);
    autoTune.setEnabled(settings.autoGameplayTuning);

    if(!coordinator.isActive()) {
        inputDriver.setPrebufferFrames(settings.manualInputDelayFrames);
        return {
            settings.manualInputDelayFrames,
            runtimeInputBufferCapacity(settings.manualInputDelayFrames)
        };
    }

    const RoomState& room = coordinator.session().roomState();
    if(coordinator.isHosting()) {
        if(settings.autoGameplayTuning) {
            const NetplayAutoTune::Recommendations recommendations = autoTune.update(
                room,
                coordinator.recoveryStats(),
                settings.regionFps
            );
            if(recommendations.inputDelayFrames.has_value() &&
               room.inputDelayFrames != *recommendations.inputDelayFrames) {
                coordinator.setInputDelayFrames(*recommendations.inputDelayFrames);
            }
        } else {
            const uint8_t manualDelay =
                static_cast<uint8_t>(std::min<uint32_t>(settings.manualInputDelayFrames, 0xffu));
            if(room.inputDelayFrames != manualDelay) {
                coordinator.setInputDelayFrames(manualDelay);
            }
        }
    }

    const RoomState& effectiveRoom = coordinator.session().roomState();
    inputDriver.setPrebufferFrames(static_cast<uint32_t>(effectiveRoom.inputDelayFrames));

    return {
        static_cast<uint32_t>(effectiveRoom.inputDelayFrames),
        runtimeInputBufferCapacity(effectiveRoom.inputDelayFrames)
    };
}

std::string runtimeRomKey(const std::optional<NetplayRomSelection>& selection)
{
    if(!selection.has_value() || !selection->loaded) return "none";

    const RomValidationData& v = selection->validation;
    return selection->gameName + "|" +
           std::to_string(v.romCrc32) + "|" +
           std::to_string(v.mapperId) + "|" +
           std::to_string(v.subMapperId) + "|" +
           std::to_string(v.prgRomSize) + "|" +
           std::to_string(v.chrRomSize) + "|" +
           std::to_string(v.chrRamSize) + "|" +
           std::to_string(v.fileSize) + "|" +
           runtimeContentHashKey(v);
}

std::string runtimeSessionBlockedReason(bool active,
                                        const RoomState& room,
                                        const std::optional<NetplayRomSelection>& localRom)
{
    if(!active) return "Netplay is not active.";

    if(!localRom.has_value() || !localRom->loaded) {
        return "Load a ROM locally to populate room validation.";
    }
    if(room.selectedGameName.empty()) {
        return "Load a ROM locally to populate room validation.";
    }

    bool anyMissingRom = false;
    bool anyIncompatibleRom = false;
    for(const ParticipantInfo& participant : room.participants) {
        if(participantIsObserver(participant)) continue;
        if(!participant.connected) continue;
        if(!participant.romLoaded) anyMissingRom = true;
        else if(!participant.romCompatible) anyIncompatibleRom = true;
    }

    if(anyMissingRom) return "Waiting for assigned participants to load the selected ROM.";
    if(anyIncompatibleRom) return "One or more assigned participants have an incompatible ROM.";
    return "";
}

RuntimeRomValidationResult runtimeSyncRomValidation(NetplayCoordinator& coordinator,
                                                    RuntimeRomValidationState& state,
                                                    const std::optional<NetplayRomSelection>& localRom)
{
    RuntimeRomValidationResult result;
    const std::string localRomKey = runtimeRomKey(localRom);

    if(!coordinator.isActive()) {
        state.lastSelectedRomKey.clear();
        state.lastSubmittedValidationKey.clear();
        result.stickyStatusMessage = state.stickyStatusMessage;
        return result;
    }

    if(coordinator.isHosting() && localRomKey != state.lastSelectedRomKey && localRom.has_value()) {
        coordinator.selectRom(localRom->gameName, localRom->validation);
        state.lastSelectedRomKey = localRomKey;
        state.lastSubmittedValidationKey.clear();
        result.selectedRomChanged = true;
    }

    const RoomState& room = coordinator.session().roomState();
    const bool hostRomSelected = !room.selectedGameName.empty();
    const bool romLoaded = localRom.has_value() && localRom->loaded;
    const bool romCompatible =
        hostRomSelected &&
        romLoaded &&
        NetplayCoordinator::romValidationMatches(localRom->validation, room.romValidation);

    const std::string validationKey =
        std::to_string(room.sessionId) + "|" +
        std::to_string(room.romValidation.romCrc32) + "|" +
        runtimeContentHashKey(room.romValidation) + "|" +
        localRomKey + "|" +
        (romLoaded ? "1" : "0") + "|" +
        (romCompatible ? "1" : "0");

    if(validationKey != state.lastSubmittedValidationKey) {
        coordinator.submitLocalRomValidation(
            romLoaded,
            romCompatible,
            localRom.has_value() ? localRom->validation : RomValidationData{}
        );
        state.lastSubmittedValidationKey = validationKey;
        result.submittedValidationChanged = true;
    }

    if(!coordinator.isHosting() &&
       !room.selectedGameName.empty() &&
       (!romLoaded || !romCompatible)) {
        std::ostringstream oss;
        oss << "Room requires ROM \"" << room.selectedGameName
            << "\" (CRC " << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
            << room.romValidation.romCrc32 << ").";
        state.stickyStatusMessage = oss.str();
        coordinator.disconnect();
        result.disconnectedForMismatch = true;
        result.stickyStatusMessage = state.stickyStatusMessage;
        return result;
    }

    if(!coordinator.isHosting() && romLoaded && romCompatible && !room.selectedGameName.empty()) {
        state.stickyStatusMessage.clear();
    }

    result.stickyStatusMessage = state.stickyStatusMessage;
    return result;
}

void runtimeSyncEmulatorInputTimelineEpoch(const NetplayCoordinator& coordinator,
                                           INetplayStateBridge& emu)
{
    const uint32_t timelineEpoch =
        coordinator.isActive() ? coordinator.session().roomState().timelineEpoch : 0u;
    if(emu.inputTimelineEpoch() != timelineEpoch) {
        emu.setInputTimelineEpoch(timelineEpoch);
    }
}

std::vector<uint8_t> runtimeBuildAuthoritativeStatePayload(INetplayStateBridge& emu,
                                                           const INetplayStateHostBridge& runtimeHost,
                                                           FrameNumber authoritativeFrame,
                                                           bool preferConfirmedSnapshot)
{
    if(preferConfirmedSnapshot) {
        if(const std::optional<std::shared_ptr<const std::vector<uint8_t>>> snapshot =
               runtimeHost.netplaySnapshotForFrame(authoritativeFrame);
           snapshot.has_value()) {
            return **snapshot;
        }
        if(!emu.valid() || emu.frameCount() != authoritativeFrame) {
            return {};
        }
    }

    return emu.saveStateToMemory();
}

uint32_t runtimeComputeAuthoritativeStateCrc32(INetplayStateBridge& emu,
                                               const INetplayStateHostBridge& runtimeHost,
                                               FrameNumber authoritativeFrame,
                                               bool preferConfirmedSnapshot)
{
    if(preferConfirmedSnapshot) {
        if(const std::optional<uint32_t> snapshotCrc32 =
               runtimeHost.netplaySnapshotCrc32ForFrame(authoritativeFrame);
           snapshotCrc32.has_value()) {
            return *snapshotCrc32;
        }
    }

    if(!preferConfirmedSnapshot &&
       emu.valid() &&
       emu.frameCount() == authoritativeFrame) {
        return runtimeStateCrc32(emu.saveStateToMemory());
    }

    return 0;
}

RuntimeAuthoritativeStateResult runtimeApplyAuthoritativeStateLocally(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    FrameNumber targetFrame,
    const std::vector<uint8_t>& payload)
{
    RuntimeAuthoritativeStateResult result;
    if(payload.empty()) return result;

    runtimeHost.beginPresentationHoldUntilNextFrameReady();
    if(!emu.loadStateFromMemoryOnCleanBoot(payload)) return result;

    const uint32_t loadedCrc32 = runtimeStateCrc32(emu.saveStateToMemory());
    emu.discardQueuedInputFramesAfter(targetFrame);
    runtimeSyncEmulatorInputTimelineEpoch(coordinator, emu);
    coordinator.setLocalSimulationFrame(targetFrame);
    runtimeHost.discardQueuedNetplayInputsAfter(targetFrame);
    runtimeHost.seedNetplaySnapshot(targetFrame, payload, loadedCrc32);
    runtimeHost.setAuthoritativeFrameReadyState(targetFrame, loadedCrc32);
    inputDriver.reanchor(targetFrame);

    result.started = true;
    result.localStateApplied = true;
    result.loadedAuthoritativeFrame = targetFrame;
    result.reanchorFrame = targetFrame;
    result.stateCrc32 = loadedCrc32;
    return result;
}

RuntimeAuthoritativeStateResult runtimeBeginAuthoritativeResync(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    FrameNumber authoritativeFrame,
    const std::vector<uint8_t>& statePayload,
    bool preferConfirmedSnapshot,
    ResyncReason reason,
    ParticipantId targetParticipantId)
{
    RuntimeAuthoritativeStateResult result;
    if(statePayload.empty()) return result;

    const uint32_t payloadCrc32 = crc32(statePayload.data(), statePayload.size());
    uint32_t stateCrc32 =
        runtimeComputeAuthoritativeStateCrc32(emu, runtimeHost, authoritativeFrame, preferConfirmedSnapshot);
    RuntimeAuthoritativeStateResult localStateResult;
    if(targetParticipantId == kInvalidParticipantId) {
        localStateResult = runtimeApplyAuthoritativeStateLocally(
            coordinator,
            inputDriver,
            emu,
            runtimeHost,
            authoritativeFrame,
            statePayload
        );
        if(!localStateResult.localStateApplied) {
            return result;
        }
        stateCrc32 = localStateResult.stateCrc32;
    } else if(!preferConfirmedSnapshot) {
        stateCrc32 = 0;
    }
    if(!coordinator.beginResync(
           authoritativeFrame,
           statePayload,
           payloadCrc32,
           stateCrc32,
           reason,
           targetParticipantId
       )) {
        return result;
    }

    result.started = true;
    result.stateCrc32 = stateCrc32;
    if(targetParticipantId == kInvalidParticipantId) {
        runtimeSyncEmulatorInputTimelineEpoch(coordinator, emu);
        coordinator.setLocalSimulationFrame(authoritativeFrame);
        inputDriver.reanchor(authoritativeFrame);
        result = localStateResult;
        result.started = true;
        result.stateCrc32 = stateCrc32;
    }
    return result;
}

RuntimeAuthoritativeStateResult runtimeBeginAuthoritativeResyncWithoutLocalReload(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    FrameNumber authoritativeFrame,
    const std::vector<uint8_t>& statePayload,
    bool preferConfirmedSnapshot,
    ResyncReason reason)
{
    RuntimeAuthoritativeStateResult result;
    if(statePayload.empty()) return result;

    const uint32_t payloadCrc32 = crc32(statePayload.data(), statePayload.size());
    const uint32_t stateCrc32 =
        runtimeComputeAuthoritativeStateCrc32(emu, runtimeHost, authoritativeFrame, preferConfirmedSnapshot);
    if(!coordinator.beginResync(authoritativeFrame, statePayload, payloadCrc32, stateCrc32, reason)) {
        return result;
    }

    runtimeSyncEmulatorInputTimelineEpoch(coordinator, emu);
    coordinator.invalidateLocalCrcHistoryAfter(authoritativeFrame);
    coordinator.setLocalSimulationFrame(authoritativeFrame);
    if(stateCrc32 != 0) {
        runtimeHost.seedNetplaySnapshot(authoritativeFrame, statePayload, stateCrc32);
        runtimeHost.setAuthoritativeFrameReadyState(authoritativeFrame, stateCrc32);
    } else {
        runtimeHost.seedNetplaySnapshot(authoritativeFrame, statePayload);
    }
    inputDriver.reanchor(authoritativeFrame);

    result.started = true;
    result.reanchorFrame = authoritativeFrame;
    result.stateCrc32 = stateCrc32;
    return result;
}

RuntimePeriodicCrcResult runtimeSubmitPeriodicLocalCrcIfNeeded(
    NetplayCoordinator& coordinator,
    INetplayStateBridge& emu,
    const INetplayStateHostBridge& runtimeHost,
    RuntimePeriodicCrcState& state)
{
    RuntimePeriodicCrcResult result;

    if(!kDesyncMonitorEnabled) return result;
    if(!coordinator.isActive()) return result;
    if(coordinator.session().roomState().state != SessionState::Running) return result;
    if(!emu.valid()) return result;

    const FrameNumber confirmedFrame = coordinator.latestConfirmedFrame();
    const FrameNumber lastFrameReadyFrame = runtimeHost.lastFrameReadyFrame();
    // CRC comparison is only meaningful once the local emulator has fully
    // caught up to the currently confirmed frontier. When one peer has already
    // confirmed future inputs but is still frame-ready on an older frame, the
    // cached "frame-ready" CRC for that older frame can diverge transiently
    // from a peer that is only confirmed through the older frame. Wait until
    // confirmed and frame-ready have converged before submitting periodic CRCs.
    const FrameNumber crcCheckpointFrame =
        confirmedFrame != 0u && confirmedFrame == lastFrameReadyFrame
            ? confirmedFrame
            : 0u;
    if(crcCheckpointFrame == 0) return result;

    const bool periodicDue = crcCheckpointFrame >= state.nextScheduledLocalCrcFrame;
    const bool postRecoveryRapidDue =
        crcCheckpointFrame > state.lastSubmittedLocalCrcFrame &&
        crcCheckpointFrame <= state.postRecoveryRapidCrcThroughFrame;
    const bool forcedDue =
        state.forceNextConfirmedCrcSubmission &&
        crcCheckpointFrame != state.lastSubmittedLocalCrcFrame;
    if(!periodicDue && !forcedDue && !postRecoveryRapidDue) return result;

    // Periodic desync checks compare only the live canonical state at the
    // confirmed checkpoint. Cached per-frame snapshots can be stale relative to
    // the currently loaded simulation state and are not trusted for resync.
    std::optional<uint32_t> crc32;
    const char* submittedSource = nullptr;
    CrcSubmissionSource submittedSourceKind = CrcSubmissionSource::Unknown;
    if(emu.frameCount() == crcCheckpointFrame) {
        crc32 = runtimeStateCrc32(emu.saveStateToMemory());
        submittedSource = "local CRC submission (live-canonical)";
        submittedSourceKind = CrcSubmissionSource::LiveCanonical;
    }
    if(!crc32.has_value()) return result;

    coordinator.submitLocalCrc(
        crcCheckpointFrame,
        *crc32,
        submittedSource,
        submittedSourceKind,
        emu.frameCount(),
        confirmedFrame
    );
    state.lastSubmittedLocalCrcFrame = crcCheckpointFrame;
    state.forceNextConfirmedCrcSubmission = false;
    if(crcCheckpointFrame >= state.postRecoveryRapidCrcThroughFrame) {
        state.postRecoveryRapidCrcThroughFrame = 0;
    }
    state.nextScheduledLocalCrcFrame =
        ((crcCheckpointFrame / kDesyncCrcIntervalFrames) + 1u) * kDesyncCrcIntervalFrames;

    result.submitted = true;
    result.submittedFrame = crcCheckpointFrame;
    result.submittedCrc32 = *crc32;
    result.submittedSource = submittedSource;
    return result;
}

std::string runtimeAssignmentLayoutKey(const NetplayCoordinator& coordinator)
{
    if(!coordinator.isActive()) return {};

    std::string key;
    for(const ParticipantInfo& participant : coordinator.session().roomState().participants) {
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

RuntimeAutoStartResult runtimeProcessAutoStartIfNeeded(NetplayCoordinator& coordinator,
                                                       const INetplayStateBridge& emu,
                                                       const std::optional<NetplayRomSelection>& localRom)
{
    RuntimeAutoStartResult result;
    if(!coordinator.isActive() || !coordinator.isHosting()) return result;
    if(!localRom.has_value() || !localRom->loaded) return result;

    const RoomState& room = coordinator.session().roomState();
    if(room.selectedGameName.empty()) return result;
    if(room.state == SessionState::Starting ||
       room.state == SessionState::Running ||
       room.state == SessionState::Resyncing ||
       room.state == SessionState::Paused ||
       room.state == SessionState::Ended) {
        return result;
    }
    if(!runtimeSessionBlockedReason(coordinator.isActive(), room, localRom).empty()) return result;

    coordinator.setLocalSimulationFrame(emu.frameCount());
    result.started = coordinator.startSession();
    result.initialSyncNeeded = result.started && emu.frameCount() > 0;
    return result;
}

RuntimeHostResyncProcessResult runtimeBeginInitialSessionSyncIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost)
{
    RuntimeHostResyncProcessResult result;
    if(!coordinator.isHosting()) return result;
    if(!emu.valid()) return result;
    if(coordinator.session().roomState().state != SessionState::Starting) return result;

    const FrameNumber authoritativeFrame = emu.frameCount();
    const std::vector<uint8_t> statePayload =
        runtimeBuildAuthoritativeStatePayload(emu, runtimeHost, authoritativeFrame, false);
    const RuntimeAuthoritativeStateResult stateResult =
        runtimeBeginAuthoritativeResync(
            coordinator,
            inputDriver,
            emu,
            runtimeHost,
            authoritativeFrame,
            statePayload,
            false
        );
    if(!stateResult.started) return result;

    coordinator.appendNetplayLog("Netplay initial session sync started");
    result.started = true;
    result.localStateApplied = stateResult.localStateApplied;
    result.initialSessionSync = true;
    result.authoritativeFrame = authoritativeFrame;
    result.loadedAuthoritativeFrame = stateResult.loadedAuthoritativeFrame;
    result.reanchorFrame = stateResult.reanchorFrame;
    result.reason = ResyncReason::InitialSessionSync;
    return result;
}

RuntimeHostResyncProcessResult runtimeProcessHostResyncIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    NetplayAutoTune& autoTune,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    bool autoGameplayTuning)
{
    RuntimeHostResyncProcessResult processResult;
    if(!coordinator.isHosting()) return processResult;

    std::optional<NetplayCoordinator::PendingHostResyncRequest> pending =
        coordinator.consumePendingHostResyncFrame();
    if(!pending.has_value()) return processResult;
    if(!emu.valid()) return processResult;

    const bool initialSessionSync =
        coordinator.session().roomState().state == SessionState::Starting;

    const FrameNumber requestedFrame =
        initialSessionSync ? emu.frameCount() : pending->frame;
    FrameNumber authoritativeFrame =
        std::min<FrameNumber>(requestedFrame, emu.frameCount());
    bool preferConfirmedSnapshot = !initialSessionSync;

    std::vector<uint8_t> statePayload =
        runtimeBuildAuthoritativeStatePayload(emu, runtimeHost, authoritativeFrame, preferConfirmedSnapshot);
    if(statePayload.empty() && preferConfirmedSnapshot && authoritativeFrame != emu.frameCount()) {
        coordinator.appendNetplayLog(
            "Netplay authoritative snapshot unavailable at frame " +
            std::to_string(authoritativeFrame) +
            "; using current host frame " +
            std::to_string(emu.frameCount()) +
            " for resync"
        );
        authoritativeFrame = emu.frameCount();
        preferConfirmedSnapshot = false;
        statePayload =
            runtimeBuildAuthoritativeStatePayload(emu, runtimeHost, authoritativeFrame, preferConfirmedSnapshot);
    }
    if(statePayload.empty()) return processResult;

    const ResyncReason reason =
        initialSessionSync ? ResyncReason::InitialSessionSync : pending->reason;
    if(!initialSessionSync && autoGameplayTuning) {
        const NetplayAutoTune::Recommendations tuningRecommendations =
            autoTune.recommendForImpendingResync(coordinator.session().roomState(), reason);
        if(tuningRecommendations.inputDelayFrames.has_value() &&
           coordinator.session().roomState().inputDelayFrames != *tuningRecommendations.inputDelayFrames) {
            coordinator.setInputDelayFrames(*tuningRecommendations.inputDelayFrames);
        }
    }

    const RuntimeAuthoritativeStateResult stateResult =
        runtimeBeginAuthoritativeResync(
            coordinator,
            inputDriver,
            emu,
            runtimeHost,
            authoritativeFrame,
            statePayload,
            preferConfirmedSnapshot,
            reason,
            pending->participantId
        );
    if(!stateResult.started) return processResult;

    if(initialSessionSync) {
        coordinator.appendNetplayLog("Netplay initial session sync started");
    } else {
        coordinator.appendNetplayLog(
            "Netplay hard resync started after reason " + std::string(runtimeResyncReasonLabel(reason)) +
            " at frame " + std::to_string(pending->frame) +
            ", using authoritative frame " + std::to_string(authoritativeFrame)
        );
    }

    processResult.started = true;
    processResult.localStateApplied = stateResult.localStateApplied;
    processResult.initialSessionSync = initialSessionSync;
    processResult.requestedFrame = pending->frame;
    processResult.authoritativeFrame = authoritativeFrame;
    processResult.loadedAuthoritativeFrame = stateResult.loadedAuthoritativeFrame;
    processResult.reanchorFrame = stateResult.reanchorFrame;
    processResult.reason = reason;
    processResult.targetParticipantId = pending->participantId;
    return processResult;
}

RuntimeHostResyncProcessResult runtimeProcessHostLateJoinResyncIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost)
{
    RuntimeHostResyncProcessResult processResult;
    if(!coordinator.isHosting()) return processResult;

    std::optional<ParticipantId> participantId =
        coordinator.consumePendingHostLateJoinResyncParticipant();
    if(!participantId.has_value()) return processResult;
    if(!emu.valid()) return processResult;

    const ParticipantInfo* participant =
        coordinator.session().findParticipant(*participantId);
    const bool reconnectResume =
        participant != nullptr && participant->inputResumeAwaitingResync;

    FrameNumber authoritativeFrame = emu.frameCount();
    bool preferConfirmedSnapshot = false;
    if(!reconnectResume) {
        const FrameNumber hostConfirmedFrame =
            std::max(
                coordinator.session().roomState().lastConfirmedFrame,
                coordinator.hostConfirmedFrame()
            );
        authoritativeFrame =
            std::min<FrameNumber>(hostConfirmedFrame, emu.frameCount());
        preferConfirmedSnapshot = true;
    }

    std::vector<uint8_t> statePayload =
        runtimeBuildAuthoritativeStatePayload(emu, runtimeHost, authoritativeFrame, preferConfirmedSnapshot);
    if(statePayload.empty() && authoritativeFrame != emu.frameCount()) {
        coordinator.appendNetplayLog(
            "Netplay late-join snapshot unavailable at frame " +
            std::to_string(authoritativeFrame) +
            "; using current host frame " +
            std::to_string(emu.frameCount())
        );
        authoritativeFrame = emu.frameCount();
        preferConfirmedSnapshot = false;
        statePayload =
            runtimeBuildAuthoritativeStatePayload(emu, runtimeHost, authoritativeFrame, preferConfirmedSnapshot);
    }
    if(statePayload.empty()) return processResult;

    const RuntimeAuthoritativeStateResult stateResult =
        runtimeBeginAuthoritativeResync(
            coordinator,
            inputDriver,
            emu,
            runtimeHost,
            authoritativeFrame,
            statePayload,
            preferConfirmedSnapshot,
            ResyncReason::InitialSessionSync,
            *participantId
        );
    if(!stateResult.started) return processResult;

    coordinator.appendNetplayLog(
        "Netplay late-join resync started for participant " +
        std::to_string(static_cast<int>(*participantId))
    );

    processResult.started = true;
    processResult.localStateApplied = stateResult.localStateApplied;
    processResult.lateJoinSync = true;
    processResult.authoritativeFrame = authoritativeFrame;
    processResult.loadedAuthoritativeFrame = stateResult.loadedAuthoritativeFrame;
    processResult.reanchorFrame = stateResult.reanchorFrame;
    processResult.reason = ResyncReason::InitialSessionSync;
    processResult.targetParticipantId = *participantId;
    return processResult;
}

RuntimeHostResyncProcessResult runtimeProcessSelfStallRecoveryIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    SelfStallDetector& selfStallDetector,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    std::chrono::steady_clock::time_point now)
{
    RuntimeHostResyncProcessResult processResult;
    const RoomState& room = coordinator.session().roomState();
    const SelfStallDetector::Snapshot snapshot =
        runtimeBuildSelfStallSnapshot(coordinator, emu.frameCount());

    const SelfStallDetector::UpdateResult update =
        selfStallDetector.update(snapshot, now);
    if(!update.shouldResync) {
        return processResult;
    }

    if(coordinator.isHosting()) {
        coordinator.appendNetplayLog(
            "Self stall detector triggered authoritative resync " + update.detail
        );
        const FrameNumber authoritativeFrame = emu.frameCount();
        const std::vector<uint8_t> statePayload =
            runtimeBuildAuthoritativeStatePayload(emu, runtimeHost, authoritativeFrame, false);
        const RuntimeAuthoritativeStateResult stateResult =
            runtimeBeginAuthoritativeResync(
                coordinator,
                inputDriver,
                emu,
                runtimeHost,
                authoritativeFrame,
                statePayload,
                false,
                ResyncReason::HostStallRecovery
            );
        if(!stateResult.started) return processResult;

        processResult.started = true;
        processResult.localStateApplied = stateResult.localStateApplied;
        processResult.authoritativeFrame = authoritativeFrame;
        processResult.loadedAuthoritativeFrame = stateResult.loadedAuthoritativeFrame;
        processResult.reanchorFrame = stateResult.reanchorFrame;
        processResult.reason = ResyncReason::HostStallRecovery;
        return processResult;
    }

    ResyncRequestData request;
    request.reason = ResyncReason::ClientStallRecovery;
    request.localFrame = emu.frameCount();
    request.estimatedHostFrame = room.currentFrame;
    request.confirmedThroughFrame = inputDriver.confirmedThroughFrame(coordinator);
    request.lagFrames =
        static_cast<uint16_t>(
            std::min<FrameNumber>(
                room.currentFrame > request.localFrame ? (room.currentFrame - request.localFrame) : 0u,
                0xffffu
            )
        );
    request.catchupBudgetFrames = room.inputDelayFrames;
    request.source = 3u; // self stall detector

    if(coordinator.requestHostResync(request)) {
        coordinator.appendNetplayLog(
            "Self stall detector requested host resync " + update.detail
        );
        processResult.started = true;
        processResult.requestedFrame = request.localFrame;
        processResult.reason = ResyncReason::ClientStallRecovery;
    }
    return processResult;
}

RuntimePendingResyncApplyResult runtimeProcessPendingResyncApplyIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost)
{
    RuntimePendingResyncApplyResult result;
    std::optional<NetplayCoordinator::PendingResyncApply> pending =
        coordinator.consumePendingResyncApply();
    if(!pending.has_value()) return result;

    result.consumed = true;
    result.resyncId = pending->resyncId;
    result.targetFrame = pending->targetFrame;

    runtimeHost.beginPresentationHoldUntilNextFrameReady();
    const bool loaded = emu.loadStateFromMemoryOnCleanBoot(pending->payload);
    const FrameNumber loadedFrame = emu.frameCount();
    const bool loadedExpectedFrame =
        loaded &&
        (pending->targetFrame == 0u || loadedFrame == pending->targetFrame);
    const uint32_t loadedCrc32 = loadedExpectedFrame ? runtimeStateCrc32(emu.saveStateToMemory()) : 0u;

    result.loadedExpectedFrame = loadedExpectedFrame;
    result.loadedFrame = loadedFrame;
    result.loadedCrc32 = loadedCrc32;

    if(loadedExpectedFrame) {
        emu.discardQueuedInputFramesAfter(pending->targetFrame);
        runtimeSyncEmulatorInputTimelineEpoch(coordinator, emu);
        coordinator.setLocalSimulationFrame(pending->targetFrame);
        runtimeHost.discardQueuedNetplayInputsAfter(pending->targetFrame);
        runtimeHost.seedNetplaySnapshot(pending->targetFrame, pending->payload, loadedCrc32);
        runtimeHost.setAuthoritativeFrameReadyState(
            pending->frameReadyFrame != 0u ? pending->frameReadyFrame : pending->targetFrame,
            pending->frameReadyCrc32 != 0u ? pending->frameReadyCrc32 : loadedCrc32
        );
        inputDriver.reanchor(pending->targetFrame);
        result.loadedAuthoritativeFrame = pending->targetFrame;
        result.reanchorFrame = pending->targetFrame;

        std::ostringstream oss;
        oss << "Netplay resync post-load validation accepted"
            << " targetFrame " << pending->targetFrame
            << " loadedCrc32 " << loadedCrc32
            << " frameReadyFrame "
            << (pending->frameReadyFrame != 0u ? pending->frameReadyFrame : pending->targetFrame);
        coordinator.appendNetplayLog(oss.str());
    }

    coordinator.acknowledgeResync(pending->resyncId, pending->targetFrame, loadedCrc32, loadedExpectedFrame);

    if(!loadedExpectedFrame) {
        if(loaded && loadedFrame != pending->targetFrame) {
            std::ostringstream oss;
            oss << "Netplay resync load frame mismatch: expected "
                << pending->targetFrame
                << ", got "
                << loadedFrame
                << " after clean-boot state load";
            coordinator.appendNetplayLog(oss.str());
        }
        coordinator.appendNetplayLog("Netplay resync post-load validation rejected");
        coordinator.appendNetplayLog("Netplay resync failed");
    }

    return result;
}

RuntimeManualStateResyncProcessResult runtimeProcessHostManualStateChangesIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    std::deque<RuntimePendingManualStateResync>& pendingManualStateResyncs,
    const std::vector<NetplayManualStateChangeRecord>& events)
{
    RuntimeManualStateResyncProcessResult processResult;
    if(events.empty()) return processResult;

    for(const NetplayManualStateChangeRecord& event : events) {
        if(!coordinator.isHosting()) continue;

        const RoomState& room = coordinator.session().roomState();
        const SessionState state = room.state;
        if(state != SessionState::Running &&
           state != SessionState::Paused &&
           state != SessionState::Resyncing) {
            continue;
        }
        if(!emu.valid()) continue;

        const ResyncReason reason =
            event.kind == NetplayManualStateChangeKind::Reset
                ? ResyncReason::HostReset
                : ResyncReason::HostLoadedState;
        {
            std::ostringstream oss;
            oss << "Owner manual state change detected"
                << " reason " << NetplayCoordinator::resyncReasonToast(reason)
                << " eventFrame " << event.frame
                << " emuFrame " << emu.frameCount()
                << " roomEpoch " << room.timelineEpoch;
            coordinator.appendNetplayLog(oss.str());
        }
        const std::string toast = NetplayCoordinator::resyncReasonToast(reason);
        if(!toast.empty()) {
            coordinator.appendNetplayLog(toast);
        }

        const FrameNumber eventFrame = std::min<FrameNumber>(event.frame, emu.frameCount());
        const bool resyncBusy =
            state == SessionState::Resyncing ||
            room.activeResyncId != 0 ||
            room.pendingResyncAckCount != 0;
        emu.discardQueuedInputFramesAfter(eventFrame);
        runtimeSyncEmulatorInputTimelineEpoch(coordinator, emu);
        inputDriver.reanchor(eventFrame);
        processResult.reanchorFrame = eventFrame;

        const bool hasRemotePeers = std::any_of(
            room.participants.begin(),
            room.participants.end(),
            [&coordinator](const ParticipantInfo& participant) {
                return participant.id != coordinator.localParticipantId();
            }
        );
        if(!hasRemotePeers) {
            continue;
        }

        if(resyncBusy) {
            coordinator.appendNetplayLog(
                "Deferring manual host recovery until the active resync/bootstrap finishes"
            );
            pendingManualStateResyncs.clear();
            pendingManualStateResyncs.push_back(RuntimePendingManualStateResync{
                reason,
                eventFrame,
                event.kind == NetplayManualStateChangeKind::Reset
            });
            processResult.deferredResync = true;
            continue;
        }

        coordinator.discardTimelineAfter(eventFrame);
        coordinator.invalidateLocalCrcHistoryAfter(eventFrame);
        coordinator.setLocalSimulationFrame(eventFrame);

        if(event.kind == NetplayManualStateChangeKind::LoadState) {
            const std::vector<uint8_t> statePayload =
                runtimeBuildAuthoritativeStatePayload(emu, runtimeHost, eventFrame, false);
            if(statePayload.empty()) {
                continue;
            }

            pendingManualStateResyncs.clear();
            const RuntimeAuthoritativeStateResult stateResult =
                runtimeBeginAuthoritativeResync(
                    coordinator,
                    inputDriver,
                    emu,
                    runtimeHost,
                    eventFrame,
                    statePayload,
                    false,
                    reason
                );
            if(stateResult.started) {
                processResult.startedResync = true;
                processResult.loadedAuthoritativeFrame = stateResult.loadedAuthoritativeFrame;
                processResult.reanchorFrame = stateResult.reanchorFrame != 0
                    ? stateResult.reanchorFrame
                    : processResult.reanchorFrame;
            }
            continue;
        }

        pendingManualStateResyncs.clear();
        pendingManualStateResyncs.push_back(RuntimePendingManualStateResync{reason, eventFrame, true});
        processResult.deferredResync = true;
    }

    return processResult;
}

RuntimeManualStateResyncProcessResult runtimeProcessPendingManualStateResyncIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayStateBridge& emu,
    INetplayStateHostBridge& runtimeHost,
    std::deque<RuntimePendingManualStateResync>& pendingManualStateResyncs)
{
    RuntimeManualStateResyncProcessResult processResult;
    if(pendingManualStateResyncs.empty()) return processResult;
    if(!coordinator.isHosting()) {
        pendingManualStateResyncs.clear();
        return processResult;
    }

    const SessionState state = coordinator.session().roomState().state;
    if(state != SessionState::Running && state != SessionState::Paused) {
        return processResult;
    }

    while(!pendingManualStateResyncs.empty()) {
        const RuntimePendingManualStateResync pending = pendingManualStateResyncs.front();
        if(pending.waitForAdvance && emu.frameCount() <= pending.eventFrame) {
            return processResult;
        }

        const FrameNumber authoritativeFrame = emu.frameCount();
        const std::vector<uint8_t> statePayload =
            runtimeBuildAuthoritativeStatePayload(emu, runtimeHost, authoritativeFrame, false);
        if(statePayload.empty()) {
            return processResult;
        }

        const RuntimeAuthoritativeStateResult stateResult =
            runtimeBeginAuthoritativeResync(
                coordinator,
                inputDriver,
                emu,
                runtimeHost,
                authoritativeFrame,
                statePayload,
                false,
                pending.reason
            );
        if(!stateResult.started) {
            return processResult;
        }

        std::ostringstream oss;
        oss << "Applying deferred manual host recovery"
            << " reason " << NetplayCoordinator::resyncReasonToast(pending.reason)
            << " authoritativeFrame " << authoritativeFrame;
        coordinator.appendNetplayLog(oss.str());
        pendingManualStateResyncs.pop_front();
        processResult.startedResync = true;
        processResult.loadedAuthoritativeFrame = stateResult.loadedAuthoritativeFrame;
        processResult.reanchorFrame = stateResult.reanchorFrame;
    }

    return processResult;
}

RuntimeSessionTransitionResult runtimeHandleSessionStateTransitions(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayRuntimeSessionControls& controls,
    RuntimeSessionTransitionState& sessionState,
    RuntimePeriodicCrcState& periodicCrcState,
    RuntimeRecoveryProcessState& recoveryState,
    RuntimeSharedClockCatchupState& sharedClockState,
    FrameNumber& lastLoadedAuthoritativeFrame)
{
    RuntimeSessionTransitionResult result;
    if(!coordinator.isActive()) {
        sessionState.lastSessionState.reset();
        inputDriver.reset();
        periodicCrcState = RuntimePeriodicCrcState{};
        recoveryState = RuntimeRecoveryProcessState{};
        sharedClockState = RuntimeSharedClockCatchupState{};
        lastLoadedAuthoritativeFrame = 0;
        result.inactive = true;
        result.resetWorkerTick = true;
        return result;
    }

    const SessionState currentState = coordinator.session().roomState().state;
    const std::optional<SessionState> previousState = sessionState.lastSessionState;
    if(previousState.has_value() && *previousState == currentState) {
        return result;
    }

    const bool enteringResync = currentState == SessionState::Resyncing;
    const bool leavingResync =
        previousState.has_value() &&
        *previousState == SessionState::Resyncing &&
        currentState != SessionState::Resyncing;
    if(enteringResync || leavingResync) {
        controls.restartAudio();
    }
    if(enteringResync) {
        const FrameNumber discardAfterFrame =
            coordinator.session().roomState().resyncTargetFrame != 0u
                ? coordinator.session().roomState().resyncTargetFrame
                : controls.frameCount();
        controls.discardQueuedNetplayInputsAfter(discardAfterFrame);
    }

    if(currentState != SessionState::Running) {
        inputDriver.reset();
        result.resetWorkerTick = true;
    }

    if(currentState == SessionState::Paused) {
        controls.setSimulationSuspended(true);
        if(!previousState.has_value() || *previousState != SessionState::Paused) {
            controls.discardQueuedAudio();
        }
    }

    if(previousState.has_value() &&
       *previousState == SessionState::Paused &&
       currentState != SessionState::Paused &&
       !sessionState.observerVisibilityResyncPending) {
        controls.setSimulationSuspended(false);
    }

    if(currentState == SessionState::Running &&
       previousState.has_value() &&
       (*previousState == SessionState::Starting || *previousState == SessionState::Resyncing)) {
        const FrameNumber anchorFrame = coordinator.session().roomState().lastConfirmedFrame;
        inputDriver.reanchor(anchorFrame);
        recoveryState.lastRecoveryReanchorFrame = anchorFrame;
        periodicCrcState.postRecoveryRapidCrcThroughFrame = anchorFrame + 3u;
        if(sessionState.observerVisibilityResyncPending) {
            sessionState.observerVisibilityResyncPending = false;
            controls.setSimulationSuspended(false);
        }
    }

    if(currentState == SessionState::Running &&
       (!previousState.has_value() || *previousState != SessionState::Running)) {
        periodicCrcState.forceNextConfirmedCrcSubmission = true;
    }

    sessionState.lastSessionState = currentState;
    return result;
}

uint32_t runtimeAdvanceToSharedClockIfNeeded(
    NetplayCoordinator& coordinator,
    ConfirmedInputBufferDriver& inputDriver,
    INetplayConsole& console,
    RuntimeSharedClockCatchupState& state,
    uint32_t maxFrames,
    bool requireLagTrigger)
{
    if(maxFrames == 0u) return 0u;
    if(coordinator.isHosting()) return 0u;
    if(!coordinator.isActive() || coordinator.session().roomState().state != SessionState::Running) return 0u;
    if(!console.valid()) return 0u;

    const uint32_t frameDt = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, console.regionFps()));
    const uint64_t frameDtMicros =
        std::max<uint64_t>(1u, 1000000ull / std::max<uint64_t>(1u, static_cast<uint64_t>(console.regionFps())));
    const FrameNumber confirmedThroughFrame = inputDriver.confirmedThroughFrame(coordinator);
    bool lagTriggerActivated = false;
    FrameNumber estimatedHostFrameFromClock = 0u;
    FrameNumber estimatedLagFrames = 0u;

    constexpr FrameNumber kContinuousCatchupLagTriggerFrames = 2u;
    const RoomState& room = coordinator.session().roomState();
    if(state.resyncRequestPending && room.timelineEpoch != state.resyncRequestEpoch) {
        state.resyncRequestPending = false;
        state.lagOverBudgetSince = {};
        state.lagOverBudgetSinceFrame = 0;
    }
    if(room.lastAuthoritativeClockFrame != 0u &&
       room.lastAuthoritativeClockMicros != 0u &&
       room.sharedClockSynchronized) {
        const uint64_t nowSharedClockMicros = coordinator.sharedClockNowMicros();
        if(nowSharedClockMicros != 0u && nowSharedClockMicros > room.lastAuthoritativeClockMicros) {
            const uint64_t elapsedSinceAuthoritativeMicros =
                nowSharedClockMicros - room.lastAuthoritativeClockMicros;
            estimatedHostFrameFromClock =
                room.lastAuthoritativeClockFrame +
                static_cast<FrameNumber>(elapsedSinceAuthoritativeMicros / frameDtMicros);
            const FrameNumber localFrame = console.frameCount();
            estimatedLagFrames =
                estimatedHostFrameFromClock > localFrame ? (estimatedHostFrameFromClock - localFrame) : 0u;
        }
    }

    if(requireLagTrigger) {
        if(estimatedLagFrames < kContinuousCatchupLagTriggerFrames) return 0u;
        lagTriggerActivated = true;
        if(console.frameCount() >= confirmedThroughFrame) return 0u;
    }

    if(estimatedLagFrames > maxFrames) {
        constexpr auto kLargeLagPersistence = std::chrono::milliseconds(250);
        constexpr auto kSharedClockResyncRequestCooldown = std::chrono::milliseconds(1500);
        const auto now = std::chrono::steady_clock::now();
        const FrameNumber localFrame = console.frameCount();
        const FrameNumber confirmedLagFrames =
            confirmedThroughFrame > localFrame ? (confirmedThroughFrame - localFrame) : 0u;

        if(state.lagOverBudgetSince.time_since_epoch().count() == 0 ||
           estimatedLagFrames <= maxFrames) {
            state.lagOverBudgetSince = now;
            state.lagOverBudgetSinceFrame = localFrame;
        }

        if(confirmedLagFrames <= maxFrames) {
            state.lagOverBudgetSince = {};
            state.lagOverBudgetSinceFrame = 0;
            if(localFrame >= state.lastConfirmedLagWaitLogFrame + 300u) {
                state.lastConfirmedLagWaitLogFrame = localFrame;
                std::ostringstream oss;
                oss << "Netplay shared-clock catchup over budget but confirmed input is not far enough ahead"
                    << " lag " << estimatedLagFrames
                    << " confirmedLag " << confirmedLagFrames
                    << " budget " << maxFrames
                    << "; waiting instead of requesting resync";
                coordinator.appendNetplayLog(oss.str());
            }
            return 0u;
        }

        const bool lagPersisted = now - state.lagOverBudgetSince >= kLargeLagPersistence;
        if(!lagPersisted) return 0u;

        if(state.lastResyncRequestAt.time_since_epoch().count() != 0 &&
           now - state.lastResyncRequestAt < kSharedClockResyncRequestCooldown) {
            return 0u;
        }
        state.resyncRequestPending = false;

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

        if(coordinator.requestHostResync(request)) {
            state.resyncRequestPending = true;
            state.resyncRequestEpoch = room.timelineEpoch;
            state.lastResyncRequestAt = now;
            state.lastResyncRequestFrame = localFrame;
            std::ostringstream oss;
            oss << "Netplay shared-clock catchup skipped; lag "
                << estimatedLagFrames
                << " frame(s) exceeds catchup budget "
                << maxFrames
                << " since local frame "
                << state.lagOverBudgetSinceFrame
                << ", requested authoritative resync";
            coordinator.appendNetplayLog(oss.str());
        }
        return 0u;
    }

    state.lagOverBudgetSince = {};
    state.lagOverBudgetSinceFrame = 0;

    uint32_t advancedFrames = 0u;
    while(advancedFrames < maxFrames) {
        const FrameNumber playbackFrameNumber = console.frameCount();
        if(playbackFrameNumber > confirmedThroughFrame) break;

        const uint64_t nextFrameClockMicros = coordinator.authoritativeFrameStartClockMicros(playbackFrameNumber);
        if(nextFrameClockMicros == 0u) break;

        const uint64_t nowSharedClockMicros = coordinator.sharedClockNowMicros();
        if(nowSharedClockMicros == 0u || nowSharedClockMicros < nextFrameClockMicros) break;

        NetplayCoordinator::ConfirmedFrameInputs confirmedPlaybackFrame;
        if(!coordinator.tryBuildPlaybackFrame(playbackFrameNumber, confirmedPlaybackFrame)) break;
        if(!console.queuePlaybackInputFrame(confirmedPlaybackFrame)) break;
        if(!console.updateUntilFrame(frameDt, false)) break;

        ++advancedFrames;
        coordinator.setLocalSimulationFrame(console.frameCount());
    }

    if(advancedFrames > 0u) {
        std::ostringstream oss;
        oss << "Netplay continuous shared-clock catchup advanced "
            << advancedFrames
            << " frame(s)"
            << " localFrame="
            << console.frameCount()
            << " confirmedThrough="
            << confirmedThroughFrame;
        if(lagTriggerActivated) {
            oss << " triggerLagFrames="
                << estimatedLagFrames
                << " estimatedHostFrame="
                << estimatedHostFrameFromClock;
        }
        oss << " (audio muted)";
        coordinator.appendNetplayLog(oss.str());
    }

    return advancedFrames;
}

uint32_t runtimeAdvanceObserverPeerIfNeeded(NetplayCoordinator& coordinator,
                                            ConfirmedInputBufferDriver& inputDriver,
                                            INetplayConsole& console,
                                            uint32_t maxFrames,
                                            bool /*showDebugLog*/)
{
    if(maxFrames == 0u) return 0u;
    if(!coordinator.isActive()) return 0u;
    if(coordinator.session().roomState().state != SessionState::Running) return 0u;
    if(!console.valid()) return 0u;

    const std::vector<PlayerSlot> localSlots = runtimeLocalAssignedSlots(coordinator);
    if(!localSlots.empty()) return 0u;

    const FrameNumber confirmedThroughFrame = inputDriver.confirmedThroughFrame(coordinator);
    const FrameNumber localFrame = console.frameCount();
    if(confirmedThroughFrame <= localFrame) return 0u;
    const FrameNumber peerVisibleTargetFrame =
        coordinator.isHosting()
            ? confirmedThroughFrame
            : std::min(confirmedThroughFrame, coordinator.session().roomState().currentFrame);
    if(peerVisibleTargetFrame <= localFrame) return 0u;

    const uint32_t frameDt =
        std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, console.regionFps()));
    const FrameNumber targetFrame =
        std::min<FrameNumber>(peerVisibleTargetFrame, localFrame + static_cast<FrameNumber>(maxFrames));

    uint32_t advancedFrames = 0u;
    while(console.frameCount() < targetFrame) {
        const FrameNumber playbackFrameNumber = console.frameCount();
        NetplayCoordinator::ConfirmedFrameInputs confirmedPlaybackFrame;
        if(!coordinator.tryBuildPlaybackFrame(playbackFrameNumber, confirmedPlaybackFrame)) break;
        if(!console.queuePlaybackInputFrame(confirmedPlaybackFrame)) break;
        if(!console.updateUntilFrame(frameDt, false)) break;
        ++advancedFrames;
        coordinator.setLocalSimulationFrame(console.frameCount());
    }

    return advancedFrames;
}

bool runtimeProcessAutoResumeIfNeeded(NetplayCoordinator& coordinator,
                                      bool& webVisibilityManagedPause,
                                      bool webPageVisible,
                                      const std::optional<NetplayRomSelection>& localRom)
{
    if(!coordinator.isActive() || !coordinator.isHosting()) return false;

    const RoomState& room = coordinator.session().roomState();
    if(room.state != SessionState::Paused) return false;
    if(room.activeResyncId != 0 || room.pendingResyncAckCount != 0) return false;
    if(!runtimeSessionBlockedReason(coordinator.isActive(), room, localRom).empty()) return false;

    if(!webVisibilityManagedPause) return false;
    if(!webPageVisible) return false;

    if(coordinator.resumeSession()) {
        webVisibilityManagedPause = false;
        return true;
    }
    return false;
}

bool runtimeShouldRecoverStandaloneInputWhileNetplayActive(const NetplayCoordinator& coordinator)
{
    if(!coordinator.isActive()) {
        return false;
    }

    if(!coordinator.isConnected()) {
        return true;
    }

    const RoomState& room = coordinator.session().roomState();
    if(room.state == SessionState::Ended) {
        return true;
    }

    if(!coordinator.isHosting() || room.state != SessionState::Paused) {
        return false;
    }

    const ParticipantId localParticipantId = coordinator.localParticipantId();
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

bool runtimeShouldNetplayOwnEmulationInput(const NetplayCoordinator& coordinator)
{
    if(runtimeShouldRecoverStandaloneInputWhileNetplayActive(coordinator)) {
        return false;
    }

    const SessionState state = coordinator.session().roomState().state;
    return state == SessionState::Starting ||
           state == SessionState::Running ||
           state == SessionState::Resyncing ||
           state == SessionState::Paused;
}

std::vector<PlayerSlot> runtimeLocalAssignedSlots(const NetplayCoordinator& coordinator)
{
    const ParticipantId localId = coordinator.localParticipantId();
    if(localId == kInvalidParticipantId) return {};
    if(const ParticipantInfo* participant = coordinator.session().findParticipant(localId)) {
        return participantAssignments(*participant);
    }
    return {};
}

RuntimeAssignmentLayoutResult runtimeSyncAssignmentLayout(NetplayCoordinator& coordinator,
                                                          ConfirmedInputBufferDriver& inputDriver,
                                                          std::string& lastAssignmentLayoutKey,
                                                          std::vector<PlayerSlot>& lastLocalAssignedSlots,
                                                          FrameNumber localFrame,
                                                          bool running)
{
    RuntimeAssignmentLayoutResult result;
    result.localSlots = runtimeLocalAssignedSlots(coordinator);
    result.layoutKey = runtimeAssignmentLayoutKey(coordinator);

    if(!lastAssignmentLayoutKey.empty() && result.layoutKey != lastAssignmentLayoutKey) {
        result.layoutChanged = true;
        if(running) {
            inputDriver.reanchor(localFrame);
            result.reanchorInputDriver = true;
        } else {
            inputDriver.reset();
            result.resetInputDriver = true;
        }
    }
    lastAssignmentLayoutKey = result.layoutKey;

    if(result.localSlots != lastLocalAssignedSlots) {
        result.localSlotsChanged = true;
        if(running) {
            inputDriver.reanchor(localFrame);
            result.reanchorInputDriver = true;
        } else {
            inputDriver.reset();
            result.resetInputDriver = true;
        }
        lastLocalAssignedSlots = result.localSlots;
    }

    return result;
}

void runtimeProduceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                       ConfirmedInputBufferDriver& inputDriver,
                                       const std::vector<PlayerSlot>& localSlots,
                                       uint32_t workerDtMs,
                                       const ConfirmedInputBufferDriver::LocalInputBuilder& buildLocalInput,
                                       uint32_t regionFps,
                                       FrameNumber localFrame)
{
    inputDriver.produceLocalBufferedInputs(
        coordinator,
        coordinator.isActive(),
        false,
        coordinator.session().roomState().state,
        localSlots,
        workerDtMs,
        coordinator.session().roomState(),
        buildLocalInput,
        regionFps,
        localFrame,
        inputDriver.confirmedThroughFrame(coordinator)
    );
}

void runtimeProduceLocalBufferedInputs(NetplayCoordinator& coordinator,
                                       ConfirmedInputBufferDriver& inputDriver,
                                       INetplayConsole& console,
                                       const std::vector<PlayerSlot>& localSlots,
                                       uint32_t workerDtMs)
{
    runtimeProduceLocalBufferedInputs(
        coordinator,
        inputDriver,
        localSlots,
        workerDtMs,
        [&console](PlayerSlot slot, FrameNumber frame, const RoomState& room) {
            return console.buildLocalInputContribution(slot, frame, room);
        },
        console.regionFps(),
        console.frameCount()
    );
}

void runtimePreparePlaybackFrames(NetplayCoordinator& coordinator,
                                  ConfirmedInputBufferDriver& inputDriver,
                                  FrameNumber localFrame,
                                  std::optional<FrameNumber> maxPlaybackFrame,
                                  const ConfirmedInputBufferDriver::PendingFrameConsumer& consumeFrame)
{
    inputDriver.preparePlaybackFramesForEmulationThread(
        coordinator,
        coordinator.isActive(),
        false,
        coordinator.session().roomState().state,
        localFrame,
        maxPlaybackFrame
    );
    FrameNumber queueLimitFrame = localFrame + inputDriver.prebufferFrames();
    if(maxPlaybackFrame.has_value()) {
        queueLimitFrame = std::min(queueLimitFrame, *maxPlaybackFrame);
    }
    inputDriver.consumePendingFrames(
        localFrame,
        queueLimitFrame,
        consumeFrame
    );
}

void runtimePreparePlaybackFrames(NetplayCoordinator& coordinator,
                                  ConfirmedInputBufferDriver& inputDriver,
                                  INetplayConsole& console)
{
    const std::optional<FrameNumber> maxPlaybackFrame =
        runtimeClientHostPlaybackCapFrame(coordinator);
    runtimePreparePlaybackFrames(
        coordinator,
        inputDriver,
        console.frameCount(),
        maxPlaybackFrame,
        [&console](const NetplayCoordinator::ConfirmedFrameInputs& confirmed) {
            (void)console.queuePlaybackInputFrame(confirmed);
        }
    );
}

void runtimeRecordPlaybackStop(NetplayCoordinator& coordinator,
                               const ConfirmedInputBufferDriver& inputDriver,
                               FrameNumber frame)
{
    (void)inputDriver;
    coordinator.recordPlaybackStop(frame);
}

bool runtimeTryBuildPlaybackConfirmedFrame(NetplayCoordinator& coordinator,
                                           const ConfirmedInputBufferDriver& inputDriver,
                                           FrameNumber frame,
                                           NetplayCoordinator::ConfirmedFrameInputs& outFrame)
{
    const std::optional<FrameNumber> maxPlaybackFrame = runtimeClientHostPlaybackCapFrame(coordinator);
    if(maxPlaybackFrame.has_value() && frame > *maxPlaybackFrame) {
        runtimeRecordPlaybackStop(coordinator, inputDriver, frame);
        return false;
    }
    if(!coordinator.tryBuildPlaybackFrame(frame, outFrame)) {
        runtimeRecordPlaybackStop(coordinator, inputDriver, frame);
        return false;
    }
    return true;
}

SelfStallDetector::Snapshot runtimeBuildSelfStallSnapshot(const NetplayCoordinator& coordinator,
                                                          FrameNumber localSimulationFrame)
{
    const RoomState& room = coordinator.session().roomState();

    SelfStallDetector::Snapshot snapshot;
    snapshot.active = coordinator.isActive();
    snapshot.hosting = coordinator.isHosting();
    snapshot.role = coordinator.isHosting()
        ? SelfStallDetector::Role::Host
        : SelfStallDetector::Role::Client;
    snapshot.sessionState = room.state;
    snapshot.recoveryInputMode = room.recoveryInputMode;
    snapshot.timelineEpoch = room.timelineEpoch;
    snapshot.activeResyncId = room.activeResyncId;
    snapshot.pendingResyncAckCount = room.pendingResyncAckCount;
    snapshot.localSimulationFrame = localSimulationFrame;
    snapshot.confirmedFrame = room.lastConfirmedFrame;
    snapshot.playbackStopCount = coordinator.recoveryStats().playbackStopCount;

    for(const ParticipantInfo& participant : room.participants) {
        if(participant.id == coordinator.localParticipantId() || !participant.connected) {
            continue;
        }
        ++snapshot.connectedRemoteParticipantCount;
        snapshot.maxRemoteReportedCurrentFrame =
            std::max(snapshot.maxRemoteReportedCurrentFrame, participant.lastReportedCurrentFrame);
        snapshot.maxRemoteReportedConfirmedFrame =
            std::max(snapshot.maxRemoteReportedConfirmedFrame, participant.lastReportedConfirmedFrame);
    }

    return snapshot;
}

} // namespace ConsoleNetplay



