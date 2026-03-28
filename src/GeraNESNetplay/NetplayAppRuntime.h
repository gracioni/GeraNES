#pragma once

#ifndef __EMSCRIPTEN__

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

#include "GeraNES/util/Crc32.h"
#include "GeraNESApp/AppSettings.h"
#include "GeraNESApp/EmulationHost.h"
#include "GeraNESNetplay/ConfirmedInputBufferDriver.h"
#include "GeraNESNetplay/NetplayCoordinator.h"
#include "logger/logger.h"

namespace Netplay {

class NetplayAppRuntime
{
public:
    struct UiSnapshot
    {
        bool valid = false;
        bool active = false;
        bool hosting = false;
        bool connected = false;
        bool awaitingSpectatorSync = false;
        ParticipantId localParticipantId = kInvalidParticipantId;
        std::string lastError;
        RoomState room;
        size_t localInputCount = 0;
        size_t remoteInputCount = 0;
        std::optional<TimelineInputEntry> latestLocalInput;
        std::optional<TimelineInputEntry> latestRemoteInput;
        RollbackStats predictionStats;
        EmulationHost::NetplayDiagnosticsSnapshot runtimeDiagnostics;
        std::string sessionBlockedReason;
        std::vector<std::string> eventLog;
    };

    struct RomSelection
    {
        bool loaded = false;
        std::string gameName;
        RomValidationData validation = {};
    };

private:
    using WorkerCommand = std::function<void(NetplayAppRuntime&, GeraNESEmu&)>;

    EmulationHost& m_emuHost;
    NetplayCoordinator m_coordinator;
    ConfirmedInputBufferDriver m_inputDriver;

    mutable std::mutex m_stateMutex;
    std::deque<WorkerCommand> m_pendingCommands;
    std::array<uint64_t, 4> m_latestRawMasks = {};
    UiSnapshot m_uiSnapshot;
    uint64_t m_cachedReconnectToken = 0;
    bool m_hasCachedReconnectToken = false;

    std::chrono::steady_clock::time_point m_runtimeLastTickTime = {};
    std::string m_lastSelectedRomKey;
    std::string m_lastSubmittedValidationKey;
    std::optional<SessionState> m_lastSessionState;
    std::atomic<bool> m_runtimeActive{false};
    std::atomic<bool> m_runtimeRunning{false};

    static std::string buildRomKey(const std::optional<RomSelection>& selection)
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

    static std::optional<RomSelection> captureCurrentRomSelection(GeraNESEmu& emu)
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

    std::optional<PlayerSlot> localAssignedSlot() const
    {
        if(!m_coordinator.isActive()) return std::nullopt;

        const ParticipantId localParticipantId = m_coordinator.localParticipantId();
        for(const auto& participant : m_coordinator.session().roomState().participants) {
            if(participant.id == localParticipantId &&
               participant.controllerAssignment != kObserverPlayerSlot) {
                return participant.controllerAssignment;
            }
        }

        return std::nullopt;
    }

    std::string computeSessionBlockedReason(const std::optional<RomSelection>& localRom) const
    {
        if(!m_coordinator.isActive()) return "Netplay is not active.";

        const auto& room = m_coordinator.session().roomState();
        if(!localRom.has_value() || !localRom->loaded) {
            return "Load a ROM locally to populate room validation.";
        }
        if(room.selectedGameName.empty()) {
            return "Load a ROM locally to populate room validation.";
        }

        bool anyAssigned = false;
        bool anyMissingRom = false;
        bool anyIncompatibleRom = false;
        bool anyNotReady = false;
        bool anyDisconnected = false;

        for(const auto& participant : room.participants) {
            if(participant.controllerAssignment == kObserverPlayerSlot) continue;
            anyAssigned = true;
            if(!participant.connected) anyDisconnected = true;
            if(!participant.romLoaded) anyMissingRom = true;
            else if(!participant.romCompatible) anyIncompatibleRom = true;
            if(!participant.ready) anyNotReady = true;
        }

        if(!anyAssigned) return "Assign at least one participant to a controller slot.";
        if(anyDisconnected) return "Waiting for a disconnected assigned participant to reconnect.";
        if(anyMissingRom) return "Waiting for assigned participants to load the selected ROM.";
        if(anyIncompatibleRom) return "One or more assigned participants have an incompatible ROM.";
        if(anyNotReady) return "Waiting for assigned participants to mark themselves ready.";
        return "";
    }

