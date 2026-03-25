#pragma once

#ifndef __EMSCRIPTEN__

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "GeraNES/GeraNESEmu.h"
#include "GeraNES/util/Crc32.h"
#include "GeraNESNetplay/NetplayCoordinator.h"
#include "GeraNESNetplay/NetplayRuntime.h"

class NetplayTest
{
public:
    struct Options
    {
        std::string romPath;
        std::string reportPath;
        uint32_t frames = 600;
        uint32_t inputDelayFrames = 2;
        uint32_t rollbackWindowFrames = 600;
        uint32_t crcIntervalFrames = 10;
        uint32_t port = 27888;
        uint32_t startupTimeoutSteps = 10000;
        uint32_t frameStepLimit = 200000;
        uint32_t forceDesyncFrame = 0;
        uint32_t desyncAddress = 0x0000;
        uint32_t desyncValueXor = 0x01;
    };

private:
    struct Buttons
    {
        bool a = false;
        bool b = false;
        bool select = false;
        bool start = false;
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
    };

    struct InputState
    {
        bool p1A = false;
        bool p1B = false;
        bool p1Select = false;
        bool p1Start = false;
        bool p1Up = false;
        bool p1Down = false;
        bool p1Left = false;
        bool p1Right = false;

        bool p2A = false;
        bool p2B = false;
        bool p2Select = false;
        bool p2Start = false;
        bool p2Up = false;
        bool p2Down = false;
        bool p2Left = false;
        bool p2Right = false;

        bool p3A = false;
        bool p3B = false;
        bool p3Select = false;
        bool p3Start = false;
        bool p3Up = false;
        bool p3Down = false;
        bool p3Left = false;
        bool p3Right = false;

        bool p4A = false;
        bool p4B = false;
        bool p4Select = false;
        bool p4Start = false;
        bool p4Up = false;
        bool p4Down = false;
        bool p4Left = false;
        bool p4Right = false;
    };

    class DeterministicInputGenerator
    {
    private:
        uint32_t m_seed = 0;

        static uint32_t mix(uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            value ^= value >> 16;
            return value;
        }

    public:
        explicit DeterministicInputGenerator(uint32_t seed)
            : m_seed(seed == 0 ? 0x12345678u : seed)
        {
        }

        Buttons buttonsForFrame(uint32_t frame, uint32_t fps)
        {
            const uint32_t startupQuietFrames = std::max<uint32_t>(fps, 30u);
            if(frame <= startupQuietFrames) {
                return {};
            }

            Buttons buttons;
            const uint32_t segmentLength = 5u + (mix(m_seed ^ 0xA511E9B3u) % 4u);
            const uint32_t segment = (frame - startupQuietFrames - 1u) / segmentLength;
            const uint32_t localFrame = (frame - startupQuietFrames - 1u) % segmentLength;
            const uint32_t action = mix(m_seed ^ segment ^ 0x9E3779B9u);
            const uint32_t direction = action % 10u;
            const bool tapStart = (action % 37u) == 0u && localFrame == 0u;
            const bool tapSelect = (action % 53u) == 0u && localFrame == 0u;

            if(tapStart) buttons.start = true;
            if(tapSelect) buttons.select = true;

            switch(direction) {
                case 0: buttons.up = true; break;
                case 1: buttons.down = true; break;
                case 2: buttons.left = true; break;
                case 3: buttons.right = true; break;
                case 4: buttons.up = true; buttons.right = true; break;
                case 5: buttons.up = true; buttons.left = true; break;
                case 6: buttons.down = true; buttons.right = true; break;
                case 7: buttons.down = true; buttons.left = true; break;
                default: break;
            }

            buttons.a = ((action >> 8) & 0x3u) != 0u;
            buttons.b = ((action >> 10) & 0x3u) == 1u || ((action >> 10) & 0x3u) == 2u;
            return buttons;
        }
    };

    struct PeerState
    {
        std::string name;
        bool host = false;
        GeraNESEmu emu{DummyAudioOutput::instance()};
        Netplay::NetplayRuntime runtime;
        Netplay::NetplayCoordinator coordinator;
        std::array<std::deque<uint64_t>, 4> inputDelayBuffers;
        uint8_t lastInputDelayFrames = 0xFF;
        uint32_t lastCrcReportFrame = 0;
        bool desyncInjected = false;
        uint32_t lastResyncTargetFrame = 0;
        uint32_t lastResyncLoadedFrameCount = 0;
        DeterministicInputGenerator inputGenerator;

        explicit PeerState(const std::string& peerName, bool isHost, uint32_t inputSeed)
            : name(peerName)
            , host(isHost)
            , inputGenerator(inputSeed)
        {
        }
    };

    static constexpr int RESULT_FAILED = 1;
    static constexpr int RESULT_ERROR = 2;

    static uint64_t buildPadMask(const Buttons& buttons)
    {
        uint64_t mask = 0;
        if(buttons.a) mask |= (1ull << 0);
        if(buttons.b) mask |= (1ull << 1);
        if(buttons.select) mask |= (1ull << 2);
        if(buttons.start) mask |= (1ull << 3);
        if(buttons.up) mask |= (1ull << 4);
        if(buttons.down) mask |= (1ull << 5);
        if(buttons.left) mask |= (1ull << 6);
        if(buttons.right) mask |= (1ull << 7);
        return mask;
    }

    static std::string stateLabel(Netplay::SessionState state)
    {
        switch(state) {
            case Netplay::SessionState::Lobby: return "Lobby";
            case Netplay::SessionState::ValidatingRom: return "ValidatingRom";
            case Netplay::SessionState::ReadyCheck: return "ReadyCheck";
            case Netplay::SessionState::Starting: return "Starting";
            case Netplay::SessionState::Running: return "Running";
            case Netplay::SessionState::Resyncing: return "Resyncing";
            case Netplay::SessionState::Paused: return "Paused";
            case Netplay::SessionState::Ended: return "Ended";
        }
        return "Unknown";
    }

    static void resetInputDelayBuffers(PeerState& peer)
    {
        for(auto& buffer : peer.inputDelayBuffers) {
            buffer.clear();
        }
    }

