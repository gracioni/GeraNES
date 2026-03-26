#pragma once

#ifndef __EMSCRIPTEN__

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <thread>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "GeraNES/GeraNESEmu.h"
#include "GeraNESApp/EmulationHost.h"
#include "GeraNESNetplay/ConfirmedInputBufferDriver.h"
#include "GeraNESNetplay/NetplayCoordinator.h"

class NetplayTest
{
public:
    struct Options
    {
        std::string romPath;
        std::string reportPath;
        uint32_t frames = 600;
        uint32_t inputDelayFrames = 10;
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

        Buttons buttonsForFrame(uint32_t frameIndex, uint32_t fps) const
        {
            const uint32_t startupQuietFrames = std::max<uint32_t>(fps, 30u);
            if(frameIndex < startupQuietFrames) {
                return {};
            }

            Buttons buttons;
            const uint32_t activeIndex = frameIndex - startupQuietFrames;
            const uint32_t segmentLength = 5u + (mix(m_seed ^ 0xA511E9B3u) % 4u);
            const uint32_t segment = activeIndex / segmentLength;
            const uint32_t action = mix(m_seed ^ segment ^ 0x9E3779B9u);

            switch(action % 10u) {
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
            buttons.start = (action % 37u) == 0u;
            buttons.select = (action % 53u) == 0u;
            return buttons;
        }
    };

    struct PeerState
    {
        std::string name;
        bool host = false;
        GeraNESEmu emu{DummyAudioOutput::instance()};
        Netplay::NetplayCoordinator coordinator;
        DeterministicInputGenerator generator;
        uint32_t publishedThroughFrame = 0;
        uint32_t queuedThroughFrame = 0;

        explicit PeerState(const std::string& peerName, bool isHost, uint32_t seed)
            : name(peerName)
            , host(isHost)
            , generator(seed)
        {
        }
    };

    struct RunArtifacts
    {
        int exitCode = 1;
        nlohmann::json report;
    };

    struct AppPeerState
    {
        std::string name;
        bool host = false;
        EmulationHost emu{DummyAudioOutput::instance()};
        Netplay::NetplayCoordinator coordinator;
        Netplay::ConfirmedInputBufferDriver driver;
        DeterministicInputGenerator generator;
        uint32_t maxObservedInputBufferSize = 0;
        uint32_t maxObservedFutureBufferedFrames = 0;
        size_t maxObservedPendingFrameCount = 0;

        explicit AppPeerState(const std::string& peerName, bool isHost, uint32_t seed)
            : name(peerName)
            , host(isHost)
            , generator(seed)
        {
            emu.setSimulationSuspended(true);
            emu.setAutoQueuePendingInputOnFrameStart(false);
            emu.setAllowPresenterTimeoutAdvance(false);
            emu.setFrameInputResolver({});
            emu.setPreAdvanceHook([this](GeraNESEmu& emuRef) {
                driver.queuePendingFramesToEmu(emuRef);
            });
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

    static std::optional<Netplay::RomValidationData> captureRomValidation(EmulationHost& emu)
    {
        if(!emu.valid()) return std::nullopt;
        return emu.withExclusiveAccess([&](auto& innerEmu) {
            return captureRomValidation(innerEmu);
        });
    }

    static std::optional<Netplay::PlayerSlot> localAssignedSlot(const Netplay::NetplayCoordinator& coordinator)
    {
        for(const auto& participant : coordinator.session().roomState().participants) {
            if(participant.id == coordinator.localParticipantId() &&
               participant.controllerAssignment != Netplay::kObserverPlayerSlot) {
                return participant.controllerAssignment;
            }
        }
        return std::nullopt;
    }

    static uint32_t confirmedThroughFrame(const PeerState& peer)
    {
        return peer.coordinator.latestConfirmedFrame();
    }

    static InputFrame buildEmuInputFrame(PeerState& peer, uint32_t frame)
    {
        InputFrame inputFrame = peer.emu.createInputFrame(frame);
        const auto* confirmed = peer.coordinator.findConfirmedFrame(frame);
        if(confirmed == nullptr) {
            return inputFrame;
        }

        for(Netplay::PlayerSlot slot = 0; slot < 4; ++slot) {
            const uint64_t mask = confirmed->buttonMaskLo[slot];
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
                    inputFrame.p1A = a; inputFrame.p1B = b; inputFrame.p1Select = select; inputFrame.p1Start = start;
                    inputFrame.p1Up = up; inputFrame.p1Down = down; inputFrame.p1Left = left; inputFrame.p1Right = right;
                    break;
                case 1:
                    inputFrame.p2A = a; inputFrame.p2B = b; inputFrame.p2Select = select; inputFrame.p2Start = start;
                    inputFrame.p2Up = up; inputFrame.p2Down = down; inputFrame.p2Left = left; inputFrame.p2Right = right;
                    break;
                case 2:
                    inputFrame.p3A = a; inputFrame.p3B = b; inputFrame.p3Select = select; inputFrame.p3Start = start;
                    inputFrame.p3Up = up; inputFrame.p3Down = down; inputFrame.p3Left = left; inputFrame.p3Right = right;
                    break;
                case 3:
                    inputFrame.p4A = a; inputFrame.p4B = b; inputFrame.p4Select = select; inputFrame.p4Start = start;
                    inputFrame.p4Up = up; inputFrame.p4Down = down; inputFrame.p4Left = left; inputFrame.p4Right = right;
                    break;
                default:
                    break;
            }
        }

        return inputFrame;
    }

    static void pumpNetwork(PeerState& hostPeer, PeerState& clientPeer, uint32_t timeoutMs = 0)
    {
        for(int i = 0; i < 3; ++i) {
            hostPeer.coordinator.update(timeoutMs);
            clientPeer.coordinator.update(timeoutMs);
            timeoutMs = 0;
        }
    }

    template<typename HostPeer, typename ClientPeer>
    static void pumpCoordinators(HostPeer& hostPeer, ClientPeer& clientPeer, uint32_t timeoutMs = 0)
    {
        for(int i = 0; i < 3; ++i) {
            hostPeer.coordinator.update(timeoutMs);
            clientPeer.coordinator.update(timeoutMs);
            timeoutMs = 0;
        }
    }

