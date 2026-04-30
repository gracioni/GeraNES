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

} // namespace

size_t runtimeInputBufferCapacity(uint32_t prebufferFrames, uint32_t predictFrames)
{
    const size_t horizon = static_cast<size_t>(prebufferFrames) + static_cast<size_t>(predictFrames);
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
        inputDriver.setPredictFrames(settings.manualPredictFrames);
        return {
            settings.manualInputDelayFrames,
            settings.manualPredictFrames,
            runtimeInputBufferCapacity(settings.manualInputDelayFrames, settings.manualPredictFrames)
        };
    }

    const RoomState& room = coordinator.session().roomState();
    if(coordinator.isHosting()) {
        if(settings.autoGameplayTuning) {
            const NetplayAutoTune::Recommendations recommendations = autoTune.update(
                room,
                coordinator.predictionStats(),
                coordinator.unresolvedPredictedRemoteFrameCount(),
                settings.regionFps
            );
            if(recommendations.inputDelayFrames.has_value() &&
               room.inputDelayFrames != *recommendations.inputDelayFrames) {
                coordinator.setInputDelayFrames(*recommendations.inputDelayFrames);
            }
            if(recommendations.predictFrames.has_value() &&
               room.predictFrames != *recommendations.predictFrames) {
                coordinator.setPredictFrames(*recommendations.predictFrames);
            }
        } else {
            const uint8_t manualDelay =
                static_cast<uint8_t>(std::min<uint32_t>(settings.manualInputDelayFrames, 0xffu));
            const uint8_t manualPredict =
                static_cast<uint8_t>(std::min<uint32_t>(settings.manualPredictFrames, 0xffu));
            if(room.inputDelayFrames != manualDelay) {
                coordinator.setInputDelayFrames(manualDelay);
            }
            if(room.predictFrames != manualPredict) {
                coordinator.setPredictFrames(manualPredict);
            }
        }
    }

    const RoomState& effectiveRoom = coordinator.session().roomState();
    inputDriver.setPrebufferFrames(static_cast<uint32_t>(effectiveRoom.inputDelayFrames));
    inputDriver.setPredictFrames(static_cast<uint32_t>(effectiveRoom.predictFrames));

    return {
        static_cast<uint32_t>(effectiveRoom.inputDelayFrames),
        static_cast<uint32_t>(effectiveRoom.predictFrames),
        runtimeInputBufferCapacity(effectiveRoom.inputDelayFrames, effectiveRoom.predictFrames)
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

    return emu.saveNetplayStateToMemory();
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
        return emu.canonicalNetplayStateCrc32();
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

    const uint32_t loadedCrc32 = emu.canonicalNetplayStateCrc32();
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
    const uint32_t stateCrc32 =
        runtimeComputeAuthoritativeStateCrc32(emu, runtimeHost, authoritativeFrame, preferConfirmedSnapshot);
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
        result = runtimeApplyAuthoritativeStateLocally(
            coordinator,
            inputDriver,
            emu,
            runtimeHost,
            authoritativeFrame,
            statePayload
        );
        result.started = true;
        result.stateCrc32 = stateCrc32 != 0 ? stateCrc32 : result.stateCrc32;
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
    const FrameNumber safeConfirmedFrame =
        confirmedFrame == 0u ? 0u : std::min(confirmedFrame, lastFrameReadyFrame);
    const FrameNumber authoritativeCheckpointFrame =
        (state.lastLoadedAuthoritativeFrame != 0u &&
         lastFrameReadyFrame == state.lastLoadedAuthoritativeFrame)
            ? state.lastLoadedAuthoritativeFrame
            : 0u;
    const FrameNumber crcCheckpointFrame = std::max(safeConfirmedFrame, authoritativeCheckpointFrame);
    if(crcCheckpointFrame == 0) return result;

    const bool periodicDue = crcCheckpointFrame >= state.nextScheduledLocalCrcFrame;
    const bool postRecoveryRapidDue =
        crcCheckpointFrame > state.lastSubmittedLocalCrcFrame &&
        crcCheckpointFrame <= std::max(state.postRecoveryRapidCrcThroughFrame, authoritativeCheckpointFrame);
    const bool forcedDue =
        state.forceNextConfirmedCrcSubmission &&
        crcCheckpointFrame != state.lastSubmittedLocalCrcFrame;
    if(!periodicDue && !forcedDue && !postRecoveryRapidDue) return result;

    std::optional<uint32_t> crc32;
    if(crcCheckpointFrame == authoritativeCheckpointFrame &&
       runtimeHost.lastFrameReadyNetplayCrc32() != 0u) {
        crc32 = runtimeHost.lastFrameReadyNetplayCrc32();
    }
    if(!crc32.has_value()) {
        crc32 = runtimeHost.netplaySnapshotCrc32ForFrame(crcCheckpointFrame);
    }
    if(!crc32.has_value() && emu.frameCount() == crcCheckpointFrame) {
        crc32 = emu.canonicalNetplayStateCrc32();
    }
    if(!crc32.has_value()) return result;

    coordinator.submitLocalCrc(crcCheckpointFrame, *crc32);
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
        if(tuningRecommendations.predictFrames.has_value() &&
           coordinator.session().roomState().predictFrames != *tuningRecommendations.predictFrames) {
            coordinator.setPredictFrames(*tuningRecommendations.predictFrames);
        }
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
            "Netplay hard resync started after reason " + std::to_string(static_cast<int>(reason)) +
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
    request.catchupBudgetFrames = room.predictFrames;
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
                                  const ConfirmedInputBufferDriver::PendingFrameConsumer& consumeFrame)
{
    inputDriver.preparePlaybackFramesForEmulationThread(
        coordinator,
        coordinator.isActive(),
        false,
        coordinator.session().roomState().state,
        localFrame
    );
    inputDriver.consumePendingFrames(
        localFrame,
        localFrame + inputDriver.prebufferFrames() + inputDriver.predictFrames(),
        consumeFrame
    );
}