    static uint64_t sampleDelayedPadMask(PeerState& peer, Netplay::PlayerSlot slot, uint64_t rawMask)
    {
        const uint8_t inputDelayFrames = peer.coordinator.session().roomState().inputDelayFrames;
        if(peer.lastInputDelayFrames != inputDelayFrames) {
            resetInputDelayBuffers(peer);
            peer.lastInputDelayFrames = inputDelayFrames;
        }

        std::deque<uint64_t>& buffer = peer.inputDelayBuffers[slot];
        buffer.push_back(rawMask);
        if(buffer.size() <= inputDelayFrames) {
            return 0;
        }

        const uint64_t delayedMask = buffer.front();
        buffer.pop_front();
        return delayedMask;
    }

    static void applyPadMaskToInputState(InputState& state, Netplay::PlayerSlot slot, uint64_t mask)
    {
        const bool a = (mask & (1ull << 0)) != 0;
        const bool b = (mask & (1ull << 1)) != 0;
        const bool select = (mask & (1ull << 2)) != 0;
        const bool start = (mask & (1ull << 3)) != 0;
        const bool up = (mask & (1ull << 4)) != 0;
        const bool down = (mask & (1ull << 5)) != 0;
        const bool left = (mask & (1ull << 6)) != 0;
        const bool right = (mask & (1ull << 7)) != 0;

        switch(slot) {
            case 0:
                state.p1A = a; state.p1B = b; state.p1Select = select; state.p1Start = start;
                state.p1Up = up; state.p1Down = down; state.p1Left = left; state.p1Right = right;
                break;
            case 1:
                state.p2A = a; state.p2B = b; state.p2Select = select; state.p2Start = start;
                state.p2Up = up; state.p2Down = down; state.p2Left = left; state.p2Right = right;
                break;
            case 2:
                state.p3A = a; state.p3B = b; state.p3Select = select; state.p3Start = start;
                state.p3Up = up; state.p3Down = down; state.p3Left = left; state.p3Right = right;
                break;
            case 3:
                state.p4A = a; state.p4B = b; state.p4Select = select; state.p4Start = start;
                state.p4Up = up; state.p4Down = down; state.p4Left = left; state.p4Right = right;
                break;
            default:
                break;
        }
    }

    static void applyInputStateToEmu(GeraNESEmu& emu, const InputState& state)
    {
        emu.setController1Buttons(state.p1A, state.p1B, state.p1Select, state.p1Start, state.p1Up, state.p1Down, state.p1Left, state.p1Right);
        emu.setController2Buttons(state.p2A, state.p2B, state.p2Select, state.p2Start, state.p2Up, state.p2Down, state.p2Left, state.p2Right);
        emu.setController3Buttons(state.p3A, state.p3B, state.p3Select, state.p3Start, state.p3Up, state.p3Down, state.p3Left, state.p3Right);
        emu.setController4Buttons(state.p4A, state.p4B, state.p4Select, state.p4Start, state.p4Up, state.p4Down, state.p4Left, state.p4Right);
    }

    static std::optional<Netplay::RomValidationData> captureRomValidation(GeraNESEmu& emu)
    {
        if(!emu.valid()) return std::nullopt;

        Cartridge& cart = emu.getConsole().cartridge();
        Netplay::RomValidationData validation;
        validation.romCrc32 = cart.romFile().fileCrc32();
        validation.mapperId = static_cast<uint16_t>(std::max(0, cart.mapperId()));
        validation.subMapperId = static_cast<uint16_t>(std::max(0, cart.subMapperId()));
        validation.prgRomSize = static_cast<uint32_t>(std::max(0, cart.prgSize()));
        validation.chrRomSize = static_cast<uint32_t>(std::max(0, cart.chrSize()));
        validation.chrRamSize = static_cast<uint32_t>(std::max(0, cart.chrRamSize()));
        validation.fileSize = static_cast<uint32_t>(cart.romFile().size());
        return validation;
    }

    static bool tryResolvePadMask(const PeerState& peer, Netplay::FrameNumber frame, Netplay::PlayerSlot slot, uint64_t& outMask)
    {
        if(!peer.coordinator.isActive()) return false;

        const auto& room = peer.coordinator.session().roomState();
        const auto& localInputs = peer.coordinator.localInputs();
        const auto& remoteInputs = peer.coordinator.remoteInputs();
        const Netplay::ParticipantId localParticipant = peer.coordinator.localParticipantId();

        for(const auto& participant : room.participants) {
            if(participant.controllerAssignment != slot) continue;

            const Netplay::TimelineInputEntry* entry =
                participant.id == localParticipant
                    ? localInputs.find(frame, participant.id, slot)
                    : remoteInputs.find(frame, participant.id, slot);

            outMask = entry != nullptr ? entry->buttonMaskLo : 0;
            return true;
        }

        return false;
    }

    static InputState buildReplayInputState(const PeerState& peer, Netplay::FrameNumber frame)
    {
        InputState state{};
        for(Netplay::PlayerSlot slot = 0; slot < 4; ++slot) {
            uint64_t mask = 0;
            if(tryResolvePadMask(peer, frame, slot, mask)) {
                applyPadMaskToInputState(state, slot, mask);
            }
        }
        return state;
    }

    static void configurePeerRuntime(PeerState& peer, size_t rollbackWindowFrames)
    {
        peer.runtime.reset();
        peer.runtime.configureRollbackWindow(rollbackWindowFrames);
        peer.lastCrcReportFrame = 0;
        resetInputDelayBuffers(peer);
        peer.lastInputDelayFrames = 0xFF;
    }

    static bool advancePeerToNextFrame(PeerState& peer, const InputState& state)
    {
        if(!peer.emu.valid()) return false;

        applyInputStateToEmu(peer.emu, state);
        const uint32_t previousFrame = peer.emu.frameCount();
        const uint32_t frameDt = std::max<uint32_t>(1, 1000u / std::max<uint32_t>(1, peer.emu.getRegionFPS()));
        while(peer.emu.valid() && peer.emu.frameCount() == previousFrame) {
            peer.emu.update(frameDt);
        }

        if(!peer.emu.valid()) {
            return false;
        }

        const uint32_t newFrame = peer.emu.frameCount();
        peer.runtime.setCurrentFrame(newFrame);
        peer.runtime.captureSnapshot(newFrame, [&peer]() {
            return peer.emu.saveStateToMemory();
        });
        return true;
    }