    static bool bootstrapSession(PeerState& hostPeer, PeerState& clientPeer, const Options& options, std::string& failureReason)
    {
        if(!hostPeer.emu.open(options.romPath) || !hostPeer.emu.valid()) {
            failureReason = "Failed to open ROM on host.";
            return false;
        }
        if(!clientPeer.emu.open(options.romPath) || !clientPeer.emu.valid()) {
            failureReason = "Failed to open ROM on client.";
            return false;
        }

        bool sessionBootstrapped = false;
        for(uint32_t portOffset = 0; portOffset < 8u; ++portOffset) {
            const uint16_t port = static_cast<uint16_t>(options.port + portOffset);
            hostPeer.coordinator.disconnect();
            clientPeer.coordinator.disconnect();
            if(!hostPeer.coordinator.host(port, 1, hostPeer.name)) {
                continue;
            }
            if(!clientPeer.coordinator.join("127.0.0.1", port, clientPeer.name)) {
                hostPeer.coordinator.disconnect();
                continue;
            }
            sessionBootstrapped = true;
            break;
        }
        if(!sessionBootstrapped) {
            failureReason = "Failed to host/join room: host=" + hostPeer.coordinator.lastError() +
                            " client=" + clientPeer.coordinator.lastError();
            return false;
        }

        uint32_t waitSteps = 0;
        while(waitSteps < options.startupTimeoutSteps) {
            pumpNetwork(hostPeer, clientPeer, 1);

            const bool connected =
                hostPeer.coordinator.isConnected() &&
                clientPeer.coordinator.isConnected() &&
                hostPeer.coordinator.localParticipantId() != Netplay::kInvalidParticipantId &&
                clientPeer.coordinator.localParticipantId() != Netplay::kInvalidParticipantId &&
                hostPeer.coordinator.session().roomState().participants.size() >= 2 &&
                clientPeer.coordinator.session().roomState().participants.size() >= 2;
            if(connected) {
                break;
            }
            ++waitSteps;
        }

        if(waitSteps >= options.startupTimeoutSteps) {
            failureReason = "Timed out waiting for host/client connection.";
            return false;
        }

        const auto hostRom = captureRomValidation(hostPeer.emu);
        const auto clientRom = captureRomValidation(clientPeer.emu);
        if(!hostRom.has_value() || !clientRom.has_value()) {
            failureReason = "Failed to capture ROM validation data.";
            return false;
        }

        if(!hostPeer.coordinator.selectRom("NetplayTest", *hostRom)) {
            failureReason = "Host failed to select ROM.";
            return false;
        }
        if(!hostPeer.coordinator.submitLocalRomValidation(true, true, *hostRom)) {
            failureReason = "Host failed to submit ROM validation.";
            return false;
        }
        if(!clientPeer.coordinator.submitLocalRomValidation(true, true, *clientRom)) {
            failureReason = "Client failed to submit ROM validation.";
            return false;
        }

        for(uint32_t i = 0; i < options.startupTimeoutSteps; ++i) {
            pumpNetwork(hostPeer, clientPeer, 1);
            bool allCompatible = true;
            for(const auto& participant : hostPeer.coordinator.session().roomState().participants) {
                if(!participant.romLoaded || !participant.romCompatible) {
                    allCompatible = false;
                    break;
                }
            }
            if(allCompatible) {
                break;
            }
            if(i + 1u == options.startupTimeoutSteps) {
                failureReason = "Timed out waiting for ROM validation sync.";
                return false;
            }
        }

        const Netplay::ParticipantId hostId = hostPeer.coordinator.localParticipantId();
        const Netplay::ParticipantId clientId = clientPeer.coordinator.localParticipantId();
        if(!hostPeer.coordinator.assignController(hostId, 0)) {
            failureReason = "Failed to assign host to P1.";
            return false;
        }
        if(!hostPeer.coordinator.assignController(clientId, 1)) {
            failureReason = "Failed to assign client to P2.";
            return false;
        }
        hostPeer.coordinator.setInputDelayFrames(static_cast<uint8_t>(options.inputDelayFrames));

        for(uint32_t i = 0; i < options.startupTimeoutSteps; ++i) {
            pumpNetwork(hostPeer, clientPeer, 1);
            const auto hostSlot = localAssignedSlot(hostPeer.coordinator);
            const auto clientSlot = localAssignedSlot(clientPeer.coordinator);
            if(hostSlot == std::optional<Netplay::PlayerSlot>(0) &&
               clientSlot == std::optional<Netplay::PlayerSlot>(1)) {
                if(!hostPeer.coordinator.setLocalReady(true)) {
                    failureReason = "Host failed to set ready.";
                    return false;
                }
                if(!clientPeer.coordinator.setLocalReady(true)) {
                    failureReason = "Client failed to set ready.";
                    return false;
                }

                bool hostReady = false;
                bool clientReady = false;
                for(uint32_t readyStep = 0; readyStep < options.startupTimeoutSteps; ++readyStep) {
                    pumpNetwork(hostPeer, clientPeer, 1);

                    const Netplay::ParticipantInfo* hostParticipant =
                        hostPeer.coordinator.session().findParticipant(hostPeer.coordinator.localParticipantId());
                    const Netplay::ParticipantInfo* clientParticipant =
                        clientPeer.coordinator.session().findParticipant(clientPeer.coordinator.localParticipantId());
                    hostReady = hostParticipant != nullptr && hostParticipant->ready;
                    clientReady = clientParticipant != nullptr && clientParticipant->ready;
                    if(hostReady && clientReady) {
                        break;
                    }
                }

                if(!hostReady || !clientReady) {
                    failureReason = "Timed out waiting for ready state sync.";
                    return false;
                }

                if(!hostPeer.coordinator.startSession()) {
                    failureReason = "Host failed to start session.";
                    return false;
                }

                for(uint32_t startStep = 0; startStep < options.startupTimeoutSteps; ++startStep) {
                    pumpNetwork(hostPeer, clientPeer, 1);
                    if(hostPeer.coordinator.session().roomState().state == Netplay::SessionState::Running &&
                       clientPeer.coordinator.session().roomState().state == Netplay::SessionState::Running) {
                        return true;
                    }
                }

                failureReason = "Timed out waiting for running session state.";
                return false;
            }
        }

        failureReason = "Timed out waiting for controller assignments.";
        return false;
    }

    static void publishLocalInputs(PeerState& peer, const Options& options)
    {
        const std::optional<Netplay::PlayerSlot> slot = localAssignedSlot(peer.coordinator);
        if(!slot.has_value()) {
            return;
        }

        const uint32_t desiredThroughFrame = std::min<uint32_t>(
            options.frames + options.inputDelayFrames,
            peer.emu.frameCount() + options.inputDelayFrames
        );
        const uint32_t fps = std::max<uint32_t>(1u, peer.emu.getRegionFPS());

        while(peer.publishedThroughFrame < desiredThroughFrame) {
            ++peer.publishedThroughFrame;

            uint64_t mask = 0;
            if(peer.publishedThroughFrame > options.inputDelayFrames) {
                const uint32_t sourceFrame = peer.publishedThroughFrame - options.inputDelayFrames - 1u;
                const Buttons buttons = peer.generator.buttonsForFrame(sourceFrame, fps);
                mask = buildPadMask(buttons);
            }

            peer.coordinator.recordLocalInputFrame(peer.publishedThroughFrame, *slot, mask);
        }
    }

