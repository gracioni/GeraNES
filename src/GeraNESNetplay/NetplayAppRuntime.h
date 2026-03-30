#pragma once

#ifndef __EMSCRIPTEN__

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "GeraNES/util/Crc32.h"
#include "GeraNESApp/AppSettings.h"
#include "GeraNESApp/EmulationHost.h"
#include "GeraNESNetplay/ConfirmedInputBufferDriver.h"
#include "GeraNESNetplay/NetplayAutoSettings.h"
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
        bool localRomLoaded = false;
        std::string localRomGameName;
        uint32_t localRomCrc32 = 0;
        ParticipantId localParticipantId = kInvalidParticipantId;
        std::string lastError;
        RoomState room;
        size_t localInputCount = 0;
        size_t remoteInputCount = 0;
        std::optional<TimelineInputEntry> latestLocalInput;
        std::optional<TimelineInputEntry> latestRemoteInput;
        RollbackStats predictionStats;
        NetplayAutoSettings::Snapshot autoSettings;
        uint32_t unresolvedPredictedRemoteFrameCount = 0;
        FrameNumber latestPredictedRemoteFrame = 0;
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

    struct MenuSnapshot
    {
        bool active = false;
        bool hosting = false;
        bool connected = false;
        ParticipantId localParticipantId = kInvalidParticipantId;
        RoomState room;
    };