    void syncRomValidation(const std::optional<RomSelection>& localRom);
    void syncInputDelayFromSettings();
    void processAutoResumeIfNeeded(const std::optional<RomSelection>& localRom);
    uint32_t consumeWorkerDtMs();
    void handleSessionStateTransitionsOnWorker(GeraNESEmu& emu);
    bool beginInitialSessionSyncOnWorker(GeraNESEmu& emu);
    void processHostResyncIfNeededOnWorker(GeraNESEmu& emu);
    void processHostSpectatorSyncIfNeededOnWorker(GeraNESEmu& emu);
    void processResyncIfNeededOnWorker(GeraNESEmu& emu);
    void processSpectatorSyncIfNeededOnWorker(GeraNESEmu& emu);
    void processRollbackIfNeededOnWorker(GeraNESEmu& emu);
    bool tryBuildPlaybackConfirmedFrame(uint32_t frame, NetplayCoordinator::ConfirmedFrameInputs& outFrame);
    bool tryBuildPlaybackReplayFrame(uint32_t frame, EmulationHost::ReplayFrameInput& outFrame);
    void updateUiSnapshot(const std::optional<RomSelection>& localRom);
    bool tryQueuePlaybackFrameToEmu(GeraNESEmu& emu, uint32_t frame);

    template<typename Fn>
    void enqueueCommand(Fn&& fn)
    {
        std::scoped_lock stateLock(m_stateMutex);
        m_pendingCommands.emplace_back(std::forward<Fn>(fn));
    }

    void drainPendingCommands(GeraNESEmu& emu)
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

public:
    explicit NetplayAppRuntime(EmulationHost& emuHost)
        : m_emuHost(emuHost)
    {
    }

    void setLocalReconnectToken(uint64_t token);
    void refreshLocalRomSelectionImmediate();
    void updateLatestRawMasks(const std::array<uint64_t, 4>& masks);
    UiSnapshot uiSnapshot() const;
    bool runtimeActive() const;
    bool runtimeRunning() const;

    void host(uint16_t port, size_t maxPeers, const std::string& displayName);
    void join(const std::string& hostName, uint16_t port, const std::string& displayName);
    void disconnect();
    void setLocalReady(bool ready);
    void pauseSession();
    void resumeSession();
    void endSession();
    void cancelControllerRequest();
    void requestControllerSlot(PlayerSlot slot);
    void setParticipantRole(ParticipantId participantId, ParticipantRole role);
    void assignController(ParticipantId participantId, PlayerSlot slot);
    void kickParticipant(ParticipantId participantId);
    void removeReconnectReservation(ParticipantId participantId);
    void approveControllerRequest(ParticipantId participantId);
    void denyControllerRequest(ParticipantId participantId);
    void requestStartSession();
    void requestForceResync();
    void shutdown();
    void runOnEmulationThread(GeraNESEmu& emu);
};

inline void NetplayAppRuntime::syncRomValidation(const std::optional<RomSelection>& localRom)
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
}

inline void NetplayAppRuntime::syncInputDelayFromSettings()
{
    auto& cfg = AppSettings::instance().data.netplay;
    if(!m_coordinator.isActive()) {
        m_inputDriver.setPrebufferFrames(static_cast<uint32_t>(std::max(1, cfg.inputDelayFrames)));
        m_inputDriver.setPredictFrames(static_cast<uint32_t>(std::max(0, cfg.predictFrames)));
        return;
    }

    const auto& room = m_coordinator.session().roomState();
    const bool canChangeWindows =
        m_coordinator.isHosting() &&
        (room.state == SessionState::Lobby ||
         room.state == SessionState::ValidatingRom ||
         room.state == SessionState::ReadyCheck ||
         room.state == SessionState::Starting);

    if(canChangeWindows) {
        m_inputDriver.setPrebufferFrames(static_cast<uint32_t>(std::max(1, cfg.inputDelayFrames)));
        m_inputDriver.setPredictFrames(static_cast<uint32_t>(std::max(0, cfg.predictFrames)));
        if(room.inputDelayFrames != m_inputDriver.prebufferFrames()) {
            m_coordinator.setInputDelayFrames(static_cast<uint8_t>(m_inputDriver.prebufferFrames()));
        }
        if(room.predictFrames != m_inputDriver.predictFrames()) {
            m_coordinator.setPredictFrames(static_cast<uint8_t>(m_inputDriver.predictFrames()));
        }
    } else {
        m_inputDriver.setPrebufferFrames(static_cast<uint32_t>(std::max<uint8_t>(1u, room.inputDelayFrames)));
        m_inputDriver.setPredictFrames(static_cast<uint32_t>(room.predictFrames));
    }

    cfg.inputDelayFrames = static_cast<int>(m_inputDriver.prebufferFrames());
    cfg.predictFrames = static_cast<int>(m_inputDriver.predictFrames());
}