    static void queueConfirmedFrames(PeerState& peer)
    {
        const uint32_t throughFrame = confirmedThroughFrame(peer);
        while(peer.queuedThroughFrame < throughFrame) {
            ++peer.queuedThroughFrame;
            peer.emu.queueInputFrame(buildEmuInputFrame(peer, peer.queuedThroughFrame));
        }
    }

    static bool advanceExactlyOneFrame(PeerState& peer)
    {
        if(!peer.emu.valid()) return false;

        const uint32_t previousFrame = peer.emu.frameCount();
        uint32_t guard = 0;
        while(peer.emu.valid() && peer.emu.frameCount() == previousFrame && guard < 4096u) {
            peer.emu.update(1u);
            ++guard;
        }

        return peer.emu.valid() && peer.emu.frameCount() == previousFrame + 1u;
    }

    static nlohmann::json buildPeerReport(PeerState& peer)
    {
        nlohmann::json participants = nlohmann::json::array();
        for(const auto& participant : peer.coordinator.session().roomState().participants) {
            participants.push_back({
                {"id", participant.id},
                {"name", participant.displayName},
                {"controllerAssignment", participant.controllerAssignment},
                {"lastReceivedInputFrame", participant.lastReceivedInputFrame},
                {"lastContiguousInputFrame", participant.lastContiguousInputFrame},
                {"ready", participant.ready},
                {"romCompatible", participant.romCompatible}
            });
        }

        nlohmann::json eventTail = nlohmann::json::array();
        const auto& log = peer.coordinator.eventLog();
        const size_t start = log.size() > 50 ? (log.size() - 50) : 0;
        for(size_t i = start; i < log.size(); ++i) {
            eventTail.push_back(log[i]);
        }

        const std::optional<Netplay::PlayerSlot> localSlot = localAssignedSlot(peer.coordinator);
        const Netplay::TimelineInputEntry* latestLocal = nullptr;
        const Netplay::TimelineInputEntry* latestLocalConfirmed = nullptr;
        if(localSlot.has_value()) {
            latestLocal = peer.coordinator.localInputs().latestFor(peer.coordinator.localParticipantId(), *localSlot);
            latestLocalConfirmed = peer.coordinator.localInputs().latestConfirmedFor(peer.coordinator.localParticipantId(), *localSlot);
        }
        const auto* latestConfirmedFrame = peer.coordinator.findConfirmedFrame(peer.coordinator.latestConfirmedFrame());

        const InputFrame* frame0 = peer.emu.inputBuffer().findByFrame(0);
        const InputFrame* frame1 = peer.emu.inputBuffer().findByFrame(1);
        const std::vector<uint8_t> state = peer.emu.saveStateToMemory();

        return {
            {"name", peer.name},
            {"host", peer.host},
            {"frame", peer.emu.frameCount()},
            {"sessionState", static_cast<int>(peer.coordinator.session().roomState().state)},
            {"publishedThroughFrame", peer.publishedThroughFrame},
            {"queuedThroughFrame", peer.queuedThroughFrame},
            {"confirmedThroughFrame", confirmedThroughFrame(peer)},
            {"crc32", peer.emu.valid() ? peer.emu.canonicalStateCrc32() : 0u},
            {"stateSize", state.size()},
            {"inputBufferSize", peer.emu.inputBuffer().size()},
            {"inputFrame0", frame0 != nullptr ? frame0->toJson() : nlohmann::json()},
            {"inputFrame1", frame1 != nullptr ? frame1->toJson() : nlohmann::json()},
            {"localTimelineSize", peer.coordinator.localInputs().size()},
            {"remoteTimelineSize", peer.coordinator.remoteInputs().size()},
            {"latestLocalFrame", latestLocal != nullptr ? nlohmann::json{
                {"frame", latestLocal->frame},
                {"sequence", latestLocal->sequence},
                {"confirmed", latestLocal->confirmed}
            } : nlohmann::json()},
            {"latestLocalConfirmedFrame", latestLocalConfirmed != nullptr ? nlohmann::json{
                {"frame", latestLocalConfirmed->frame},
                {"sequence", latestLocalConfirmed->sequence},
                {"confirmed", latestLocalConfirmed->confirmed}
            } : nlohmann::json()},
            {"latestConfirmedBundleFrame", latestConfirmedFrame != nullptr ? nlohmann::json{
                {"frame", latestConfirmedFrame->frame},
                {"slot0MaskLo", latestConfirmedFrame->buttonMaskLo[0]},
                {"slot1MaskLo", latestConfirmedFrame->buttonMaskLo[1]},
                {"slot2MaskLo", latestConfirmedFrame->buttonMaskLo[2]},
                {"slot3MaskLo", latestConfirmedFrame->buttonMaskLo[3]}
            } : nlohmann::json()},
            {"participants", participants},
            {"eventLogTail", eventTail}
        };
    }

    static nlohmann::json buildReport(const Options& options,
                                      PeerState& hostPeer,
                                      PeerState& clientPeer,
                                      const std::string& status,
                                      const std::string& failureReason,
                                      uint32_t lastCheckedFrame)
    {
        return {
            {"status", status},
            {"failureReason", failureReason},
            {"romPath", options.romPath},
            {"frames", options.frames},
            {"inputDelayFrames", options.inputDelayFrames},
            {"lastCheckedFrame", lastCheckedFrame},
            {"finalCrcMatch", hostPeer.emu.valid() && clientPeer.emu.valid()
                ? (hostPeer.emu.canonicalStateCrc32() == clientPeer.emu.canonicalStateCrc32())
                : false},
            {"host", buildPeerReport(hostPeer)},
            {"client", buildPeerReport(clientPeer)}
        };
    }