private:
    using WorkerCommand = std::function<void(NetplayAppRuntime&, GeraNESEmu&)>;

    EmulationHost& m_emuHost;
    NetplayCoordinator m_coordinator;
    ConfirmedInputBufferDriver m_inputDriver;
    NetplayAutoSettings m_autoSettings;

    mutable std::mutex m_stateMutex;
    std::deque<WorkerCommand> m_pendingCommands;
    std::array<uint64_t, 4> m_latestRawMasks = {};
    EmulationHost::InputState m_latestInputState = {};
    UiSnapshot m_uiSnapshot;
    uint64_t m_cachedReconnectToken = 0;
    bool m_hasCachedReconnectToken = false;
    std::string m_stickyStatusMessage;

    std::chrono::steady_clock::time_point m_runtimeLastTickTime = {};
    std::string m_lastSelectedRomKey;
    std::string m_lastSubmittedValidationKey;
    std::optional<SessionState> m_lastSessionState;
    std::optional<PlayerSlot> m_lastLocalAssignedSlot;
    std::string m_lastAssignmentLayoutKey;
    uint32_t m_lastManualResetGeneration = 0;
    uint32_t m_lastManualLoadStateGeneration = 0;
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

    std::string buildAssignmentLayoutKey() const
    {
        if(!m_coordinator.isActive()) return {};

        std::string key;
        for(const auto& participant : m_coordinator.session().roomState().participants) {
            key += std::to_string(participant.id);
            key += ":";
            key += std::to_string(static_cast<int>(participant.controllerAssignment));
            key += ";";
        }
        return key;
    }

    void reanchorInputDriver(FrameNumber anchorFrame, std::optional<PlayerSlot> localSlot)
    {
        m_inputDriver.reanchor(anchorFrame);
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

        bool anyMissingRom = false;
        bool anyIncompatibleRom = false;
        bool anyDisconnected = false;

        for(const auto& participant : room.participants) {
            if(participant.controllerAssignment == kObserverPlayerSlot) continue;
            if(!participant.connected) anyDisconnected = true;
            if(!participant.romLoaded) anyMissingRom = true;
            else if(!participant.romCompatible) anyIncompatibleRom = true;
        }

        if(anyDisconnected) return "Session is paused because an assigned participant disconnected. Reassign when they return.";
        if(anyMissingRom) return "Waiting for assigned participants to load the selected ROM.";
        if(anyIncompatibleRom) return "One or more assigned participants have an incompatible ROM.";
        return "";
    }

    void syncRomValidation(const std::optional<RomSelection>& localRom);
    void syncInputDelayFromSettings(GeraNESEmu& emu);
    void processAutoStartIfNeeded(GeraNESEmu& emu, const std::optional<RomSelection>& localRom);
    void processAutoResumeIfNeeded(const std::optional<RomSelection>& localRom);
    void processHostManualStateChangeResyncIfNeeded(GeraNESEmu& emu);
    uint32_t consumeWorkerDtMs();
    void handleSessionStateTransitionsOnWorker(GeraNESEmu& emu);
    bool beginInitialSessionSyncOnWorker(GeraNESEmu& emu);
    void processHostResyncIfNeededOnWorker(GeraNESEmu& emu);
    void processHostLateJoinResyncIfNeededOnWorker(GeraNESEmu& emu);
    void processResyncIfNeededOnWorker(GeraNESEmu& emu);
    void processRollbackIfNeededOnWorker(GeraNESEmu& emu);
    bool tryBuildPlaybackConfirmedFrame(uint32_t frame, NetplayCoordinator::ConfirmedFrameInputs& outFrame);
    bool tryBuildPlaybackReplayFrame(uint32_t frame, EmulationHost::ReplayFrameInput& outFrame);
    void updateUiSnapshot(const std::optional<RomSelection>& localRom);
    bool tryQueuePlaybackFrameToEmu(GeraNESEmu& emu, uint32_t frame);
    void recordPlaybackStop(FrameNumber frame);

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
    void updateLatestInputState(const EmulationHost::InputState& inputState);
    void updateLatestRawMasks(const std::array<uint64_t, 4>& masks);
    UiSnapshot uiSnapshot() const;
    MenuSnapshot menuSnapshot() const;
    bool runtimeActive() const;
    bool runtimeRunning() const;

    void host(uint16_t port, size_t maxPeers, const std::string& displayName);
    void join(const std::string& hostName, uint16_t port, const std::string& displayName);
    void disconnect();
    void assignController(ParticipantId participantId, PlayerSlot slot);
    void configureInputAssignment(ParticipantId participantId,
                                  std::optional<Settings::Device> port1Device,
                                  std::optional<Settings::Device> port2Device,
                                  Settings::ExpansionDevice expansionDevice,
                                  Settings::NesMultitapDevice nesMultitapDevice,
                                  Settings::FamicomMultitapDevice famicomMultitapDevice,
                                  PlayerSlot slot);
    void kickParticipant(ParticipantId participantId);
    void removeReconnectReservation(ParticipantId participantId);
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
        m_lastLocalAssignedSlot.reset();
        m_lastAssignmentLayoutKey.clear();
        return;
    }

    if(!m_coordinator.isHosting() && romLoaded && romCompatible && !room.selectedGameName.empty()) {
        m_stickyStatusMessage.clear();
    }
}

inline void NetplayAppRuntime::syncInputDelayFromSettings(GeraNESEmu& emu)
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

inline void NetplayAppRuntime::processAutoResumeIfNeeded(const std::optional<RomSelection>& localRom)
{
    if(!m_coordinator.isActive() || !m_coordinator.isHosting()) return;

    const auto& room = m_coordinator.session().roomState();
    if(room.state != SessionState::Paused) return;
    if(!computeSessionBlockedReason(localRom).empty()) return;
}