inline void NetplayAppRuntime::processAutoResumeIfNeeded(const std::optional<RomSelection>& localRom)
{
    if(!m_coordinator.isActive() || !m_coordinator.isHosting()) return;

    const auto& room = m_coordinator.session().roomState();
    if(room.state != SessionState::Paused) return;
    if(!AppSettings::instance().data.netplay.autoResumeWhenReady) return;
    if(!computeSessionBlockedReason(localRom).empty()) return;

    if(m_coordinator.resumeSession()) {
        Logger::instance().log("Netplay auto-resumed when all participants became ready", Logger::Type::USER);
    }
}

inline uint32_t NetplayAppRuntime::consumeWorkerDtMs()
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

inline void NetplayAppRuntime::handleSessionStateTransitionsOnWorker(GeraNESEmu& emu)
{
    if(!m_coordinator.isActive()) {
        m_lastSessionState.reset();
        m_inputDriver.reset();
        m_runtimeLastTickTime = {};
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

    if(currentState != SessionState::Running) {
        m_inputDriver.reset();
        m_runtimeLastTickTime = {};
    }

    if(currentState == SessionState::Running &&
       previousState.has_value() &&
       (*previousState == SessionState::Starting || *previousState == SessionState::Resyncing)) {
        const uint32_t anchorFrame = std::max<uint32_t>(
            m_coordinator.session().roomState().lastConfirmedFrame,
            emu.frameCount()
        );
        m_inputDriver.reanchor(anchorFrame);
    }

    if(enteringResync) {
        Logger::instance().log("Netplay resync in progress", Logger::Type::USER);
    } else if(leavingResync) {
        Logger::instance().log("Netplay resync finished", Logger::Type::USER);
    }

    m_lastSessionState = currentState;
}

inline bool NetplayAppRuntime::beginInitialSessionSyncOnWorker(GeraNESEmu& emu)
{
    if(!m_coordinator.isHosting()) return false;
    if(!emu.valid()) return false;
    if(m_coordinator.session().roomState().state != SessionState::Starting) return false;

    const FrameNumber authoritativeFrame = emu.frameCount();
    const std::vector<uint8_t> statePayload = emu.saveStateToMemory();
    if(statePayload.empty()) return false;

    const uint32_t payloadCrc32 =
        Crc32::calc(reinterpret_cast<const char*>(statePayload.data()), statePayload.size());
    if(!m_coordinator.beginResync(authoritativeFrame, statePayload, payloadCrc32)) {
        return false;
    }

    Logger::instance().log("Netplay initial session sync started", Logger::Type::INFO);
    return true;
}

inline void NetplayAppRuntime::processHostResyncIfNeededOnWorker(GeraNESEmu& emu)
{
    if(!m_coordinator.isHosting()) return;

    std::optional<FrameNumber> pendingFrame = m_coordinator.consumePendingHostResyncFrame();
    if(!pendingFrame.has_value()) return;
    if(!emu.valid()) return;

    const bool initialSessionSync =
        m_coordinator.session().roomState().state == SessionState::Starting;

    const FrameNumber requestedFrame =
        initialSessionSync
            ? emu.frameCount()
            : m_coordinator.session().roomState().lastConfirmedFrame;
    const FrameNumber authoritativeFrame =
        std::min<FrameNumber>(requestedFrame, emu.frameCount());

    const std::optional<std::vector<uint8_t>> confirmedSnapshot =
        initialSessionSync ? std::nullopt : m_emuHost.netplaySnapshotForFrame(authoritativeFrame);
    std::vector<uint8_t> statePayload;
    if(initialSessionSync) {
        statePayload = emu.saveStateToMemory();
    } else if(confirmedSnapshot.has_value()) {
        statePayload = *confirmedSnapshot;
    } else {
        statePayload = emu.saveStateToMemory();
    }
    if(statePayload.empty()) return;

    const uint32_t payloadCrc32 =
        Crc32::calc(reinterpret_cast<const char*>(statePayload.data()), statePayload.size());
    if(m_coordinator.beginResync(authoritativeFrame, statePayload, payloadCrc32)) {
        if(initialSessionSync) {
            Logger::instance().log("Netplay initial session sync started", Logger::Type::INFO);
        } else {
            Logger::instance().log(
                "Netplay hard resync started after confirmed desync at frame " + std::to_string(*pendingFrame) +
                ", using authoritative frame " + std::to_string(authoritativeFrame),
                Logger::Type::WARNING
            );
        }
    }
}

inline void NetplayAppRuntime::processHostSpectatorSyncIfNeededOnWorker(GeraNESEmu& emu)
{
    if(!m_coordinator.isHosting()) return;

    std::optional<ParticipantId> participantId = m_coordinator.consumePendingHostSpectatorSyncParticipant();
    if(!participantId.has_value()) return;
    if(!emu.valid()) return;

    const std::vector<uint8_t> statePayload = emu.saveStateToMemory();
    if(statePayload.empty()) return;

    const uint32_t payloadCrc32 =
        Crc32::calc(reinterpret_cast<const char*>(statePayload.data()), statePayload.size());
    if(m_coordinator.beginSpectatorSync(*participantId, emu.frameCount(), statePayload, payloadCrc32)) {
        Logger::instance().log(
            "Netplay spectator sync started for participant " +
            std::to_string(static_cast<int>(*participantId)),
            Logger::Type::INFO
        );
    }
}

inline void NetplayAppRuntime::processResyncIfNeededOnWorker(GeraNESEmu& emu)
{
    std::optional<NetplayCoordinator::PendingResyncApply> pending = m_coordinator.consumePendingResyncApply();
    if(!pending.has_value()) return;

    const bool loaded = emu.loadStateFromMemoryOnCleanBoot(pending->payload);
    if(loaded) {
        m_coordinator.setLocalSimulationFrame(pending->targetFrame);
        m_emuHost.seedNetplaySnapshot(pending->targetFrame, pending->payload);
        m_inputDriver.reanchor(pending->targetFrame);
    }
    const uint32_t loadedCrc32 = loaded ? emu.canonicalNetplayStateCrc32() : 0;
    m_coordinator.acknowledgeResync(pending->resyncId, pending->targetFrame, loadedCrc32, loaded);

    if(loaded) {
        Logger::instance().log("Netplay resync applied", Logger::Type::INFO);
    } else {
        Logger::instance().log("Netplay resync failed", Logger::Type::WARNING);
    }
}

inline void NetplayAppRuntime::processSpectatorSyncIfNeededOnWorker(GeraNESEmu& emu)
{
    std::optional<NetplayCoordinator::PendingResyncApply> pending = m_coordinator.consumePendingSpectatorSyncApply();
    if(!pending.has_value()) return;

    const bool loaded = emu.loadStateFromMemoryOnCleanBoot(pending->payload);
    if(loaded) {
        m_coordinator.setLocalSimulationFrame(pending->targetFrame);
        m_emuHost.seedNetplaySnapshot(pending->targetFrame, pending->payload);
        m_inputDriver.reanchor(pending->targetFrame);
    }
    const uint32_t loadedCrc32 = loaded ? emu.canonicalNetplayStateCrc32() : 0;
    m_coordinator.acknowledgeSpectatorSync(pending->resyncId, pending->targetFrame, loadedCrc32, loaded);

    if(loaded) {
        Logger::instance().log("Netplay spectator sync applied", Logger::Type::INFO);
    } else {
        Logger::instance().log("Netplay spectator sync failed", Logger::Type::WARNING);
    }
}

inline void NetplayAppRuntime::processRollbackIfNeededOnWorker(GeraNESEmu& emu)
{
    std::optional<FrameNumber> rollbackFrame = m_coordinator.consumePendingRollbackFrame();
    if(!rollbackFrame.has_value()) return;

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
        Logger::instance().log("Netplay rollback failed: snapshot unavailable", Logger::Type::WARNING);
        return;
    }

    const uint32_t rollbackFromFrame = currentFrame;
    if(!emu.loadStateFromMemoryOnCleanBoot(*snapshotData)) {
        Logger::instance().log("Netplay rollback failed", Logger::Type::WARNING);
        return;
    }

    m_emuHost.seedNetplaySnapshot(*rollbackFrame, *snapshotData);
    m_coordinator.setLocalSimulationFrame(*rollbackFrame);
    m_coordinator.invalidateLocalCrcHistoryAfter(*rollbackFrame);
    m_inputDriver.reanchor(*rollbackFrame);

    const uint32_t frameDt =
        std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));
    while(emu.frameCount() < currentFrame) {
        const uint32_t nextFrame = emu.frameCount() + 1u;
        NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
        const bool allowPrediction = nextFrame > m_inputDriver.confirmedThroughFrame(m_coordinator);
        if(!m_coordinator.tryBuildPlaybackFrame(nextFrame, allowPrediction, playbackFrame)) {
            Logger::instance().log("Netplay resimulation failed", Logger::Type::WARNING);
            return;
        }

        InputFrame inputFrame = emu.createInputFrame(nextFrame);
        inputFrame.speculative = playbackFrame.predicted;
        for(PlayerSlot slot = 0; slot < 4; ++slot) {
            ConfirmedInputBufferDriver::applyPadMaskToInputFrame(inputFrame, slot, playbackFrame.buttonMaskLo[slot]);
        }
        emu.queueInputFrame(inputFrame);
        emu.setForceSilentAudio(playbackFrame.predicted);
        emu.updateUntilFrame(frameDt);
    }
    emu.setForceSilentAudio(false);
    m_coordinator.setLocalSimulationFrame(emu.frameCount());

    Logger::instance().log(
        "Netplay rollback applied (" + std::to_string(rollbackFromFrame) +
        " -> " + std::to_string(*rollbackFrame) + ")",
        Logger::Type::INFO
    );
}