    static nlohmann::json buildAppPeerReport(AppPeerState& peer)
    {
        nlohmann::json participants = nlohmann::json::array();
        for(const auto& participant : peer.coordinator.session().roomState().participants) {
            participants.push_back({
                {"id", participant.id},
                {"name", participant.displayName},
                {"controllerAssignment", participant.controllerAssignment},
                {"lastReceivedInputFrame", participant.lastReceivedInputFrame},
                {"lastContiguousInputFrame", participant.lastContiguousInputFrame},
                {"ready", participant.ready},
                {"romCompatible", participant.romCompatible}
            });
        }

        nlohmann::json eventTail = nlohmann::json::array();
        const auto& log = peer.coordinator.eventLog();
        const size_t start = log.size() > 50 ? (log.size() - 50) : 0;
        for(size_t i = start; i < log.size(); ++i) {
            eventTail.push_back(log[i]);
        }

        uint32_t inputBufferSize = 0;
        uint32_t futureBufferedFrames = 0;
        nlohmann::json nextFrameJson;
        nlohmann::json currentFrameJson;
        nlohmann::json previousFrameJson;
        nlohmann::json inputWindow = nlohmann::json::array();
        peer.emu.withExclusiveAccess([&](auto& innerEmu) {
            inputBufferSize = static_cast<uint32_t>(innerEmu.inputBuffer().size());
            const uint32_t frame = innerEmu.frameCount();
            for(uint32_t probe = frame + 1u; probe < frame + 256u; ++probe) {
                if(innerEmu.inputBuffer().findByFrame(probe) == nullptr) {
                    break;
                }
                ++futureBufferedFrames;
            }
            if(const InputFrame* previousFrame = innerEmu.inputBuffer().findByFrame(innerEmu.frameCount()); previousFrame != nullptr) {
                previousFrameJson = previousFrame->toJson();
            }
            if(const InputFrame* nextFrame = innerEmu.inputBuffer().findByFrame(innerEmu.frameCount() + 1u); nextFrame != nullptr) {
                nextFrameJson = nextFrame->toJson();
            }
            if(const InputFrame* currentFrame = innerEmu.inputBuffer().findByFrame(innerEmu.frameCount() + 2u); currentFrame != nullptr) {
                currentFrameJson = currentFrame->toJson();
            }
            const uint32_t windowStart = frame > 2u ? frame - 2u : 0u;
            for(uint32_t probe = windowStart; probe <= frame + 4u; ++probe) {
                if(const InputFrame* entry = innerEmu.inputBuffer().findByFrame(probe); entry != nullptr) {
                    inputWindow.push_back(entry->toJson());
                }
            }
        });

        nlohmann::json confirmedWindow = nlohmann::json::array();
        const uint32_t confirmedTail = peer.coordinator.latestConfirmedFrame();
        const uint32_t confirmedStart = confirmedTail > 6u ? confirmedTail - 6u : 1u;
        for(uint32_t frame = confirmedStart; frame <= confirmedTail; ++frame) {
            if(const auto* confirmed = peer.coordinator.findConfirmedFrame(frame); confirmed != nullptr) {
                confirmedWindow.push_back({
                    {"frame", confirmed->frame},
                    {"slot0MaskLo", confirmed->buttonMaskLo[0]},
                    {"slot1MaskLo", confirmed->buttonMaskLo[1]},
                    {"slot2MaskLo", confirmed->buttonMaskLo[2]},
                    {"slot3MaskLo", confirmed->buttonMaskLo[3]}
                });
            }
        }

        return {
            {"name", peer.name},
            {"host", peer.host},
            {"frame", peer.emu.frameCount()},
            {"lastFrameReadyFrame", peer.emu.lastFrameReadyFrame()},
            {"lastFrameReadyNetplayCrc32", peer.emu.lastFrameReadyNetplayCrc32()},
            {"sessionState", static_cast<int>(peer.coordinator.session().roomState().state)},
            {"confirmedThroughFrame", peer.coordinator.latestConfirmedFrame()},
            {"crc32", peer.emu.valid() ? peer.emu.canonicalStateCrc32() : 0u},
            {"netplayCrc32", peer.emu.valid() ? peer.emu.canonicalNetplayStateCrc32() : 0u},
            {"inputBufferSize", inputBufferSize},
            {"futureBufferedFrames", futureBufferedFrames},
            {"maxObservedInputBufferSize", peer.maxObservedInputBufferSize},
            {"maxObservedFutureBufferedFrames", peer.maxObservedFutureBufferedFrames},
            {"pendingDriverFrames", peer.driver.pendingFrameCount()},
            {"maxObservedPendingDriverFrames", peer.maxObservedPendingFrameCount},
            {"driverProducedThroughFrame", peer.driver.producedThroughFrame()},
            {"driverQueuedThroughFrame", peer.driver.queuedThroughFrame()},
            {"currentFrameInput", previousFrameJson},
            {"nextFrame", nextFrameJson},
            {"nextNextFrame", currentFrameJson},
            {"inputWindow", inputWindow},
            {"confirmedWindow", confirmedWindow},
            {"participants", participants},
            {"eventLogTail", eventTail}
        };
    }

    static nlohmann::json buildAppReport(const Options& options,
                                         AppPeerState& hostPeer,
                                         AppPeerState& clientPeer,
                                         const std::string& status,
                                         const std::string& failureReason,
                                         uint32_t lastCheckedFrame,
                                         uint32_t maxStallSteps)
    {
        return {
            {"status", status},
            {"failureReason", failureReason},
            {"romPath", options.romPath},
            {"frames", options.frames},
            {"inputDelayFrames", options.inputDelayFrames},
            {"lastCheckedFrame", lastCheckedFrame},
            {"maxStallSteps", maxStallSteps},
            {"finalCrcMatch", hostPeer.emu.valid() && clientPeer.emu.valid()
                ? (hostPeer.emu.canonicalStateCrc32() == clientPeer.emu.canonicalStateCrc32())
                : false},
            {"finalNetplayCrcMatch", hostPeer.emu.valid() && clientPeer.emu.valid()
                ? (hostPeer.emu.canonicalNetplayStateCrc32() == clientPeer.emu.canonicalNetplayStateCrc32())
                : false},
            {"finalFrameReadyCrcMatch", hostPeer.emu.lastFrameReadyFrame() == clientPeer.emu.lastFrameReadyFrame()
                ? (hostPeer.emu.lastFrameReadyNetplayCrc32() == clientPeer.emu.lastFrameReadyNetplayCrc32())
                : false},
            {"host", buildAppPeerReport(hostPeer)},
            {"client", buildAppPeerReport(clientPeer)}
        };
    }