inline void NetplayAppRuntime::processHostManualStateChangeResyncIfNeeded(GeraNESEmu& emu)
{
    const uint32_t currentResetGeneration = m_emuHost.manualResetGeneration();
    const uint32_t currentLoadStateGeneration = m_emuHost.manualLoadStateGeneration();

    if(currentResetGeneration == m_lastManualResetGeneration &&
       currentLoadStateGeneration == m_lastManualLoadStateGeneration) {
        return;
    }

    const bool resetTriggered = currentResetGeneration != m_lastManualResetGeneration;
    const bool loadStateTriggered = currentLoadStateGeneration != m_lastManualLoadStateGeneration;
    m_lastManualResetGeneration = currentResetGeneration;
    m_lastManualLoadStateGeneration = currentLoadStateGeneration;

    if(!m_coordinator.isHosting()) return;
    const SessionState state = m_coordinator.session().roomState().state;
    if(state != SessionState::Running && state != SessionState::Paused) return;
    if(!emu.valid()) return;

    const std::vector<uint8_t> statePayload = emu.saveStateToMemory();
    if(statePayload.empty()) return;

    const ResyncReason reason = resetTriggered ? ResyncReason::HostReset : ResyncReason::HostLoadedState;
    const std::string toast = NetplayCoordinator::resyncReasonToast(reason);
    if(!toast.empty()) {
        Logger::instance().log(toast, Logger::Type::USER);
    }

    const uint32_t payloadCrc32 =
        Crc32::calc(reinterpret_cast<const char*>(statePayload.data()), statePayload.size());
    m_coordinator.beginResync(emu.frameCount(), statePayload, payloadCrc32, reason);
}