inline void NetplayAppRuntime::updateUiSnapshot(const std::optional<RomSelection>& localRom)
{
    UiSnapshot snapshot;
    snapshot.valid = true;
    snapshot.active = m_coordinator.isActive();
    snapshot.hosting = m_coordinator.isHosting();
    snapshot.connected = m_coordinator.isConnected();
    snapshot.awaitingSpectatorSync = m_coordinator.awaitingSpectatorSync();
    snapshot.localParticipantId = m_coordinator.localParticipantId();
    snapshot.lastError = m_coordinator.lastError();
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
    snapshot.runtimeDiagnostics = m_emuHost.getNetplayDiagnostics();
    snapshot.sessionBlockedReason = computeSessionBlockedReason(localRom);
    snapshot.eventLog = m_coordinator.eventLog();

    std::scoped_lock stateLock(m_stateMutex);
    m_uiSnapshot = std::move(snapshot);
}

inline bool NetplayAppRuntime::tryBuildPlaybackConfirmedFrame(uint32_t frame,
                                                              NetplayCoordinator::ConfirmedFrameInputs& outFrame)
{
    const bool allowPrediction = frame > m_inputDriver.confirmedThroughFrame(m_coordinator);
    if(!m_coordinator.tryBuildPlaybackFrame(frame, allowPrediction, outFrame)) {
        return false;
    }
    return true;
}