    static void processRollbackIfNeeded(PeerState& peer)
    {
        std::optional<Netplay::FrameNumber> rollbackFrame = peer.coordinator.consumePendingRollbackFrame();
        if(!rollbackFrame.has_value()) return;

        const uint32_t currentFrame = peer.emu.frameCount();
        if(currentFrame == 0) return;

        const Netplay::FrameNumber confirmedFrame = peer.coordinator.session().roomState().lastConfirmedFrame;
        const Netplay::FrameNumber latestSafeRollbackFrame = static_cast<Netplay::FrameNumber>(currentFrame - 1u);
        const Netplay::FrameNumber rollbackFloor = std::min(confirmedFrame, latestSafeRollbackFrame);
        if(*rollbackFrame < rollbackFloor) {
            rollbackFrame = rollbackFloor;
        }

        if(*rollbackFrame >= currentFrame) return;

        if(!peer.runtime.rollbackTo(*rollbackFrame, [&peer](const std::vector<uint8_t>& data) {
            peer.emu.loadStateFromMemoryOnCleanBoot(data);
        })) {
            return;
        }

        peer.coordinator.invalidateLocalCrcHistoryAfter(*rollbackFrame);
        if(peer.lastCrcReportFrame > *rollbackFrame) {
            peer.lastCrcReportFrame = *rollbackFrame;
        }

        while(peer.emu.frameCount() < currentFrame) {
            const uint32_t replayFrame = peer.emu.frameCount() + 1u;
            const InputState replayInput = buildReplayInputState(peer, replayFrame);
            if(!advancePeerToNextFrame(peer, replayInput)) {
                return;
            }
        }
    }

    static void processPendingResyncIfNeeded(PeerState& peer, size_t rollbackWindowFrames)
    {
        std::optional<Netplay::NetplayCoordinator::PendingResyncApply> pending = peer.coordinator.consumePendingResyncApply();
        if(!pending.has_value()) return;

        const bool loaded = !pending->payload.empty() && peer.emu.valid();
        if(loaded) {
            peer.emu.loadStateFromMemoryOnCleanBoot(pending->payload);
            peer.lastResyncTargetFrame = pending->targetFrame;
            peer.lastResyncLoadedFrameCount = peer.emu.frameCount();
            configurePeerRuntime(peer, rollbackWindowFrames);
            peer.runtime.setCurrentFrame(pending->targetFrame);
            peer.runtime.captureSnapshot(pending->targetFrame, [&pending]() {
                return pending->payload;
            });
        }

        const uint32_t loadedCrc32 = loaded ? peer.emu.canonicalStateCrc32() : 0;
        peer.coordinator.acknowledgeResync(pending->resyncId, pending->targetFrame, loadedCrc32, loaded);
    }

    static void processHostResyncIfNeeded(PeerState& peer)
    {
        if(!peer.coordinator.isHosting()) return;

        std::optional<Netplay::FrameNumber> pendingFrame = peer.coordinator.consumePendingHostResyncFrame();
        if(!pendingFrame.has_value()) return;
        if(!peer.emu.valid()) return;

        const Netplay::FrameNumber targetFrame = peer.coordinator.session().roomState().lastConfirmedFrame;
        const Netplay::SnapshotRecord* snapshot = peer.runtime.snapshots().find(targetFrame);
        const std::vector<uint8_t> payload =
            snapshot != nullptr ? snapshot->data : peer.emu.saveStateToMemory();
        if(payload.empty()) return;

        if(targetFrame < peer.emu.frameCount()) {
            if(!peer.runtime.rollbackTo(targetFrame, [&peer](const std::vector<uint8_t>& data) {
                peer.emu.loadStateFromMemoryOnCleanBoot(data);
            })) {
                return;
            }
        }
        peer.lastResyncTargetFrame = targetFrame;
        peer.lastResyncLoadedFrameCount = peer.emu.frameCount();

        resetInputDelayBuffers(peer);
        peer.lastInputDelayFrames = 0xFF;

        const uint32_t payloadCrc32 = Crc32::calc(reinterpret_cast<const char*>(payload.data()), payload.size());
        peer.coordinator.beginResync(targetFrame, payload, payloadCrc32);
    }

    static void syncLocalRomValidation(PeerState& peer, const std::optional<Netplay::RomValidationData>& validation)
    {
        const auto& room = peer.coordinator.session().roomState();
        const bool romLoaded = validation.has_value();
        const bool romCompatible =
            romLoaded &&
            !room.selectedGameName.empty() &&
            Netplay::NetplayCoordinator::romValidationMatches(*validation, room.romValidation);

        peer.coordinator.submitLocalRomValidation(
            romLoaded,
            romCompatible,
            validation.value_or(Netplay::RomValidationData{})
        );
    }

    static void maybeSubmitCrc(PeerState& peer, uint32_t crcIntervalFrames)
    {
        if(!peer.coordinator.isActive()) {
            peer.lastCrcReportFrame = 0;
            return;
        }

        if(peer.coordinator.session().roomState().state != Netplay::SessionState::Running) {
            peer.lastCrcReportFrame = 0;
            return;
        }

        const uint32_t confirmedFrame = peer.coordinator.session().roomState().lastConfirmedFrame;
        if(confirmedFrame == 0 || confirmedFrame == peer.lastCrcReportFrame) return;
        if((confirmedFrame % std::max<uint32_t>(1, crcIntervalFrames)) != 0u) return;

        const std::optional<uint32_t> snapshotCrc32 = peer.runtime.snapshots().crc32ForFrame(confirmedFrame);
        if(!snapshotCrc32.has_value()) return;

        peer.coordinator.submitLocalCrc(confirmedFrame, *snapshotCrc32);
        peer.lastCrcReportFrame = confirmedFrame;
    }

    static bool netplayStateBlocksSimulation(const PeerState& peer)
    {
        return peer.coordinator.isActive() &&
               (peer.coordinator.awaitingSpectatorSync() ||
                peer.coordinator.session().roomState().state != Netplay::SessionState::Running);
    }