    static bool bootstrapSession(AppPeerState& hostPeer, AppPeerState& clientPeer, const Options& options, std::string& failureReason)
    {
        if(!hostPeer.emu.open(options.romPath) || !hostPeer.emu.valid()) {
            failureReason = "Failed to open ROM on host.";
            return false;
        }
        if(!clientPeer.emu.open(options.romPath) || !clientPeer.emu.valid()) {
            failureReason = "Failed to open ROM on client.";
            return false;
        }

        hostPeer.emu.setSimulationSuspended(true);
        clientPeer.emu.setSimulationSuspended(true);

        bool sessionBootstrapped = false;
        for(uint32_t portOffset = 0; portOffset < 8u; ++portOffset) {
            const uint16_t port = static_cast<uint16_t>(options.port + portOffset);
            hostPeer.coordinator.disconnect();
            clientPeer.coordinator.disconnect();
            if(!hostPeer.coordinator.host(port, 1, hostPeer.name)) {
                continue;
            }
            if(!clientPeer.coordinator.join("127.0.0.1", port, clientPeer.name)) {
                hostPeer.coordinator.disconnect();
                continue;
            }
            sessionBootstrapped = true;
            break;
        }
        if(!sessionBootstrapped) {
            failureReason = "Failed to host/join room: host=" + hostPeer.coordinator.lastError() +
                            " client=" + clientPeer.coordinator.lastError();
            return false;
        }

        uint32_t waitSteps = 0;
        while(waitSteps < options.startupTimeoutSteps) {
            pumpCoordinators(hostPeer, clientPeer, 1);
            const bool connected =
                hostPeer.coordinator.isConnected() &&
                clientPeer.coordinator.isConnected() &&
                hostPeer.coordinator.localParticipantId() != Netplay::kInvalidParticipantId &&
                clientPeer.coordinator.localParticipantId() != Netplay::kInvalidParticipantId &&
                hostPeer.coordinator.session().roomState().participants.size() >= 2 &&
                clientPeer.coordinator.session().roomState().participants.size() >= 2;
            if(connected) {
                break;
            }
            ++waitSteps;
        }

        if(waitSteps >= options.startupTimeoutSteps) {
            failureReason = "Timed out waiting for host/client connection.";
            return false;
        }

        const auto hostRom = captureRomValidation(hostPeer.emu);
        const auto clientRom = captureRomValidation(clientPeer.emu);
        if(!hostRom.has_value() || !clientRom.has_value()) {
            failureReason = "Failed to capture ROM validation data.";
            return false;
        }

        if(!hostPeer.coordinator.selectRom("NetplayTest", *hostRom)) {
            failureReason = "Host failed to select ROM.";
            return false;
        }
        if(!hostPeer.coordinator.submitLocalRomValidation(true, true, *hostRom)) {
            failureReason = "Host failed to submit ROM validation.";
            return false;
        }
        if(!clientPeer.coordinator.submitLocalRomValidation(true, true, *clientRom)) {
            failureReason = "Client failed to submit ROM validation.";
            return false;
        }

        for(uint32_t i = 0; i < options.startupTimeoutSteps; ++i) {
            pumpCoordinators(hostPeer, clientPeer, 1);
            bool allCompatible = true;
            for(const auto& participant : hostPeer.coordinator.session().roomState().participants) {
                if(!participant.romLoaded || !participant.romCompatible) {
                    allCompatible = false;
                    break;
                }
            }
            if(allCompatible) break;
        }

        const Netplay::ParticipantId hostId = hostPeer.coordinator.localParticipantId();
        const Netplay::ParticipantId clientId = clientPeer.coordinator.localParticipantId();
        hostPeer.coordinator.setInputDelayFrames(static_cast<uint8_t>(options.inputDelayFrames));

        for(uint32_t i = 0; i < options.startupTimeoutSteps; ++i) {
            const bool hostAssigned = hostPeer.coordinator.assignController(hostId, 0);
            const bool clientAssigned = hostPeer.coordinator.assignController(clientId, 1);
            pumpCoordinators(hostPeer, clientPeer, 1);
            const auto hostSlot = localAssignedSlot(hostPeer.coordinator);
            const auto clientSlot = localAssignedSlot(clientPeer.coordinator);
            if(hostAssigned &&
               clientAssigned &&
               hostSlot == std::optional<Netplay::PlayerSlot>(0) &&
               clientSlot == std::optional<Netplay::PlayerSlot>(1)) {
                if(!hostPeer.coordinator.setLocalReady(true)) {
                    failureReason = "Host failed to set ready.";
                    return false;
                }
                if(!clientPeer.coordinator.setLocalReady(true)) {
                    failureReason = "Client failed to set ready.";
                    return false;
                }

                bool hostReady = false;
                bool clientReady = false;
                for(uint32_t readyStep = 0; readyStep < options.startupTimeoutSteps; ++readyStep) {
                    pumpCoordinators(hostPeer, clientPeer, 1);
                    const Netplay::ParticipantInfo* hostParticipant =
                        hostPeer.coordinator.session().findParticipant(hostPeer.coordinator.localParticipantId());
                    const Netplay::ParticipantInfo* clientParticipant =
                        clientPeer.coordinator.session().findParticipant(clientPeer.coordinator.localParticipantId());
                    hostReady = hostParticipant != nullptr && hostParticipant->ready;
                    clientReady = clientParticipant != nullptr && clientParticipant->ready;
                    if(hostReady && clientReady) {
                        break;
                    }
                }

                if(!hostReady || !clientReady) {
                    failureReason = "Timed out waiting for ready state sync.";
                    return false;
                }

                if(!hostPeer.coordinator.startSession()) {
                    failureReason = "Host failed to start session.";
                    return false;
                }

                for(uint32_t startStep = 0; startStep < options.startupTimeoutSteps; ++startStep) {
                    pumpCoordinators(hostPeer, clientPeer, 1);
                    if(hostPeer.coordinator.session().roomState().state == Netplay::SessionState::Running &&
                       clientPeer.coordinator.session().roomState().state == Netplay::SessionState::Running) {
                        return true;
                    }
                }

                failureReason = "Timed out waiting for running session state.";
                return false;
            }
        }

        failureReason = "Timed out waiting for controller assignments.";
        return false;
    }

    static void produceAppLocalInputs(AppPeerState& peer, const Options& options, uint32_t dtMs)
    {
        const std::optional<Netplay::PlayerSlot> slot = localAssignedSlot(peer.coordinator);
        uint64_t localPrimaryMask = 0;
        if(slot.has_value()) {
            const uint32_t fps = std::max<uint32_t>(1u, peer.emu.getRegionFPS());
            const uint32_t sourceFrame = peer.driver.producedThroughFrame() + 1u;
            const Buttons buttons = peer.generator.buttonsForFrame(sourceFrame, fps);
            localPrimaryMask = buildPadMask(buttons);
        }

        peer.driver.produceLocalBufferedInputs(
            peer.coordinator,
            peer.coordinator.isActive(),
            peer.coordinator.awaitingSpectatorSync(),
            peer.coordinator.session().roomState().state,
            slot,
            dtMs,
            localPrimaryMask,
            peer.emu.getRegionFPS(),
            peer.emu.frameCount(),
            peer.coordinator.latestConfirmedFrame()
        );
    }