inline void NetplayAppRuntime::processAutoStartIfNeeded(GeraNESEmu& emu, const std::optional<RomSelection>& localRom)
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

    if(previousState.has_value() &&
       *previousState != SessionState::Paused &&
       currentState == SessionState::Paused) {
        m_emuHost.discardQueuedAudio();
    }

    if(currentState == SessionState::Running &&
       previousState.has_value() &&
       (*previousState == SessionState::Starting || *previousState == SessionState::Resyncing)) {
        const uint32_t anchorFrame = m_coordinator.session().roomState().lastConfirmedFrame;
        reanchorInputDriver(anchorFrame, localAssignedSlot());
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

inline void NetplayAppRuntime::processHostLateJoinResyncIfNeededOnWorker(GeraNESEmu& emu)
{
    if(!m_coordinator.isHosting()) return;

    std::optional<ParticipantId> participantId = m_coordinator.consumePendingHostLateJoinResyncParticipant();
    if(!participantId.has_value()) return;
    if(!emu.valid()) return;

    const FrameNumber authoritativeFrame =
        std::min<FrameNumber>(m_coordinator.session().roomState().lastConfirmedFrame, emu.frameCount());
    const std::optional<std::vector<uint8_t>> confirmedSnapshot =
        m_emuHost.netplaySnapshotForFrame(authoritativeFrame);
    const std::vector<uint8_t> statePayload =
        confirmedSnapshot.has_value() ? *confirmedSnapshot : emu.saveStateToMemory();
    if(statePayload.empty()) return;

    const uint32_t payloadCrc32 =
        Crc32::calc(reinterpret_cast<const char*>(statePayload.data()), statePayload.size());
    if(m_coordinator.beginResync(authoritativeFrame, statePayload, payloadCrc32)) {
        Logger::instance().log(
            "Netplay late-join resync started for participant " +
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
        reanchorInputDriver(pending->targetFrame, localAssignedSlot());
    }
    const uint32_t loadedCrc32 = loaded ? emu.canonicalNetplayStateCrc32() : 0;
    m_coordinator.acknowledgeResync(pending->resyncId, pending->targetFrame, loadedCrc32, loaded);

    if(loaded) {
        Logger::instance().log("Netplay resync applied", Logger::Type::INFO);
    } else {
        Logger::instance().log("Netplay resync failed", Logger::Type::WARNING);
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
    reanchorInputDriver(*rollbackFrame, localAssignedSlot());

    const uint32_t frameDt =
        std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, emu.getRegionFPS()));
    emu.setForceSilentAudio(true);
    while(emu.frameCount() < currentFrame) {
        const uint32_t nextFrame = emu.frameCount() + 1u;
        NetplayCoordinator::ConfirmedFrameInputs playbackFrame;
        const bool allowPrediction = nextFrame > m_inputDriver.confirmedThroughFrame(m_coordinator);
        if(!m_coordinator.tryBuildPlaybackFrame(nextFrame, allowPrediction, playbackFrame)) {
            Logger::instance().log("Netplay resimulation failed", Logger::Type::WARNING);
            emu.setForceSilentAudio(false);
            return;
        }

        InputFrame inputFrame = playbackFrame.inputFrame;
        inputFrame.speculative = playbackFrame.predicted;
        emu.queueInputFrame(inputFrame);
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
    snapshot.localRomLoaded = localRom.has_value() && localRom->loaded;
    snapshot.localRomGameName = localRom.has_value() ? localRom->gameName : "";
    snapshot.localRomCrc32 = localRom.has_value() ? localRom->validation.romCrc32 : 0;
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
    snapshot.autoSettings = m_autoSettings.snapshot();
    snapshot.unresolvedPredictedRemoteFrameCount = m_coordinator.unresolvedPredictedRemoteFrameCount();
    snapshot.latestPredictedRemoteFrame = m_coordinator.latestPredictedRemoteFrame();
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
        recordPlaybackStop(frame);
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
    outFrame.hasFrameOverride = true;
    outFrame.frameOverride = playbackFrame.inputFrame;
    outFrame.frameOverride.frame = frame;
    for(PlayerSlot slot = 0; slot <= kMaxAssignedPlayerSlot; ++slot) {
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

    InputFrame inputFrame = playbackFrame.inputFrame;
    inputFrame.speculative = playbackFrame.predicted;
    emu.queueInputFrame(inputFrame);
    return true;
}

inline void NetplayAppRuntime::recordPlaybackStop(FrameNumber frame)
{
    const FrameNumber confirmedThroughFrame = m_inputDriver.confirmedThroughFrame(m_coordinator);
    const FrameNumber predictedThroughFrame =
        confirmedThroughFrame + static_cast<FrameNumber>(m_inputDriver.predictFrames());
    const bool predictionLimitReached = frame > predictedThroughFrame;
    m_coordinator.recordPlaybackStop(frame, predictionLimitReached);
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
    m_latestInputState = {};
    ConfirmedInputBufferDriver::applyPadMaskToInputState(m_latestInputState, kPort1PlayerSlot, masks[0]);
    ConfirmedInputBufferDriver::applyPadMaskToInputState(m_latestInputState, kPort2PlayerSlot, masks[1]);
    ConfirmedInputBufferDriver::applyPadMaskToInputState(m_latestInputState, kExpansionPlayerSlot, masks[2]);
    ConfirmedInputBufferDriver::applyPadMaskToInputState(m_latestInputState, kMultitapP4PlayerSlot, masks[3]);
}

inline void NetplayAppRuntime::updateLatestInputState(const EmulationHost::InputState& inputState)
{
    std::scoped_lock stateLock(m_stateMutex);
    m_latestInputState = inputState;
}

inline NetplayAppRuntime::UiSnapshot NetplayAppRuntime::uiSnapshot() const
{
    std::scoped_lock stateLock(m_stateMutex);
    return m_uiSnapshot;
}

inline NetplayAppRuntime::MenuSnapshot NetplayAppRuntime::menuSnapshot() const
{
    std::scoped_lock stateLock(m_stateMutex);
    MenuSnapshot snapshot;
    snapshot.active = m_uiSnapshot.active;
    snapshot.hosting = m_uiSnapshot.hosting;
    snapshot.connected = m_uiSnapshot.connected;
    snapshot.localParticipantId = m_uiSnapshot.localParticipantId;
    snapshot.room = m_uiSnapshot.room;
    return snapshot;
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
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu& emu) {
        self.m_stickyStatusMessage.clear();
        if(!emu.valid()) {
            self.m_stickyStatusMessage = "Load a ROM before hosting.";
            return;
        }
        self.m_coordinator.host(port, maxPeers, displayName);
    });
}

inline void NetplayAppRuntime::join(const std::string& hostName, uint16_t port, const std::string& displayName)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_stickyStatusMessage.clear();
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

inline void NetplayAppRuntime::assignController(ParticipantId participantId, PlayerSlot slot)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu&) {
        self.m_coordinator.assignController(participantId, slot);
    });
}