inline bool NetplayAppRuntime::tryBuildPlaybackReplayFrame(uint32_t frame, EmulationHost::ReplayFrameInput& outFrame)
{
    NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
    if(!tryBuildPlaybackConfirmedFrame(frame, playbackFrame)) {
        return false;
    }
    outFrame = {};
    outFrame.speculative = playbackFrame.predicted;
    for(PlayerSlot slot = 0; slot < 4; ++slot) {
        ConfirmedInputBufferDriver::applyPadMaskToInputState(outFrame.state, slot, playbackFrame.buttonMaskLo[slot]);
    }
    return true;
}

inline bool NetplayAppRuntime::tryQueuePlaybackFrameToEmu(GeraNESEmu& emu, uint32_t frame)
{
    if(emu.inputBuffer().findByFrame(frame) != nullptr) {
        return true;
    }

    NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
    if(!tryBuildPlaybackConfirmedFrame(frame, playbackFrame)) {
        return false;
    }

    InputFrame inputFrame = emu.createInputFrame(frame);
    inputFrame.speculative = playbackFrame.predicted;
    for(PlayerSlot slot = 0; slot < 4; ++slot) {
        ConfirmedInputBufferDriver::applyPadMaskToInputFrame(inputFrame, slot, playbackFrame.buttonMaskLo[slot]);
    }
    emu.queueInputFrame(inputFrame);
    return true;
}