    static RunArtifacts runSingleCaseAppFlow(const Options& options)
    {
        RunArtifacts result;
        AppPeerState hostPeer("Host", true, options.hostInputSeed);
        AppPeerState clientPeer("Client", false, options.clientInputSeed);
        uint32_t lastCheckedFrame = 0;
        uint32_t stallSteps = 0;
        uint32_t maxStallSteps = 0;

        const auto cleanup = [&]() {
            clientPeer.coordinator.disconnect();
            hostPeer.coordinator.disconnect();
            clientPeer.emu.shutdown();
            hostPeer.emu.shutdown();
        };

        std::string failureReason;
        if(!bootstrapSession(hostPeer, clientPeer, options, failureReason)) {
            result.report = buildAppReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
            result.exitCode = RESULT_ERROR;
            cleanup();
            return result;
        }

        const uint32_t frameDt = std::max<uint32_t>(1u, 1000u / std::max<uint32_t>(1u, hostPeer.emu.getRegionFPS()));
        auto lastLoopTime = std::chrono::steady_clock::now();
        const auto freezePeersForInspection = [&]() {
            hostPeer.emu.setSimulationSuspended(true);
            clientPeer.emu.setSimulationSuspended(true);

            for(uint32_t settle = 0; settle < 120u; ++settle) {
                const uint32_t hostBefore = hostPeer.emu.frameCount();
                const uint32_t clientBefore = clientPeer.emu.frameCount();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                const uint32_t hostAfter = hostPeer.emu.frameCount();
                const uint32_t clientAfter = clientPeer.emu.frameCount();
                if(hostBefore == hostAfter && clientBefore == clientAfter) {
                    return true;
                }
            }
            return false;
        };

        for(uint32_t step = 0; step < options.frameStepLimit; ++step) {
            const auto now = std::chrono::steady_clock::now();
            const uint32_t loopDtMs = std::max<uint32_t>(
                1u,
                static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLoopTime).count())
            );
            lastLoopTime = now;

            hostPeer.coordinator.setLocalSimulationFrame(hostPeer.emu.exactEmulationFrame());
            clientPeer.coordinator.setLocalSimulationFrame(clientPeer.emu.exactEmulationFrame());

            produceAppLocalInputs(hostPeer, options, loopDtMs);
            produceAppLocalInputs(clientPeer, options, loopDtMs);
            pumpCoordinators(hostPeer, clientPeer, 0);

            hostPeer.driver.prepareConfirmedFramesForEmulationThread(
                hostPeer.coordinator,
                hostPeer.coordinator.isActive(),
                hostPeer.coordinator.awaitingSpectatorSync(),
                hostPeer.coordinator.session().roomState().state,
                hostPeer.emu.exactEmulationFrame()
            );
            clientPeer.driver.prepareConfirmedFramesForEmulationThread(
                clientPeer.coordinator,
                clientPeer.coordinator.isActive(),
                clientPeer.coordinator.awaitingSpectatorSync(),
                clientPeer.coordinator.session().roomState().state,
                clientPeer.emu.exactEmulationFrame()
            );

            const uint32_t hostFrame = hostPeer.emu.exactEmulationFrame();
            const uint32_t clientFrame = clientPeer.emu.exactEmulationFrame();
            const bool hostCanAdvance = hostFrame + 1u <= hostPeer.coordinator.latestConfirmedFrame();
            const bool clientCanAdvance = clientFrame + 1u <= clientPeer.coordinator.latestConfirmedFrame();

            hostPeer.emu.setSimulationSuspended(!hostCanAdvance);
            clientPeer.emu.setSimulationSuspended(!clientCanAdvance);
            if(hostCanAdvance) hostPeer.emu.update(loopDtMs);
            if(clientCanAdvance) clientPeer.emu.update(loopDtMs);

            std::this_thread::sleep_for(std::chrono::milliseconds(2));

            const uint32_t newHostFrame = hostPeer.emu.exactEmulationFrame();
            const uint32_t newClientFrame = clientPeer.emu.exactEmulationFrame();

            hostPeer.emu.withExclusiveAccess([&](auto& emu) {
                hostPeer.maxObservedInputBufferSize = std::max<uint32_t>(
                    hostPeer.maxObservedInputBufferSize,
                    static_cast<uint32_t>(emu.inputBuffer().size())
                );
                uint32_t futureBufferedFrames = 0;
                const uint32_t frame = emu.frameCount();
                for(uint32_t probe = frame + 1u; probe < frame + 256u; ++probe) {
                    if(emu.inputBuffer().findByFrame(probe) == nullptr) {
                        break;
                    }
                    ++futureBufferedFrames;
                }
                hostPeer.maxObservedFutureBufferedFrames = std::max(hostPeer.maxObservedFutureBufferedFrames, futureBufferedFrames);
            });
            clientPeer.emu.withExclusiveAccess([&](auto& emu) {
                clientPeer.maxObservedInputBufferSize = std::max<uint32_t>(
                    clientPeer.maxObservedInputBufferSize,
                    static_cast<uint32_t>(emu.inputBuffer().size())
                );
                uint32_t futureBufferedFrames = 0;
                const uint32_t frame = emu.frameCount();
                for(uint32_t probe = frame + 1u; probe < frame + 256u; ++probe) {
                    if(emu.inputBuffer().findByFrame(probe) == nullptr) {
                        break;
                    }
                    ++futureBufferedFrames;
                }
                clientPeer.maxObservedFutureBufferedFrames = std::max(clientPeer.maxObservedFutureBufferedFrames, futureBufferedFrames);
            });
            hostPeer.maxObservedPendingFrameCount = std::max(hostPeer.maxObservedPendingFrameCount, hostPeer.driver.pendingFrameCount());
            clientPeer.maxObservedPendingFrameCount = std::max(clientPeer.maxObservedPendingFrameCount, clientPeer.driver.pendingFrameCount());

            if(newHostFrame == hostFrame && newClientFrame == clientFrame && (hostCanAdvance || clientCanAdvance)) {
                ++stallSteps;
            } else {
                stallSteps = 0;
            }
            maxStallSteps = std::max(maxStallSteps, stallSteps);

            if(hostPeer.maxObservedFutureBufferedFrames > options.inputDelayFrames + 16u ||
               clientPeer.maxObservedFutureBufferedFrames > options.inputDelayFrames + 16u) {
                failureReason = "Future InputBuffer horizon grew beyond the expected bound under app flow.";
                result.report = buildAppReport(options, hostPeer, clientPeer, "buffer_overfill", failureReason, lastCheckedFrame, maxStallSteps);
                result.exitCode = RESULT_FAILED;
                cleanup();
                return result;
            }

            if(hostPeer.maxObservedPendingFrameCount > options.inputDelayFrames + 64u ||
               clientPeer.maxObservedPendingFrameCount > options.inputDelayFrames + 64u) {
                failureReason = "Pending confirmed frame queue grew beyond the expected bound under app flow.";
                result.report = buildAppReport(options, hostPeer, clientPeer, "pending_overfill", failureReason, lastCheckedFrame, maxStallSteps);
                result.exitCode = RESULT_FAILED;
                cleanup();
                return result;
            }

            if(stallSteps > 240u) {
                failureReason = "App flow stalled while confirmed frames existed.";
                result.report = buildAppReport(options, hostPeer, clientPeer, "stalled", failureReason, lastCheckedFrame, maxStallSteps);
                result.exitCode = RESULT_FAILED;
                cleanup();
                return result;
            }