    static bool clientIsTooFarAhead(const PeerState& peer)
    {
        if(!peer.coordinator.isActive() || peer.coordinator.isHosting()) return false;
        if(peer.coordinator.session().roomState().state != Netplay::SessionState::Running) return false;

        const uint32_t authoritativeFrame = peer.coordinator.session().roomState().currentFrame;
        return authoritativeFrame > 0 && const_cast<GeraNESEmu&>(peer.emu).frameCount() > authoritativeFrame + 1u;
    }

    static bool canSimulateFrame(const PeerState& peer)
    {
        return !(netplayStateBlocksSimulation(peer) || clientIsTooFarAhead(peer));
    }

    static bool queueLocalInputForNextFrame(PeerState& peer, uint32_t maxFrame)
    {
        if(!canSimulateFrame(peer)) {
            return false;
        }
        if(peer.emu.frameCount() >= maxFrame) {
            return false;
        }

        const auto& room = peer.coordinator.session().roomState();
        const Netplay::ParticipantId localParticipantId = peer.coordinator.localParticipantId();
        const Netplay::ParticipantInfo* localParticipant = nullptr;
        for(const auto& participant : room.participants) {
            if(participant.id == localParticipantId) {
                localParticipant = &participant;
                break;
            }
        }

        const uint32_t playbackFrame = peer.emu.frameCount() + 1u;
        if(localParticipant != nullptr && localParticipant->controllerAssignment != Netplay::kObserverPlayerSlot) {
            const Netplay::PlayerSlot localSlot = localParticipant->controllerAssignment;
            const Netplay::TimelineInputEntry* existing =
                peer.coordinator.localInputs().find(playbackFrame, peer.coordinator.localParticipantId(), localSlot);
            if(existing == nullptr) {
                const Buttons buttons = peer.inputGenerator.buttonsForFrame(playbackFrame, std::max<uint32_t>(1, peer.emu.getRegionFPS()));
                const uint64_t rawMask = buildPadMask(buttons);
                peer.coordinator.recordLocalInputFrame(
                    playbackFrame,
                    localSlot,
                    sampleDelayedPadMask(peer, localSlot, rawMask)
                );
            }
        } else {
            resetInputDelayBuffers(peer);
            peer.lastInputDelayFrames = 0xFF;
        }

        peer.coordinator.predictRemoteInputsForFrame(playbackFrame);
        return true;
    }

    static bool advanceQueuedFrameIfPossible(PeerState& peer, uint32_t maxFrame)
    {
        if(!canSimulateFrame(peer)) {
            return false;
        }
        if(peer.emu.frameCount() >= maxFrame) {
            return false;
        }

        const uint32_t playbackFrame = peer.emu.frameCount() + 1u;
        const InputState state = buildReplayInputState(peer, playbackFrame);
        return advancePeerToNextFrame(peer, state);
    }

    static std::optional<Netplay::ParticipantId> findRemoteParticipantId(const PeerState& peer)
    {
        for(const auto& participant : peer.coordinator.session().roomState().participants) {
            if(participant.id != peer.coordinator.localParticipantId()) {
                return participant.id;
            }
        }
        return std::nullopt;
    }

    static bool bootstrapSession(PeerState& hostPeer,
                                 PeerState& clientPeer,
                                 const Options& options,
                                 const std::optional<Netplay::RomValidationData>& romValidation,
                                 std::string& failureReason)
    {
        bool hostRomSelected = false;
        bool hostAssigned = false;
        bool readyMarked = false;
        bool sessionStarted = false;

        for(uint32_t step = 0; step < options.startupTimeoutSteps; ++step) {
            hostPeer.coordinator.update(0);
            clientPeer.coordinator.update(0);

            syncLocalRomValidation(hostPeer, romValidation);
            syncLocalRomValidation(clientPeer, romValidation);

            processHostResyncIfNeeded(hostPeer);
            processPendingResyncIfNeeded(hostPeer, options.rollbackWindowFrames);
            processPendingResyncIfNeeded(clientPeer, options.rollbackWindowFrames);

            if(!hostRomSelected && romValidation.has_value() && hostPeer.coordinator.isConnected()) {
                Cartridge& cart = hostPeer.emu.getConsole().cartridge();
                hostRomSelected = hostPeer.coordinator.selectRom(cart.romFile().fileName(), *romValidation);
            }

            const std::optional<Netplay::ParticipantId> remoteId = findRemoteParticipantId(hostPeer);
            if(!hostAssigned && remoteId.has_value()) {
                const bool assignedLocal = hostPeer.coordinator.assignController(hostPeer.coordinator.localParticipantId(), 0);
                const bool assignedRemote = hostPeer.coordinator.assignController(*remoteId, 1);
                hostAssigned = assignedLocal && assignedRemote;
            }

            if(hostAssigned && !readyMarked) {
                const bool hostReady = hostPeer.coordinator.setLocalReady(true);
                const bool clientReady = clientPeer.coordinator.setLocalReady(true);
                readyMarked = hostReady && clientReady;
            }

            if(readyMarked && !sessionStarted && hostPeer.coordinator.session().roomState().state == Netplay::SessionState::ReadyCheck) {
                sessionStarted = hostPeer.coordinator.startSession();
            }

            if(hostPeer.coordinator.session().roomState().state == Netplay::SessionState::Running &&
               clientPeer.coordinator.session().roomState().state == Netplay::SessionState::Running) {
                return true;
            }
        }

        std::ostringstream failure;
        failure << "Session bootstrap timed out. Host state="
                << stateLabel(hostPeer.coordinator.session().roomState().state)
                << ", Client state="
                << stateLabel(clientPeer.coordinator.session().roomState().state);
        failureReason = failure.str();
        return false;
    }