inline void NetplayAppRuntime::configureInputAssignment(ParticipantId participantId,
                                                        std::optional<Settings::Device> port1Device,
                                                        std::optional<Settings::Device> port2Device,
                                                        Settings::ExpansionDevice expansionDevice,
                                                        Settings::NesMultitapDevice nesMultitapDevice,
                                                        Settings::FamicomMultitapDevice famicomMultitapDevice,
                                                        PlayerSlot slot)
{
    enqueueCommand([=](NetplayAppRuntime& self, GeraNESEmu& emu) {
        const FrameNumber rebuildFromFrame = emu.frameCount() > 0 ? (emu.frameCount() - 1u) : 0u;
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
            slot
        );
        self.m_coordinator.assignController(participantId, slot);
        emu.discardQueuedInputFramesAfter(rebuildFromFrame);
        self.reanchorInputDriver(rebuildFromFrame, self.localAssignedSlot());
        self.m_lastAssignmentLayoutKey.clear();
        self.m_lastLocalAssignedSlot.reset();
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

    syncInputDelayFromSettings(emu);

    if(!m_coordinator.isActive()) {
        m_emuHost.setAutoQueuePendingInputOnFrameStart(true);
        m_emuHost.setFrameInputResolver({});
        m_emuHost.setAllowPresenterTimeoutAdvance(true);
        m_runtimeActive.store(false, std::memory_order_release);
        m_runtimeRunning.store(false, std::memory_order_release);
        m_inputDriver.reset();
        m_runtimeLastTickTime = {};
        m_lastSessionState.reset();
        m_lastLocalAssignedSlot.reset();
        m_lastAssignmentLayoutKey.clear();
        m_lastManualResetGeneration = m_emuHost.manualResetGeneration();
        m_lastManualLoadStateGeneration = m_emuHost.manualLoadStateGeneration();
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
    m_coordinator.setRoomInputTopology(
        emu.getPortDevice(Settings::Port::P_1),
        emu.getPortDevice(Settings::Port::P_2),
        emu.getExpansionDevice(),
        emu.getNesMultitapDevice(),
        emu.getFamicomMultitapDevice()
    );
    syncRomValidation(localRom);
    m_coordinator.setLocalSimulationFrame(emu.frameCount());
    m_coordinator.update(0);
    handleSessionStateTransitionsOnWorker(emu);
    processAutoStartIfNeeded(emu, localRom);
    processHostManualStateChangeResyncIfNeeded(emu);

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

    const bool running = m_coordinator.session().roomState().state == SessionState::Running;
    m_runtimeRunning.store(running, std::memory_order_release);

    std::array<uint64_t, 4> latestRawMasks = {};
    EmulationHost::InputState latestInputState{};
    {
        std::scoped_lock stateLock(m_stateMutex);
        latestRawMasks = m_latestRawMasks;
        latestInputState = m_latestInputState;
    }

    const std::optional<PlayerSlot> localSlot = localAssignedSlot();
    const std::string assignmentLayoutKey = buildAssignmentLayoutKey();
    if(!m_lastAssignmentLayoutKey.empty() && assignmentLayoutKey != m_lastAssignmentLayoutKey) {
        m_emuHost.discardQueuedNetplayInputsAfter(emu.frameCount());
        if(running) {
            reanchorInputDriver(emu.frameCount(), localSlot);
        } else {
            m_inputDriver.reset();
        }
    }
    m_lastAssignmentLayoutKey = assignmentLayoutKey;
    if(localSlot != m_lastLocalAssignedSlot) {
        if(running) {
            reanchorInputDriver(emu.frameCount(), localSlot);
        } else {
            m_inputDriver.reset();
        }
        m_lastLocalAssignedSlot = localSlot;
    }
    const uint32_t workerDtMs = consumeWorkerDtMs();

    m_inputDriver.produceLocalBufferedInputs(
        m_coordinator,
        m_coordinator.isActive(),
        false,
        m_coordinator.session().roomState().state,
        localSlot,
        workerDtMs,
        latestInputState,
        m_coordinator.session().roomState(),
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