            const uint32_t hostReadyFrame = hostPeer.emu.lastFrameReadyFrame();
            const uint32_t clientReadyFrame = clientPeer.emu.lastFrameReadyFrame();
            if(hostReadyFrame == clientReadyFrame && hostReadyFrame > lastCheckedFrame) {
                lastCheckedFrame = hostReadyFrame;
                if(hostPeer.emu.lastFrameReadyNetplayCrc32() != clientPeer.emu.lastFrameReadyNetplayCrc32()) {
                    freezePeersForInspection();
                    const uint32_t settledHostReadyFrame = hostPeer.emu.lastFrameReadyFrame();
                    const uint32_t settledClientReadyFrame = clientPeer.emu.lastFrameReadyFrame();
                    if(settledHostReadyFrame != settledClientReadyFrame ||
                       hostPeer.emu.lastFrameReadyNetplayCrc32() == clientPeer.emu.lastFrameReadyNetplayCrc32()) {
                        continue;
                    }
                    const std::vector<uint8_t> hostState = hostPeer.emu.saveNetplayStateToMemory();
                    const std::vector<uint8_t> clientState = clientPeer.emu.saveNetplayStateToMemory();
                    size_t firstDiff = std::min(hostState.size(), clientState.size());
                    for(size_t i = 0; i < std::min(hostState.size(), clientState.size()); ++i) {
                        if(hostState[i] != clientState[i]) {
                            firstDiff = i;
                            break;
                        }
                    }

                    failureReason = "Canonical netplay CRC mismatch at frame " + std::to_string(lastCheckedFrame) + " under app flow.";
                    if(hostState.size() != clientState.size()) {
                        failureReason += " stateSize(host=" + std::to_string(hostState.size()) +
                                         ", client=" + std::to_string(clientState.size()) + ")";
                    } else if(firstDiff < hostState.size()) {
                        failureReason += " firstDiffByte=" + std::to_string(firstDiff) +
                                         " host=" + std::to_string(hostState[firstDiff]) +
                                         " client=" + std::to_string(clientState[firstDiff]);
                    }
                    result.report = buildAppReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps);
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
            }

            if(newHostFrame >= options.frames && newClientFrame >= options.frames) {
                result.report = buildAppReport(options, hostPeer, clientPeer, "ok", "", lastCheckedFrame, maxStallSteps);
                result.exitCode = EXIT_SUCCESS;
                cleanup();
                return result;
            }
        }

        failureReason = "App-flow netplay test reached the step limit.";
        result.report = buildAppReport(options, hostPeer, clientPeer, "stalled", failureReason, lastCheckedFrame, maxStallSteps);
        result.exitCode = RESULT_FAILED;
        cleanup();
        return result;
    }

    static void writeReport(const Options& options, const nlohmann::json& report)
    {
        const std::string serialized = report.dump(2);
        if(!options.reportPath.empty()) {
            std::ofstream output(options.reportPath, std::ios::binary);
            output << serialized;
            output.close();
            std::cout << options.reportPath << std::endl;
        } else {
            std::cout << serialized << std::endl;
        }
    }

    static RunArtifacts runSingleCase(const Options& options)
    {
        RunArtifacts result;
        PeerState hostPeer("Host", true, options.hostInputSeed);
        PeerState clientPeer("Client", false, options.clientInputSeed);
        uint32_t lastCheckedFrame = 0;

        const auto cleanup = [&]() {
            clientPeer.coordinator.disconnect();
            hostPeer.coordinator.disconnect();
        };

        std::string failureReason;
        if(!bootstrapSession(hostPeer, clientPeer, options, failureReason)) {
            result.report = buildReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame);
            result.exitCode = RESULT_ERROR;
            cleanup();
            return result;
        }