    static nlohmann::json buildPeerReport(const PeerState& peer)
    {
        const auto& room = peer.coordinator.session().roomState();
        const auto& runtimeStats = peer.runtime.stats();
        const auto& predictionStats = peer.coordinator.predictionStats();

        nlohmann::json participants = nlohmann::json::array();
        for(const auto& participant : room.participants) {
            participants.push_back({
                {"id", participant.id},
                {"name", participant.displayName},
                {"connected", participant.connected},
                {"role", static_cast<int>(participant.role)},
                {"controllerAssignment", participant.controllerAssignment},
                {"romLoaded", participant.romLoaded},
                {"romCompatible", participant.romCompatible},
                {"ready", participant.ready},
                {"lastReceivedInputFrame", participant.lastReceivedInputFrame},
                {"lastContiguousInputFrame", participant.lastContiguousInputFrame},
                {"lastDecision", participant.lastDecision},
                {"confirmedFrameConflictCount", participant.confirmedFrameConflictCount},
                {"rollbackScheduledCount", participant.rollbackScheduledCount},
                {"futureFrameMismatchCount", participant.futureFrameMismatchCount}
            });
        }

        return {
            {"name", peer.name},
            {"host", peer.host},
            {"frame", const_cast<GeraNESEmu&>(peer.emu).frameCount()},
            {"crc32", const_cast<GeraNESEmu&>(peer.emu).canonicalStateCrc32()},
            {"roomState", stateLabel(room.state)},
            {"currentFrame", room.currentFrame},
            {"lastConfirmedFrame", room.lastConfirmedFrame},
            {"lastRemoteCrcFrame", room.lastRemoteCrcFrame},
            {"lastRemoteCrc32", room.lastRemoteCrc32},
            {"runtimeRollbackCount", runtimeStats.rollbackCount},
            {"runtimeMaxRollbackDistance", runtimeStats.maxRollbackDistance},
            {"predictionHitCount", predictionStats.predictionHitCount},
            {"predictionMissCount", predictionStats.predictionMissCount},
            {"hardResyncCount", predictionStats.hardResyncCount},
            {"rollbackScheduledCount", predictionStats.rollbackScheduledCount},
            {"missingInputGapCount", predictionStats.missingInputGapCount},
            {"futureFrameMismatchCount", predictionStats.futureFrameMismatchCount},
            {"confirmedFrameConflictCount", predictionStats.confirmedFrameConflictCount},
            {"desyncInjected", peer.desyncInjected},
            {"lastResyncTargetFrame", peer.lastResyncTargetFrame},
            {"lastResyncLoadedFrameCount", peer.lastResyncLoadedFrameCount},
            {"participants", participants},
            {"localTimelineSize", peer.coordinator.localInputs().size()},
            {"remoteTimelineSize", peer.coordinator.remoteInputs().size()},
            {"eventLogTail", [&peer]() {
                nlohmann::json tail = nlohmann::json::array();
                const auto& log = peer.coordinator.eventLog();
                const size_t start = log.size() > 20 ? (log.size() - 20) : 0;
                for(size_t i = start; i < log.size(); ++i) {
                    tail.push_back(log[i]);
                }
                return tail;
            }()},
            {"localTimelineTail", [&peer]() {
                nlohmann::json tail = nlohmann::json::array();
                const auto& entries = peer.coordinator.localInputs().entries();
                const size_t start = entries.size() > 12 ? (entries.size() - 12) : 0;
                for(size_t i = start; i < entries.size(); ++i) {
                    const auto& entry = entries[i];
                    tail.push_back({
                        {"frame", entry.frame},
                        {"participantId", entry.participantId},
                        {"slot", entry.playerSlot},
                        {"maskLo", entry.buttonMaskLo},
                        {"sequence", entry.sequence},
                        {"predicted", entry.predicted},
                        {"confirmed", entry.confirmed}
                    });
                }
                return tail;
            }()},
            {"remoteTimelineTail", [&peer]() {
                nlohmann::json tail = nlohmann::json::array();
                const auto& entries = peer.coordinator.remoteInputs().entries();
                const size_t start = entries.size() > 12 ? (entries.size() - 12) : 0;
                for(size_t i = start; i < entries.size(); ++i) {
                    const auto& entry = entries[i];
                    tail.push_back({
                        {"frame", entry.frame},
                        {"participantId", entry.participantId},
                        {"slot", entry.playerSlot},
                        {"maskLo", entry.buttonMaskLo},
                        {"sequence", entry.sequence},
                        {"predicted", entry.predicted},
                        {"confirmed", entry.confirmed}
                    });
                }
                return tail;
            }()}
        };
    }

    static std::optional<std::string> findTimelineMismatch(const PeerState& hostPeer, const PeerState& clientPeer)
    {
        const uint32_t confirmedFrame = std::min(
            hostPeer.coordinator.session().roomState().lastConfirmedFrame,
            clientPeer.coordinator.session().roomState().lastConfirmedFrame
        );

        for(uint32_t frame = 1; frame <= confirmedFrame; ++frame) {
            for(Netplay::PlayerSlot slot = 0; slot < 4; ++slot) {
                const auto findEntry = [&](const PeerState& peer) -> const Netplay::TimelineInputEntry* {
                    const auto& room = peer.coordinator.session().roomState();
                    const auto& localInputs = peer.coordinator.localInputs();
                    const auto& remoteInputs = peer.coordinator.remoteInputs();
                    const Netplay::ParticipantId localParticipant = peer.coordinator.localParticipantId();

                    for(const auto& participant : room.participants) {
                        if(participant.controllerAssignment != slot) continue;
                        return participant.id == localParticipant
                            ? localInputs.find(frame, participant.id, slot)
                            : remoteInputs.find(frame, participant.id, slot);
                    }

                    return nullptr;
                };

                const Netplay::TimelineInputEntry* hostEntry = findEntry(hostPeer);
                const Netplay::TimelineInputEntry* clientEntry = findEntry(clientPeer);
                if(hostEntry == nullptr || clientEntry == nullptr) {
                    continue;
                }
                if(hostEntry->buttonMaskLo != clientEntry->buttonMaskLo) {
                    std::ostringstream ss;
                    ss << "Timeline mismatch at frame " << frame << ", slot " << static_cast<int>(slot)
                       << " (hostMask=" << hostEntry->buttonMaskLo
                       << ", clientMask=" << clientEntry->buttonMaskLo << ")";
                    return ss.str();
                }
            }
        }

        return std::nullopt;
    }