inline void NetplayAppRuntime::setLocalReconnectToken(uint64_t token)
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

inline void NetplayAppRuntime::refreshLocalRomSelectionImmediate()
{
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_lastSelectedRomKey.clear();
        self.m_lastSubmittedValidationKey.clear();
    });
}

inline void NetplayAppRuntime::updateLatestRawMasks(const std::array<uint64_t, 4>& masks)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_latestRawMasks = masks;
}

inline NetplayAppRuntime::UiSnapshot NetplayAppRuntime::uiSnapshot() const
{
    std::scoped_lock stateLock(m_stateMutex);
    return m_uiSnapshot;
}

inline bool NetplayAppRuntime::runtimeActive() const
{
    return m_runtimeActive.load(std::memory_order_acquire);
}

inline bool NetplayAppRuntime::runtimeRunning() const
{
    return m_runtimeRunning.load(std::memory_order_acquire);
}

inline void NetplayAppRuntime::host(uint16_t port, size_t maxPeers, const std::string& displayName)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.host(port, maxPeers, displayName);
    });
}

inline void NetplayAppRuntime::join(const std::string& hostName, uint16_t port, const std::string& displayName)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.join(hostName, port, displayName);
    });
}

inline void NetplayAppRuntime::disconnect()
{
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.disconnect();
        self.m_inputDriver.reset();
        self.m_runtimeLastTickTime = {};
    });
}

inline void NetplayAppRuntime::setLocalReady(bool ready)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.setLocalReady(ready);
    });
}

inline void NetplayAppRuntime::pauseSession()
{
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.pauseSession();
    });
}

inline void NetplayAppRuntime::resumeSession()
{
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.resumeSession();
    });
}

inline void NetplayAppRuntime::endSession()
{
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.endSession();
    });
}

inline void NetplayAppRuntime::cancelControllerRequest()
{
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.cancelControllerRequest();
    });
}

inline void NetplayAppRuntime::requestControllerSlot(PlayerSlot slot)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.requestControllerSlot(slot);
    });
}

inline void NetplayAppRuntime::setParticipantRole(ParticipantId participantId, ParticipantRole role)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.setParticipantRole(participantId, role);
    });
}

inline void NetplayAppRuntime::assignController(ParticipantId participantId, PlayerSlot slot)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.assignController(participantId, slot);
    });
}

inline void NetplayAppRuntime::kickParticipant(ParticipantId participantId)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.kickParticipant(participantId);
    });
}

inline void NetplayAppRuntime::removeReconnectReservation(ParticipantId participantId)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.removeReconnectReservation(participantId);
    });
}

inline void NetplayAppRuntime::approveControllerRequest(ParticipantId participantId)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.approveControllerRequest(participantId);
    });
}

inline void NetplayAppRuntime::denyControllerRequest(ParticipantId participantId)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.denyControllerRequest(participantId);
    });
}

inline void NetplayAppRuntime::requestStartSession()
{
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_coordinator.setLocalSimulationFrame(emu.frameCount());
        if(self.m_coordinator.startSession() && emu.frameCount() > 0) {
            self.beginInitialSessionSyncOnWorker(emu);
        }
    });
}

inline void NetplayAppRuntime::requestForceResync()
{
    enqueueCommand([](NetplayAppRuntime& self, GeraNESEmu& emu) {
        if(!self.m_coordinator.isHosting()) return;
        const auto state = self.m_coordinator.session().roomState().state;
        if(!emu.valid()) return;
        if(state != SessionState::Running && state != SessionState::Paused) return;

        const std::vector<uint8_t> statePayload = emu.saveStateToMemory();
        if(statePayload.empty()) return;
        const uint32_t payloadCrc32 =
            Crc32::calc(reinterpret_cast<const char*>(statePayload.data()), statePayload.size());
        self.m_coordinator.beginResync(emu.frameCount(), statePayload, payloadCrc32);
    });
}