        const std::vector<uint8_t> hostInitialState = hostPeer.emu.saveStateToMemory();
        const std::vector<uint8_t> clientInitialState = clientPeer.emu.saveStateToMemory();
        if(hostPeer.emu.canonicalStateCrc32() != clientPeer.emu.canonicalStateCrc32()) {
            size_t firstDiff = std::min(hostInitialState.size(), clientInitialState.size());
            for(size_t i = 0; i < std::min(hostInitialState.size(), clientInitialState.size()); ++i) {
                if(hostInitialState[i] != clientInitialState[i]) {
                    firstDiff = i;
                    break;
                }
            }

            failureReason = "Canonical CRC mismatch immediately after session bootstrap.";
            if(hostInitialState.size() != clientInitialState.size()) {
                failureReason += " stateSize(host=" + std::to_string(hostInitialState.size()) +
                                 ", client=" + std::to_string(clientInitialState.size()) + ")";
            } else if(firstDiff < hostInitialState.size()) {
                failureReason += " firstDiffByte=" + std::to_string(firstDiff) +
                                 " host=" + std::to_string(hostInitialState[firstDiff]) +
                                 " client=" + std::to_string(clientInitialState[firstDiff]);
            }
            result.report = buildReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame);
            result.exitCode = RESULT_FAILED;
            cleanup();
            return result;
        }

        for(uint32_t step = 0; step < options.frameStepLimit; ++step) {
            hostPeer.coordinator.setLocalSimulationFrame(hostPeer.emu.frameCount());
            clientPeer.coordinator.setLocalSimulationFrame(clientPeer.emu.frameCount());

            publishLocalInputs(hostPeer, options);
            publishLocalInputs(clientPeer, options);
            pumpNetwork(hostPeer, clientPeer, 0);

            queueConfirmedFrames(hostPeer);
            queueConfirmedFrames(clientPeer);

            const bool hostCanAdvance =
                hostPeer.emu.frameCount() < options.frames &&
                hostPeer.emu.inputBuffer().findByFrame(hostPeer.emu.frameCount() + 1u) != nullptr;
            const bool clientCanAdvance =
                clientPeer.emu.frameCount() < options.frames &&
                clientPeer.emu.inputBuffer().findByFrame(clientPeer.emu.frameCount() + 1u) != nullptr;

            if(hostCanAdvance != clientCanAdvance) {
                failureReason = "Only one peer can advance the next frame.";
                result.report = buildReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame);
                result.exitCode = RESULT_FAILED;
                cleanup();
                return result;
            }

            if(hostCanAdvance) {
                if(!advanceExactlyOneFrame(hostPeer) || !advanceExactlyOneFrame(clientPeer)) {
                    failureReason = "Failed to advance emulation by one frame on both peers.";
                    result.report = buildReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame);
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
            }

            if(hostPeer.emu.frameCount() == clientPeer.emu.frameCount() &&
               hostPeer.emu.frameCount() > lastCheckedFrame) {
                lastCheckedFrame = hostPeer.emu.frameCount();
                if(hostPeer.emu.canonicalStateCrc32() != clientPeer.emu.canonicalStateCrc32()) {
                    failureReason = "Canonical CRC mismatch at frame " + std::to_string(lastCheckedFrame) + ".";
                    result.report = buildReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame);
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
            }

            if(hostPeer.emu.frameCount() >= options.frames &&
               clientPeer.emu.frameCount() >= options.frames) {
                result.report = buildReport(options, hostPeer, clientPeer, "ok", "", lastCheckedFrame);
                result.exitCode = EXIT_SUCCESS;
                cleanup();
                return result;
            }
        }

        failureReason = "Netplay test reached the step limit before both peers reached the target frame.";
        result.report = buildReport(options, hostPeer, clientPeer, "stalled", failureReason, lastCheckedFrame);
        result.exitCode = RESULT_FAILED;
        cleanup();
        return result;
    }

    static RunArtifacts runRobustMatrix(const Options& baseOptions)
    {
        struct Scenario
        {
            std::string name;
            Options options;
        };

        std::vector<Scenario> scenarios;
        const auto addScenario = [&](const std::string& name, const Options& options) {
            for(const Scenario& existing : scenarios) {
                if(existing.name == name) {
                    return;
                }
            }
            scenarios.push_back({name, options});
        };

        addScenario("baseline", baseOptions);

        Options delay1 = baseOptions;
        delay1.inputDelayFrames = 1;
        addScenario("delay_1", delay1);

        Options delay4 = baseOptions;
        delay4.inputDelayFrames = 4;
        addScenario("delay_4", delay4);

        Options delay10 = baseOptions;
        delay10.inputDelayFrames = 10;
        addScenario("delay_10", delay10);

        Options seedMix = baseOptions;
        seedMix.hostInputSeed = 0x00000001u;
        seedMix.clientInputSeed = 0xDEADBEEFu;
        addScenario("seed_mix", seedMix);

        if(baseOptions.appFlow && !baseOptions.baselineLockstep) {
            Options lockstep = baseOptions;
            lockstep.baselineLockstep = true;
            addScenario("baseline_lockstep", lockstep);

            Options lockstepDelay1 = lockstep;
            lockstepDelay1.inputDelayFrames = 1;
            addScenario("baseline_lockstep_delay_1", lockstepDelay1);
        }

        const bool includeLongScenarios = baseOptions.appFlow || baseOptions.frames >= 1000u;
        const uint32_t longFrames = std::max<uint32_t>(baseOptions.frames, 2000u);
        if(includeLongScenarios) {
            Options longSession = baseOptions;
            longSession.frames = longFrames;
            addScenario("long_session", longSession);

            Options longSeedMix = longSession;
            longSeedMix.hostInputSeed = 0x00000001u;
            longSeedMix.clientInputSeed = 0xDEADBEEFu;
            addScenario("long_session_seed_mix", longSeedMix);

            if(baseOptions.appFlow && !baseOptions.baselineLockstep) {
                Options longLockstep = longSession;
                longLockstep.baselineLockstep = true;
                addScenario("long_session_lockstep", longLockstep);

                Options longLockstepSeedMix = longLockstep;
                longLockstepSeedMix.hostInputSeed = 0x00000001u;
                longLockstepSeedMix.clientInputSeed = 0xDEADBEEFu;
                addScenario("long_session_lockstep_seed_mix", longLockstepSeedMix);
            }
        }

        RunArtifacts aggregate;
        aggregate.exitCode = EXIT_SUCCESS;
        aggregate.report = {
            {"status", "ok"},
            {"mode", baseOptions.appFlow ? "app-flow" : "direct-core"},
            {"robust", true},
            {"frames", baseOptions.frames},
            {"cases", nlohmann::json::array()},
            {"summary", nlohmann::json::array()}
        };

        for(size_t index = 0; index < scenarios.size(); ++index) {
            Scenario scenario = scenarios[index];
            scenario.options.robust = false;

            std::cerr
                << "NetplayTest robust case " << (index + 1u) << "/" << scenarios.size()
                << ": " << scenario.name
                << " frames=" << scenario.options.frames
                << " inputDelay=" << scenario.options.inputDelayFrames
                << " hostSeed=" << scenario.options.hostInputSeed
                << " clientSeed=" << scenario.options.clientInputSeed
                << (scenario.options.baselineLockstep ? " lockstep" : "")
                << std::endl;

            RunArtifacts caseResult = scenario.options.appFlow
                ? runSingleCaseAppFlow(scenario.options)
                : runSingleCase(scenario.options);

            caseResult.report["scenario"] = scenario.name;
            aggregate.report["cases"].push_back(caseResult.report);
            aggregate.report["summary"].push_back({
                {"scenario", scenario.name},
                {"status", caseResult.report.value("status", std::string("unknown"))},
                {"mode", scenario.options.appFlow ? "app-flow" : "direct-core"},
                {"frames", scenario.options.frames},
                {"inputDelayFrames", scenario.options.inputDelayFrames},
                {"hostSeed", scenario.options.hostInputSeed},
                {"clientSeed", scenario.options.clientInputSeed},
                {"baselineLockstep", scenario.options.baselineLockstep},
                {"lastCheckedFrame", caseResult.report.value("lastCheckedFrame", 0u)},
                {"finalCrcMatch", caseResult.report.value("finalCrcMatch", false)},
                {"finalNetplayCrcMatch", caseResult.report.value("finalNetplayCrcMatch", false)},
                {"finalFrameReadyCrcMatch", caseResult.report.value("finalFrameReadyCrcMatch", false)},
                {"failureReason", caseResult.report.value("failureReason", std::string())}
            });

            if(caseResult.exitCode != EXIT_SUCCESS) {
                aggregate.exitCode = caseResult.exitCode;
                aggregate.report["status"] = "failed";
                aggregate.report["failedScenario"] = scenario.name;
                aggregate.report["failureReason"] =
                    caseResult.report.value("failureReason", std::string("Robust netplay scenario failed."));
                return aggregate;
            }
        }

        aggregate.report["caseCount"] = scenarios.size();
        aggregate.report["failedScenario"] = nullptr;
        aggregate.report["failureReason"] = "";
        return aggregate;
    }

public:
    static int runHeadless(const Options& options)
    {
        std::cerr
            << "NetplayTest: frames=" << options.frames
            << " inputDelay=" << options.inputDelayFrames
            << " hostSeed=" << options.hostInputSeed
            << " clientSeed=" << options.clientInputSeed
            << std::endl;

        RunArtifacts result = options.robust
            ? runRobustMatrix(options)
            : (options.appFlow ? runSingleCaseAppFlow(options) : runSingleCase(options));
        writeReport(options, result.report);
        return result.exitCode;
    }
};

#endif