    static nlohmann::json buildSnapshotMismatchReport(const PeerState& hostPeer, const PeerState& clientPeer)
    {
        const uint32_t maxFrame = std::min(
            const_cast<GeraNESEmu&>(hostPeer.emu).frameCount(),
            const_cast<GeraNESEmu&>(clientPeer.emu).frameCount()
        );

        for(uint32_t frame = 1; frame <= maxFrame; ++frame) {
            const std::optional<uint32_t> hostCrc = hostPeer.runtime.snapshots().crc32ForFrame(frame);
            const std::optional<uint32_t> clientCrc = clientPeer.runtime.snapshots().crc32ForFrame(frame);
            if(!hostCrc.has_value() || !clientCrc.has_value()) continue;
            if(*hostCrc == *clientCrc) continue;

            return {
                {"frame", frame},
                {"hostCrc32", *hostCrc},
                {"clientCrc32", *clientCrc}
            };
        }

        return {
            {"frame", 0},
            {"hostCrc32", 0},
            {"clientCrc32", 0}
        };
    }

    static nlohmann::json buildTimelineWindow(const Netplay::InputTimeline& timeline,
                                              Netplay::FrameNumber frameStart,
                                              Netplay::FrameNumber frameEnd)
    {
        nlohmann::json entries = nlohmann::json::array();
        for(const auto& entry : timeline.entries()) {
            if(entry.frame < frameStart || entry.frame > frameEnd) continue;
            entries.push_back({
                {"frame", entry.frame},
                {"participantId", entry.participantId},
                {"slot", entry.playerSlot},
                {"maskLo", entry.buttonMaskLo},
                {"sequence", entry.sequence},
                {"predicted", entry.predicted},
                {"confirmed", entry.confirmed}
            });
        }
        return entries;
    }

    static nlohmann::json buildTimelineDebugReport(const PeerState& hostPeer, const PeerState& clientPeer)
    {
        const nlohmann::json snapshotMismatch = buildSnapshotMismatchReport(hostPeer, clientPeer);
        const uint32_t mismatchFrame = snapshotMismatch.value("frame", 0u);
        const uint32_t centerFrame = mismatchFrame > 0 ? mismatchFrame : std::min(
            const_cast<GeraNESEmu&>(hostPeer.emu).frameCount(),
            const_cast<GeraNESEmu&>(clientPeer.emu).frameCount()
        );
        const uint32_t frameStart = centerFrame > 2 ? (centerFrame - 2u) : 1u;
        const uint32_t frameEnd = centerFrame + 3u;

        return {
            {"frameStart", frameStart},
            {"frameEnd", frameEnd},
            {"hostLocal", buildTimelineWindow(hostPeer.coordinator.localInputs(), frameStart, frameEnd)},
            {"hostRemote", buildTimelineWindow(hostPeer.coordinator.remoteInputs(), frameStart, frameEnd)},
            {"clientLocal", buildTimelineWindow(clientPeer.coordinator.localInputs(), frameStart, frameEnd)},
            {"clientRemote", buildTimelineWindow(clientPeer.coordinator.remoteInputs(), frameStart, frameEnd)}
        };
    }

    static nlohmann::json buildStateLoadProbe(const Options& options,
                                             const PeerState& peer,
                                             uint32_t frame)
    {
        const Netplay::SnapshotRecord* snapshot = peer.runtime.snapshots().find(frame);
        if(snapshot == nullptr) {
            return {
                {"requestedFrame", frame},
                {"snapshotFound", false}
            };
        }

        GeraNESEmu probeEmu{DummyAudioOutput::instance()};
        const bool opened = probeEmu.open(options.romPath) && probeEmu.valid();
        if(!opened) {
            return {
                {"requestedFrame", frame},
                {"snapshotFound", true},
                {"romOpened", false}
            };
        }

        probeEmu.loadStateFromMemory(snapshot->data);
        return {
            {"requestedFrame", frame},
            {"snapshotFound", true},
            {"romOpened", true},
            {"loadedFrameCount", probeEmu.frameCount()},
            {"snapshotCrc32", snapshot->crc32},
            {"loadedStateCrc32", probeEmu.canonicalStateCrc32()}
        };
    }

    static nlohmann::json buildSnapshotCrcWindow(const PeerState& hostPeer,
                                                 const PeerState& clientPeer,
                                                 uint32_t centerFrame)
    {
        const uint32_t frameStart = centerFrame > 2 ? (centerFrame - 2u) : 0u;
        const uint32_t frameEnd = centerFrame + 4u;
        nlohmann::json entries = nlohmann::json::array();

        for(uint32_t frame = frameStart; frame <= frameEnd; ++frame) {
            entries.push_back({
                {"frame", frame},
                {"hostSnapshotCrc32", hostPeer.runtime.snapshots().crc32ForFrame(frame).value_or(0u)},
                {"clientSnapshotCrc32", clientPeer.runtime.snapshots().crc32ForFrame(frame).value_or(0u)}
            });
        }

        return {
            {"frameStart", frameStart},
            {"frameEnd", frameEnd},
            {"entries", entries}
        };
    }

    static nlohmann::json buildReplayProbe(const Options& options,
                                           const PeerState& peer,
                                           uint32_t fromFrame,
                                           uint32_t toFrame)
    {
        const Netplay::SnapshotRecord* startSnapshot = peer.runtime.snapshots().find(fromFrame);
        const Netplay::SnapshotRecord* targetSnapshot = peer.runtime.snapshots().find(toFrame);
        if(startSnapshot == nullptr || targetSnapshot == nullptr || toFrame < fromFrame) {
            return {
                {"fromFrame", fromFrame},
                {"toFrame", toFrame},
                {"ready", false}
            };
        }

        GeraNESEmu probeEmu{DummyAudioOutput::instance()};
        if(!probeEmu.open(options.romPath) || !probeEmu.valid()) {
            return {
                {"fromFrame", fromFrame},
                {"toFrame", toFrame},
                {"ready", false},
                {"romOpened", false}
            };
        }

        probeEmu.loadStateFromMemory(startSnapshot->data);
        const uint32_t frameDt = std::max<uint32_t>(1, 1000u / std::max<uint32_t>(1, probeEmu.getRegionFPS()));
        while(probeEmu.valid() && probeEmu.frameCount() < toFrame) {
            const uint32_t nextFrame = probeEmu.frameCount() + 1u;
            const InputState replayInput = buildReplayInputState(peer, nextFrame);
            applyInputStateToEmu(probeEmu, replayInput);
            const uint32_t previousFrame = probeEmu.frameCount();
            while(probeEmu.valid() && probeEmu.frameCount() == previousFrame) {
                probeEmu.update(frameDt);
            }
        }

        return {
            {"fromFrame", fromFrame},
            {"toFrame", toFrame},
            {"ready", true},
            {"romOpened", true},
            {"resultFrameCount", probeEmu.frameCount()},
            {"replayedCrc32", probeEmu.canonicalStateCrc32()},
            {"targetSnapshotCrc32", targetSnapshot->crc32}
        };
    }