void runtimePreparePlaybackFrames(NetplayCoordinator& coordinator,
                                  ConfirmedInputBufferDriver& inputDriver,
                                  INetplayConsole& console)
{
    runtimePreparePlaybackFrames(
        coordinator,
        inputDriver,
        console.frameCount(),
        [&console](const NetplayCoordinator::ConfirmedFrameInputs& confirmed) {
            (void)console.queuePlaybackInputFrame(confirmed);
        }
    );
}

bool runtimeShouldAllowPredictionForFrame(const NetplayCoordinator& coordinator,
                                          const ConfirmedInputBufferDriver& inputDriver,
                                          FrameNumber frame)
{
    const FrameNumber confirmedThroughFrame = inputDriver.confirmedThroughFrame(coordinator);
    const FrameNumber delaySlackFrame =
        confirmedThroughFrame + static_cast<FrameNumber>(inputDriver.prebufferFrames());
    if(frame <= delaySlackFrame) {
        return false;
    }

    const FrameNumber predictionCapFrame =
        delaySlackFrame + static_cast<FrameNumber>(inputDriver.predictFrames());
    return frame <= predictionCapFrame;
}

void runtimeRecordPlaybackStop(NetplayCoordinator& coordinator,
                               const ConfirmedInputBufferDriver& inputDriver,
                               FrameNumber frame)
{
    const FrameNumber confirmedThroughFrame = inputDriver.confirmedThroughFrame(coordinator);
    const FrameNumber predictedThroughFrame =
        confirmedThroughFrame + static_cast<FrameNumber>(inputDriver.predictFrames());
    const bool predictionLimitReached = frame > predictedThroughFrame;
    coordinator.recordPlaybackStop(frame, predictionLimitReached);
}

bool runtimeTryBuildPlaybackConfirmedFrame(NetplayCoordinator& coordinator,
                                           const ConfirmedInputBufferDriver& inputDriver,
                                           FrameNumber frame,
                                           NetplayCoordinator::ConfirmedFrameInputs& outFrame)
{
    const bool allowPrediction = runtimeShouldAllowPredictionForFrame(coordinator, inputDriver, frame);
    const FrameNumber confirmedThroughFrame = inputDriver.confirmedThroughFrame(coordinator);
    const FrameNumber predictionCapFrame =
        confirmedThroughFrame +
        static_cast<FrameNumber>(inputDriver.prebufferFrames()) +
        static_cast<FrameNumber>(inputDriver.predictFrames());
    const bool allowHostFallback = frame > predictionCapFrame;
    if(!coordinator.tryBuildPlaybackFrame(frame, allowPrediction, outFrame, allowHostFallback)) {
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
    snapshot.playbackStopCount = coordinator.predictionStats().playbackStopCount;
    snapshot.rollbackScheduledCount = coordinator.predictionStats().rollbackScheduledCount;

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