inline void NetplayAppRuntime::shutdown()
{
    std::scoped_lock stateLock(m_stateMutex);
    m_pendingCommands.clear();
    m_runtimeActive.store(false, std::memory_order_release);
    m_runtimeRunning.store(false, std::memory_order_release);
    m_uiSnapshot = UiSnapshot{};
    m_coordinator.disconnect();
}

inline void NetplayAppRuntime::runOnEmulationThread(GeraNESEmu& emu)
{
    drainPendingCommands(emu);

    if(m_hasCachedReconnectToken) {
        m_coordinator.setLocalReconnectToken(m_cachedReconnectToken);
    }

    syncInputDelayFromSettings();

    if(!m_coordinator.isActive()) {
        m_emuHost.setAutoQueuePendingInputOnFrameStart(true);
        m_emuHost.setFrameInputResolver({});
        m_emuHost.setAllowPresenterTimeoutAdvance(true);
        m_runtimeActive.store(false, std::memory_order_release);
        m_runtimeRunning.store(false, std::memory_order_release);
        m_inputDriver.reset();
        m_runtimeLastTickTime = {};
        m_lastSessionState.reset();
        updateUiSnapshot(captureCurrentRomSelection(emu));
        return;
    }

    m_runtimeActive.store(true, std::memory_order_release);
    const SessionState currentRoomState = m_coordinator.session().roomState().state;
    const bool netplayOwnsEmulationInput =
        currentRoomState == SessionState::Starting ||
        currentRoomState == SessionState::Running ||
        currentRoomState == SessionState::Resyncing ||
        currentRoomState == SessionState::Paused;
    m_emuHost.setAutoQueuePendingInputOnFrameStart(!netplayOwnsEmulationInput);
    if(netplayOwnsEmulationInput) {
        m_emuHost.setFrameInputResolver([this](uint32_t frame, EmulationHost::ReplayFrameInput& outFrame) {
            return tryBuildPlaybackReplayFrame(frame, outFrame);
        });
    } else {
        m_emuHost.setFrameInputResolver({});
    }
    m_emuHost.setAllowPresenterTimeoutAdvance(true);

    const std::optional<RomSelection> localRom = captureCurrentRomSelection(emu);
    syncRomValidation(localRom);
    m_coordinator.setLocalSimulationFrame(emu.frameCount());
    m_coordinator.update(0);
    handleSessionStateTransitionsOnWorker(emu);

    if(m_coordinator.isHosting() &&
       m_coordinator.session().roomState().state == SessionState::Starting &&
       m_coordinator.session().roomState().activeResyncId == 0) {
        beginInitialSessionSyncOnWorker(emu);
    }

    processHostResyncIfNeededOnWorker(emu);
    processHostSpectatorSyncIfNeededOnWorker(emu);
    processResyncIfNeededOnWorker(emu);
    processSpectatorSyncIfNeededOnWorker(emu);
    processAutoResumeIfNeeded(localRom);
    processRollbackIfNeededOnWorker(emu);

    const bool running =
        !m_coordinator.awaitingSpectatorSync() &&
        m_coordinator.session().roomState().state == SessionState::Running;
    m_runtimeRunning.store(running, std::memory_order_release);

    std::array<uint64_t, 4> latestRawMasks = {};
    {
        std::scoped_lock stateLock(m_stateMutex);
        latestRawMasks = m_latestRawMasks;
    }

    const std::optional<PlayerSlot> localSlot = localAssignedSlot();
    const uint64_t localPrimaryMask = latestRawMasks[0];
    const uint32_t workerDtMs = consumeWorkerDtMs();

    m_inputDriver.produceLocalBufferedInputs(
        m_coordinator,
        m_coordinator.isActive(),
        m_coordinator.awaitingSpectatorSync(),
        m_coordinator.session().roomState().state,
        localSlot,
        workerDtMs,
        localPrimaryMask,
        emu.getRegionFPS(),
        emu.frameCount(),
        m_inputDriver.confirmedThroughFrame(m_coordinator)
    );

    if(running) {
        const uint32_t currentFrame = emu.frameCount();
        if(emu.inputBuffer().findByFrame(currentFrame) == nullptr) {
            tryQueuePlaybackFrameToEmu(emu, currentFrame);
        }
    }

    updateUiSnapshot(localRom);
}

} // namespace Netplay

#endif