    static int finalizeReport(const Options& options,
                              const PeerState& hostPeer,
                              const PeerState& clientPeer,
                              const std::string& status,
                              const std::string& failureReason,
                              const std::optional<std::string>& timelineMismatch)
    {
        const uint32_t hostCrc = const_cast<GeraNESEmu&>(hostPeer.emu).valid()
            ? const_cast<GeraNESEmu&>(hostPeer.emu).canonicalStateCrc32()
            : 0;
        const uint32_t clientCrc = const_cast<GeraNESEmu&>(clientPeer.emu).valid()
            ? const_cast<GeraNESEmu&>(clientPeer.emu).canonicalStateCrc32()
            : 0;
        const nlohmann::json snapshotMismatch = buildSnapshotMismatchReport(hostPeer, clientPeer);
        const uint32_t mismatchFrame = snapshotMismatch.value("frame", 0u);
        const uint32_t adjacentProbeFromFrame = mismatchFrame > 0 ? (mismatchFrame - 1u) : 0u;

        nlohmann::json report = {
            {"status", status},
            {"failureReason", failureReason},
            {"romPath", options.romPath},
            {"framesRequested", options.frames},
            {"inputDelayFrames", options.inputDelayFrames},
            {"rollbackWindowFrames", options.rollbackWindowFrames},
            {"crcIntervalFrames", options.crcIntervalFrames},
            {"forceDesyncFrame", options.forceDesyncFrame},
            {"finalCrcMatch", hostCrc == clientCrc},
            {"timelineMismatch", timelineMismatch.has_value() ? *timelineMismatch : ""},
            {"snapshotMismatch", snapshotMismatch},
            {"hostStateLoadProbe", buildStateLoadProbe(options, hostPeer, hostPeer.lastResyncTargetFrame)},
            {"clientStateLoadProbe", buildStateLoadProbe(options, clientPeer, clientPeer.lastResyncTargetFrame)},
            {"snapshotCrcWindow", buildSnapshotCrcWindow(
                hostPeer,
                clientPeer,
                std::max<uint32_t>(hostPeer.lastResyncTargetFrame, clientPeer.lastResyncTargetFrame)
            )},
            {"hostReplayProbe", buildReplayProbe(
                options,
                hostPeer,
                hostPeer.lastResyncTargetFrame,
                mismatchFrame
            )},
            {"clientReplayProbe", buildReplayProbe(
                options,
                clientPeer,
                clientPeer.lastResyncTargetFrame,
                mismatchFrame
            )},
            {"hostAdjacentReplayProbe", buildReplayProbe(
                options,
                hostPeer,
                adjacentProbeFromFrame,
                mismatchFrame
            )},
            {"clientAdjacentReplayProbe", buildReplayProbe(
                options,
                clientPeer,
                adjacentProbeFromFrame,
                mismatchFrame
            )},
            {"timelineDebug", buildTimelineDebugReport(hostPeer, clientPeer)},
            {"host", buildPeerReport(hostPeer)},
            {"client", buildPeerReport(clientPeer)}
        };

        const std::string serialized = report.dump(2);
        if(!options.reportPath.empty()) {
            std::ofstream out(options.reportPath, std::ios::binary | std::ios::trunc);
            out << serialized;
            std::cout << options.reportPath << std::endl;
        } else {
            std::cout << serialized << std::endl;
        }

        return status == "ok" ? 0 : RESULT_FAILED;
    }

public:
    static int runHeadless(const Options& options)
    {
        if(options.romPath.empty()) {
            std::cerr << "Netplay test requires a ROM path." << std::endl;
            return RESULT_ERROR;
        }

        PeerState hostPeer("Host", true, 0x13572468u);
        PeerState clientPeer("Client", false, 0x24681357u);

        hostPeer.emu.setSpeedBoost(false);
        clientPeer.emu.setSpeedBoost(false);
        hostPeer.emu.setPaused(false);
        clientPeer.emu.setPaused(false);

        if(!hostPeer.emu.open(options.romPath) || !hostPeer.emu.valid()) {
            std::cerr << "Failed to open ROM for host peer." << std::endl;
            return RESULT_ERROR;
        }
        if(!clientPeer.emu.open(options.romPath) || !clientPeer.emu.valid()) {
            std::cerr << "Failed to open ROM for client peer." << std::endl;
            return RESULT_ERROR;
        }

        configurePeerRuntime(hostPeer, options.rollbackWindowFrames);
        configurePeerRuntime(clientPeer, options.rollbackWindowFrames);

        const std::optional<Netplay::RomValidationData> romValidation = captureRomValidation(hostPeer.emu);
        if(!romValidation.has_value()) {
            std::cerr << "Failed to capture ROM validation data." << std::endl;
            return RESULT_ERROR;
        }

        if(!hostPeer.coordinator.host(static_cast<uint16_t>(options.port), 2, "Host")) {
            std::cerr << "Failed to start netplay host: " << hostPeer.coordinator.lastError() << std::endl;
            return RESULT_ERROR;
        }

        if(!clientPeer.coordinator.join("127.0.0.1", static_cast<uint16_t>(options.port), "Client")) {
            std::cerr << "Failed to connect netplay client: " << clientPeer.coordinator.lastError() << std::endl;
            return RESULT_ERROR;
        }

        hostPeer.coordinator.setInputDelayFrames(static_cast<uint8_t>(std::min<uint32_t>(options.inputDelayFrames, 8u)));

        std::string failureReason;
        if(!bootstrapSession(hostPeer, clientPeer, options, romValidation, failureReason)) {
            return finalizeReport(options, hostPeer, clientPeer, "bootstrap_failed", failureReason, std::nullopt);
        }

        uint32_t progressedFrames = 0;
        for(uint32_t step = 0; step < options.frameStepLimit && progressedFrames < options.frames; ++step) {
            const uint32_t previousHostFrame = hostPeer.emu.frameCount();
            const uint32_t previousClientFrame = clientPeer.emu.frameCount();

            hostPeer.coordinator.update(0);
            clientPeer.coordinator.update(0);

            syncLocalRomValidation(hostPeer, romValidation);
            syncLocalRomValidation(clientPeer, romValidation);

            processHostResyncIfNeeded(hostPeer);
            processPendingResyncIfNeeded(hostPeer, options.rollbackWindowFrames);
            processPendingResyncIfNeeded(clientPeer, options.rollbackWindowFrames);
            processRollbackIfNeeded(hostPeer);
            processRollbackIfNeeded(clientPeer);

            maybeSubmitCrc(hostPeer, options.crcIntervalFrames);
            maybeSubmitCrc(clientPeer, options.crcIntervalFrames);

            const bool hostQueued = queueLocalInputForNextFrame(hostPeer, options.frames);
            const bool clientQueued = queueLocalInputForNextFrame(clientPeer, options.frames);

            if(hostPeer.coordinator.session().roomState().state != Netplay::SessionState::Running) {
                resetInputDelayBuffers(hostPeer);
                hostPeer.lastInputDelayFrames = 0xFF;
            }
            if(clientPeer.coordinator.session().roomState().state != Netplay::SessionState::Running) {
                resetInputDelayBuffers(clientPeer);
                clientPeer.lastInputDelayFrames = 0xFF;
            }

            const bool hostAdvanced = advanceQueuedFrameIfPossible(hostPeer, options.frames);
            const bool clientAdvanced = advanceQueuedFrameIfPossible(clientPeer, options.frames);

            if(options.forceDesyncFrame > 0 &&
               !clientPeer.desyncInjected &&
               clientPeer.emu.frameCount() >= options.forceDesyncFrame) {
                const uint8_t currentValue = clientPeer.emu.read(static_cast<int>(options.desyncAddress & 0xFFFFu));
                clientPeer.emu.write(static_cast<int>(options.desyncAddress & 0xFFFFu),
                                    static_cast<uint8_t>(currentValue ^ static_cast<uint8_t>(options.desyncValueXor & 0xFFu)));
                clientPeer.desyncInjected = true;
            }

            if(hostPeer.emu.frameCount() > previousHostFrame || clientPeer.emu.frameCount() > previousClientFrame) {
                progressedFrames = std::min(hostPeer.emu.frameCount(), clientPeer.emu.frameCount());
            }

            if(!hostAdvanced && !clientAdvanced &&
               hostPeer.coordinator.session().roomState().state == Netplay::SessionState::Running &&
               clientPeer.coordinator.session().roomState().state == Netplay::SessionState::Running &&
               hostPeer.emu.frameCount() == previousHostFrame &&
               clientPeer.emu.frameCount() == previousClientFrame) {
                failureReason = "Simulation stopped making progress while both peers were running.";
                return finalizeReport(options, hostPeer, clientPeer, "stalled", failureReason, std::nullopt);
            }
        }

        if(progressedFrames < options.frames) {
            std::ostringstream ss;
            ss << "Frame target not reached. Host frame=" << hostPeer.emu.frameCount()
               << ", Client frame=" << clientPeer.emu.frameCount();
            failureReason = ss.str();
            return finalizeReport(options, hostPeer, clientPeer, "frame_limit_reached", failureReason, std::nullopt);
        }

        hostPeer.coordinator.update(0);
        clientPeer.coordinator.update(0);
        processHostResyncIfNeeded(hostPeer);
        processPendingResyncIfNeeded(hostPeer, options.rollbackWindowFrames);
        processPendingResyncIfNeeded(clientPeer, options.rollbackWindowFrames);
        processRollbackIfNeeded(hostPeer);
        processRollbackIfNeeded(clientPeer);
        maybeSubmitCrc(hostPeer, options.crcIntervalFrames);
        maybeSubmitCrc(clientPeer, options.crcIntervalFrames);
        hostPeer.coordinator.update(0);
        clientPeer.coordinator.update(0);

        const std::optional<std::string> timelineMismatch = findTimelineMismatch(hostPeer, clientPeer);
        if(timelineMismatch.has_value()) {
            failureReason = *timelineMismatch;
            return finalizeReport(options, hostPeer, clientPeer, "timeline_mismatch", failureReason, timelineMismatch);
        }

        const uint32_t hostCrc = hostPeer.emu.canonicalStateCrc32();
        const uint32_t clientCrc = clientPeer.emu.canonicalStateCrc32();
        if(hostCrc != clientCrc) {
            std::ostringstream ss;
            ss << "Final CRC mismatch. Host=" << hostCrc << ", Client=" << clientCrc;
            failureReason = ss.str();
            return finalizeReport(options, hostPeer, clientPeer, "crc_mismatch", failureReason, std::nullopt);
        }

        if(options.forceDesyncFrame == 0 &&
           (hostPeer.coordinator.predictionStats().hardResyncCount > 0 ||
            clientPeer.coordinator.predictionStats().hardResyncCount > 0)) {
            std::ostringstream ss;
            ss << "Unexpected hard resyncs under local test. Host="
               << hostPeer.coordinator.predictionStats().hardResyncCount
               << ", Client="
               << clientPeer.coordinator.predictionStats().hardResyncCount;
            failureReason = ss.str();
            return finalizeReport(options, hostPeer, clientPeer, "unexpected_resync", failureReason, std::nullopt);
        }

        return finalizeReport(options, hostPeer, clientPeer, "ok", "", std::nullopt);
    }
};

#endif
