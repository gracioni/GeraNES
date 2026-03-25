#pragma once

#ifndef __EMSCRIPTEN__

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "GeraNES/GeraNESEmu.h"
#include "GeraNES/util/Crc32.h"
#include "GeraNESApp/EmulationHost.h"
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
        uint32_t settleStepLimit = 2048;
        uint32_t forceDesyncFrame = 0;
        uint32_t desyncAddress = 0x0000;
        uint32_t desyncValueXor = 0x01;
        uint32_t hostInputSeed = 0x13572468u;
        uint32_t clientInputSeed = 0x24681357u;
        bool robust = false;
        bool appFlow = false;
        bool baselineLockstep = false;
    };

private:
    static bool realtimeEnhancementsEnabled(const Options& options)
    {
        return !options.baselineLockstep;
    }

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
        std::array<std::map<uint32_t, uint64_t>, 4> localRawInputHistory;
        std::array<uint64_t, 4> lastRawMasks = {};
        std::array<bool, 4> hasLastRawMask = {};
        std::array<uint32_t, 4> lastSampledLocalFrame = {};
        std::array<uint32_t, 4> lastRecordedFrame = {};
        uint8_t lastInputDelayFrames = 0xFF;
        uint32_t lastCrcReportFrame = 0;
        bool desyncInjected = false;
        uint32_t lastResyncTargetFrame = 0;
        uint32_t lastResyncLoadedFrameCount = 0;
        DeterministicInputGenerator inputGenerator;
        std::vector<std::string> rollbackDebugLog;

        explicit PeerState(const std::string& peerName, bool isHost, uint32_t inputSeed)
            : name(peerName)
            , host(isHost)
            , inputGenerator(inputSeed)
        {
        }
    };

    struct DesktopPeerState
    {
        std::string name;
        bool host = false;
        EmulationHost emu{DummyAudioOutput::instance()};
        Netplay::NetplayCoordinator coordinator;
        std::array<std::map<uint32_t, uint64_t>, 4> localRawInputHistory;
        std::array<uint64_t, 4> lastRawMasks = {};
        std::array<bool, 4> hasLastRawMask = {};
        std::array<uint32_t, 4> lastSampledLocalFrame = {};
        std::array<uint32_t, 4> lastRecordedFrame = {};
        uint8_t lastInputDelayFrames = 0xFF;
        uint32_t lastCrcReportFrame = 0;
        bool desyncInjected = false;
        uint32_t lastResyncTargetFrame = 0;
        uint32_t lastResyncLoadedFrameCount = 0;
        std::optional<Netplay::SessionState> lastSessionState;
        mutable std::mutex preparedInputMutex;
        std::map<uint32_t, EmulationHost::InputState> preparedInputs;
        DeterministicInputGenerator inputGenerator;
        std::vector<std::string> rollbackDebugLog;

        explicit DesktopPeerState(const std::string& peerName, bool isHost, uint32_t inputSeed)
            : name(peerName)
            , host(isHost)
            , inputGenerator(inputSeed)
        {
            emu.setRepeatLastFrameProviderInput(false);
            emu.setFrameInputProvider([this](uint32_t frame) {
                std::scoped_lock preparedLock(preparedInputMutex);
                for(auto it = preparedInputs.begin(); it != preparedInputs.end();) {
                    if(it->first < frame) {
                        it = preparedInputs.erase(it);
                    } else {
                        break;
                    }
                }

                const auto it = preparedInputs.find(frame);
                if(it == preparedInputs.end()) {
                    return std::optional<EmulationHost::InputState>{};
                }

                EmulationHost::InputState input = it->second;
                preparedInputs.erase(it);
                return std::optional<EmulationHost::InputState>{input};
            });
        }
    };

    static constexpr int RESULT_FAILED = 1;
    static constexpr int RESULT_ERROR = 2;

    struct RunArtifacts
    {
        int exitCode = RESULT_ERROR;
        nlohmann::json report;
    };

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
        for(auto& history : peer.localRawInputHistory) {
            history.clear();
        }
        peer.lastRawMasks.fill(0);
        peer.hasLastRawMask.fill(false);
        peer.lastSampledLocalFrame.fill(0);
        peer.lastRecordedFrame.fill(0);
        if(peer.coordinator.isActive()) {
            const Netplay::ParticipantId localParticipantId = peer.coordinator.localParticipantId();
            for(const auto& participant : peer.coordinator.session().roomState().participants) {
                if(participant.id != localParticipantId) continue;
                if(participant.controllerAssignment == Netplay::kObserverPlayerSlot) break;

                if(const Netplay::TimelineInputEntry* latestConfirmed =
                       peer.coordinator.localInputs().latestConfirmedFor(localParticipantId, participant.controllerAssignment)) {
                    const Netplay::PlayerSlot slot = participant.controllerAssignment;
                    peer.lastRecordedFrame[slot] = latestConfirmed->frame;
                    peer.lastRawMasks[slot] = latestConfirmed->buttonMaskLo;
                    peer.hasLastRawMask[slot] = true;
                    peer.lastSampledLocalFrame[slot] = latestConfirmed->frame;
                    peer.localRawInputHistory[slot][latestConfirmed->frame] = latestConfirmed->buttonMaskLo;
                }
                break;
            }
        }
    }

    static void reanchorInputDelayBuffers(PeerState& peer)
    {
        peer.lastRecordedFrame.fill(0);
        if(peer.coordinator.isActive()) {
            const Netplay::ParticipantId localParticipantId = peer.coordinator.localParticipantId();
            for(const auto& participant : peer.coordinator.session().roomState().participants) {
                if(participant.id != localParticipantId) continue;
                if(participant.controllerAssignment == Netplay::kObserverPlayerSlot) break;

                if(const Netplay::TimelineInputEntry* latestConfirmed =
                       peer.coordinator.localInputs().latestConfirmedFor(localParticipantId, participant.controllerAssignment)) {
                    const Netplay::PlayerSlot slot = participant.controllerAssignment;
                    peer.lastRecordedFrame[slot] = latestConfirmed->frame;
                    if(!peer.hasLastRawMask[slot]) {
                        peer.lastRawMasks[slot] = latestConfirmed->buttonMaskLo;
                        peer.hasLastRawMask[slot] = true;
                        peer.lastSampledLocalFrame[slot] = latestConfirmed->frame;
                        peer.localRawInputHistory[slot][latestConfirmed->frame] = latestConfirmed->buttonMaskLo;
                    }
                }
                break;
            }
        }
    }

    static void extendLocalRawInputHistory(PeerState& peer,
                                           Netplay::PlayerSlot slot,
                                           Netplay::FrameNumber currentFrame,
                                           uint64_t currentRawMask)
    {
        auto& history = peer.localRawInputHistory[slot];
        uint32_t nextSampleFrame = peer.lastSampledLocalFrame[slot] + 1u;

        uint64_t fillMask = 0;
        if(peer.hasLastRawMask[slot]) {
            fillMask = peer.lastRawMasks[slot];
        }

        while(nextSampleFrame <= currentFrame) {
            history[nextSampleFrame] = fillMask;
            ++nextSampleFrame;
        }

        history[currentFrame + 1u] = currentRawMask;
        peer.lastSampledLocalFrame[slot] = std::max<uint32_t>(peer.lastSampledLocalFrame[slot], currentFrame + 1u);
        peer.lastRawMasks[slot] = currentRawMask;
        peer.hasLastRawMask[slot] = true;

        const uint32_t pruneBeforeFrame = currentFrame > 64u ? (currentFrame - 64u) : 0u;
        for(auto it = history.begin(); it != history.end() && it->first < pruneBeforeFrame;) {
            it = history.erase(it);
        }
    }

    static uint64_t delayedPadMaskForFrame(const PeerState& peer,
                                           Netplay::PlayerSlot slot,
                                           Netplay::FrameNumber frame)
    {
        const uint8_t inputDelayFrames = peer.coordinator.session().roomState().inputDelayFrames;
        if(frame <= inputDelayFrames) {
            return 0;
        }

        const uint32_t sourceFrame = frame - inputDelayFrames;
        const auto& history = peer.localRawInputHistory[slot];
        if(const auto it = history.find(sourceFrame); it != history.end()) {
            return it->second;
        }

        return peer.hasLastRawMask[slot] ? peer.lastRawMasks[slot] : 0;
    }

    static void recordLocalInputFrame(PeerState& peer,
                                      Netplay::FrameNumber frame,
                                      Netplay::PlayerSlot slot)
    {
        const Netplay::ParticipantId localParticipantId = peer.coordinator.localParticipantId();
        const Netplay::TimelineInputEntry* existing =
            peer.coordinator.localInputs().find(frame, localParticipantId, slot);
        if(existing == nullptr) {
            peer.coordinator.recordLocalInputFrame(frame, slot, delayedPadMaskForFrame(peer, slot, frame));
        }
        peer.lastRecordedFrame[slot] = std::max<uint32_t>(peer.lastRecordedFrame[slot], frame);
    }

    static void extendLocalTimeline(PeerState& peer,
                                    Netplay::ParticipantId localParticipantId,
                                    Netplay::PlayerSlot slot,
                                    uint32_t currentFrame,
                                    uint32_t recordThroughFrame,
                                    uint64_t currentRawMask)
    {
        const uint8_t inputDelayFrames = peer.coordinator.session().roomState().inputDelayFrames;
        if(peer.lastInputDelayFrames != inputDelayFrames) {
            resetInputDelayBuffers(peer);
            peer.lastInputDelayFrames = inputDelayFrames;
        }

        extendLocalRawInputHistory(peer, slot, currentFrame, currentRawMask);
        const uint32_t safeCommitThroughFrame = currentFrame + 1u + static_cast<uint32_t>(inputDelayFrames);
        recordThroughFrame = std::max(recordThroughFrame, safeCommitThroughFrame);

        uint32_t nextFrameToRecord = peer.lastRecordedFrame[slot] + 1u;
        while(nextFrameToRecord <= recordThroughFrame) {
            recordLocalInputFrame(peer, nextFrameToRecord, slot);
            ++nextFrameToRecord;
        }
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
        InputFrame frame = emu.createInputFrame(emu.frameCount() + 1u);
        frame.p1A = state.p1A; frame.p1B = state.p1B; frame.p1Select = state.p1Select; frame.p1Start = state.p1Start;
        frame.p1Up = state.p1Up; frame.p1Down = state.p1Down; frame.p1Left = state.p1Left; frame.p1Right = state.p1Right;
        frame.p2A = state.p2A; frame.p2B = state.p2B; frame.p2Select = state.p2Select; frame.p2Start = state.p2Start;
        frame.p2Up = state.p2Up; frame.p2Down = state.p2Down; frame.p2Left = state.p2Left; frame.p2Right = state.p2Right;
        frame.p3A = state.p3A; frame.p3B = state.p3B; frame.p3Select = state.p3Select; frame.p3Start = state.p3Start;
        frame.p3Up = state.p3Up; frame.p3Down = state.p3Down; frame.p3Left = state.p3Left; frame.p3Right = state.p3Right;
        frame.p4A = state.p4A; frame.p4B = state.p4B; frame.p4Select = state.p4Select; frame.p4Start = state.p4Start;
        frame.p4Up = state.p4Up; frame.p4Down = state.p4Down; frame.p4Left = state.p4Left; frame.p4Right = state.p4Right;
        emu.queueInputFrame(frame);
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

    static void resetInputDelayBuffers(DesktopPeerState& peer)
    {
        for(auto& history : peer.localRawInputHistory) {
            history.clear();
        }
        peer.lastRawMasks.fill(0);
        peer.hasLastRawMask.fill(false);
        peer.lastSampledLocalFrame.fill(0);
        peer.lastRecordedFrame.fill(0);
        if(peer.coordinator.isActive()) {
            const Netplay::ParticipantId localParticipantId = peer.coordinator.localParticipantId();
            for(const auto& participant : peer.coordinator.session().roomState().participants) {
                if(participant.id != localParticipantId) continue;
                if(participant.controllerAssignment == Netplay::kObserverPlayerSlot) break;

                if(const Netplay::TimelineInputEntry* latestConfirmed =
                       peer.coordinator.localInputs().latestConfirmedFor(localParticipantId, participant.controllerAssignment)) {
                    const Netplay::PlayerSlot slot = participant.controllerAssignment;
                    peer.lastRecordedFrame[slot] = latestConfirmed->frame;
                    peer.lastRawMasks[slot] = latestConfirmed->buttonMaskLo;
                    peer.hasLastRawMask[slot] = true;
                    peer.lastSampledLocalFrame[slot] = latestConfirmed->frame;
                    peer.localRawInputHistory[slot][latestConfirmed->frame] = latestConfirmed->buttonMaskLo;
                }
                break;
            }
        }
        {
            std::scoped_lock preparedLock(peer.preparedInputMutex);
            peer.preparedInputs.clear();
        }
    }

    static void reanchorInputDelayBuffers(DesktopPeerState& peer)
    {
        peer.lastRecordedFrame.fill(0);
        if(peer.coordinator.isActive()) {
            const Netplay::ParticipantId localParticipantId = peer.coordinator.localParticipantId();
            for(const auto& participant : peer.coordinator.session().roomState().participants) {
                if(participant.id != localParticipantId) continue;
                if(participant.controllerAssignment == Netplay::kObserverPlayerSlot) break;

                if(const Netplay::TimelineInputEntry* latestConfirmed =
                       peer.coordinator.localInputs().latestConfirmedFor(localParticipantId, participant.controllerAssignment)) {
                    const Netplay::PlayerSlot slot = participant.controllerAssignment;
                    peer.lastRecordedFrame[slot] = latestConfirmed->frame;
                    if(!peer.hasLastRawMask[slot]) {
                        peer.lastRawMasks[slot] = latestConfirmed->buttonMaskLo;
                        peer.hasLastRawMask[slot] = true;
                        peer.lastSampledLocalFrame[slot] = latestConfirmed->frame;
                        peer.localRawInputHistory[slot][latestConfirmed->frame] = latestConfirmed->buttonMaskLo;
                    }
                }
                break;
            }
        }
        {
            std::scoped_lock preparedLock(peer.preparedInputMutex);
            peer.preparedInputs.clear();
        }
    }

    static void extendLocalRawInputHistory(DesktopPeerState& peer,
                                           Netplay::PlayerSlot slot,
                                           Netplay::FrameNumber currentFrame,
                                           uint64_t currentRawMask)
    {
        auto& history = peer.localRawInputHistory[slot];
        uint32_t nextSampleFrame = peer.lastSampledLocalFrame[slot] + 1u;

        uint64_t fillMask = 0;
        if(peer.hasLastRawMask[slot]) {
            fillMask = peer.lastRawMasks[slot];
        }

        while(nextSampleFrame <= currentFrame) {
            history[nextSampleFrame] = fillMask;
            ++nextSampleFrame;
        }

        history[currentFrame + 1u] = currentRawMask;
        peer.lastSampledLocalFrame[slot] = std::max<uint32_t>(peer.lastSampledLocalFrame[slot], currentFrame + 1u);
        peer.lastRawMasks[slot] = currentRawMask;
        peer.hasLastRawMask[slot] = true;

        const uint32_t pruneBeforeFrame = currentFrame > 64u ? (currentFrame - 64u) : 0u;
        for(auto it = history.begin(); it != history.end() && it->first < pruneBeforeFrame;) {
            it = history.erase(it);
        }
    }

    static uint64_t delayedPadMaskForFrame(const DesktopPeerState& peer,
                                           Netplay::PlayerSlot slot,
                                           Netplay::FrameNumber frame)
    {
        const uint8_t inputDelayFrames = peer.coordinator.session().roomState().inputDelayFrames;
        if(frame <= inputDelayFrames) {
            return 0;
        }

        const uint32_t sourceFrame = frame - inputDelayFrames;
        const auto& history = peer.localRawInputHistory[slot];
        if(const auto it = history.find(sourceFrame); it != history.end()) {
            return it->second;
        }

        return peer.hasLastRawMask[slot] ? peer.lastRawMasks[slot] : 0;
    }

    static std::optional<Netplay::RomValidationData> captureRomValidation(EmulationHost& emu)
    {
        if(!emu.valid()) return std::nullopt;

        return emu.withExclusiveAccess([](auto& core) -> std::optional<Netplay::RomValidationData> {
            if(!core.valid()) return std::nullopt;

            Cartridge& cart = core.getConsole().cartridge();
            Netplay::RomValidationData validation;
            validation.romCrc32 = cart.romFile().fileCrc32();
            validation.mapperId = static_cast<uint16_t>(std::max(0, cart.mapperId()));
            validation.subMapperId = static_cast<uint16_t>(std::max(0, cart.subMapperId()));
            validation.prgRomSize = static_cast<uint32_t>(std::max(0, cart.prgSize()));
            validation.chrRomSize = static_cast<uint32_t>(std::max(0, cart.chrSize()));
            validation.chrRamSize = static_cast<uint32_t>(std::max(0, cart.chrRamSize()));
            validation.fileSize = static_cast<uint32_t>(cart.romFile().size());
            return validation;
        });
    }

    static void applyPadMaskToInputState(EmulationHost::InputState& state, Netplay::PlayerSlot slot, uint64_t mask)
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

    static bool tryResolvePadMask(const DesktopPeerState& peer, Netplay::FrameNumber frame, Netplay::PlayerSlot slot, uint64_t& outMask)
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

    static EmulationHost::InputState buildReplayInputState(const DesktopPeerState& peer, Netplay::FrameNumber frame)
    {
        EmulationHost::InputState state{};
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
        peer.coordinator.setLocalSimulationFrame(peer.emu.frameCount());
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
        peer.coordinator.setLocalSimulationFrame(newFrame);
        peer.runtime.setCurrentFrame(newFrame);
        peer.runtime.captureSnapshot(newFrame, [&peer]() {
            return peer.emu.saveNetplayStateToMemory();
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
        const Netplay::FrameNumber earliestConfirmedReplayFrame =
            confirmedFrame > 0 ? (confirmedFrame - 1u) : 0u;
        const Netplay::FrameNumber rollbackFloor = std::min(earliestConfirmedReplayFrame, latestSafeRollbackFrame);
        if(*rollbackFrame < rollbackFloor) {
            rollbackFrame = rollbackFloor;
        }

        if(*rollbackFrame >= currentFrame) {
            std::ostringstream oss;
            oss << "Deferred rollback frame " << *rollbackFrame
                << " while local frame was " << currentFrame;
            peer.rollbackDebugLog.push_back(oss.str());
            peer.coordinator.rescheduleRollbackFrame(*rollbackFrame);
            return;
        }

        if(!peer.runtime.rollbackTo(*rollbackFrame, [&peer](const std::vector<uint8_t>& data) {
            peer.emu.loadStateFromMemoryOnCleanBoot(data);
        })) {
            std::ostringstream oss;
            oss << "Rollback restore failed for frame " << *rollbackFrame
                << " at local frame " << currentFrame;
            peer.rollbackDebugLog.push_back(oss.str());
            return;
        }
        peer.coordinator.setLocalSimulationFrame(peer.emu.frameCount());
        {
            std::ostringstream oss;
            oss << "Rollback applied from " << currentFrame
                << " to " << *rollbackFrame;
            peer.rollbackDebugLog.push_back(oss.str());
        }

        peer.coordinator.invalidateLocalCrcHistoryAfter(*rollbackFrame);
        if(peer.lastCrcReportFrame > *rollbackFrame) {
            peer.lastCrcReportFrame = *rollbackFrame;
        }

        while(peer.emu.frameCount() < currentFrame) {
            const uint32_t replayFrame = peer.emu.frameCount() + 1u;
            const InputState replayInput = buildReplayInputState(peer, replayFrame);
            if(!advancePeerToNextFrame(peer, replayInput)) {
                std::ostringstream oss;
                oss << "Rollback resimulation failed at frame " << replayFrame
                    << " while targeting " << currentFrame;
                peer.rollbackDebugLog.push_back(oss.str());
                return;
            }
        }
        {
            std::ostringstream oss;
            oss << "Rollback resimulated to " << peer.emu.frameCount();
            peer.rollbackDebugLog.push_back(oss.str());
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
            peer.coordinator.setLocalSimulationFrame(peer.emu.frameCount());
            peer.runtime.setCurrentFrame(pending->targetFrame);
            peer.runtime.captureSnapshot(pending->targetFrame, [&pending]() {
                return pending->payload;
            });
        }

        const uint32_t loadedCrc32 = loaded ? peer.emu.canonicalNetplayStateCrc32() : 0;
        peer.coordinator.acknowledgeResync(pending->resyncId, pending->targetFrame, loadedCrc32, loaded);
    }

    static void processHostResyncIfNeeded(PeerState& peer)
    {
        if(!peer.coordinator.isHosting()) return;

        std::optional<Netplay::FrameNumber> pendingFrame = peer.coordinator.consumePendingHostResyncFrame();
        if(!pendingFrame.has_value()) return;
        if(!peer.emu.valid()) return;

        const Netplay::FrameNumber requestedFrame = peer.coordinator.session().roomState().lastConfirmedFrame;
        const Netplay::FrameNumber targetFrame =
            std::min<Netplay::FrameNumber>(requestedFrame, peer.emu.frameCount());
        const Netplay::SnapshotRecord* snapshot = peer.runtime.snapshots().find(targetFrame);
        const std::vector<uint8_t> payload =
            snapshot != nullptr ? snapshot->data : peer.emu.saveNetplayStateToMemory();
        if(payload.empty()) return;

        if(snapshot != nullptr) {
            if(!peer.runtime.rollbackTo(targetFrame, [&peer](const std::vector<uint8_t>& data) {
                peer.emu.loadStateFromMemoryOnCleanBoot(data);
            })) {
                return;
            }
        }
        peer.lastResyncTargetFrame = targetFrame;
        peer.lastResyncLoadedFrameCount = peer.emu.frameCount();
        peer.coordinator.setLocalSimulationFrame(peer.emu.frameCount());

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

    static void syncLocalRomValidation(Netplay::NetplayCoordinator& coordinator,
                                       const std::optional<Netplay::RomValidationData>& validation)
    {
        const auto& room = coordinator.session().roomState();
        const bool romLoaded = validation.has_value();
        const bool romCompatible =
            romLoaded &&
            !room.selectedGameName.empty() &&
            Netplay::NetplayCoordinator::romValidationMatches(*validation, room.romValidation);

        coordinator.submitLocalRomValidation(
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

    static bool hasConfirmedInputsForNextFrame(const PeerState& peer)
    {
        if(!peer.coordinator.isActive()) return true;
        if(peer.coordinator.awaitingSpectatorSync()) return false;
        if(peer.coordinator.session().roomState().state != Netplay::SessionState::Running) return false;

        const uint32_t nextFrame = const_cast<GeraNESEmu&>(peer.emu).frameCount() + 1u;
        const auto& room = peer.coordinator.session().roomState();
        for(const auto& participant : room.participants) {
            if(participant.controllerAssignment == Netplay::kObserverPlayerSlot) continue;

            const Netplay::TimelineInputEntry* entry =
                participant.id == peer.coordinator.localParticipantId()
                    ? peer.coordinator.localInputs().find(nextFrame, participant.id, participant.controllerAssignment)
                    : peer.coordinator.remoteInputs().find(nextFrame, participant.id, participant.controllerAssignment);
            if(entry == nullptr || entry->predicted) {
                return false;
            }
        }

        return true;
    }

    static bool canSimulateFrame(const PeerState& peer, const Options& options)
    {
        if(!realtimeEnhancementsEnabled(options)) {
            return hasConfirmedInputsForNextFrame(peer);
        }
        return !(netplayStateBlocksSimulation(peer) || clientIsTooFarAhead(peer));
    }

    static bool queueLocalInputForNextFrame(PeerState& peer, uint32_t maxFrame, const Options& options)
    {
        if(netplayStateBlocksSimulation(peer)) {
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

        const uint32_t currentFrame = peer.emu.frameCount();
        const uint32_t playbackFrame = currentFrame + 1u;
        if(localParticipant != nullptr && localParticipant->controllerAssignment != Netplay::kObserverPlayerSlot) {
            const Netplay::PlayerSlot localSlot = localParticipant->controllerAssignment;
            const Buttons buttons = peer.inputGenerator.buttonsForFrame(playbackFrame, std::max<uint32_t>(1, peer.emu.getRegionFPS()));
            const uint64_t rawMask = buildPadMask(buttons);
            extendLocalTimeline(peer,
                               localParticipantId,
                               localSlot,
                               currentFrame,
                               std::min<uint32_t>(maxFrame, currentFrame + 1u + peer.coordinator.session().roomState().inputDelayFrames),
                               rawMask);
        } else {
            resetInputDelayBuffers(peer);
            peer.lastInputDelayFrames = 0xFF;
        }

        if(realtimeEnhancementsEnabled(options)) {
            peer.coordinator.predictRemoteInputsForFrame(playbackFrame);
        }
        return true;
    }

    static bool advanceQueuedFrameIfPossible(PeerState& peer, uint32_t maxFrame, const Options& options)
    {
        if(!canSimulateFrame(peer, options)) {
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

    static void runMaintenanceStep(PeerState& hostPeer,
                                   PeerState& clientPeer,
                                   const Options& options,
                                   const std::optional<Netplay::RomValidationData>& romValidation)
    {
        hostPeer.coordinator.setLocalSimulationFrame(hostPeer.emu.frameCount());
        clientPeer.coordinator.setLocalSimulationFrame(clientPeer.emu.frameCount());
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
    }

    static void configurePeerRuntime(DesktopPeerState& peer, size_t rollbackWindowFrames)
    {
        peer.emu.configureNetplaySnapshots(rollbackWindowFrames);
        peer.coordinator.setLocalSimulationFrame(peer.emu.frameCount());
        peer.lastCrcReportFrame = 0;
        resetInputDelayBuffers(peer);
        peer.lastInputDelayFrames = 0xFF;
    }

    static void recordDesktopLocalInput(DesktopPeerState& peer, Netplay::FrameNumber frame, Netplay::PlayerSlot slot, uint64_t rawMask)
    {
        const Netplay::ParticipantId localParticipantId = peer.coordinator.localParticipantId();
        const Netplay::TimelineInputEntry* existing =
            peer.coordinator.localInputs().find(frame, localParticipantId, slot);
        if(existing == nullptr) {
            peer.coordinator.recordLocalInputFrame(frame, slot, delayedPadMaskForFrame(peer, slot, frame));
        }
        peer.lastRecordedFrame[slot] = std::max<uint32_t>(peer.lastRecordedFrame[slot], frame);
    }

    static void extendDesktopLocalTimeline(DesktopPeerState& peer,
                                           Netplay::ParticipantId localParticipantId,
                                           Netplay::PlayerSlot slot,
                                           uint32_t currentFrame,
                                           uint32_t recordThroughFrame,
                                           uint64_t currentRawMask)
    {
        const uint8_t inputDelayFrames = peer.coordinator.session().roomState().inputDelayFrames;
        if(peer.lastInputDelayFrames != inputDelayFrames) {
            resetInputDelayBuffers(peer);
            peer.lastInputDelayFrames = inputDelayFrames;
        }

        extendLocalRawInputHistory(peer, slot, currentFrame, currentRawMask);
        const uint32_t safeCommitThroughFrame = currentFrame + 1u + static_cast<uint32_t>(inputDelayFrames);
        recordThroughFrame = std::max(recordThroughFrame, safeCommitThroughFrame);

        uint32_t nextFrameToRecord = peer.lastRecordedFrame[slot] + 1u;

        while(nextFrameToRecord <= currentFrame) {
            recordDesktopLocalInput(peer, nextFrameToRecord, slot, currentRawMask);
            ++nextFrameToRecord;
        }

        while(nextFrameToRecord <= recordThroughFrame) {
            recordDesktopLocalInput(peer, nextFrameToRecord, slot, currentRawMask);
            ++nextFrameToRecord;
        }
    }

    static bool hasConfirmedInputsForNextFrame(const DesktopPeerState& peer)
    {
        if(!peer.coordinator.isActive()) return true;
        if(peer.coordinator.awaitingSpectatorSync()) return false;
        if(peer.coordinator.session().roomState().state != Netplay::SessionState::Running) return false;

        const uint32_t nextFrame = peer.emu.frameCount() + 1u;
        const auto& room = peer.coordinator.session().roomState();
        for(const auto& participant : room.participants) {
            if(participant.controllerAssignment == Netplay::kObserverPlayerSlot) continue;

            const Netplay::TimelineInputEntry* entry =
                participant.id == peer.coordinator.localParticipantId()
                    ? peer.coordinator.localInputs().find(nextFrame, participant.id, participant.controllerAssignment)
                    : peer.coordinator.remoteInputs().find(nextFrame, participant.id, participant.controllerAssignment);
            if(entry == nullptr || entry->predicted) {
                return false;
            }
        }

        return true;
    }

    static bool hasPreparedInputForNextFrame(const DesktopPeerState& peer)
    {
        const uint32_t nextFrame = peer.emu.frameCount() + 1u;
        std::scoped_lock preparedLock(peer.preparedInputMutex);
        return peer.preparedInputs.find(nextFrame) != peer.preparedInputs.end();
    }

    static bool canSimulateFrame(const DesktopPeerState& peer, const Options& options)
    {
        if(!realtimeEnhancementsEnabled(options)) {
            return hasConfirmedInputsForNextFrame(peer) && hasPreparedInputForNextFrame(peer);
        }
        if(!peer.coordinator.isActive()) return true;
        if(peer.coordinator.awaitingSpectatorSync()) return false;
        if(peer.coordinator.session().roomState().state != Netplay::SessionState::Running) return false;
        if(!hasPreparedInputForNextFrame(peer)) return false;
        if(peer.coordinator.isHosting()) return true;

        uint32_t authoritativeFrame = peer.coordinator.session().roomState().currentFrame;
        for(const auto& participant : peer.coordinator.session().roomState().participants) {
            if(participant.id == peer.coordinator.localParticipantId()) continue;
            authoritativeFrame = std::max(authoritativeFrame, participant.lastReceivedInputFrame);
        }
        return authoritativeFrame == 0 || peer.emu.frameCount() <= authoritativeFrame + 1u;
    }

    static bool queueLocalInputForNextFrame(DesktopPeerState& peer, uint32_t maxFrame, const Options& options)
    {
        if(!peer.coordinator.isActive()) return false;
        if(peer.coordinator.awaitingSpectatorSync()) return false;
        if(peer.coordinator.session().roomState().state != Netplay::SessionState::Running) return false;
        const uint32_t currentFrame = peer.emu.frameCount();
        if(currentFrame > maxFrame) return false;

        const auto& room = peer.coordinator.session().roomState();
        const Netplay::ParticipantId localParticipantId = peer.coordinator.localParticipantId();
        const Netplay::ParticipantInfo* localParticipant = nullptr;
        for(const auto& participant : room.participants) {
            if(participant.id == localParticipantId) {
                localParticipant = &participant;
                break;
            }
        }

        const uint32_t prepareThroughFrame = [&]() {
            uint32_t horizon = std::min<uint32_t>(
                maxFrame,
                currentFrame + 1u + static_cast<uint32_t>(room.inputDelayFrames)
            );
            if(peer.coordinator.isHosting()) {
                return horizon;
            }

            uint32_t authoritativeFrame = room.currentFrame;
            for(const auto& participant : room.participants) {
                if(participant.id == localParticipantId) continue;
                authoritativeFrame = std::max(authoritativeFrame, participant.lastReceivedInputFrame);
            }
            return horizon;
        }();

        if(localParticipant != nullptr && localParticipant->controllerAssignment != Netplay::kObserverPlayerSlot) {
            const Buttons buttons =
                peer.inputGenerator.buttonsForFrame(currentFrame + 1u, std::max<uint32_t>(1, peer.emu.getRegionFPS()));
            const uint64_t rawMask = buildPadMask(buttons);
            extendDesktopLocalTimeline(
                peer,
                localParticipantId,
                localParticipant->controllerAssignment,
                std::min(currentFrame, maxFrame),
                prepareThroughFrame,
                rawMask
            );
        } else {
            resetInputDelayBuffers(peer);
            peer.lastInputDelayFrames = 0xFF;
        }

        if(currentFrame >= maxFrame) {
            return false;
        }

        {
            std::scoped_lock preparedLock(peer.preparedInputMutex);
            for(auto it = peer.preparedInputs.begin(); it != peer.preparedInputs.end();) {
                if(it->first <= currentFrame) {
                    it = peer.preparedInputs.erase(it);
                } else {
                    ++it;
                }
            }

            for(uint32_t frame = currentFrame + 1u; frame <= prepareThroughFrame; ++frame) {
                if(realtimeEnhancementsEnabled(options)) {
                    peer.coordinator.predictRemoteInputsForFrame(frame);
                }
                EmulationHost::InputState frameInput = buildReplayInputState(peer, frame);
                peer.preparedInputs[frame] = frameInput;
            }
        }
        return true;
    }

    static void processRollbackIfNeeded(DesktopPeerState& peer)
    {
        std::optional<Netplay::FrameNumber> rollbackFrame = peer.coordinator.consumePendingRollbackFrame();
        if(!rollbackFrame.has_value()) return;

        const uint32_t currentFrame = peer.emu.frameCount();
        if(currentFrame == 0) return;

        const Netplay::FrameNumber confirmedFrame = peer.coordinator.session().roomState().lastConfirmedFrame;
        const Netplay::FrameNumber latestSafeRollbackFrame = static_cast<Netplay::FrameNumber>(currentFrame - 1u);
        const Netplay::FrameNumber earliestConfirmedReplayFrame = confirmedFrame > 0 ? (confirmedFrame - 1u) : 0u;
        const Netplay::FrameNumber rollbackFloor = std::min(earliestConfirmedReplayFrame, latestSafeRollbackFrame);
        if(*rollbackFrame < rollbackFloor) {
            rollbackFrame = rollbackFloor;
        }
        if(*rollbackFrame >= currentFrame) {
            peer.coordinator.rescheduleRollbackFrame(*rollbackFrame);
            return;
        }

        if(!peer.emu.rollbackToFrame(*rollbackFrame)) {
            peer.rollbackDebugLog.push_back("Rollback restore failed");
            return;
        }
        peer.coordinator.setLocalSimulationFrame(*rollbackFrame);
        peer.coordinator.invalidateLocalCrcHistoryAfter(*rollbackFrame);
        if(peer.lastCrcReportFrame > *rollbackFrame) {
            peer.lastCrcReportFrame = *rollbackFrame;
        }

        if(!peer.emu.resimulateToFrame(currentFrame, [&peer](uint32_t frame) {
            return buildReplayInputState(peer, frame);
        })) {
            peer.rollbackDebugLog.push_back("Rollback resimulation failed");
            return;
        }
        peer.coordinator.setLocalSimulationFrame(peer.emu.frameCount());
    }

    static void processPendingResyncIfNeeded(DesktopPeerState& peer)
    {
        std::optional<Netplay::NetplayCoordinator::PendingResyncApply> pending = peer.coordinator.consumePendingResyncApply();
        if(!pending.has_value()) return;

        const bool loaded = peer.emu.loadStateFromMemory(pending->payload);
        if(loaded) {
            peer.lastResyncTargetFrame = pending->targetFrame;
            peer.lastResyncLoadedFrameCount = peer.emu.frameCount();
            peer.coordinator.setLocalSimulationFrame(pending->targetFrame);
            peer.emu.seedNetplaySnapshot(pending->targetFrame, pending->payload);
        }
        const uint32_t loadedCrc32 = loaded ? peer.emu.canonicalNetplayStateCrc32() : 0;
        peer.coordinator.acknowledgeResync(pending->resyncId, pending->targetFrame, loadedCrc32, loaded);
        if(loaded) {
            reanchorInputDelayBuffers(peer);
            peer.lastInputDelayFrames = 0xFF;
        }
    }

    static uint32_t probePayloadFrameCount(const std::string& romPath,
                                           const std::vector<uint8_t>& payload,
                                           uint32_t fallbackFrame)
    {
        if(payload.empty()) return fallbackFrame;

        GeraNESEmu probeEmu{DummyAudioOutput::instance()};
        if(!probeEmu.open(romPath) || !probeEmu.valid()) {
            return fallbackFrame;
        }
        if(!probeEmu.loadStateFromMemoryOnCleanBoot(payload) || !probeEmu.valid()) {
            return fallbackFrame;
        }

        return probeEmu.frameCount();
    }

    static void processHostResyncIfNeeded(DesktopPeerState& peer, const Options& options)
    {
        if(!peer.coordinator.isHosting()) return;

        std::optional<Netplay::FrameNumber> pendingFrame = peer.coordinator.consumePendingHostResyncFrame();
        if(!pendingFrame.has_value()) return;
        if(!peer.emu.valid()) return;

        const bool initialSessionSync = peer.coordinator.session().roomState().state == Netplay::SessionState::Starting;
        const Netplay::FrameNumber requestedFrame =
            initialSessionSync ? peer.emu.frameCount() : peer.coordinator.session().roomState().lastConfirmedFrame;
        const Netplay::FrameNumber authoritativeFrame =
            std::min<Netplay::FrameNumber>(requestedFrame, peer.emu.frameCount());

        const std::optional<std::vector<uint8_t>> snapshot = peer.emu.netplaySnapshotForFrame(authoritativeFrame);
        const std::vector<uint8_t> statePayload = snapshot.has_value() ? *snapshot : peer.emu.saveNetplayStateToMemory();
        if(statePayload.empty()) return;
        const Netplay::FrameNumber payloadFrame =
            initialSessionSync
                ? probePayloadFrameCount(options.romPath, statePayload, authoritativeFrame)
                : authoritativeFrame;

        if(!initialSessionSync && snapshot.has_value()) {
            if(!peer.emu.rollbackToFrame(authoritativeFrame)) {
                return;
            }
        }

        peer.lastResyncTargetFrame = payloadFrame;
        peer.lastResyncLoadedFrameCount = peer.emu.frameCount();

        const uint32_t payloadCrc32 = Crc32::calc(reinterpret_cast<const char*>(statePayload.data()), statePayload.size());
        peer.coordinator.beginResync(payloadFrame, statePayload, payloadCrc32);
    }

    static void maybeSubmitCrc(DesktopPeerState& peer, uint32_t crcIntervalFrames)
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

        const std::optional<uint32_t> snapshotCrc32 = peer.emu.netplaySnapshotCrc32ForFrame(confirmedFrame);
        if(!snapshotCrc32.has_value()) return;

        peer.coordinator.submitLocalCrc(confirmedFrame, *snapshotCrc32);
        peer.lastCrcReportFrame = confirmedFrame;
    }

    static void handleSessionStateTransitions(DesktopPeerState& peer)
    {
        if(!peer.coordinator.isActive()) {
            peer.lastSessionState.reset();
            resetInputDelayBuffers(peer);
            peer.lastInputDelayFrames = 0xFF;
            return;
        }

        const Netplay::SessionState currentState = peer.coordinator.session().roomState().state;
        if(peer.lastSessionState.has_value() && *peer.lastSessionState == currentState) {
            return;
        }

        if(currentState != Netplay::SessionState::Running) {
            resetInputDelayBuffers(peer);
            peer.lastInputDelayFrames = 0xFF;
        }

        peer.lastSessionState = currentState;
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
            {"crc32", const_cast<GeraNESEmu&>(peer.emu).canonicalNetplayStateCrc32()},
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
            {"rollbackDebugTail", [&peer]() {
                nlohmann::json tail = nlohmann::json::array();
                const size_t start = peer.rollbackDebugLog.size() > 20 ? (peer.rollbackDebugLog.size() - 20) : 0;
                for(size_t i = start; i < peer.rollbackDebugLog.size(); ++i) {
                    tail.push_back(peer.rollbackDebugLog[i]);
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
            }()},
            {"localTimelineAroundConfirmed", [&peer]() {
                nlohmann::json window = nlohmann::json::array();
                const auto& entries = peer.coordinator.localInputs().entries();
                const uint32_t confirmed = peer.coordinator.session().roomState().lastConfirmedFrame;
                for(const auto& entry : entries) {
                    if(entry.frame + 2u < confirmed || entry.frame > confirmed + 4u) continue;
                    window.push_back({
                        {"frame", entry.frame},
                        {"participantId", entry.participantId},
                        {"slot", entry.playerSlot},
                        {"maskLo", entry.buttonMaskLo},
                        {"sequence", entry.sequence},
                        {"predicted", entry.predicted},
                        {"confirmed", entry.confirmed}
                    });
                }
                return window;
            }()},
            {"remoteTimelineAroundConfirmed", [&peer]() {
                nlohmann::json window = nlohmann::json::array();
                const auto& entries = peer.coordinator.remoteInputs().entries();
                const uint32_t confirmed = peer.coordinator.session().roomState().lastConfirmedFrame;
                for(const auto& entry : entries) {
                    if(entry.frame + 2u < confirmed || entry.frame > confirmed + 4u) continue;
                    window.push_back({
                        {"frame", entry.frame},
                        {"participantId", entry.participantId},
                        {"slot", entry.playerSlot},
                        {"maskLo", entry.buttonMaskLo},
                        {"sequence", entry.sequence},
                        {"predicted", entry.predicted},
                        {"confirmed", entry.confirmed}
                    });
                }
                return window;
            }()}
        };
    }

    static nlohmann::json buildPeerReport(const DesktopPeerState& peer)
    {
        const auto& room = peer.coordinator.session().roomState();
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
            {"frame", peer.emu.frameCount()},
            {"crc32", const_cast<EmulationHost&>(peer.emu).canonicalNetplayStateCrc32()},
            {"roomState", stateLabel(room.state)},
            {"currentFrame", room.currentFrame},
            {"lastConfirmedFrame", room.lastConfirmedFrame},
            {"lastRemoteCrcFrame", room.lastRemoteCrcFrame},
            {"lastRemoteCrc32", room.lastRemoteCrc32},
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
            }()},
            {"localTimelineAroundConfirmed", [&peer]() {
                nlohmann::json window = nlohmann::json::array();
                const auto& entries = peer.coordinator.localInputs().entries();
                const uint32_t confirmed = peer.coordinator.session().roomState().lastConfirmedFrame;
                for(const auto& entry : entries) {
                    if(entry.frame + 2u < confirmed || entry.frame > confirmed + 4u) continue;
                    window.push_back({
                        {"frame", entry.frame},
                        {"participantId", entry.participantId},
                        {"slot", entry.playerSlot},
                        {"maskLo", entry.buttonMaskLo},
                        {"sequence", entry.sequence},
                        {"predicted", entry.predicted},
                        {"confirmed", entry.confirmed}
                    });
                }
                return window;
            }()},
            {"remoteTimelineAroundConfirmed", [&peer]() {
                nlohmann::json window = nlohmann::json::array();
                const auto& entries = peer.coordinator.remoteInputs().entries();
                const uint32_t confirmed = peer.coordinator.session().roomState().lastConfirmedFrame;
                for(const auto& entry : entries) {
                    if(entry.frame + 2u < confirmed || entry.frame > confirmed + 4u) continue;
                    window.push_back({
                        {"frame", entry.frame},
                        {"participantId", entry.participantId},
                        {"slot", entry.playerSlot},
                        {"maskLo", entry.buttonMaskLo},
                        {"sequence", entry.sequence},
                        {"predicted", entry.predicted},
                        {"confirmed", entry.confirmed}
                    });
                }
                return window;
            }()}
        };
    }

    static bool hasEventLogMessage(const PeerState& peer, const std::string& needle)
    {
        for(const std::string& message : peer.coordinator.eventLog()) {
            if(message.find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    static bool hasEventLogMessage(const DesktopPeerState& peer, const std::string& needle)
    {
        for(const std::string& message : peer.coordinator.eventLog()) {
            if(message.find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
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

    static std::optional<Netplay::ParticipantId> findRemoteParticipantId(const Netplay::NetplayCoordinator& coordinator)
    {
        for(const auto& participant : coordinator.session().roomState().participants) {
            if(participant.id != coordinator.localParticipantId()) {
                return participant.id;
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
            {"loadedStateCrc32", probeEmu.canonicalNetplayStateCrc32()}
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
            {"replayedCrc32", probeEmu.canonicalNetplayStateCrc32()},
            {"targetSnapshotCrc32", targetSnapshot->crc32}
        };
    }

    static nlohmann::json buildReport(const Options& options,
                                      const PeerState& hostPeer,
                                      const PeerState& clientPeer,
                                      const std::string& status,
                                      const std::string& failureReason,
                                      const std::optional<std::string>& timelineMismatch)
    {
        const uint32_t hostCrc = const_cast<GeraNESEmu&>(hostPeer.emu).valid()
            ? const_cast<GeraNESEmu&>(hostPeer.emu).canonicalNetplayStateCrc32()
            : 0;
        const uint32_t clientCrc = const_cast<GeraNESEmu&>(clientPeer.emu).valid()
            ? const_cast<GeraNESEmu&>(clientPeer.emu).canonicalNetplayStateCrc32()
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
            {"baselineLockstep", options.baselineLockstep},
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

        return report;
    }

    static std::optional<std::string> findTimelineMismatch(const DesktopPeerState& hostPeer, const DesktopPeerState& clientPeer)
    {
        const uint32_t confirmedFrame = std::min(
            hostPeer.coordinator.session().roomState().lastConfirmedFrame,
            clientPeer.coordinator.session().roomState().lastConfirmedFrame
        );

        for(uint32_t frame = 1; frame <= confirmedFrame; ++frame) {
            for(Netplay::PlayerSlot slot = 0; slot < 4; ++slot) {
                const auto findEntry = [&](const DesktopPeerState& peer) -> const Netplay::TimelineInputEntry* {
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
                if(hostEntry == nullptr && clientEntry == nullptr) continue;
                if(hostEntry == nullptr || clientEntry == nullptr) {
                    std::ostringstream ss;
                    ss << "Desktop flow timeline mismatch at frame " << frame
                       << " slot " << static_cast<unsigned>(slot) + 1u
                       << " missing entry on " << (hostEntry == nullptr ? "host" : "client");
                    return ss.str();
                }
                if(hostEntry->buttonMaskLo != clientEntry->buttonMaskLo || hostEntry->buttonMaskHi != clientEntry->buttonMaskHi) {
                    std::ostringstream ss;
                    ss << "Desktop flow timeline mismatch at frame " << frame
                       << " slot " << static_cast<unsigned>(slot) + 1u
                       << " hostMaskLo " << hostEntry->buttonMaskLo
                       << " clientMaskLo " << clientEntry->buttonMaskLo;
                    return ss.str();
                }
            }
        }

        return std::nullopt;
    }

    static nlohmann::json buildDesktopFlowReport(const Options& options,
                                                 const DesktopPeerState& hostPeer,
                                                 const DesktopPeerState& clientPeer,
                                                 const std::string& status,
                                                 const std::string& failureReason,
                                                 const std::optional<std::string>& timelineMismatch)
    {
        const uint32_t hostCrc = hostPeer.emu.valid() ? const_cast<EmulationHost&>(hostPeer.emu).canonicalNetplayStateCrc32() : 0;
        const uint32_t clientCrc = clientPeer.emu.valid() ? const_cast<EmulationHost&>(clientPeer.emu).canonicalNetplayStateCrc32() : 0;
        const uint32_t confirmedFrame = std::min(
            hostPeer.coordinator.session().roomState().lastConfirmedFrame,
            clientPeer.coordinator.session().roomState().lastConfirmedFrame
        );
        const std::optional<uint32_t> hostConfirmedCrc =
            confirmedFrame > 0 ? hostPeer.emu.netplaySnapshotCrc32ForFrame(confirmedFrame) : std::nullopt;
        const std::optional<uint32_t> clientConfirmedCrc =
            confirmedFrame > 0 ? clientPeer.emu.netplaySnapshotCrc32ForFrame(confirmedFrame) : std::nullopt;
        const nlohmann::json snapshotMismatch = [&]() {
            const uint32_t maxFrame = std::min(hostPeer.emu.frameCount(), clientPeer.emu.frameCount());
            for(uint32_t frame = 1; frame <= maxFrame; ++frame) {
                const std::optional<uint32_t> hostSnapshot = hostPeer.emu.netplaySnapshotCrc32ForFrame(frame);
                const std::optional<uint32_t> clientSnapshot = clientPeer.emu.netplaySnapshotCrc32ForFrame(frame);
                if(!hostSnapshot.has_value() || !clientSnapshot.has_value()) continue;
                if(*hostSnapshot == *clientSnapshot) continue;
                return nlohmann::json{
                    {"frame", frame},
                    {"hostCrc32", *hostSnapshot},
                    {"clientCrc32", *clientSnapshot}
                };
            }
            return nlohmann::json{
                {"frame", 0},
                {"hostCrc32", 0},
                {"clientCrc32", 0}
            };
        }();
        const uint32_t mismatchFrame = snapshotMismatch.value("frame", 0u);
        const auto buildDesktopStateLoadProbe = [&](const DesktopPeerState& peer, uint32_t frame) {
            const std::optional<std::vector<uint8_t>> snapshot = peer.emu.netplaySnapshotForFrame(frame);
            if(!snapshot.has_value()) {
                return nlohmann::json{
                    {"requestedFrame", frame},
                    {"snapshotFound", false}
                };
            }

            GeraNESEmu probeEmu{DummyAudioOutput::instance()};
            if(!probeEmu.open(options.romPath) || !probeEmu.valid()) {
                return nlohmann::json{
                    {"requestedFrame", frame},
                    {"snapshotFound", true},
                    {"romOpened", false}
                };
            }

            probeEmu.loadStateFromMemory(snapshot.value());
            return nlohmann::json{
                {"requestedFrame", frame},
                {"snapshotFound", true},
                {"romOpened", true},
                {"loadedFrameCount", probeEmu.frameCount()},
                {"loadedStateCrc32", probeEmu.canonicalNetplayStateCrc32()}
            };
        };

        return {
            {"status", status},
            {"failureReason", failureReason},
            {"flowMode", "desktop_app"},
            {"romPath", options.romPath},
            {"framesRequested", options.frames},
            {"inputDelayFrames", options.inputDelayFrames},
            {"baselineLockstep", options.baselineLockstep},
            {"rollbackWindowFrames", options.rollbackWindowFrames},
            {"crcIntervalFrames", options.crcIntervalFrames},
            {"forceDesyncFrame", options.forceDesyncFrame},
            {"finalCrcMatch", hostCrc == clientCrc},
            {"confirmedFrame", confirmedFrame},
            {"confirmedSnapshotCrcMatch",
                hostConfirmedCrc.has_value() &&
                clientConfirmedCrc.has_value() &&
                *hostConfirmedCrc == *clientConfirmedCrc},
            {"hostConfirmedSnapshotCrc32", hostConfirmedCrc.value_or(0)},
            {"clientConfirmedSnapshotCrc32", clientConfirmedCrc.value_or(0)},
            {"snapshotMismatch", snapshotMismatch},
            {"hostStateLoadProbe", buildDesktopStateLoadProbe(hostPeer, hostPeer.lastResyncTargetFrame)},
            {"clientStateLoadProbe", buildDesktopStateLoadProbe(clientPeer, clientPeer.lastResyncTargetFrame)},
            {"hostMismatchStateLoadProbe", buildDesktopStateLoadProbe(hostPeer, mismatchFrame)},
            {"clientMismatchStateLoadProbe", buildDesktopStateLoadProbe(clientPeer, mismatchFrame)},
            {"timelineMismatch", timelineMismatch.has_value() ? *timelineMismatch : ""},
            {"host", buildPeerReport(hostPeer)},
            {"client", buildPeerReport(clientPeer)}
        };
    }

    static int emitReport(const Options& options, const nlohmann::json& report)
    {
        const std::string serialized = report.dump(2);
        if(!options.reportPath.empty()) {
            std::ofstream out(options.reportPath, std::ios::binary | std::ios::trunc);
            out << serialized;
            std::cout << options.reportPath << std::endl;
        } else {
            std::cout << serialized << std::endl;
        }

        return report.value("status", std::string()) == "ok" ? 0 : RESULT_FAILED;
    }

    static RunArtifacts finalizeArtifacts(const Options& options,
                                          const PeerState& hostPeer,
                                          const PeerState& clientPeer,
                                          const std::string& status,
                                          const std::string& failureReason,
                                          const std::optional<std::string>& timelineMismatch)
    {
        RunArtifacts artifacts;
        artifacts.report = buildReport(options, hostPeer, clientPeer, status, failureReason, timelineMismatch);
        artifacts.exitCode = artifacts.report.value("status", std::string()) == "ok" ? 0 : RESULT_FAILED;
        return artifacts;
    }

    static RunArtifacts runSingleCase(const Options& options)
    {
        if(options.romPath.empty()) {
            std::cerr << "Netplay test requires a ROM path." << std::endl;
            return {RESULT_ERROR, {{"status", "error"}, {"failureReason", "Netplay test requires a ROM path."}}};
        }

        PeerState hostPeer("Host", true, options.hostInputSeed);
        PeerState clientPeer("Client", false, options.clientInputSeed);

        hostPeer.emu.setSpeedBoost(false);
        clientPeer.emu.setSpeedBoost(false);
        hostPeer.emu.setPaused(false);
        clientPeer.emu.setPaused(false);

        if(!hostPeer.emu.open(options.romPath) || !hostPeer.emu.valid()) {
            std::cerr << "Failed to open ROM for host peer." << std::endl;
            return {RESULT_ERROR, {{"status", "error"}, {"failureReason", "Failed to open ROM for host peer."}}};
        }
        if(!clientPeer.emu.open(options.romPath) || !clientPeer.emu.valid()) {
            std::cerr << "Failed to open ROM for client peer." << std::endl;
            return {RESULT_ERROR, {{"status", "error"}, {"failureReason", "Failed to open ROM for client peer."}}};
        }

        configurePeerRuntime(hostPeer, options.rollbackWindowFrames);
        configurePeerRuntime(clientPeer, options.rollbackWindowFrames);

        const std::optional<Netplay::RomValidationData> romValidation = captureRomValidation(hostPeer.emu);
        if(!romValidation.has_value()) {
            std::cerr << "Failed to capture ROM validation data." << std::endl;
            return {RESULT_ERROR, {{"status", "error"}, {"failureReason", "Failed to capture ROM validation data."}}};
        }

        if(!hostPeer.coordinator.host(static_cast<uint16_t>(options.port), 2, "Host")) {
            std::cerr << "Failed to start netplay host: " << hostPeer.coordinator.lastError() << std::endl;
            return {RESULT_ERROR, {{"status", "error"}, {"failureReason", "Failed to start netplay host."}}};
        }

        if(!clientPeer.coordinator.join("127.0.0.1", static_cast<uint16_t>(options.port), "Client")) {
            std::cerr << "Failed to connect netplay client: " << clientPeer.coordinator.lastError() << std::endl;
            return {RESULT_ERROR, {{"status", "error"}, {"failureReason", "Failed to connect netplay client."}}};
        }

        const uint32_t configuredInputDelayFrames = realtimeEnhancementsEnabled(options)
            ? options.inputDelayFrames
            : 0u;
        hostPeer.coordinator.setInputDelayFrames(static_cast<uint8_t>(std::min<uint32_t>(configuredInputDelayFrames, 8u)));

        std::string failureReason;
        if(!bootstrapSession(hostPeer, clientPeer, options, romValidation, failureReason)) {
            return finalizeArtifacts(options, hostPeer, clientPeer, "bootstrap_failed", failureReason, std::nullopt);
        }

        uint32_t progressedFrames = 0;
        for(uint32_t step = 0; step < options.frameStepLimit && progressedFrames < options.frames; ++step) {
            const uint32_t previousHostFrame = hostPeer.emu.frameCount();
            const uint32_t previousClientFrame = clientPeer.emu.frameCount();

            runMaintenanceStep(hostPeer, clientPeer, options, romValidation);

            const bool hostQueued = queueLocalInputForNextFrame(hostPeer, options.frames, options);
            const bool clientQueued = queueLocalInputForNextFrame(clientPeer, options.frames, options);

            if(hostPeer.coordinator.session().roomState().state != Netplay::SessionState::Running) {
                resetInputDelayBuffers(hostPeer);
                hostPeer.lastInputDelayFrames = 0xFF;
            }
            if(clientPeer.coordinator.session().roomState().state != Netplay::SessionState::Running) {
                resetInputDelayBuffers(clientPeer);
                clientPeer.lastInputDelayFrames = 0xFF;
            }

            const bool hostAdvanced = advanceQueuedFrameIfPossible(hostPeer, options.frames, options);
            const bool clientAdvanced = advanceQueuedFrameIfPossible(clientPeer, options.frames, options);

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
                return finalizeArtifacts(options, hostPeer, clientPeer, "stalled", failureReason, std::nullopt);
            }
        }

        if(progressedFrames < options.frames) {
            std::ostringstream ss;
            ss << "Frame target not reached. Host frame=" << hostPeer.emu.frameCount()
               << ", Client frame=" << clientPeer.emu.frameCount();
            failureReason = ss.str();
            return finalizeArtifacts(options, hostPeer, clientPeer, "frame_limit_reached", failureReason, std::nullopt);
        }

        for(uint32_t settleStep = 0; settleStep < options.settleStepLimit; ++settleStep) {
            runMaintenanceStep(hostPeer, clientPeer, options, romValidation);
            hostPeer.coordinator.update(0);
            clientPeer.coordinator.update(0);

            const Netplay::FrameNumber settledConfirmedFrame = std::min(
                hostPeer.coordinator.session().roomState().lastConfirmedFrame,
                clientPeer.coordinator.session().roomState().lastConfirmedFrame
            );
            if(settledConfirmedFrame >= options.frames &&
               hostPeer.coordinator.session().roomState().state == Netplay::SessionState::Running &&
               clientPeer.coordinator.session().roomState().state == Netplay::SessionState::Running) {
                break;
            }
        }

        const std::optional<std::string> timelineMismatch = findTimelineMismatch(hostPeer, clientPeer);
        if(timelineMismatch.has_value()) {
            failureReason = *timelineMismatch;
            return finalizeArtifacts(options, hostPeer, clientPeer, "timeline_mismatch", failureReason, timelineMismatch);
        }

        const uint32_t hostCrc = hostPeer.emu.canonicalNetplayStateCrc32();
        const uint32_t clientCrc = clientPeer.emu.canonicalNetplayStateCrc32();
        if(hostCrc != clientCrc) {
            std::ostringstream ss;
            ss << "Final CRC mismatch. Host=" << hostCrc << ", Client=" << clientCrc;
            failureReason = ss.str();
            return finalizeArtifacts(options, hostPeer, clientPeer, "crc_mismatch", failureReason, std::nullopt);
        }

        uint32_t hostUnexpectedHardResyncs = hostPeer.coordinator.predictionStats().hardResyncCount;
        if(hasEventLogMessage(hostPeer, "Host started session setup; waiting for initial sync") &&
           hostUnexpectedHardResyncs > 0) {
            --hostUnexpectedHardResyncs;
        }

        if(options.forceDesyncFrame == 0 &&
           (hostUnexpectedHardResyncs > 0 ||
            clientPeer.coordinator.predictionStats().hardResyncCount > 0)) {
            std::ostringstream ss;
            ss << "Unexpected hard resyncs under local test. Host="
               << hostUnexpectedHardResyncs
               << ", Client="
               << clientPeer.coordinator.predictionStats().hardResyncCount;
            failureReason = ss.str();
            return finalizeArtifacts(options, hostPeer, clientPeer, "unexpected_resync", failureReason, std::nullopt);
        }

        return finalizeArtifacts(options, hostPeer, clientPeer, "ok", "", std::nullopt);
    }

    static RunArtifacts runSingleCaseAppFlow(const Options& options)
    {
        if(options.romPath.empty()) {
            return {RESULT_ERROR, {{"status", "error"}, {"failureReason", "Netplay test requires a ROM path."}}};
        }

        DesktopPeerState hostPeer("Host", true, options.hostInputSeed);
        DesktopPeerState clientPeer("Client", false, options.clientInputSeed);

        if(!hostPeer.emu.open(options.romPath) || !hostPeer.emu.valid()) {
            return {RESULT_ERROR, {{"status", "error"}, {"failureReason", "Failed to open ROM for host peer."}}};
        }
        if(!clientPeer.emu.open(options.romPath) || !clientPeer.emu.valid()) {
            return {RESULT_ERROR, {{"status", "error"}, {"failureReason", "Failed to open ROM for client peer."}}};
        }
        hostPeer.emu.setSimulationSuspended(true);
        clientPeer.emu.setSimulationSuspended(true);

        configurePeerRuntime(hostPeer, options.rollbackWindowFrames);
        configurePeerRuntime(clientPeer, options.rollbackWindowFrames);

        const std::optional<Netplay::RomValidationData> romValidation = captureRomValidation(hostPeer.emu);
        if(!romValidation.has_value()) {
            return {RESULT_ERROR, {{"status", "error"}, {"failureReason", "Failed to capture ROM validation data."}}};
        }

        if(!hostPeer.coordinator.host(static_cast<uint16_t>(options.port), 2, "Host")) {
            return {RESULT_ERROR, {{"status", "error"}, {"failureReason", "Failed to start netplay host."}}};
        }
        if(!clientPeer.coordinator.join("127.0.0.1", static_cast<uint16_t>(options.port), "Client")) {
            return {RESULT_ERROR, {{"status", "error"}, {"failureReason", "Failed to connect netplay client."}}};
        }

        const uint32_t configuredInputDelayFrames = realtimeEnhancementsEnabled(options)
            ? options.inputDelayFrames
            : 0u;
        hostPeer.coordinator.setInputDelayFrames(static_cast<uint8_t>(std::min<uint32_t>(configuredInputDelayFrames, 8u)));

        std::string failureReason;
        {
            bool hostRomSelected = false;
            bool hostAssigned = false;
            bool readyMarked = false;
            bool sessionStarted = false;

            for(uint32_t step = 0; step < options.startupTimeoutSteps; ++step) {
                hostPeer.coordinator.setLocalSimulationFrame(hostPeer.emu.frameCount());
                clientPeer.coordinator.setLocalSimulationFrame(clientPeer.emu.frameCount());
                hostPeer.coordinator.update(0);
                clientPeer.coordinator.update(0);
                handleSessionStateTransitions(hostPeer);
                handleSessionStateTransitions(clientPeer);

                syncLocalRomValidation(hostPeer.coordinator, romValidation);
                syncLocalRomValidation(clientPeer.coordinator, romValidation);
                hostPeer.emu.setSimulationSuspended(true);
                clientPeer.emu.setSimulationSuspended(true);

                processHostResyncIfNeeded(hostPeer, options);
                processPendingResyncIfNeeded(hostPeer);
                processPendingResyncIfNeeded(clientPeer);

                if(!hostRomSelected && romValidation.has_value() && hostPeer.coordinator.isConnected()) {
                    hostRomSelected = hostPeer.coordinator.selectRom(options.romPath, *romValidation);
                }

                const std::optional<Netplay::ParticipantId> remoteId = findRemoteParticipantId(hostPeer.coordinator);
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
                    break;
                }
            }
        }

        if(hostPeer.coordinator.session().roomState().state != Netplay::SessionState::Running ||
           clientPeer.coordinator.session().roomState().state != Netplay::SessionState::Running) {
            failureReason = "Desktop flow bootstrap timed out.";
            RunArtifacts artifacts;
            artifacts.report = buildDesktopFlowReport(options, hostPeer, clientPeer, "bootstrap_failed", failureReason, std::nullopt);
            artifacts.exitCode = RESULT_FAILED;
            return artifacts;
        }

        const uint32_t frameDt = std::max<uint32_t>(1, 1000u / std::max<uint32_t>(1, hostPeer.emu.getRegionFPS()));
        uint32_t progressedFrames = 0;
        for(uint32_t step = 0; step < options.frameStepLimit && progressedFrames < options.frames; ++step) {
            hostPeer.coordinator.setLocalSimulationFrame(hostPeer.emu.frameCount());
            clientPeer.coordinator.setLocalSimulationFrame(clientPeer.emu.frameCount());
            hostPeer.coordinator.update(0);
            clientPeer.coordinator.update(0);
            handleSessionStateTransitions(hostPeer);
            handleSessionStateTransitions(clientPeer);

            syncLocalRomValidation(hostPeer.coordinator, romValidation);
            syncLocalRomValidation(clientPeer.coordinator, romValidation);

            processHostResyncIfNeeded(hostPeer, options);
            processPendingResyncIfNeeded(hostPeer);
            processPendingResyncIfNeeded(clientPeer);
            processRollbackIfNeeded(hostPeer);
            processRollbackIfNeeded(clientPeer);
            maybeSubmitCrc(hostPeer, options.crcIntervalFrames);
            maybeSubmitCrc(clientPeer, options.crcIntervalFrames);

            queueLocalInputForNextFrame(hostPeer, options.frames, options);
            queueLocalInputForNextFrame(clientPeer, options.frames, options);
            const bool hostCanAdvance =
                canSimulateFrame(hostPeer, options) &&
                hostPeer.emu.frameCount() < options.frames;
            const bool clientCanAdvance =
                canSimulateFrame(clientPeer, options) &&
                clientPeer.emu.frameCount() < options.frames;
            if(options.baselineLockstep) {
                if(!hostCanAdvance) {
                    hostPeer.emu.setSimulationSuspended(true);
                }
                if(!clientCanAdvance) {
                    clientPeer.emu.setSimulationSuspended(true);
                }
                if(hostCanAdvance) {
                    hostPeer.emu.updateUntilFrame(frameDt);
                }
                if(clientCanAdvance) {
                    clientPeer.emu.updateUntilFrame(frameDt);
                }
            } else {
                hostPeer.emu.setSimulationSuspended(!hostCanAdvance);
                clientPeer.emu.setSimulationSuspended(!clientCanAdvance);
                hostPeer.emu.update(frameDt);
                clientPeer.emu.update(frameDt);
                std::this_thread::sleep_for(std::chrono::milliseconds(std::clamp<uint32_t>(frameDt, 1u, 16u)));
            }

            hostPeer.coordinator.setLocalSimulationFrame(hostPeer.emu.frameCount());
            clientPeer.coordinator.setLocalSimulationFrame(clientPeer.emu.frameCount());
            hostPeer.coordinator.update(0);
            clientPeer.coordinator.update(0);

            progressedFrames = std::min(hostPeer.emu.frameCount(), clientPeer.emu.frameCount());

            if(options.forceDesyncFrame > 0 &&
               !clientPeer.desyncInjected &&
               clientPeer.emu.frameCount() >= options.forceDesyncFrame) {
                clientPeer.emu.withExclusiveAccess([&](auto& emu) {
                    const uint8_t currentValue = emu.read(static_cast<int>(options.desyncAddress & 0xFFFFu));
                    emu.write(static_cast<int>(options.desyncAddress & 0xFFFFu),
                              static_cast<uint8_t>(currentValue ^ static_cast<uint8_t>(options.desyncValueXor & 0xFFu)));
                });
                clientPeer.desyncInjected = true;
            }
        }

        for(uint32_t settleStep = 0; settleStep < options.settleStepLimit; ++settleStep) {
            hostPeer.coordinator.setLocalSimulationFrame(hostPeer.emu.frameCount());
            clientPeer.coordinator.setLocalSimulationFrame(clientPeer.emu.frameCount());
            hostPeer.coordinator.update(0);
            clientPeer.coordinator.update(0);
            handleSessionStateTransitions(hostPeer);
            handleSessionStateTransitions(clientPeer);
            syncLocalRomValidation(hostPeer.coordinator, romValidation);
            syncLocalRomValidation(clientPeer.coordinator, romValidation);
            processHostResyncIfNeeded(hostPeer, options);
            processPendingResyncIfNeeded(hostPeer);
            processPendingResyncIfNeeded(clientPeer);
            processRollbackIfNeeded(hostPeer);
            processRollbackIfNeeded(clientPeer);
            maybeSubmitCrc(hostPeer, options.crcIntervalFrames);
            maybeSubmitCrc(clientPeer, options.crcIntervalFrames);
            const bool hostCanAdvance =
                canSimulateFrame(hostPeer, options) &&
                hostPeer.emu.frameCount() < options.frames;
            const bool clientCanAdvance =
                canSimulateFrame(clientPeer, options) &&
                clientPeer.emu.frameCount() < options.frames;
            queueLocalInputForNextFrame(hostPeer, options.frames, options);
            queueLocalInputForNextFrame(clientPeer, options.frames, options);
            if(options.baselineLockstep) {
                if(!hostCanAdvance) {
                    hostPeer.emu.setSimulationSuspended(true);
                }
                if(!clientCanAdvance) {
                    clientPeer.emu.setSimulationSuspended(true);
                }
                if(hostCanAdvance) {
                    hostPeer.emu.updateUntilFrame(frameDt);
                }
                if(clientCanAdvance) {
                    clientPeer.emu.updateUntilFrame(frameDt);
                }
            } else {
                hostPeer.emu.setSimulationSuspended(!hostCanAdvance);
                clientPeer.emu.setSimulationSuspended(!clientCanAdvance);
                hostPeer.emu.update(frameDt);
                clientPeer.emu.update(frameDt);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            const uint32_t settledConfirmedFrame = std::min(
                hostPeer.coordinator.session().roomState().lastConfirmedFrame,
                clientPeer.coordinator.session().roomState().lastConfirmedFrame
            );
            const std::optional<uint32_t> hostConfirmedSnapshotCrc32 =
                settledConfirmedFrame > 0 ? hostPeer.emu.netplaySnapshotCrc32ForFrame(settledConfirmedFrame) : std::nullopt;
            const std::optional<uint32_t> clientConfirmedSnapshotCrc32 =
                settledConfirmedFrame > 0 ? clientPeer.emu.netplaySnapshotCrc32ForFrame(settledConfirmedFrame) : std::nullopt;
            if(settledConfirmedFrame >= options.frames &&
               hostConfirmedSnapshotCrc32.has_value() &&
               clientConfirmedSnapshotCrc32.has_value() &&
               *hostConfirmedSnapshotCrc32 == *clientConfirmedSnapshotCrc32) {
                break;
            }
        }

        if(progressedFrames < options.frames) {
            std::ostringstream ss;
            ss << "Desktop flow frame target not reached. Host frame=" << hostPeer.emu.frameCount()
               << ", Client frame=" << clientPeer.emu.frameCount();
            failureReason = ss.str();
            RunArtifacts artifacts;
            artifacts.report = buildDesktopFlowReport(options, hostPeer, clientPeer, "frame_limit_reached", failureReason, std::nullopt);
            artifacts.exitCode = RESULT_FAILED;
            return artifacts;
        }

        const std::optional<std::string> timelineMismatch = findTimelineMismatch(hostPeer, clientPeer);
        if(timelineMismatch.has_value()) {
            RunArtifacts artifacts;
            artifacts.report = buildDesktopFlowReport(options, hostPeer, clientPeer, "timeline_mismatch", *timelineMismatch, timelineMismatch);
            artifacts.exitCode = RESULT_FAILED;
            return artifacts;
        }

        uint32_t hostUnexpectedHardResyncs = hostPeer.coordinator.predictionStats().hardResyncCount;
        if(hasEventLogMessage(hostPeer, "Host started session setup; waiting for initial sync") &&
           hostUnexpectedHardResyncs > 0) {
            --hostUnexpectedHardResyncs;
        }
        if(options.forceDesyncFrame == 0 &&
           (hostUnexpectedHardResyncs > 0 ||
            clientPeer.coordinator.predictionStats().hardResyncCount > 0)) {
            std::ostringstream ss;
            ss << "Unexpected hard resyncs under desktop app flow. Host="
               << hostUnexpectedHardResyncs
               << ", Client="
               << clientPeer.coordinator.predictionStats().hardResyncCount;
            RunArtifacts artifacts;
            artifacts.report = buildDesktopFlowReport(options, hostPeer, clientPeer, "unexpected_resync", ss.str(), std::nullopt);
            artifacts.exitCode = RESULT_FAILED;
            return artifacts;
        }

        const uint32_t confirmedFrame = std::min(
            hostPeer.coordinator.session().roomState().lastConfirmedFrame,
            clientPeer.coordinator.session().roomState().lastConfirmedFrame
        );
        const std::optional<uint32_t> hostConfirmedSnapshotCrc32 =
            confirmedFrame > 0 ? hostPeer.emu.netplaySnapshotCrc32ForFrame(confirmedFrame) : std::nullopt;
        const std::optional<uint32_t> clientConfirmedSnapshotCrc32 =
            confirmedFrame > 0 ? clientPeer.emu.netplaySnapshotCrc32ForFrame(confirmedFrame) : std::nullopt;
        if(confirmedFrame >= options.frames &&
           hostConfirmedSnapshotCrc32.has_value() &&
           clientConfirmedSnapshotCrc32.has_value() &&
           *hostConfirmedSnapshotCrc32 == *clientConfirmedSnapshotCrc32) {
            RunArtifacts artifacts;
            artifacts.report = buildDesktopFlowReport(options, hostPeer, clientPeer, "ok", "", std::nullopt);
            artifacts.exitCode = 0;
            return artifacts;
        }

        const uint32_t hostCrc = hostPeer.emu.canonicalNetplayStateCrc32();
        const uint32_t clientCrc = clientPeer.emu.canonicalNetplayStateCrc32();
        if(hostCrc != clientCrc) {
            std::ostringstream ss;
            ss << "Desktop flow CRC mismatch. Host=" << hostCrc << ", Client=" << clientCrc;
            failureReason = ss.str();
            RunArtifacts artifacts;
            artifacts.report = buildDesktopFlowReport(options, hostPeer, clientPeer, "crc_mismatch", failureReason, std::nullopt);
            artifacts.exitCode = RESULT_FAILED;
            return artifacts;
        }

        RunArtifacts artifacts;
        artifacts.report = buildDesktopFlowReport(options, hostPeer, clientPeer, "ok", "", std::nullopt);
        artifacts.exitCode = 0;
        return artifacts;
    }

public:
    static int runHeadless(const Options& options)
    {
        if(options.appFlow) {
            const RunArtifacts artifacts = runSingleCaseAppFlow(options);
            if(artifacts.exitCode == RESULT_ERROR && artifacts.report.empty()) {
                return RESULT_ERROR;
            }
            return emitReport(options, artifacts.report);
        }

        if(!options.robust) {
            const RunArtifacts artifacts = runSingleCase(options);
            if(artifacts.exitCode == RESULT_ERROR && artifacts.report.empty()) {
                return RESULT_ERROR;
            }
            return emitReport(options, artifacts.report);
        }

        std::vector<std::pair<std::string, Options>> cases;
        cases.push_back({"baseline", options});

        Options zeroDelay = options;
        zeroDelay.inputDelayFrames = 0;
        cases.push_back({"input_delay_0", zeroDelay});

        Options highDelay = options;
        highDelay.inputDelayFrames = std::min<uint32_t>(4u, 8u);
        cases.push_back({"input_delay_4", highDelay});

        Options alternateSeeds = options;
        alternateSeeds.hostInputSeed = 0x89ABCDEFu;
        alternateSeeds.clientInputSeed = 0x10293847u;
        cases.push_back({"alternate_seeds", alternateSeeds});

        nlohmann::json caseReports = nlohmann::json::array();
        int overallExitCode = 0;
        for(const auto& [label, caseOptions] : cases) {
            RunArtifacts artifacts = runSingleCase(caseOptions);
            nlohmann::json caseReport = artifacts.report;
            caseReport["caseLabel"] = label;
            caseReport["hostInputSeed"] = caseOptions.hostInputSeed;
            caseReport["clientInputSeed"] = caseOptions.clientInputSeed;
            caseReports.push_back(std::move(caseReport));
            if(artifacts.exitCode != 0 && overallExitCode == 0) {
                overallExitCode = artifacts.exitCode;
            }
        }

        nlohmann::json summary = {
            {"status", overallExitCode == 0 ? "ok" : "failed"},
            {"robust", true},
            {"romPath", options.romPath},
            {"framesRequested", options.frames},
            {"caseCount", caseReports.size()},
            {"cases", caseReports}
        };
        return emitReport(options, summary);
    }
};

#endif
