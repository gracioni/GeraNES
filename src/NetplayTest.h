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
#include "GeraNESApp/AppSettings.h"
#include "GeraNESApp/EmulationHost.h"
#include "GeraNESApp/SingleThreadEmulationHost.h"
#include "GeraNESNetplay/GeraNESInputFrameAdapter.h"
#include "GeraNESNetplay/GeraNESNetplayAdapters.h"
#include "GeraNESNetplay/GeraNESNetplayConsole.h"
#include "GeraNESNetplay/GeraNESNetplayAssignmentHelpers.h"
#include "GeraNESNetplay/GeraNESNetplayMenuHelpers.h"
#include "GeraNESNetplay/GeraNESNetplayRuntimeDriver.h"
#include "ConsoleNetplay/NetplayAutoTune.h"
#include "ConsoleNetplay/NetplayCoordinator.h"

class NetplayTest
{
public:
    struct Options
    {
        std::string romPath;
        std::string reportPath;
        uint32_t frames = 600;
        uint32_t inputDelayFrames = 10;
        uint32_t snapshotWindowFrames = 600;
        uint32_t crcIntervalFrames = 10;
        uint32_t port = 27888;
        uint32_t startupTimeoutSteps = 10000;
        uint32_t frameStepLimit = 200000;
        uint32_t wallClockTimeoutSeconds = 60;
        uint32_t settleStepLimit = 2048;
        uint32_t preSessionWarmupFrames = 0;
        uint32_t gameplayReceiveDelayMs = 0;
        uint32_t hostLoopDtMs = 16;
        uint32_t clientLoopDtMs = 16;
        uint32_t hostStepStride = 1;
        uint32_t clientStepStride = 1;
        uint32_t forceDesyncFrame = 0;
        uint32_t desyncAddress = 0x0000;
        uint32_t desyncValueXor = 0x01;
        uint32_t hostInputSeed = 0x13572468u;
        uint32_t clientInputSeed = 0x24681357u;
        uint32_t networkPumpStride = 1;
        uint32_t reconnectAfterFrames = 0;
        uint32_t assignmentSwapAfterFrames = 0;
        uint32_t forceManualResyncFrame = 0;
        uint32_t forceHostResetFrame = 0;
        uint32_t dropClientIncomingResyncChunkMessages = 0;
        uint32_t dropClientIncomingResyncCompleteMessages = 0;
        uint32_t reconnectReservationSecondsForTests = 0;
        uint32_t hostSaveStateFrame = 0;
        uint32_t hostDisconnectFrame = 0;
        uint32_t clientRuntimePauseAfterFrames = 0;
        uint32_t clientRuntimePauseDurationFrames = 0;
        uint32_t webObserverVisibilitySuspendAfterFrames = 0;
        uint32_t webObserverVisibilitySuspendDurationFrames = 0;
        bool spamHostInputDuringResync = false;
        bool spamClientInputDuringResync = false;
        bool reconnectDuringResync = false;
        bool expectReconnectReservationExpiry = false;
        bool robust = false;
        bool appFlow = false;
        bool runtimeFlow = false;
        bool singleThreadRuntimeFlow = false;
        bool captureHostTrace = false;
        bool autoSettingsProbe = false;
        bool baselineLockstep = false;
        bool hostAssignedBeforeJoinOnly = false;
        bool hostAssignedAfterJoinOnly = false;
        bool assignLateJoinClientAfterJoin = false;
        bool assignLateJoinClientToMultitapAfterJoin = false;
        bool hostMultitapAssignedBeforeJoinOnly = false;
        bool clientAssignedOnly = false;
        bool clientAssignedPort1Only = false;
        bool assignmentPatternCheck = false;
        bool hostControllerAndZapperObserverScenario = false;
        bool requireHostManualLoadDuringResync = false;
        ConsoleNetplay::NetTransportBackend transportBackend = ConsoleNetplay::NetTransportBackend::ENet;
        ConsoleNetplay::NetTransportOptions transportOptions = {};
        std::vector<uint32_t> hostManualLoadStateFrames;
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
        ConsoleNetplay::NetplayCoordinator coordinator;
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
        ConsoleNetplay::NetplayCoordinator coordinator;
        ConsoleNetplay::ConfirmedInputBufferDriver driver;
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
                emuRef.setInputTimelineEpoch(coordinator.session().roomState().timelineEpoch);
                driver.consumePendingFrames(
                    emuRef.frameCount(),
                    emuRef.frameCount() + driver.prebufferFrames(),
                    [&emuRef](const ConsoleNetplay::NetplayCoordinator::ConfirmedFrameInputs& confirmed) {
                        InputFrame inputFrame = GeraNESNetplay::toGeraNESInputFrame(confirmed.netplayFrame);
                        inputFrame.timelineEpoch = emuRef.inputTimelineEpoch();
                        (void)emuRef.queueInputFrame(inputFrame);
                    }
                );
            });
        }
    };

    template<typename HostT>
    struct RuntimePeerStateT
    {
        std::string name;
        bool host = false;
        HostT emu{DummyAudioOutput::instance()};
        ConsoleNetplay::NetplayAppRuntime runtime;
        IEmulationHost::InputState latestInputState = {};
        DeterministicInputGenerator generator;
        uint32_t maxObservedInputBufferSize = 0;
        uint32_t maxObservedFutureBufferedFrames = 0;

        explicit RuntimePeerStateT(const std::string& peerName, bool isHost, uint32_t seed)
            : name(peerName)
            , host(isHost)
            , generator(seed)
        {
            emu.setAllowPresenterTimeoutAdvance(false);
            GeraNESNetplay::attachRuntimeWakeToHost(runtime, emu);
            GeraNESNetplay::installProcessGlobalFrontendNetplayLogCallbackOnce();
            emu.setPreAdvanceHook([this](GeraNESEmu& innerEmu) {
                const auto& cfg = AppSettings::instance().data.netplay;
                const ConsoleNetplay::RuntimeExecutionSettings runtimeSettings =
                    GeraNESNetplay::buildGeraNESRuntimeExecutionSettings(
                        emu,
                        cfg.autoGameplayTuning,
                        cfg.showNetplayDebugLog,
                        cfg.gameplayReceiveDelayMs,
                        cfg.inputDelayFrames
                    );
                (void)GeraNESNetplay::executeRuntimeFrame(
                    runtime,
                    emu,
                    innerEmu,
                    latestInputState,
                    runtimeSettings
                );
            });
        }

        void updateLatestInputState(const IEmulationHost::InputState& inputState)
        {
            latestInputState = inputState;
        }
    };

    using RuntimePeerState = RuntimePeerStateT<EmulationHost>;
    using SingleThreadRuntimePeerState = RuntimePeerStateT<SingleThreadEmulationHost>;

    template<typename HostT>
    static void configureHostTraceSinkIfSupported(HostT& emu, std::function<void(const std::string&)> sink)
    {
        if constexpr(requires { emu.setDebugTraceSink(std::move(sink)); }) {
            emu.setDebugTraceSink(std::move(sink));
        } else {
            (void)emu;
            (void)sink;
        }
    }

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

    static Buttons playbackButtonsForFrame(const Options& options,
                                           const DeterministicInputGenerator& generator,
                                           bool hostSide,
                                           uint32_t frameIndex,
                                           uint32_t fps)
    {
        (void)options;
        (void)hostSide;
        return generator.buttonsForFrame(frameIndex, fps);
    }

    static Buttons assignmentPatternButtons(bool hostSide, bool swapped, uint32_t frameIndex)
    {
        Buttons buttons{};
        if(frameIndex < 2u) {
            return buttons;
        }

        (void)swapped;
        const bool even = (frameIndex % 2u) == 0u;
        if(hostSide) {
            buttons.start = true;
            buttons.right = even;
        } else {
            buttons.a = true;
            buttons.up = even;
        }
        return buttons;
    }

    static std::optional<ConsoleNetplay::RomValidationData> captureRomValidation(GeraNESEmu& emu)
    {
        if(!emu.valid()) return std::nullopt;

        Cartridge& cart = emu.getConsole().cartridge();
        ConsoleNetplay::RomValidationData validation;
        validation.romCrc32 = cart.romFile().fileCrc32();
        validation.mapperId = static_cast<uint16_t>(std::max(0, cart.mapperId()));
        validation.subMapperId = static_cast<uint16_t>(std::max(0, cart.subMapperId()));
        validation.prgRomSize = static_cast<uint32_t>(std::max(0, cart.prgSize()));
        validation.chrRomSize = static_cast<uint32_t>(std::max(0, cart.chrSize()));
        validation.chrRamSize = static_cast<uint32_t>(std::max(0, cart.chrRamSize()));
        validation.fileSize = static_cast<uint32_t>(cart.romFile().size());
        validation.contentHash = cart.romFile().contentHash32();
        return validation;
    }

    static std::optional<ConsoleNetplay::RomValidationData> captureRomValidation(EmulationHost& emu)
    {
        if(!emu.valid()) return std::nullopt;
        return emu.withExclusiveAccess([&](auto& innerEmu) {
            return captureRomValidation(innerEmu);
        });
    }

    static std::optional<ConsoleNetplay::PlayerSlot> localAssignedSlot(const ConsoleNetplay::NetplayCoordinator& coordinator)
    {
        for(const auto& participant : coordinator.session().roomState().participants) {
            if(participant.id == coordinator.localParticipantId() &&
               participant.controllerAssignment != ConsoleNetplay::kObserverPlayerSlot) {
                return participant.controllerAssignment;
            }
        }
        return std::nullopt;
    }

    static std::optional<ConsoleNetplay::PlayerSlot> localAssignedSlot(const ConsoleNetplay::RoomState& room,
                                                                ConsoleNetplay::ParticipantId localParticipantId)
    {
        for(const auto& participant : room.participants) {
            if(participant.id == localParticipantId &&
               participant.controllerAssignment != ConsoleNetplay::kObserverPlayerSlot) {
                return participant.controllerAssignment;
            }
        }
        return std::nullopt;
    }

    static uint32_t confirmedThroughFrame(const PeerState& peer)
    {
        return peer.coordinator.latestPublishedConfirmedFrame();
    }

    static InputFrame buildEmuInputFrame(PeerState& peer, uint32_t frame)
    {
        const auto* confirmed = peer.coordinator.findConfirmedFrame(frame);
        if(confirmed == nullptr) {
            return peer.emu.createInputFrame(frame);
        }
        InputFrame inputFrame = GeraNESNetplay::toGeraNESInputFrame(confirmed->netplayFrame);
        inputFrame.frame = frame;
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

    static bool verifyEmulatorVersionJoinRejection(const Options& options,
                                                   nlohmann::json& report,
                                                   std::string& failureReason)
    {
        PeerState hostPeer("Host", true, options.hostInputSeed);
        PeerState clientPeer("Client", false, options.clientInputSeed);
        const auto cleanup = [&]() {
            clientPeer.coordinator.disconnect();
            hostPeer.coordinator.disconnect();
        };

        bool hostStarted = false;
        bool joinStarted = false;
        uint16_t selectedPort = 0;
        for(uint32_t portOffset = 0; portOffset < 8u; ++portOffset) {
            const uint16_t port = static_cast<uint16_t>(options.port + 100u + portOffset);
            hostPeer.coordinator.disconnect();
            clientPeer.coordinator.disconnect();
            clientPeer.coordinator.setLocalEmulatorVersionForTests("mismatch-test-version");
            if(!hostPeer.coordinator.host(port, 1, hostPeer.name)) {
                continue;
            }
            hostStarted = true;
            selectedPort = port;
            if(!clientPeer.coordinator.join("127.0.0.1", port, clientPeer.name)) {
                continue;
            }
            joinStarted = true;
            break;
        }

        report["hostStarted"] = hostStarted;
        report["joinStarted"] = joinStarted;
        report["port"] = selectedPort;

        if(!hostStarted) {
            failureReason = "Failed to start temporary host for emulator version mismatch validation.";
            cleanup();
            return false;
        }
        if(!joinStarted) {
            failureReason = "Failed to start temporary client join for emulator version mismatch validation.";
            cleanup();
            return false;
        }

        bool rejected = false;
        for(uint32_t step = 0; step < options.startupTimeoutSteps; ++step) {
            pumpNetwork(hostPeer, clientPeer, 1);
            if(clientPeer.coordinator.lastError().find("Emulator version mismatch") != std::string::npos) {
                rejected = true;
                break;
            }
        }

        report["clientError"] = clientPeer.coordinator.lastError();
        report["hostLog"] = hostPeer.coordinator.eventLog();

        if(!rejected) {
            failureReason = "Client with mismatched emulator version was not rejected with the expected message.";
            cleanup();
            return false;
        }

        cleanup();
        return true;
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
                hostPeer.coordinator.localParticipantId() != ConsoleNetplay::kInvalidParticipantId &&
                clientPeer.coordinator.localParticipantId() != ConsoleNetplay::kInvalidParticipantId &&
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

        const ConsoleNetplay::ParticipantId hostId = hostPeer.coordinator.localParticipantId();
        const ConsoleNetplay::ParticipantId clientId = clientPeer.coordinator.localParticipantId();
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
            if(hostSlot == std::optional<ConsoleNetplay::PlayerSlot>(0) &&
               clientSlot == std::optional<ConsoleNetplay::PlayerSlot>(1)) {
                hostPeer.coordinator.setLocalSimulationFrame(hostPeer.emu.frameCount());
                clientPeer.coordinator.setLocalSimulationFrame(clientPeer.emu.frameCount());
                if(!hostPeer.coordinator.startSession()) {
                    failureReason = "Host failed to start session.";
                    return false;
                }

                for(uint32_t startStep = 0; startStep < options.startupTimeoutSteps; ++startStep) {
                    pumpNetwork(hostPeer, clientPeer, 1);
                    if(hostPeer.coordinator.session().roomState().state == ConsoleNetplay::SessionState::Running &&
                       clientPeer.coordinator.session().roomState().state == ConsoleNetplay::SessionState::Running) {
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
        const std::optional<ConsoleNetplay::PlayerSlot> slot = localAssignedSlot(peer.coordinator);
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
                const Buttons buttons = playbackButtonsForFrame(options, peer.generator, peer.host, sourceFrame, fps);
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
                {"romCompatible", participant.romCompatible}
            });
        }

        nlohmann::json eventTail = nlohmann::json::array();
        const auto& log = peer.coordinator.eventLog();
        const size_t start = log.size() > 50 ? (log.size() - 50) : 0;
        for(size_t i = start; i < log.size(); ++i) {
            eventTail.push_back(log[i]);
        }

        const std::optional<ConsoleNetplay::PlayerSlot> localSlot = localAssignedSlot(peer.coordinator);
        const ConsoleNetplay::TimelineInputEntry* latestLocal = nullptr;
        const ConsoleNetplay::TimelineInputEntry* latestLocalConfirmed = nullptr;
        if(localSlot.has_value()) {
            latestLocal = peer.coordinator.localInputs().latestFor(peer.coordinator.localParticipantId(), *localSlot);
            latestLocalConfirmed = peer.coordinator.localInputs().latestConfirmedFor(peer.coordinator.localParticipantId(), *localSlot);
        }
        const auto* latestConfirmedFrame = peer.coordinator.findConfirmedFrame(peer.coordinator.latestConfirmedFrame());

        const InputFrame* frame0 = peer.emu.inputBuffer().findByFrame(0, peer.emu.inputTimelineEpoch());
        const InputFrame* frame1 = peer.emu.inputBuffer().findByFrame(1, peer.emu.inputTimelineEpoch());
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
            {"remoteTimelineSize", peer.coordinator.remoteInputs().size()},            {"latestLocalFrame", latestLocal != nullptr ? nlohmann::json{
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
            {"wallClockTimeoutSeconds", options.wallClockTimeoutSeconds},
            {"inputDelayFrames", options.inputDelayFrames},
            {"networkPumpStride", options.networkPumpStride},
            {"hostLoopDtMs", options.hostLoopDtMs},
            {"clientLoopDtMs", options.clientLoopDtMs},
            {"hostStepStride", options.hostStepStride},
            {"clientStepStride", options.clientStepStride},
            {"reconnectDuringResync", options.reconnectDuringResync},
            {"dropClientIncomingResyncChunkMessages", options.dropClientIncomingResyncChunkMessages},
            {"dropClientIncomingResyncCompleteMessages", options.dropClientIncomingResyncCompleteMessages},
            {"reconnectReservationSecondsForTests", options.reconnectReservationSecondsForTests},
            {"expectReconnectReservationExpiry", options.expectReconnectReservationExpiry},
            {"assignLateJoinClientAfterJoin", options.assignLateJoinClientAfterJoin},
            {"assignLateJoinClientToMultitapAfterJoin", options.assignLateJoinClientToMultitapAfterJoin},
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
        const uint32_t exactFrame = peer.emu.exactEmulationFrame();

        nlohmann::json participants = nlohmann::json::array();
        for(const auto& participant : peer.coordinator.session().roomState().participants) {
            participants.push_back({
                {"id", participant.id},
                {"name", participant.displayName},
                {"controllerAssignment", participant.controllerAssignment},
                {"lastReceivedInputFrame", participant.lastReceivedInputFrame},
                {"lastContiguousInputFrame", participant.lastContiguousInputFrame},
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
        uint32_t expectedPlaybackFrame = 0;
        bool hasExpectedFrameInput = false;
        bool hasNextFrameInput = false;
        nlohmann::json nextFrameJson;
        nlohmann::json currentFrameJson;
        nlohmann::json previousFrameJson;
        nlohmann::json inputWindow = nlohmann::json::array();
        peer.emu.withExclusiveAccess([&](auto& innerEmu) {
            inputBufferSize = static_cast<uint32_t>(innerEmu.inputBuffer().size());
            const uint32_t frame = exactFrame;
            const uint32_t timelineEpoch = innerEmu.inputTimelineEpoch();
            expectedPlaybackFrame = frame;
            hasExpectedFrameInput = innerEmu.inputBuffer().findByFrame(frame, timelineEpoch) != nullptr;
            hasNextFrameInput = innerEmu.inputBuffer().findByFrame(frame + 1u, timelineEpoch) != nullptr;
            for(uint32_t probe = frame + 1u; probe < frame + 256u; ++probe) {
                if(innerEmu.inputBuffer().findByFrame(probe, timelineEpoch) == nullptr) {
                    break;
                }
                ++futureBufferedFrames;
            }
            if(const InputFrame* previousFrame = innerEmu.inputBuffer().findByFrame(frame, timelineEpoch); previousFrame != nullptr) {
                previousFrameJson = previousFrame->toJson();
            }
            if(const InputFrame* nextFrame = innerEmu.inputBuffer().findByFrame(frame + 1u, timelineEpoch); nextFrame != nullptr) {
                nextFrameJson = nextFrame->toJson();
            }
            if(const InputFrame* currentFrame = innerEmu.inputBuffer().findByFrame(frame + 2u, timelineEpoch); currentFrame != nullptr) {
                currentFrameJson = currentFrame->toJson();
            }
            const uint32_t windowStart = frame > 2u ? frame - 2u : 0u;
            for(uint32_t probe = windowStart; probe <= frame + 4u; ++probe) {
                if(const InputFrame* entry = innerEmu.inputBuffer().findByFrame(probe, timelineEpoch); entry != nullptr) {
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
            {"frame", exactFrame},
            {"localSimulationFrame", peer.coordinator.localSimulationFrame()},
            {"roomCurrentFrame", peer.coordinator.session().roomState().currentFrame},
            {"lastFrameReadyFrame", peer.emu.lastFrameReadyFrame()},
            {"lastFrameReadyNetplayCrc32", peer.emu.lastFrameReadyNetplayCrc32()},
            {"sessionState", static_cast<int>(peer.coordinator.session().roomState().state)},
            {"timelineEpoch", peer.coordinator.session().roomState().timelineEpoch},
            {"currentFrame", peer.coordinator.localSimulationFrame()},
            {"publishedConfirmedFrame", peer.coordinator.latestPublishedConfirmedFrame()},
            {"roomLastConfirmedFrame", peer.coordinator.session().roomState().lastConfirmedFrame},
            {"confirmedThroughFrame", peer.coordinator.latestPublishedConfirmedFrame()},
            {"resyncTargetFrame", peer.coordinator.authoritativeResyncTargetFrame()},
            {"lastRemoteCrcFrame", peer.coordinator.session().roomState().lastRemoteCrcFrame},
            {"lastRemoteCrc32", peer.coordinator.session().roomState().lastRemoteCrc32},
            {"localConfirmedCrcType", "canonical_netplay_state_crc32"},
            {"lastRemoteCrcType", "canonical_netplay_state_crc32"},
            {"frameReadyCrcType", "frame_ready_canonical_crc32"},
            {"resyncPayloadCrcType", "payload_crc32"},
            {"resyncStateCrcType", "canonical_netplay_state_crc32"},
            {"crc32", peer.emu.valid() ? peer.emu.canonicalStateCrc32() : 0u},
            {"netplayCrc32", peer.emu.valid() ? peer.emu.canonicalNetplayStateCrc32() : 0u},
            {"inputBufferSize", inputBufferSize},
            {"expectedPlaybackFrame", expectedPlaybackFrame},
            {"hasExpectedFrameInput", hasExpectedFrameInput},
            {"hasNextFrameInput", hasNextFrameInput},
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
                                         uint32_t maxStallSteps,
                                         bool assignmentSwapTriggered = false,
                                         bool assignmentSwapVerified = false,
                                         bool assignmentPatternVerified = false)
    {
        nlohmann::json hostReport = buildAppPeerReport(hostPeer);
        nlohmann::json clientReport = buildAppPeerReport(clientPeer);
        const uint32_t hostReadyFrame = hostReport.at("lastFrameReadyFrame");
        const uint32_t clientReadyFrame = clientReport.at("lastFrameReadyFrame");
        const uint32_t commonReadyFrame = std::min(hostReadyFrame, clientReadyFrame);
        const auto resolveFrameReadyCrc = [](IEmulationHost& emu,
                                             uint32_t readyFrame,
                                             uint32_t readyCrc,
                                             uint32_t probeFrame) -> std::optional<uint32_t> {
            if(probeFrame == 0u) {
                return std::nullopt;
            }
            if(probeFrame == readyFrame) {
                return readyCrc;
            }
            return emu.netplaySnapshotCrc32ForFrame(probeFrame);
        };
        const std::optional<uint32_t> hostCommonReadyCrc = resolveFrameReadyCrc(
            hostPeer.emu,
            hostReadyFrame,
            hostReport.at("lastFrameReadyNetplayCrc32"),
            commonReadyFrame
        );
        const std::optional<uint32_t> clientCommonReadyCrc = resolveFrameReadyCrc(
            clientPeer.emu,
            clientReadyFrame,
            clientReport.at("lastFrameReadyNetplayCrc32"),
            commonReadyFrame
        );
        const bool finalFrameReadyCrcMatch =
            commonReadyFrame > 0u &&
            hostCommonReadyCrc.has_value() &&
            clientCommonReadyCrc.has_value() &&
            *hostCommonReadyCrc == *clientCommonReadyCrc;
        return {
            {"status", status},
            {"failureReason", failureReason},
            {"romPath", options.romPath},
            {"frames", options.frames},
            {"inputDelayFrames", options.inputDelayFrames},
            {"assignmentSwapAfterFrames", options.assignmentSwapAfterFrames},
            {"assignmentSwapTriggered", assignmentSwapTriggered},
            {"assignmentSwapVerified", assignmentSwapVerified},
            {"assignmentPatternVerified", assignmentPatternVerified},
            {"networkPumpStride", options.networkPumpStride},
            {"hostLoopDtMs", options.hostLoopDtMs},
            {"clientLoopDtMs", options.clientLoopDtMs},
            {"hostStepStride", options.hostStepStride},
            {"clientStepStride", options.clientStepStride},
            {"reconnectDuringResync", options.reconnectDuringResync},
            {"dropClientIncomingResyncChunkMessages", options.dropClientIncomingResyncChunkMessages},
            {"dropClientIncomingResyncCompleteMessages", options.dropClientIncomingResyncCompleteMessages},
            {"reconnectReservationSecondsForTests", options.reconnectReservationSecondsForTests},
            {"expectReconnectReservationExpiry", options.expectReconnectReservationExpiry},
            {"lastCheckedFrame", lastCheckedFrame},
            {"maxStallSteps", maxStallSteps},
            {"finalCrcMatch", hostPeer.emu.valid() && clientPeer.emu.valid()
                ? (hostPeer.emu.canonicalStateCrc32() == clientPeer.emu.canonicalStateCrc32())
                : false},
            {"finalNetplayCrcMatch", hostPeer.emu.valid() && clientPeer.emu.valid()
                ? (hostPeer.emu.canonicalNetplayStateCrc32() == clientPeer.emu.canonicalNetplayStateCrc32())
                : false},
            {"finalFrameReadyFrameAligned", hostReadyFrame == clientReadyFrame},
            {"finalCommonFrameReadyFrame", commonReadyFrame},
            {"finalFrameReadyCrcMatch", finalFrameReadyCrcMatch},
            {"host", std::move(hostReport)},
            {"client", std::move(clientReport)}
        };
    }

    static std::optional<ConsoleNetplay::ParticipantId> findParticipantIdByName(const ConsoleNetplay::RoomState& room,
                                                                         const std::string& name)
    {
        for(const auto& participant : room.participants) {
            if(participant.displayName == name) {
                return participant.id;
            }
        }
        return std::nullopt;
    }

    static const ConsoleNetplay::ParticipantInfo* findParticipantInRoom(const ConsoleNetplay::RoomState& room,
                                                                 ConsoleNetplay::ParticipantId id)
    {
        for(const auto& participant : room.participants) {
            if(participant.id == id) {
                return &participant;
            }
        }
        return nullptr;
    }

    static bool allAssignedParticipantsReadyAndCompatible(const ConsoleNetplay::RoomState& room)
    {
        for(const auto& participant : room.participants) {
            if(participant.controllerAssignment == ConsoleNetplay::kObserverPlayerSlot) continue;
            if(!participant.connected || !participant.romLoaded || !participant.romCompatible) {
                return false;
            }
        }
        return true;
    }

    static IEmulationHost::InputState buildRuntimeInputStateForSlot(ConsoleNetplay::PlayerSlot slot, const Buttons& buttons)
    {
        IEmulationHost::InputState inputState{};
        GeraNESNetplay::applyPadMaskToInputState(inputState, slot, buildPadMask(buttons));
        return inputState;
    }

    static IEmulationHost::InputState buildDuckHuntLikeRuntimeInputState(uint32_t frameIndex, uint32_t fps)
    {
        IEmulationHost::InputState inputState{};

        const uint32_t startupQuietFrames = std::max<uint32_t>(fps / 2u, 20u);
        if(frameIndex < startupQuietFrames) {
            inputState.zapperX = 128;
            inputState.zapperY = 96;
            return inputState;
        }

        const uint32_t activeFrame = frameIndex - startupQuietFrames;

        // Nudge the title/menu flow with a brief Start pulse, then keep feeding
        // deterministic zapper motion/trigger data so any post-resync input
        // divergence shows up quickly in the emulated state.
        if(activeFrame >= 4u && activeFrame < 8u) {
            inputState.p1Start = true;
        }

        inputState.zapperX = static_cast<int>(96 + ((activeFrame * 17u) % 64u));
        inputState.zapperY = static_cast<int>(72 + ((activeFrame * 11u) % 80u));
        inputState.zapperP2Trigger =
            activeFrame >= 12u &&
            ((activeFrame % 19u) == 0u || (activeFrame % 19u) == 1u);
        return inputState;
    }

    static Buttons runtimeButtonsForPeer(const Options& options,
                                         const DeterministicInputGenerator& generator,
                                         bool hostSide,
                                         bool swapped,
                                         uint32_t frameIndex,
                                         uint32_t fps)
    {
        if(options.assignmentPatternCheck) {
            return assignmentPatternButtons(hostSide, swapped, frameIndex);
        }
        return playbackButtonsForFrame(options, generator, hostSide, frameIndex, fps);
    }

    static bool participantHasAssignments(const ConsoleNetplay::RoomState& room,
                                          ConsoleNetplay::ParticipantId participantId,
                                          std::initializer_list<ConsoleNetplay::PlayerSlot> expectedAssignments)
    {
        const auto* participant = findParticipantInRoom(room, participantId);
        if(participant == nullptr) return false;
        for(ConsoleNetplay::PlayerSlot slot : expectedAssignments) {
            if(!participant->hasControllerAssignment(slot)) {
                return false;
            }
        }
        return participant->controllerAssignments.size() == expectedAssignments.size();
    }

    static bool inputFrameMatchesButtonsForSlot(const InputFrame& inputFrame,
                                                ConsoleNetplay::PlayerSlot slot,
                                                const Buttons& buttons)
    {
        switch(slot) {
            case GeraNESNetplay::kPort1PlayerSlot:
            case GeraNESNetplay::kMultitapP1PlayerSlot:
                return inputFrame.p1A == buttons.a &&
                       inputFrame.p1B == buttons.b &&
                       inputFrame.p1Select == buttons.select &&
                       inputFrame.p1Start == buttons.start &&
                       inputFrame.p1Up == buttons.up &&
                       inputFrame.p1Down == buttons.down &&
                       inputFrame.p1Left == buttons.left &&
                       inputFrame.p1Right == buttons.right;
            case GeraNESNetplay::kPort2PlayerSlot:
            case GeraNESNetplay::kMultitapP2PlayerSlot:
                return inputFrame.p2A == buttons.a &&
                       inputFrame.p2B == buttons.b &&
                       inputFrame.p2Select == buttons.select &&
                       inputFrame.p2Start == buttons.start &&
                       inputFrame.p2Up == buttons.up &&
                       inputFrame.p2Down == buttons.down &&
                       inputFrame.p2Left == buttons.left &&
                       inputFrame.p2Right == buttons.right;
            case GeraNESNetplay::kExpansionPlayerSlot:
            case GeraNESNetplay::kMultitapP3PlayerSlot:
                return inputFrame.p3A == buttons.a &&
                       inputFrame.p3B == buttons.b &&
                       inputFrame.p3Select == buttons.select &&
                       inputFrame.p3Start == buttons.start &&
                       inputFrame.p3Up == buttons.up &&
                       inputFrame.p3Down == buttons.down &&
                       inputFrame.p3Left == buttons.left &&
                       inputFrame.p3Right == buttons.right;
            case GeraNESNetplay::kMultitapP4PlayerSlot:
                return inputFrame.p4A == buttons.a &&
                       inputFrame.p4B == buttons.b &&
                       inputFrame.p4Select == buttons.select &&
                       inputFrame.p4Start == buttons.start &&
                       inputFrame.p4Up == buttons.up &&
                       inputFrame.p4Down == buttons.down &&
                       inputFrame.p4Left == buttons.left &&
                       inputFrame.p4Right == buttons.right;
            default:
                return false;
        }
    }

    template<typename InspectFn>
    static bool emuHasBufferedPattern(InspectFn&& inspect,
                                      const DeterministicInputGenerator& hostGenerator,
                                      const DeterministicInputGenerator& clientGenerator,
                                      uint32_t windowStartFrame,
                                      uint32_t windowEndFrame,
                                      ConsoleNetplay::PlayerSlot hostSlot,
                                      ConsoleNetplay::PlayerSlot clientSlot,
                                      const Options& options,
                                      bool swapped)
    {
        bool matched = false;
        inspect([&](auto& innerEmu) {
            const uint32_t fps = std::max<uint32_t>(1u, innerEmu.getRegionFPS());
            for(uint32_t probeFrame = windowStartFrame; probeFrame <= windowEndFrame; ++probeFrame) {
                const InputFrame* inputFrame = innerEmu.inputBuffer().findByFrame(probeFrame, innerEmu.inputTimelineEpoch());
                if(inputFrame == nullptr) continue;

                const Buttons hostButtons = runtimeButtonsForPeer(options, hostGenerator, true, swapped, probeFrame, fps);
                const Buttons clientButtons = runtimeButtonsForPeer(options, clientGenerator, false, swapped, probeFrame, fps);
                if(inputFrameMatchesButtonsForSlot(*inputFrame, hostSlot, hostButtons) &&
                   inputFrameMatchesButtonsForSlot(*inputFrame, clientSlot, clientButtons)) {
                    matched = true;
                    return;
                }
            }
        });
        return matched;
    }

    template<typename PeerT>
    static bool peerHasBufferedPattern(PeerT& peer,
                                       const DeterministicInputGenerator& hostGenerator,
                                       const DeterministicInputGenerator& clientGenerator,
                                       uint32_t windowStartFrame,
                                       uint32_t windowEndFrame,
                                       ConsoleNetplay::PlayerSlot hostSlot,
                                       ConsoleNetplay::PlayerSlot clientSlot,
                                       const Options& options,
                                       bool swapped)
    {
        return emuHasBufferedPattern(
            [&](auto&& fn) {
                peer.emu.withExclusiveAccess(fn);
            },
            hostGenerator,
            clientGenerator,
            windowStartFrame,
            windowEndFrame,
            hostSlot,
            clientSlot,
            options,
            swapped
        );
    }

    template<typename PeerT>
    static bool peerHasBufferedLocalPattern(PeerT& peer,
                                            const DeterministicInputGenerator& generator,
                                            uint32_t windowStartFrame,
                                            uint32_t windowEndFrame,
                                            ConsoleNetplay::PlayerSlot slot,
                                            const Options& options)
    {
        bool matched = false;
        peer.emu.withExclusiveAccess([&](auto& innerEmu) {
            const uint32_t fps = std::max<uint32_t>(1u, innerEmu.getRegionFPS());
            for(uint32_t probeFrame = windowStartFrame; probeFrame <= windowEndFrame; ++probeFrame) {
                const InputFrame* inputFrame = innerEmu.inputBuffer().findByFrame(probeFrame, innerEmu.inputTimelineEpoch());
                if(inputFrame == nullptr) continue;

                const Buttons buttons = runtimeButtonsForPeer(options, generator, true, false, probeFrame, fps);
                if(inputFrameMatchesButtonsForSlot(*inputFrame, slot, buttons)) {
                    matched = true;
                    return;
                }
            }
        });
        return matched;
    }

    static bool peerHasBufferedPattern(AppPeerState& peer,
                                       const DeterministicInputGenerator& hostGenerator,
                                       const DeterministicInputGenerator& clientGenerator,
                                       uint32_t windowStartFrame,
                                       uint32_t windowEndFrame,
                                       ConsoleNetplay::PlayerSlot hostSlot,
                                       ConsoleNetplay::PlayerSlot clientSlot,
                                       const Options& options,
                                       bool swapped)
    {
        return emuHasBufferedPattern(
            [&](auto&& fn) {
                peer.emu.withExclusiveAccess(fn);
            },
            hostGenerator,
            clientGenerator,
            windowStartFrame,
            windowEndFrame,
            hostSlot,
            clientSlot,
            options,
            swapped
        );
    }

    template<typename PeerT>
    static nlohmann::json buildRuntimePeerReport(PeerT& peer)
    {
        const auto snapshot = peer.runtime.uiSnapshot();
        const auto activeResyncReasonLabel = [&]() -> const char* {
            switch(snapshot.room.activeResyncReason) {
                case ConsoleNetplay::ResyncReason::Unspecified: return "Unspecified";
                case ConsoleNetplay::ResyncReason::InitialSessionSync: return "InitialSessionSync";
                case ConsoleNetplay::ResyncReason::ConfirmedDesync: return "ConfirmedDesync";
                case ConsoleNetplay::ResyncReason::AssignmentChanged: return "AssignmentChanged";
                case ConsoleNetplay::ResyncReason::ManualForce: return "ManualForce";
                case ConsoleNetplay::ResyncReason::HostReset: return "HostReset";
                case ConsoleNetplay::ResyncReason::HostLoadedState: return "HostLoadedState";
                case ConsoleNetplay::ResyncReason::ObserverVisibilityRestore: return "ObserverVisibilityRestore";
                default: return "Unknown";
            }
        };
        const auto recoveryInputModeLabel = [&]() -> const char* {
            switch(snapshot.room.recoveryInputMode) {
                case ConsoleNetplay::RecoveryInputMode::Normal: return "Normal";
                case ConsoleNetplay::RecoveryInputMode::ResyncLocked: return "ResyncLocked";
                case ConsoleNetplay::RecoveryInputMode::PostResyncStabilizing: return "PostResyncStabilizing";
                default: return "Unknown";
            }
        };

        uint32_t inputBufferSize = 0;
        uint32_t futureBufferedFrames = 0;
        uint32_t expectedPlaybackFrame = 0;
        bool hasExpectedFrameInput = false;
        bool hasNextFrameInput = false;
        InputBuffer::EnqueueCounters enqueueCounters{};
        nlohmann::json currentFrameJson;
        nlohmann::json nextFrameJson;
        nlohmann::json inputWindow = nlohmann::json::array();
        peer.emu.withExclusiveAccess([&](auto& innerEmu) {
            inputBufferSize = static_cast<uint32_t>(innerEmu.inputBuffer().size());
            expectedPlaybackFrame = innerEmu.frameCount();
            const uint32_t timelineEpoch = innerEmu.inputTimelineEpoch();
            hasExpectedFrameInput = innerEmu.inputBuffer().findByFrame(expectedPlaybackFrame, timelineEpoch) != nullptr;
            hasNextFrameInput = innerEmu.inputBuffer().findByFrame(expectedPlaybackFrame + 1u, timelineEpoch) != nullptr;
            if(const InputFrame* current = innerEmu.inputBuffer().findByFrame(expectedPlaybackFrame, timelineEpoch); current != nullptr) {
                currentFrameJson = current->toJson();
            }
            if(const InputFrame* next = innerEmu.inputBuffer().findByFrame(expectedPlaybackFrame + 1u, timelineEpoch); next != nullptr) {
                nextFrameJson = next->toJson();
            }
            for(uint32_t probe = expectedPlaybackFrame + 1u; probe < expectedPlaybackFrame + 256u; ++probe) {
                if(innerEmu.inputBuffer().findByFrame(probe, timelineEpoch) == nullptr) {
                    break;
                }
                ++futureBufferedFrames;
            }
            const uint32_t windowStart = expectedPlaybackFrame > 2u ? expectedPlaybackFrame - 2u : 0u;
            for(uint32_t probe = windowStart; probe <= expectedPlaybackFrame + 4u; ++probe) {
                if(const InputFrame* entry = innerEmu.inputBuffer().findByFrame(probe, timelineEpoch); entry != nullptr) {
                    inputWindow.push_back(entry->toJson());
                }
            }
            enqueueCounters = innerEmu.inputEnqueueCounters();
        });

        peer.maxObservedInputBufferSize = std::max(peer.maxObservedInputBufferSize, inputBufferSize);
        peer.maxObservedFutureBufferedFrames = std::max(peer.maxObservedFutureBufferedFrames, futureBufferedFrames);

        nlohmann::json participants = nlohmann::json::array();
        for(const auto& participant : snapshot.room.participants) {
            participants.push_back({
                {"id", participant.id},
                {"name", participant.displayName},
                {"controllerAssignment", participant.controllerAssignment},
                {"lastReceivedInputFrame", participant.lastReceivedInputFrame},
                {"lastContiguousInputFrame", participant.lastContiguousInputFrame},
                {"romLoaded", participant.romLoaded},
                {"romCompatible", participant.romCompatible}
            });
        }

        return {
            {"name", peer.name},
            {"host", peer.host},
            {"frame", peer.emu.exactEmulationFrame()},
            {"lastFrameReadyFrame", peer.emu.lastFrameReadyFrame()},
            {"lastFrameReadyNetplayCrc32", peer.emu.lastFrameReadyNetplayCrc32()},
            {"runtimeActive", snapshot.active},
            {"runtimeRunning", snapshot.room.state == ConsoleNetplay::SessionState::Running},
            {"connected", snapshot.connected},
            {"reconnecting", snapshot.reconnecting},
            {"lastError", snapshot.lastError},
            {"localParticipantId", snapshot.localParticipantId},
            {"sessionState", static_cast<int>(snapshot.room.state)},
            {"timelineEpoch", snapshot.room.timelineEpoch},
            {"localSimulationFrame", snapshot.localSimulationFrame},
            {"roomCurrentFrame", snapshot.room.currentFrame},
            {"currentFrame", snapshot.localSimulationFrame},
            {"publishedConfirmedFrame", snapshot.publishedConfirmedFrame},
            {"roomLastConfirmedFrame", snapshot.room.lastConfirmedFrame},
            {"confirmedThroughFrame", snapshot.room.lastConfirmedFrame},
            {"lastRemoteCrcFrame", snapshot.room.lastRemoteCrcFrame},
            {"lastRemoteCrc32", snapshot.room.lastRemoteCrc32},
            {"localConfirmedCrcType", "canonical_netplay_state_crc32"},
            {"lastRemoteCrcType", "canonical_netplay_state_crc32"},
            {"frameReadyCrcType", "frame_ready_canonical_crc32"},
            {"resyncPayloadCrcType", "payload_crc32"},
            {"resyncStateCrcType", "canonical_netplay_state_crc32"},
            {"lastSubmittedLocalCrcFrame", snapshot.lastSubmittedLocalCrcFrame},
                        {"lastLoadedAuthoritativeFrame", snapshot.lastLoadedAuthoritativeFrame},
            {"lastRecoveryReanchorFrame", snapshot.lastRecoveryReanchorFrame},
            {"lastAcceptedRemoteEpoch", snapshot.room.lastAcceptedRemoteEpoch},
            {"lastIgnoredStaleInputEpoch", snapshot.room.lastIgnoredStaleInputEpoch},
            {"lastIgnoredStaleFrameStatusEpoch", snapshot.room.lastIgnoredStaleFrameStatusEpoch},
            {"lastIgnoredStaleCrcEpoch", snapshot.room.lastIgnoredStaleCrcEpoch},
            {"staleInputPacketCount", snapshot.room.staleInputPacketCount},
            {"staleFrameStatusPacketCount", snapshot.room.staleFrameStatusPacketCount},
            {"staleCrcPacketCount", snapshot.room.staleCrcPacketCount},
            {"activeResyncId", snapshot.room.activeResyncId},
            {"activeResyncReason", static_cast<int>(snapshot.room.activeResyncReason)},
            {"activeResyncReasonLabel", activeResyncReasonLabel()},
            {"recoveryInputMode", static_cast<int>(snapshot.room.recoveryInputMode)},
            {"recoveryInputModeLabel", recoveryInputModeLabel()},
            {"recoveryModeTransitionCount", snapshot.room.recoveryModeTransitionCount},
            {"inputsDroppedDuringRecovery", snapshot.room.inputsDroppedDuringRecovery},
            {"stabilizationFramesRemaining", snapshot.room.stabilizationFramesRemaining},
            {"stabilizationCrcPassCount", snapshot.room.stabilizationCrcPassCount},
            {"pendingResyncAckCount", snapshot.room.pendingResyncAckCount},
            {"resyncTargetFrame", snapshot.room.resyncTargetFrame},
            {"resyncConfirmedFrame", snapshot.room.resyncConfirmedFrame},
            {"resyncFrameReadyFrame", snapshot.room.resyncFrameReadyFrame},
            {"resyncPayloadCrc32", snapshot.room.resyncPayloadCrc32},
            {"resyncFrameReadyCrc32", snapshot.room.resyncFrameReadyCrc32},
            {"localInputCount", snapshot.localInputCount},
            {"remoteInputCount", snapshot.remoteInputCount},
            {"hardResyncCount", snapshot.recoveryStats.hardResyncCount},            {"playbackStopCount", snapshot.recoveryStats.playbackStopCount},            {"sessionBlockedReason", snapshot.sessionBlockedReason},
            {"crc32", peer.emu.valid() ? peer.emu.canonicalStateCrc32() : 0u},
            {"netplayCrc32", peer.emu.valid() ? peer.emu.canonicalNetplayStateCrc32() : 0u},
            {"inputBufferSize", inputBufferSize},
            {"maxObservedInputBufferSize", peer.maxObservedInputBufferSize},
            {"expectedPlaybackFrame", expectedPlaybackFrame},
            {"hasExpectedFrameInput", hasExpectedFrameInput},
            {"hasNextFrameInput", hasNextFrameInput},
            {"futureBufferedFrames", futureBufferedFrames},
            {"maxObservedFutureBufferedFrames", peer.maxObservedFutureBufferedFrames},
            {"currentFrameInput", currentFrameJson},
            {"nextFrameInput", nextFrameJson},
            {"inputWindow", inputWindow},
            {"inputEnqueueCounters", {
                {"inserted", enqueueCounters.inserted},
                {"updatedPending", enqueueCounters.updatedPending},
                {"rejectedConsumed", enqueueCounters.rejectedConsumed},
                {"rejectedEpoch", enqueueCounters.rejectedEpoch},
                {"rejectedOutOfSequence", enqueueCounters.rejectedOutOfSequence}
            }},
            {"participants", participants},
            {"eventLogTail", snapshot.eventLog}
        };
    }

    template<typename PeerT>
    static nlohmann::json buildRuntimeReport(const Options& options,
                                             PeerT& hostPeer,
                                             PeerT& clientPeer,
                                             const std::string& status,
                                             const std::string& failureReason,
                                             uint32_t lastCheckedFrame,
                                             uint32_t maxStallSteps,
                                             uint32_t maxClientAheadOfHostFrames,
                                             uint32_t maxHostAheadOfClientFrames,
                                             bool assignmentSwapTriggered,
                                             bool assignmentSwapVerified,
                                             bool assignmentPatternVerified)
    {
        nlohmann::json hostReport = buildRuntimePeerReport(hostPeer);
        nlohmann::json clientReport = buildRuntimePeerReport(clientPeer);
        const uint32_t hostReadyFrame = hostReport.at("lastFrameReadyFrame");
        const uint32_t clientReadyFrame = clientReport.at("lastFrameReadyFrame");
        const uint32_t commonReadyFrame = std::min(hostReadyFrame, clientReadyFrame);
        const auto resolveFrameReadyCrc = [](const auto& peer,
                                             uint32_t readyFrame,
                                             uint32_t readyCrc,
                                             uint32_t probeFrame) -> std::optional<uint32_t> {
            if(probeFrame == 0u) {
                return std::nullopt;
            }
            if(probeFrame == readyFrame) {
                return readyCrc;
            }
            return peer.emu.netplaySnapshotCrc32ForFrame(probeFrame);
        };
        const std::optional<uint32_t> hostCommonReadyCrc = resolveFrameReadyCrc(
            hostPeer,
            hostReadyFrame,
            hostReport.at("lastFrameReadyNetplayCrc32"),
            commonReadyFrame
        );
        const std::optional<uint32_t> clientCommonReadyCrc = resolveFrameReadyCrc(
            clientPeer,
            clientReadyFrame,
            clientReport.at("lastFrameReadyNetplayCrc32"),
            commonReadyFrame
        );
        const bool finalFrameReadyCrcMatch =
            commonReadyFrame > 0u &&
            hostCommonReadyCrc.has_value() &&
            clientCommonReadyCrc.has_value() &&
            *hostCommonReadyCrc == *clientCommonReadyCrc;
        return {
            {"status", status},
            {"failureReason", failureReason},
            {"romPath", options.romPath},
            {"frames", options.frames},
            {"inputDelayFrames", options.inputDelayFrames},
            {"singleThreadRuntimeFlow", options.singleThreadRuntimeFlow},
            {"gameplayReceiveDelayMs", options.gameplayReceiveDelayMs},
            {"preSessionWarmupFrames", options.preSessionWarmupFrames},
            {"reconnectAfterFrames", options.reconnectAfterFrames},
            {"reconnectDuringResync", options.reconnectDuringResync},
            {"dropClientIncomingResyncChunkMessages", options.dropClientIncomingResyncChunkMessages},
            {"dropClientIncomingResyncCompleteMessages", options.dropClientIncomingResyncCompleteMessages},
            {"reconnectReservationSecondsForTests", options.reconnectReservationSecondsForTests},
            {"hostDisconnectFrame", options.hostDisconnectFrame},
            {"clientRuntimePauseAfterFrames", options.clientRuntimePauseAfterFrames},
            {"clientRuntimePauseDurationFrames", options.clientRuntimePauseDurationFrames},
            {"expectReconnectReservationExpiry", options.expectReconnectReservationExpiry},
            {"requireHostManualLoadDuringResync", options.requireHostManualLoadDuringResync},
            {"assignLateJoinClientAfterJoin", options.assignLateJoinClientAfterJoin},
            {"assignLateJoinClientToMultitapAfterJoin", options.assignLateJoinClientToMultitapAfterJoin},
            {"assignmentSwapAfterFrames", options.assignmentSwapAfterFrames},
            {"lastCheckedFrame", lastCheckedFrame},
            {"maxStallSteps", maxStallSteps},
            {"maxClientAheadOfHostFrames", maxClientAheadOfHostFrames},
            {"maxHostAheadOfClientFrames", maxHostAheadOfClientFrames},
            {"assignmentSwapTriggered", assignmentSwapTriggered},
            {"assignmentSwapVerified", assignmentSwapVerified},
            {"assignmentPatternVerified", assignmentPatternVerified},
            {"finalCrcMatch", hostPeer.emu.valid() && clientPeer.emu.valid()
                ? (hostPeer.emu.canonicalStateCrc32() == clientPeer.emu.canonicalStateCrc32())
                : false},
            {"finalNetplayCrcMatch", hostPeer.emu.valid() && clientPeer.emu.valid()
                ? (hostPeer.emu.canonicalNetplayStateCrc32() == clientPeer.emu.canonicalNetplayStateCrc32())
                : false},
            {"finalFrameReadyFrameAligned", hostReadyFrame == clientReadyFrame},
            {"finalCommonFrameReadyFrame", commonReadyFrame},
            {"finalFrameReadyCrcMatch", finalFrameReadyCrcMatch},
            {"host", std::move(hostReport)},
            {"client", std::move(clientReport)}
        };
    }

    template<typename PeerT>
    static nlohmann::json buildRuntimeReport(const Options& options,
                                             PeerT& hostPeer,
                                             PeerT& clientPeer,
                                             const std::string& status,
                                             const std::string& failureReason,
                                             uint32_t lastCheckedFrame,
                                             uint32_t maxStallSteps)
    {
        return buildRuntimeReport(
            options,
            hostPeer,
            clientPeer,
            status,
            failureReason,
            lastCheckedFrame,
            maxStallSteps,
            0u,
            0u,
            false,
            false,
            false
        );
    }

    template<typename PeerT>
    static nlohmann::json buildRuntimeReport(const Options& options,
                                             PeerT& hostPeer,
                                             PeerT& clientPeer,
                                             const std::string& status,
                                             const std::string& failureReason,
                                             uint32_t lastCheckedFrame,
                                             uint32_t maxStallSteps,
                                             bool assignmentSwapTriggered,
                                             bool assignmentSwapVerified,
                                             bool assignmentPatternVerified)
    {
        return buildRuntimeReport(
            options,
            hostPeer,
            clientPeer,
            status,
            failureReason,
            lastCheckedFrame,
            maxStallSteps,
            0u,
            0u,
            assignmentSwapTriggered,
            assignmentSwapVerified,
            assignmentPatternVerified
        );
    }

    template<typename PeerT>
    static RunArtifacts runSingleCaseRuntimeFlowImpl(const Options& options)
    {
        RunArtifacts result;
        PeerT hostPeer("Host", true, options.hostInputSeed);
        PeerT clientPeer("Client", false, options.clientInputSeed);
        std::ofstream hostTraceFile;
        std::ofstream clientTraceFile;
        if(options.captureHostTrace) {
            const std::filesystem::path reportPath =
                options.reportPath.empty()
                    ? std::filesystem::path("build/test_reports/runtime_trace.json")
                    : std::filesystem::path(options.reportPath);
            const std::filesystem::path hostTracePath =
                reportPath.parent_path() / (reportPath.stem().string() + ".host_trace.log");
            const std::filesystem::path clientTracePath =
                reportPath.parent_path() / (reportPath.stem().string() + ".client_trace.log");
            hostTraceFile.open(hostTracePath, std::ios::out | std::ios::trunc);
            clientTraceFile.open(clientTracePath, std::ios::out | std::ios::trunc);
            configureHostTraceSinkIfSupported(hostPeer.emu, [&](const std::string& line) {
                if(hostTraceFile.is_open()) hostTraceFile << line << '\n';
            });
            configureHostTraceSinkIfSupported(clientPeer.emu, [&](const std::string& line) {
                if(clientTraceFile.is_open()) clientTraceFile << line << '\n';
            });
        }
        uint32_t lastCheckedFrame = 0;
        uint32_t stallSteps = 0;
        uint32_t sharedProgressStallSteps = 0;
        uint32_t maxStallSteps = 0;
        uint32_t maxClientAheadOfHostFrames = 0;
        uint32_t maxHostAheadOfClientFrames = 0;
        bool wallClockTimedOut = false;
        const auto wallClockStart = std::chrono::steady_clock::now();
        const auto wallClockTimeout =
            std::chrono::seconds(std::max<uint32_t>(1u, options.wallClockTimeoutSeconds));
        const auto wallClockExpired = [&]() {
            return options.wallClockTimeoutSeconds > 0u &&
                   std::chrono::steady_clock::now() - wallClockStart >= wallClockTimeout;
        };
        const auto recordLeadWindow = [&]() {
            const uint32_t hostFrame = hostPeer.emu.exactEmulationFrame();
            const uint32_t clientFrame = clientPeer.emu.exactEmulationFrame();
            if(clientFrame > hostFrame) {
                maxClientAheadOfHostFrames =
                    std::max<uint32_t>(maxClientAheadOfHostFrames, clientFrame - hostFrame);
            } else if(hostFrame > clientFrame) {
                maxHostAheadOfClientFrames =
                    std::max<uint32_t>(maxHostAheadOfClientFrames, hostFrame - clientFrame);
            }
        };

        const auto cleanup = [&]() {
            clientPeer.runtime.shutdown();
            hostPeer.runtime.shutdown();
            clientPeer.emu.shutdown();
            hostPeer.emu.shutdown();
        };

        std::string failureReason;

        if(!hostPeer.emu.open(options.romPath) || !hostPeer.emu.valid()) {
            failureReason = "Failed to open ROM on host.";
            result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
            result.exitCode = RESULT_ERROR;
            cleanup();
            return result;
        }
        if(!clientPeer.emu.open(options.romPath) || !clientPeer.emu.valid()) {
            failureReason = "Failed to open ROM on client.";
            result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
            result.exitCode = RESULT_ERROR;
            cleanup();
            return result;
        }

        hostPeer.runtime.setTransportBackend(options.transportBackend);
        clientPeer.runtime.setTransportBackend(options.transportBackend);
        hostPeer.runtime.setTransportOptions(options.transportOptions);
        clientPeer.runtime.setTransportOptions(options.transportOptions);
        hostPeer.runtime.refreshLocalRomSelectionImmediate();
        clientPeer.runtime.refreshLocalRomSelectionImmediate();

        if(options.preSessionWarmupFrames > 0) {
            hostPeer.emu.setSimulationSuspended(true);
            const bool warmed = hostPeer.emu.withExclusiveAccess([&](auto& innerEmu) {
                for(uint32_t frame = 0; frame <= options.preSessionWarmupFrames + 2u; ++frame) {
                    innerEmu.queueInputFrame(innerEmu.createInputFrame(frame));
                }
                for(uint32_t step = 0; step < options.preSessionWarmupFrames; ++step) {
                    innerEmu.updateUntilFrame(16u);
                }
                return innerEmu.frameCount() >= options.preSessionWarmupFrames;
            });
            hostPeer.emu.setSimulationSuspended(false);
            if(!warmed) {
                failureReason = "Failed to warm up host emulation before session start.";
                result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                result.exitCode = RESULT_ERROR;
                cleanup();
                return result;
            }
        }

        auto pumpPeerRuntime = [](auto& peer) {
            peer.emu.withExclusiveAccess([&](GeraNESEmu& innerEmu) {
                const auto& cfg = AppSettings::instance().data.netplay;
                const ConsoleNetplay::RuntimeExecutionSettings runtimeSettings =
                    GeraNESNetplay::buildGeraNESRuntimeExecutionSettings(
                        peer.emu,
                        cfg.autoGameplayTuning,
                        cfg.showNetplayDebugLog,
                        cfg.gameplayReceiveDelayMs,
                        cfg.inputDelayFrames
                    );
                (void)GeraNESNetplay::executeRuntimeFrame(
                    peer.runtime,
                    peer.emu,
                    innerEmu,
                    peer.latestInputState,
                    runtimeSettings
                );
            });
        };

        auto waitFor = [&](auto&& predicate, uint32_t maxSteps, uint32_t sleepMs) -> bool {
            for(uint32_t i = 0; i < maxSteps; ++i) {
                if(wallClockExpired()) {
                    wallClockTimedOut = true;
                    return false;
                }
                pumpPeerRuntime(hostPeer);
                pumpPeerRuntime(clientPeer);
                if(predicate()) return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            }
            return false;
        };

        bool hosted = false;
        uint16_t hostedPort = options.port;
        for(uint32_t portOffset = 0; portOffset < 8u; ++portOffset) {
            const uint16_t port = static_cast<uint16_t>(options.port + portOffset);
            hostPeer.runtime.host(port, 1, hostPeer.name);
            pumpPeerRuntime(hostPeer);
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            if(hostPeer.runtime.uiSnapshot().active) {
                hostedPort = port;
                if(options.reconnectReservationSecondsForTests > 0u) {
                    hostPeer.runtime.setReconnectReservationTimeoutForTests(options.reconnectReservationSecondsForTests);
                }
                if(options.hostAssignedBeforeJoinOnly) {
                    const auto hostRoomBeforeJoin = hostPeer.runtime.uiSnapshot().room;
                    const auto hostIdBeforeJoin = findParticipantIdByName(hostRoomBeforeJoin, hostPeer.name);
                    if(!hostIdBeforeJoin.has_value()) {
                        failureReason = "Failed to resolve host participant id before client join.";
                        result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                        result.exitCode = RESULT_ERROR;
                        cleanup();
                        return result;
                    }
                    hostPeer.runtime.assignController(*hostIdBeforeJoin, 0);
                    if(!waitFor([&]() {
                            const auto hostSnap = hostPeer.runtime.uiSnapshot();
                            return hostSnap.room.state == ConsoleNetplay::SessionState::Running &&
                                   findParticipantIdByName(hostSnap.room, hostPeer.name).has_value();
                        }, options.startupTimeoutSteps, 5u)) {
                        failureReason = "Timed out waiting for host-only session to start before client join.";
                        result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                        result.exitCode = RESULT_ERROR;
                        cleanup();
                        return result;
                    }

                    const uint32_t hostOnlyStartFrame = hostPeer.emu.exactEmulationFrame();
                    if(!waitFor([&]() {
                            const uint32_t hostFrame = hostPeer.emu.exactEmulationFrame();
                            const Buttons hostButtons = runtimeButtonsForPeer(
                                options,
                                hostPeer.generator,
                                true,
                                false,
                                hostFrame,
                                std::max<uint32_t>(1u, hostPeer.emu.getRegionFPS())
                            );
                            hostPeer.updateLatestInputState(
                                buildRuntimeInputStateForSlot(GeraNESNetplay::kPort1PlayerSlot, hostButtons)
                            );
                            hostPeer.emu.update(16u);
                            const uint32_t newHostFrame = hostPeer.emu.exactEmulationFrame();
                            return newHostFrame >= hostOnlyStartFrame + 4u &&
                                   peerHasBufferedLocalPattern(
                                       hostPeer,
                                       hostPeer.generator,
                                       newHostFrame + 1u,
                                       newHostFrame + 8u,
                                       GeraNESNetplay::kPort1PlayerSlot,
                                       options
                                   );
                        }, options.startupTimeoutSteps, 1u)) {
                        failureReason = "Host-only assignment did not produce immediate local input before client join.";
                        result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                        result.exitCode = RESULT_ERROR;
                        cleanup();
                        return result;
                    }
                }
                if(options.hostMultitapAssignedBeforeJoinOnly) {
                    const auto hostRoomBeforeJoin = hostPeer.runtime.uiSnapshot().room;
                    const auto hostIdBeforeJoin = findParticipantIdByName(hostRoomBeforeJoin, hostPeer.name);
                    if(!hostIdBeforeJoin.has_value()) {
                        failureReason = "Failed to resolve host participant id before Four Score assignment.";
                        result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                        result.exitCode = RESULT_ERROR;
                        cleanup();
                        return result;
                    }
                    GeraNESNetplay::configureInputAssignments(
                        hostPeer.runtime,
                        *hostIdBeforeJoin,
                        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
                        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
                        Settings::ExpansionDevice::NONE,
                        Settings::NesMultitapDevice::FOUR_SCORE,
                        Settings::FamicomMultitapDevice::NONE,
                        {GeraNESNetplay::kMultitapP1PlayerSlot}
                    );
                    if(!waitFor([&]() {
                            const auto hostSnap = hostPeer.runtime.uiSnapshot();
                            const auto localId = findParticipantIdByName(hostSnap.room, hostPeer.name);
                            if(!localId.has_value()) return false;
                            const auto* hostParticipant = findParticipantInRoom(hostSnap.room, *localId);
                            return hostParticipant != nullptr &&
                                   GeraNESNetplay::geraNESNesMultitapDeviceFromTopology(hostSnap.room) == Settings::NesMultitapDevice::FOUR_SCORE &&
                                   hostParticipant->controllerAssignment == GeraNESNetplay::kMultitapP1PlayerSlot;
                        }, options.startupTimeoutSteps, 5u)) {
                        failureReason = "Host-only Four Score P1 assignment did not stick before client join.";
                        result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                        result.exitCode = RESULT_ERROR;
                        cleanup();
                        return result;
                    }
                }
                clientPeer.runtime.join("127.0.0.1", port, clientPeer.name);
                pumpPeerRuntime(clientPeer);
                hosted = true;
                break;
            }
        }
        if(!hosted) {
            failureReason = "Failed to host room.";
            result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
            result.exitCode = RESULT_ERROR;
            cleanup();
            return result;
        }

        if(!waitFor([&]() {
                const auto hostSnap = hostPeer.runtime.uiSnapshot();
                const auto clientSnap = clientPeer.runtime.uiSnapshot();
                return hostSnap.connected &&
                       clientSnap.connected &&
                       hostSnap.room.participants.size() >= 2 &&
                       clientSnap.room.participants.size() >= 2;
            }, options.startupTimeoutSteps, 5u)) {
            failureReason = "Timed out waiting for host/client connection.";
            result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
            result.exitCode = RESULT_ERROR;
            cleanup();
            return result;
        }

        if(!waitFor([&]() {
                const auto hostSnap = hostPeer.runtime.uiSnapshot();
                for(const auto& participant : hostSnap.room.participants) {
                    if(!participant.romLoaded || !participant.romCompatible) {
                        return false;
                    }
                }
                return !hostSnap.room.participants.empty();
            }, options.startupTimeoutSteps, 5u)) {
            failureReason = "Timed out waiting for ROM validation sync.";
            result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
            result.exitCode = RESULT_ERROR;
            cleanup();
            return result;
        }

        if(!waitFor([&]() {
                const auto hostSnap = hostPeer.runtime.uiSnapshot();
                const auto clientSnap = clientPeer.runtime.uiSnapshot();
                return hostSnap.room.state == ConsoleNetplay::SessionState::Running &&
                       clientSnap.room.state == ConsoleNetplay::SessionState::Running;
            }, options.startupTimeoutSteps, 5u)) {
            failureReason = "Timed out waiting for auto-started running session.";
            result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
            result.exitCode = RESULT_ERROR;
            cleanup();
            return result;
        }

        if(!options.hostAssignedBeforeJoinOnly) {
            const uint32_t observerOnlyHostFrame = hostPeer.emu.exactEmulationFrame();
            const uint32_t observerOnlyClientFrame = clientPeer.emu.exactEmulationFrame();
            if(!waitFor([&]() {
                    hostPeer.emu.update(16u);
                    clientPeer.emu.update(16u);
                    return hostPeer.emu.exactEmulationFrame() >= observerOnlyHostFrame + 4u &&
                           clientPeer.emu.exactEmulationFrame() >= observerOnlyClientFrame + 4u;
                }, options.startupTimeoutSteps, 1u)) {
                failureReason = "Observer-only session did not advance before controller assignment.";
                result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                result.exitCode = RESULT_ERROR;
                cleanup();
                return result;
            }
        }

        const auto hostRoom = hostPeer.runtime.uiSnapshot().room;
        const auto hostId = findParticipantIdByName(hostRoom, hostPeer.name);
        const auto clientId = findParticipantIdByName(hostRoom, clientPeer.name);
        if(!hostId.has_value() || !clientId.has_value()) {
            failureReason = "Failed to resolve participant ids from runtime snapshot.";
            result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
            result.exitCode = RESULT_ERROR;
            cleanup();
            return result;
        }

        if(options.hostAssignedBeforeJoinOnly || options.hostMultitapAssignedBeforeJoinOnly) {
            if(!waitFor([&]() {
                    const auto hostSnap = hostPeer.runtime.uiSnapshot();
                    const auto clientSnap = clientPeer.runtime.uiSnapshot();
                    const auto hostLocal = findParticipantIdByName(hostSnap.room, hostPeer.name);
                    const auto clientLocal = findParticipantIdByName(clientSnap.room, clientPeer.name);
                    if(!hostLocal.has_value() || !clientLocal.has_value()) return false;
                    const auto* hostParticipant = findParticipantInRoom(hostSnap.room, *hostLocal);
                    const auto* clientParticipant = findParticipantInRoom(clientSnap.room, *clientLocal);
                    const ConsoleNetplay::PlayerSlot expectedHostAssignment =
                        options.hostMultitapAssignedBeforeJoinOnly
                            ? GeraNESNetplay::kMultitapP1PlayerSlot
                            : GeraNESNetplay::kPort1PlayerSlot;
                    const Settings::NesMultitapDevice expectedNesMultitap =
                        options.hostMultitapAssignedBeforeJoinOnly
                            ? Settings::NesMultitapDevice::FOUR_SCORE
                            : Settings::NesMultitapDevice::NONE;
                    return hostParticipant != nullptr &&
                           clientParticipant != nullptr &&
                           hostParticipant->controllerAssignment == expectedHostAssignment &&
                           clientParticipant->controllerAssignment == ConsoleNetplay::kObserverPlayerSlot &&
                           GeraNESNetplay::geraNESNesMultitapDeviceFromTopology(hostSnap.room) == expectedNesMultitap &&
                           GeraNESNetplay::geraNESNesMultitapDeviceFromTopology(clientSnap.room) == expectedNesMultitap &&
                           hostSnap.room.state == ConsoleNetplay::SessionState::Running &&
                           clientSnap.room.state == ConsoleNetplay::SessionState::Running;
                }, options.startupTimeoutSteps, 5u)) {
                failureReason = options.hostMultitapAssignedBeforeJoinOnly
                    ? "Timed out waiting for late-joining observer to sync after host was already assigned to Four Score P1."
                    : "Timed out waiting for late-joining observer to sync after host was already assigned.";
                result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                result.exitCode = RESULT_ERROR;
                cleanup();
                return result;
            }

            if(options.assignLateJoinClientAfterJoin || options.assignLateJoinClientToMultitapAfterJoin) {
                if(options.assignLateJoinClientToMultitapAfterJoin) {
                    GeraNESNetplay::configureInputAssignments(
                        hostPeer.runtime,
                        *clientId,
                        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
                        std::optional<Settings::Device>(Settings::Device::CONTROLLER),
                        Settings::ExpansionDevice::NONE,
                        Settings::NesMultitapDevice::FOUR_SCORE,
                        Settings::FamicomMultitapDevice::NONE,
                        {GeraNESNetplay::kMultitapP2PlayerSlot}
                    );
                } else {
                    hostPeer.runtime.assignController(*clientId, GeraNESNetplay::kPort2PlayerSlot);
                }
                if(!waitFor([&]() {
                        const auto hostSnap = hostPeer.runtime.uiSnapshot();
                        const auto clientSnap = clientPeer.runtime.uiSnapshot();
                        const auto hostLocal = findParticipantIdByName(hostSnap.room, hostPeer.name);
                        const auto clientLocal = findParticipantIdByName(clientSnap.room, clientPeer.name);
                        if(!hostLocal.has_value() || !clientLocal.has_value()) return false;
                        const auto* hostParticipant = findParticipantInRoom(hostSnap.room, *hostLocal);
                        const auto* clientParticipantHostView = findParticipantInRoom(hostSnap.room, *clientId);
                        const auto* clientLocalParticipant = findParticipantInRoom(clientSnap.room, *clientLocal);
                        return hostParticipant != nullptr &&
                               clientParticipantHostView != nullptr &&
                               clientLocalParticipant != nullptr &&
                               hostParticipant->controllerAssignment ==
                                   (options.assignLateJoinClientToMultitapAfterJoin
                                        ? GeraNESNetplay::kMultitapP1PlayerSlot
                                        : GeraNESNetplay::kPort1PlayerSlot) &&
                               clientParticipantHostView->controllerAssignment ==
                                   (options.assignLateJoinClientToMultitapAfterJoin
                                        ? GeraNESNetplay::kMultitapP2PlayerSlot
                                        : GeraNESNetplay::kPort2PlayerSlot) &&
                               clientLocalParticipant->controllerAssignment ==
                                   (options.assignLateJoinClientToMultitapAfterJoin
                                        ? GeraNESNetplay::kMultitapP2PlayerSlot
                                        : GeraNESNetplay::kPort2PlayerSlot) &&
                               hostSnap.room.state == ConsoleNetplay::SessionState::Running &&
                               clientSnap.room.state == ConsoleNetplay::SessionState::Running &&
                               hostSnap.room.activeResyncId == 0 &&
                               clientSnap.room.activeResyncId == 0;
                    }, options.startupTimeoutSteps, 5u)) {
                    failureReason = options.assignLateJoinClientToMultitapAfterJoin
                        ? "Timed out waiting for late-joining Four Score client assignment and automatic post-assignment resync."
                        : "Timed out waiting for late-joining client assignment and automatic post-assignment resync.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                    result.exitCode = RESULT_ERROR;
                    cleanup();
                    return result;
                }
            }
        } else if(options.hostAssignedAfterJoinOnly) {
            hostPeer.runtime.clearControllerAssignments(*clientId);
            hostPeer.runtime.assignController(*hostId, GeraNESNetplay::kPort1PlayerSlot);

            if(!waitFor([&]() {
                    const auto hostSnap = hostPeer.runtime.uiSnapshot();
                    const auto clientSnap = clientPeer.runtime.uiSnapshot();
                    const auto hostLocal = findParticipantIdByName(hostSnap.room, hostPeer.name);
                    const auto clientLocal = findParticipantIdByName(clientSnap.room, clientPeer.name);
                    if(!hostLocal.has_value() || !clientLocal.has_value()) return false;
                    const auto* hostParticipant = findParticipantInRoom(hostSnap.room, *hostLocal);
                    const auto* clientParticipantHostView = findParticipantInRoom(hostSnap.room, *clientId);
                    const auto* clientLocalParticipant = findParticipantInRoom(clientSnap.room, *clientLocal);
                    return hostParticipant != nullptr &&
                           clientParticipantHostView != nullptr &&
                           clientLocalParticipant != nullptr &&
                           hostParticipant->controllerAssignment == GeraNESNetplay::kPort1PlayerSlot &&
                           clientParticipantHostView->controllerAssignment == ConsoleNetplay::kObserverPlayerSlot &&
                           clientLocalParticipant->controllerAssignment == ConsoleNetplay::kObserverPlayerSlot &&
                           hostSnap.room.state == ConsoleNetplay::SessionState::Running &&
                           clientSnap.room.state == ConsoleNetplay::SessionState::Running &&
                           hostSnap.room.activeResyncId == 0 &&
                           clientSnap.room.activeResyncId == 0;
                }, options.startupTimeoutSteps, 5u)) {
                failureReason = "Timed out waiting for host-only assignment after observer join to settle.";
                result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                result.exitCode = RESULT_ERROR;
                cleanup();
                return result;
            }
        } else if(options.hostControllerAndZapperObserverScenario) {
            hostPeer.runtime.clearControllerAssignments(*clientId);
            GeraNESNetplay::configureInputAssignments(
                hostPeer.runtime,
                *hostId,
                std::optional<Settings::Device>(Settings::Device::CONTROLLER),
                std::optional<Settings::Device>(Settings::Device::ZAPPER),
                Settings::ExpansionDevice::NONE,
                Settings::NesMultitapDevice::NONE,
                Settings::FamicomMultitapDevice::NONE,
                {GeraNESNetplay::kPort1PlayerSlot, GeraNESNetplay::kPort2PlayerSlot}
            );

            if(!waitFor([&]() {
                    const auto hostSnap = hostPeer.runtime.uiSnapshot();
                    const auto clientSnap = clientPeer.runtime.uiSnapshot();
                    return hostSnap.room.state == ConsoleNetplay::SessionState::Running &&
                           clientSnap.room.state == ConsoleNetplay::SessionState::Running &&
                           hostSnap.room.activeResyncId == 0 &&
                           clientSnap.room.activeResyncId == 0 &&
                           GeraNESNetplay::geraNESPortDeviceFromTopology(hostSnap.room, GeraNESNetplay::kPort1PlayerSlot) == Settings::Device::CONTROLLER &&
                           GeraNESNetplay::geraNESPortDeviceFromTopology(hostSnap.room, GeraNESNetplay::kPort2PlayerSlot) == Settings::Device::ZAPPER &&
                           GeraNESNetplay::geraNESPortDeviceFromTopology(clientSnap.room, GeraNESNetplay::kPort1PlayerSlot) == Settings::Device::CONTROLLER &&
                           GeraNESNetplay::geraNESPortDeviceFromTopology(clientSnap.room, GeraNESNetplay::kPort2PlayerSlot) == Settings::Device::ZAPPER &&
                           participantHasAssignments(hostSnap.room, *hostId, {GeraNESNetplay::kPort1PlayerSlot, GeraNESNetplay::kPort2PlayerSlot}) &&
                           participantHasAssignments(clientSnap.room, *hostId, {GeraNESNetplay::kPort1PlayerSlot, GeraNESNetplay::kPort2PlayerSlot}) &&
                           localAssignedSlot(clientSnap.room, clientSnap.localParticipantId) == std::nullopt;
                }, options.startupTimeoutSteps, 5u)) {
                failureReason = "Timed out waiting for host controller+zapper observer scenario to settle.";
                result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                result.exitCode = RESULT_ERROR;
                cleanup();
                return result;
            }
        } else {
            if(options.clientAssignedOnly || options.clientAssignedPort1Only) {
                hostPeer.runtime.clearControllerAssignments(*hostId);
                hostPeer.runtime.assignController(
                    *clientId,
                    options.clientAssignedPort1Only
                        ? GeraNESNetplay::kPort1PlayerSlot
                        : GeraNESNetplay::kPort2PlayerSlot
                );
            } else {
                hostPeer.runtime.assignController(*hostId, 0);
                hostPeer.runtime.assignController(*clientId, 1);
            }

            if(!waitFor([&]() {
                    const auto hostSnap = hostPeer.runtime.uiSnapshot();
                    const auto clientSnap = clientPeer.runtime.uiSnapshot();
                    const auto hostLocal = findParticipantIdByName(hostSnap.room, hostPeer.name);
                    const auto clientLocal = findParticipantIdByName(clientSnap.room, clientPeer.name);
                    if(!hostLocal.has_value() || !clientLocal.has_value()) return false;
                    const auto* hostParticipant = findParticipantInRoom(hostSnap.room, *hostLocal);
                    const auto* clientParticipant = findParticipantInRoom(clientSnap.room, *clientLocal);
                    return hostParticipant != nullptr &&
                           clientParticipant != nullptr &&
                           hostParticipant->controllerAssignment ==
                               ((options.clientAssignedOnly || options.clientAssignedPort1Only)
                                    ? ConsoleNetplay::kObserverPlayerSlot
                                    : 0) &&
                           clientParticipant->controllerAssignment ==
                               (options.clientAssignedPort1Only
                                    ? GeraNESNetplay::kPort1PlayerSlot
                                    : GeraNESNetplay::kPort2PlayerSlot) &&
                           hostSnap.room.state == ConsoleNetplay::SessionState::Running &&
                           clientSnap.room.state == ConsoleNetplay::SessionState::Running &&
                           hostSnap.room.activeResyncId == 0 &&
                           clientSnap.room.activeResyncId == 0;
                }, options.startupTimeoutSteps, 5u)) {
                failureReason = "Timed out waiting for controller assignment sync and automatic post-assignment resync.";
                result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                result.exitCode = RESULT_ERROR;
                cleanup();
                return result;
            }
        }

        const uint32_t startHostFrame = hostPeer.emu.exactEmulationFrame();
        const uint32_t startClientFrame = clientPeer.emu.exactEmulationFrame();
        const uint32_t targetHostFrame = startHostFrame + options.frames;
        const uint32_t targetClientFrame = startClientFrame + options.frames;
        if(options.hostDisconnectFrame > 0) {
            // Intentional host shutdown scenario: suppress automatic reconnect retries
            // so the client can settle into a terminal disconnected state quickly.
            clientPeer.runtime.setLocalReconnectToken(0);
        }
        const uint32_t hostLoopDtMs = std::max<uint32_t>(1u, options.hostLoopDtMs);
        const uint32_t clientLoopDtMs = std::max<uint32_t>(1u, options.clientLoopDtMs);
        const uint32_t hostStepStride = std::max<uint32_t>(1u, options.hostStepStride);
        const uint32_t clientStepStride = std::max<uint32_t>(1u, options.clientStepStride);
        bool reconnectTriggered = false;
        bool desyncInjected = false;
        bool hardResyncObserved = false;
        bool assignmentSwapTriggered = false;
        bool assignmentSwapVerified = false;
        bool assignmentPatternVerified = !options.assignmentPatternCheck;
        bool manualResyncTriggered = false;
        bool hostResetTriggered = false;
        bool hostDisconnectTriggered = false;
        bool clientRuntimePauseTriggered = false;
        bool clientRuntimePauseRestored = false;
        bool manualResyncObserved = false;
        bool manualResyncCompleted = false;
        bool hostSaveStateCaptured = false;
        bool hostManualLoadDuringResyncObserved = false;
        bool observerVisibilitySuspendTriggered = false;
        bool observerVisibilityRestoreTriggered = false;
        uint32_t manualResyncBaselineHardResyncCount = 0;
        uint32_t manualResyncBaselineHostForceResyncEvents = 0;
        uint32_t postResyncCrcCheckStartFrame = 0;
        uint32_t postResyncCrcMismatchFrame = 0;
        uint32_t postResyncConsecutiveMismatchCount = 0;
        constexpr uint32_t kPostResyncMismatchToleranceFrames = 8u;
        uint32_t hostDisconnectCompletionWaitSteps = 0;
        const uint32_t hostDisconnectCompletionWaitLimit =
            std::max<uint32_t>(240u, std::min<uint32_t>(options.startupTimeoutSteps, 3000u));
        size_t hostManualLoadTriggerIndex = 0;
        std::vector<uint8_t> hostSavedManualLoadState;
        uint32_t clientRuntimePauseHostFrame = 0;
        uint32_t observerVisibilitySuspendHostFrame = 0;
        const auto performRuntimeReconnect = [&](const char* triggerDescription) -> bool {
            const auto clientBeforeDisconnect = clientPeer.runtime.uiSnapshot();
            const ConsoleNetplay::ParticipantId previousLocalParticipantId = clientBeforeDisconnect.localParticipantId;
            const auto previousHostViewClientId = findParticipantIdByName(hostPeer.runtime.uiSnapshot().room, clientPeer.name);
            const uint32_t reconnectWaitSteps =
                options.reconnectDuringResync
                    ? std::min<uint32_t>(options.startupTimeoutSteps, 8000u)
                    : options.startupTimeoutSteps;

            clientPeer.runtime.disconnect();

            if(!waitFor([&]() {
                    const auto hostSnap = hostPeer.runtime.uiSnapshot();
                    const auto clientSnap = clientPeer.runtime.uiSnapshot();
                    const auto hostClientId = findParticipantIdByName(hostSnap.room, clientPeer.name);
                    const auto* reservedParticipant =
                        hostClientId.has_value()
                            ? findParticipantInRoom(hostSnap.room, *hostClientId)
                            : nullptr;
                    const bool hostSessionAlive =
                        hostSnap.room.state == ConsoleNetplay::SessionState::Paused ||
                        hostSnap.room.state == ConsoleNetplay::SessionState::Running ||
                        hostSnap.room.state == ConsoleNetplay::SessionState::Resyncing;
                    const bool reservationObserved =
                        hostClientId.has_value() &&
                        reservedParticipant != nullptr &&
                        !reservedParticipant->connected &&
                        reservedParticipant->reconnectReserved;
                    const bool disconnectedWithoutReservationYet =
                        options.reconnectDuringResync &&
                        !hostClientId.has_value();
                    return hostSessionAlive &&
                           !clientSnap.connected &&
                           (reservationObserved || disconnectedWithoutReservationYet);
                }, reconnectWaitSteps, 5u)) {
                failureReason = std::string("Timed out waiting for host/client disconnect state after ") + triggerDescription + ".";
                return false;
            }

            clientPeer.runtime.join("127.0.0.1", hostedPort, clientPeer.name);

            if(!waitFor([&]() {
                    const auto hostSnap = hostPeer.runtime.uiSnapshot();
                    const auto clientSnap = clientPeer.runtime.uiSnapshot();
                    const auto hostClientId = findParticipantIdByName(hostSnap.room, clientPeer.name);
                    if(!hostClientId.has_value()) return false;
                    const auto* hostClientParticipant = findParticipantInRoom(hostSnap.room, *hostClientId);
                    const auto* clientLocalParticipant = findParticipantInRoom(clientSnap.room, clientSnap.localParticipantId);
                    const bool requireReservationReuse = !options.reconnectDuringResync;
                    const bool assignmentRestored =
                        hostClientParticipant != nullptr &&
                        hostClientParticipant->controllerAssignment == 1 &&
                        clientLocalParticipant != nullptr &&
                        clientLocalParticipant->controllerAssignment == 1;
                    return clientSnap.connected &&
                           clientSnap.active &&
                           (!requireReservationReuse || clientSnap.localParticipantId == previousLocalParticipantId) &&
                           (!requireReservationReuse ||
                            !previousHostViewClientId.has_value() ||
                            *hostClientId == *previousHostViewClientId) &&
                           hostClientParticipant != nullptr &&
                           hostClientParticipant->connected &&
                           !hostClientParticipant->reconnectReserved &&
                           (assignmentRestored || options.reconnectDuringResync);
                }, reconnectWaitSteps, 5u)) {
                failureReason = std::string("Timed out waiting for reconnecting client to reclaim its reservation and assignment after ") + triggerDescription + ".";
                return false;
            }

            const auto rejoinedClientId = findParticipantIdByName(hostPeer.runtime.uiSnapshot().room, clientPeer.name);
            if(!rejoinedClientId.has_value()) {
                failureReason = "Failed to resolve rejoined client participant id.";
                return false;
            }

            if(!waitFor([&]() {
                    const auto hostSnap = hostPeer.runtime.uiSnapshot();
                    const auto clientSnap = clientPeer.runtime.uiSnapshot();
                    const auto* hostClientParticipant = findParticipantInRoom(hostSnap.room, *rejoinedClientId);
                    const auto* clientLocalParticipant = findParticipantInRoom(clientSnap.room, clientSnap.localParticipantId);
                    const bool assignmentRestored =
                        hostClientParticipant != nullptr &&
                        hostClientParticipant->controllerAssignment == 1 &&
                        clientLocalParticipant != nullptr &&
                        clientLocalParticipant->controllerAssignment == 1;
                    return hostClientParticipant != nullptr &&
                           hostClientParticipant->connected &&
                           !hostClientParticipant->reconnectReserved &&
                           clientLocalParticipant != nullptr &&
                           (assignmentRestored || options.reconnectDuringResync) &&
                           hostSnap.room.state == ConsoleNetplay::SessionState::Running &&
                           clientSnap.room.state == ConsoleNetplay::SessionState::Running &&
                           hostSnap.room.activeResyncId == 0 &&
                           clientSnap.room.activeResyncId == 0;
                }, reconnectWaitSteps, 5u)) {
                failureReason = std::string("Timed out waiting for reconnect automatic resync and session resume after ") + triggerDescription + ".";
                return false;
            }

            reconnectTriggered = true;
            return true;
        };

        const auto finalFrameReadyCrcMatches = [&]() -> bool {
            const uint32_t hostReadyFrame = hostPeer.emu.lastFrameReadyFrame();
            const uint32_t clientReadyFrame = clientPeer.emu.lastFrameReadyFrame();
            const uint32_t commonReadyFrame = std::min(hostReadyFrame, clientReadyFrame);
            if(commonReadyFrame == 0u) {
                return false;
            }
            const auto hostReadyCrc =
                commonReadyFrame == hostReadyFrame
                    ? std::optional<uint32_t>(hostPeer.emu.lastFrameReadyNetplayCrc32())
                    : hostPeer.emu.netplaySnapshotCrc32ForFrame(commonReadyFrame);
            const auto clientReadyCrc =
                commonReadyFrame == clientReadyFrame
                    ? std::optional<uint32_t>(clientPeer.emu.lastFrameReadyNetplayCrc32())
                    : clientPeer.emu.netplaySnapshotCrc32ForFrame(commonReadyFrame);
            return hostReadyCrc.has_value() &&
                   clientReadyCrc.has_value() &&
                   *hostReadyCrc == *clientReadyCrc;
        };

        const auto settleRuntimeFinalCrc = [&]() {
            for(uint32_t i = 0; i < options.settleStepLimit && !finalFrameReadyCrcMatches(); ++i) {
                pumpPeerRuntime(hostPeer);
                pumpPeerRuntime(clientPeer);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };

        for(uint32_t step = 0; step < options.frameStepLimit; ++step) {
            if(wallClockExpired()) {
                wallClockTimedOut = true;
                failureReason = "Runtime-flow netplay test exceeded wall-clock timeout.";
                result.report = buildRuntimeReport(options, hostPeer, clientPeer, "timeout", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                result.report["wallClockTimedOut"] = true;
                result.report["startHostFrame"] = startHostFrame;
                result.report["startClientFrame"] = startClientFrame;
                result.report["targetHostFrame"] = targetHostFrame;
                result.report["targetClientFrame"] = targetClientFrame;
                result.report["desyncInjected"] = desyncInjected;
                result.report["hardResyncObserved"] = hardResyncObserved;
                result.report["reconnectTriggered"] = reconnectTriggered;
                result.report["clientRuntimePauseTriggered"] = clientRuntimePauseTriggered;
                result.report["clientRuntimePauseRestored"] = clientRuntimePauseRestored;
                result.exitCode = RESULT_FAILED;
                cleanup();
                return result;
            }
            const uint32_t hostFrame = hostPeer.emu.exactEmulationFrame();
            const uint32_t clientFrame = clientPeer.emu.exactEmulationFrame();
            recordLeadWindow();
            const auto hostLoopSnapshot = hostPeer.runtime.uiSnapshot();
            const auto clientLoopSnapshot = clientPeer.runtime.uiSnapshot();
            const auto hostLocalSlot = localAssignedSlot(hostLoopSnapshot.room, hostLoopSnapshot.localParticipantId);
            const auto clientLocalSlot = localAssignedSlot(clientLoopSnapshot.room, clientLoopSnapshot.localParticipantId);
            const Buttons hostButtons = runtimeButtonsForPeer(
                options,
                hostPeer.generator,
                true,
                assignmentSwapVerified,
                hostFrame,
                std::max<uint32_t>(1u, hostPeer.emu.getRegionFPS())
            );
            const Buttons clientButtons = runtimeButtonsForPeer(
                options,
                clientPeer.generator,
                false,
                assignmentSwapVerified,
                clientFrame,
                std::max<uint32_t>(1u, clientPeer.emu.getRegionFPS())
            );
            const bool hostInRecoveryWindow =
                hostLoopSnapshot.room.state == ConsoleNetplay::SessionState::Resyncing ||
                hostLoopSnapshot.room.activeResyncId != 0u;
            const bool clientInRecoveryWindow =
                clientLoopSnapshot.room.state == ConsoleNetplay::SessionState::Resyncing ||
                clientLoopSnapshot.room.activeResyncId != 0u;
            if(options.clientRuntimePauseAfterFrames > 0 &&
               !clientRuntimePauseTriggered &&
               hostFrame >= startHostFrame + options.clientRuntimePauseAfterFrames) {
                clientRuntimePauseTriggered = true;
                clientRuntimePauseHostFrame = hostFrame;
                clientPeer.emu.setSimulationSuspended(true);
            }
            const bool clientRuntimePaused =
                clientRuntimePauseTriggered &&
                !clientRuntimePauseRestored;
            Buttons effectiveHostButtons = hostButtons;
            Buttons effectiveClientButtons = clientButtons;
            if(options.spamHostInputDuringResync && hostInRecoveryWindow) {
                const bool pulse = ((hostFrame + step) & 1u) == 0u;
                effectiveHostButtons = {};
                effectiveHostButtons.a = pulse;
                effectiveHostButtons.b = !pulse;
                effectiveHostButtons.left = pulse;
                effectiveHostButtons.right = !pulse;
                effectiveHostButtons.start = ((hostFrame + step) % 3u) == 0u;
            }
            if(options.spamClientInputDuringResync && clientInRecoveryWindow) {
                const bool pulse = ((clientFrame + step) & 1u) == 0u;
                effectiveClientButtons = {};
                effectiveClientButtons.a = !pulse;
                effectiveClientButtons.b = pulse;
                effectiveClientButtons.up = pulse;
                effectiveClientButtons.down = !pulse;
                effectiveClientButtons.select = ((clientFrame + step) % 3u) == 1u;
            }
            if(options.hostControllerAndZapperObserverScenario) {
                hostPeer.updateLatestInputState(
                    buildDuckHuntLikeRuntimeInputState(hostFrame, std::max<uint32_t>(1u, hostPeer.emu.getRegionFPS()))
                );
                clientPeer.updateLatestInputState(IEmulationHost::InputState{});
            } else {
                hostPeer.updateLatestInputState(
                    hostLocalSlot.has_value()
                        ? buildRuntimeInputStateForSlot(*hostLocalSlot, effectiveHostButtons)
                        : IEmulationHost::InputState{}
                );
                clientPeer.updateLatestInputState(
                    clientRuntimePaused ||
                            (options.hostAssignedBeforeJoinOnly &&
                             !options.assignLateJoinClientAfterJoin &&
                             !options.assignLateJoinClientToMultitapAfterJoin) ||
                            !clientLocalSlot.has_value()
                        ? IEmulationHost::InputState{}
                        : buildRuntimeInputStateForSlot(*clientLocalSlot, effectiveClientButtons)
                );
            }

            if((step % hostStepStride) == 0u) {
                hostPeer.emu.update(hostLoopDtMs);
            }
            if(clientRuntimePaused) {
                pumpPeerRuntime(clientPeer);
            } else if((step % clientStepStride) == 0u) {
                clientPeer.emu.update(clientLoopDtMs);
            }
            recordLeadWindow();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));

            if(clientRuntimePaused &&
               options.clientRuntimePauseDurationFrames > 0 &&
               hostPeer.emu.exactEmulationFrame() >=
                   clientRuntimePauseHostFrame + options.clientRuntimePauseDurationFrames) {
                clientPeer.emu.setSimulationSuspended(false);
                clientRuntimePauseRestored = true;
            }

            if(options.assignmentSwapAfterFrames > 0 &&
               !assignmentSwapTriggered &&
               hostPeer.emu.exactEmulationFrame() >= startHostFrame + options.assignmentSwapAfterFrames &&
               clientPeer.emu.exactEmulationFrame() >= startClientFrame + options.assignmentSwapAfterFrames) {
                hostPeer.runtime.assignController(*hostId, 1);
                hostPeer.runtime.assignController(*clientId, 0);
                assignmentSwapTriggered = true;

                if(!waitFor([&]() {
                        const auto hostSwapSnap = hostPeer.runtime.uiSnapshot();
                        const auto clientSwapSnap = clientPeer.runtime.uiSnapshot();
                        const auto* hostParticipant = findParticipantInRoom(hostSwapSnap.room, *hostId);
                        const auto* clientParticipantHostView = findParticipantInRoom(hostSwapSnap.room, *clientId);
                        const auto* clientLocalParticipant = findParticipantInRoom(clientSwapSnap.room, clientSwapSnap.localParticipantId);
                        return hostParticipant != nullptr &&
                               clientParticipantHostView != nullptr &&
                               clientLocalParticipant != nullptr &&
                               hostParticipant->controllerAssignment == GeraNESNetplay::kPort2PlayerSlot &&
                               clientParticipantHostView->controllerAssignment == GeraNESNetplay::kPort1PlayerSlot &&
                               clientLocalParticipant->controllerAssignment == GeraNESNetplay::kPort1PlayerSlot &&
                               hostSwapSnap.room.state == ConsoleNetplay::SessionState::Running &&
                               clientSwapSnap.room.state == ConsoleNetplay::SessionState::Running &&
                               hostSwapSnap.room.activeResyncId == 0 &&
                               clientSwapSnap.room.activeResyncId == 0;
                    }, options.startupTimeoutSteps, 5u)) {
                    failureReason = "Timed out waiting for host/client controller swap and automatic resync.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.exitCode = RESULT_ERROR;
                    cleanup();
                    return result;
                }

                assignmentSwapVerified = true;
            }

            if(options.reconnectAfterFrames > 0 &&
               !reconnectTriggered &&
               hostPeer.emu.exactEmulationFrame() >= startHostFrame + options.reconnectAfterFrames) {
                if(options.expectReconnectReservationExpiry) {
                    clientPeer.runtime.simulateTransportFailureForTests();

                    if(!waitFor([&]() {
                            const auto hostSnap = hostPeer.runtime.uiSnapshot();
                            const auto clientSnap = clientPeer.runtime.uiSnapshot();
                            const auto hostClientId = findParticipantIdByName(hostSnap.room, clientPeer.name);
                            const auto* reservedParticipant =
                                hostClientId.has_value()
                                    ? findParticipantInRoom(hostSnap.room, *hostClientId)
                                    : nullptr;
                            return hostClientId.has_value() &&
                                   reservedParticipant != nullptr &&
                                   !reservedParticipant->connected &&
                                   reservedParticipant->reconnectReserved &&
                                   !clientSnap.connected;
                        }, options.startupTimeoutSteps, 5u)) {
                        failureReason = "Timed out waiting for reconnect reservation to be created.";
                        result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                        result.exitCode = RESULT_FAILED;
                        cleanup();
                        return result;
                    }

                    if(!waitFor([&]() {
                            const auto hostSnap = hostPeer.runtime.uiSnapshot();
                            return findParticipantIdByName(hostSnap.room, clientPeer.name) == std::nullopt;
                        }, options.startupTimeoutSteps, 10u)) {
                        failureReason = "Timed out waiting for reconnect reservation expiry and participant removal.";
                        result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                        result.exitCode = RESULT_FAILED;
                        cleanup();
                        return result;
                    }

                    reconnectTriggered = true;
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "ok", "", lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.report["startHostFrame"] = startHostFrame;
                    result.report["startClientFrame"] = startClientFrame;
                    result.report["targetHostFrame"] = targetHostFrame;
                    result.report["targetClientFrame"] = targetClientFrame;
                    result.report["desyncInjected"] = desyncInjected;
                    result.report["hardResyncObserved"] = hardResyncObserved;
                    result.report["reconnectTriggered"] = reconnectTriggered;
                    result.report["manualResyncTriggered"] = manualResyncTriggered;
                    result.report["manualResyncObserved"] = manualResyncObserved;
                    result.report["manualResyncCompleted"] = manualResyncCompleted;
                    result.report["postResyncCrcCheckStartFrame"] = postResyncCrcCheckStartFrame;
                    result.report["postResyncCrcMismatchFrame"] = postResyncCrcMismatchFrame;
                    result.exitCode = EXIT_SUCCESS;
                    cleanup();
                    return result;
                }

                if(!performRuntimeReconnect("assigned client disconnect")) {
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps);
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
            }

            if(options.webObserverVisibilitySuspendAfterFrames > 0 &&
               options.hostAssignedBeforeJoinOnly &&
               !observerVisibilitySuspendTriggered &&
               hostPeer.emu.exactEmulationFrame() >= startHostFrame + options.webObserverVisibilitySuspendAfterFrames) {
                clientPeer.runtime.notifyWebVisibilityChanged(false);
                clientPeer.emu.setSimulationSuspended(true);
                observerVisibilitySuspendTriggered = true;
                observerVisibilitySuspendHostFrame = hostPeer.emu.exactEmulationFrame();
            }

            if(observerVisibilitySuspendTriggered &&
               !observerVisibilityRestoreTriggered &&
               options.webObserverVisibilitySuspendDurationFrames > 0 &&
               hostPeer.emu.exactEmulationFrame() >=
                   observerVisibilitySuspendHostFrame + options.webObserverVisibilitySuspendDurationFrames) {
                clientPeer.runtime.notifyWebVisibilityChanged(true);
                observerVisibilityRestoreTriggered = true;
            }

            if(options.forceDesyncFrame > 0 &&
               !desyncInjected &&
               hostPeer.emu.exactEmulationFrame() >= startHostFrame + options.forceDesyncFrame &&
               clientPeer.emu.exactEmulationFrame() >= startClientFrame + options.forceDesyncFrame) {
                clientPeer.emu.withExclusiveAccess([&](auto& innerEmu) {
                    const uint16_t targetAddress = static_cast<uint16_t>(options.desyncAddress & 0xFFFFu);
                    const uint8_t originalValue = innerEmu.read(targetAddress);
                    innerEmu.write(targetAddress,
                                   static_cast<uint8_t>(originalValue ^ static_cast<uint8_t>(options.desyncValueXor & 0xFFu)));
                });
                desyncInjected = true;
            }

            if(options.forceManualResyncFrame > 0 &&
               !manualResyncTriggered &&
               hostPeer.emu.exactEmulationFrame() >= startHostFrame + options.forceManualResyncFrame &&
               clientPeer.emu.exactEmulationFrame() >= startClientFrame + options.forceManualResyncFrame) {
                if(options.dropClientIncomingResyncChunkMessages > 0u) {
                    clientPeer.runtime.injectDropNextIncomingMessages(
                        ConsoleNetplay::MessageType::ResyncChunk,
                        options.dropClientIncomingResyncChunkMessages
                    );
                }
                if(options.dropClientIncomingResyncCompleteMessages > 0u) {
                    clientPeer.runtime.injectDropNextIncomingMessages(
                        ConsoleNetplay::MessageType::ResyncComplete,
                        options.dropClientIncomingResyncCompleteMessages
                    );
                }
                manualResyncBaselineHardResyncCount =
                    hostLoopSnapshot.recoveryStats.hardResyncCount +
                    clientLoopSnapshot.recoveryStats.hardResyncCount;
                manualResyncBaselineHostForceResyncEvents = static_cast<uint32_t>(
                    std::count(
                        hostLoopSnapshot.eventLog.begin(),
                        hostLoopSnapshot.eventLog.end(),
                        std::string("Owner forced resync")
                    )
                );
                hostPeer.runtime.requestForceResync();
                manualResyncTriggered = true;
                if(options.requireHostManualLoadDuringResync &&
                   hostManualLoadTriggerIndex < options.hostManualLoadStateFrames.size()) {
                    if(hostSavedManualLoadState.empty()) {
                        failureReason = "Host load-during-resync scenario requested a manual resync before a save state was captured.";
                        result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                        result.exitCode = RESULT_ERROR;
                        cleanup();
                        return result;
                    }
                    if(!hostPeer.emu.loadStateFromMemoryAsManualStateChange(hostSavedManualLoadState)) {
                        failureReason = "Host failed to load the saved state while forcing an active resync.";
                        result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                        result.exitCode = RESULT_ERROR;
                        cleanup();
                        return result;
                    }
                    hostManualLoadDuringResyncObserved = true;
                    ++hostManualLoadTriggerIndex;
                }
            }

            if(options.hostSaveStateFrame > 0 &&
               !hostSaveStateCaptured &&
               hostPeer.emu.exactEmulationFrame() >= startHostFrame + options.hostSaveStateFrame &&
               clientPeer.emu.exactEmulationFrame() >= startClientFrame + options.hostSaveStateFrame) {
                hostSavedManualLoadState = hostPeer.emu.saveStateToMemory();
                if(hostSavedManualLoadState.empty()) {
                    failureReason = "Failed to capture host save state for manual load-state scenario.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.exitCode = RESULT_ERROR;
                    cleanup();
                    return result;
                }
                hostSaveStateCaptured = true;
            }

            while(hostManualLoadTriggerIndex < options.hostManualLoadStateFrames.size()) {
                const bool requestedLoadFrameReached =
                    hostPeer.emu.exactEmulationFrame() >= startHostFrame + options.hostManualLoadStateFrames[hostManualLoadTriggerIndex] &&
                    clientPeer.emu.exactEmulationFrame() >= startClientFrame + options.hostManualLoadStateFrames[hostManualLoadTriggerIndex];
                const bool activeResyncObserved =
                    hostLoopSnapshot.room.state == ConsoleNetplay::SessionState::Resyncing ||
                    clientLoopSnapshot.room.state == ConsoleNetplay::SessionState::Resyncing ||
                    hostLoopSnapshot.room.activeResyncId != 0u ||
                    clientLoopSnapshot.room.activeResyncId != 0u ||
                    (manualResyncObserved && !manualResyncCompleted);
                const bool shouldForceLoadDuringObservedResync =
                    options.requireHostManualLoadDuringResync &&
                    !hostManualLoadDuringResyncObserved &&
                    activeResyncObserved;
                if(!requestedLoadFrameReached && !shouldForceLoadDuringObservedResync) {
                    break;
                }

                if(hostSavedManualLoadState.empty()) {
                    failureReason = "Host manual load-state scenario triggered before a save state was captured.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.exitCode = RESULT_ERROR;
                    cleanup();
                    return result;
                }

                if(activeResyncObserved) {
                    hostManualLoadDuringResyncObserved = true;
                }

                if(!hostPeer.emu.loadStateFromMemoryAsManualStateChange(hostSavedManualLoadState)) {
                    failureReason = "Host failed to load the saved state during the repeated load-state scenario.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.exitCode = RESULT_ERROR;
                    cleanup();
                    return result;
                }

                ++hostManualLoadTriggerIndex;
            }

            if(options.hostDisconnectFrame > 0 &&
               !hostDisconnectTriggered &&
               hostPeer.emu.exactEmulationFrame() >= startHostFrame + options.hostDisconnectFrame) {
                hostPeer.runtime.disconnect();
                hostDisconnectTriggered = true;
            }

            if(options.forceHostResetFrame > 0 &&
               !hostResetTriggered &&
               hostPeer.emu.exactEmulationFrame() >= startHostFrame + options.forceHostResetFrame &&
               clientPeer.emu.exactEmulationFrame() >= startClientFrame + options.forceHostResetFrame) {
                hostPeer.emu.reset();
                hostResetTriggered = true;
            }

            if(options.assignmentPatternCheck && assignmentSwapVerified && !assignmentPatternVerified) {
                const uint32_t probeStartFrame = std::min(hostPeer.emu.exactEmulationFrame(), clientPeer.emu.exactEmulationFrame()) + 1u;
                const uint32_t probeEndFrame = probeStartFrame + 12u;
                assignmentPatternVerified =
                    peerHasBufferedPattern(hostPeer,
                                           hostPeer.generator,
                                           clientPeer.generator,
                                           probeStartFrame,
                                           probeEndFrame,
                                           GeraNESNetplay::kPort2PlayerSlot,
                                           GeraNESNetplay::kPort1PlayerSlot,
                                           options,
                                           true) &&
                    peerHasBufferedPattern(clientPeer,
                                           hostPeer.generator,
                                           clientPeer.generator,
                                           probeStartFrame,
                                           probeEndFrame,
                                           GeraNESNetplay::kPort2PlayerSlot,
                                           GeraNESNetplay::kPort1PlayerSlot,
                                           options,
                                           true);
            }

            const uint32_t newHostFrame = hostPeer.emu.exactEmulationFrame();
            const uint32_t newClientFrame = clientPeer.emu.exactEmulationFrame();
            const auto hostSnap = hostPeer.runtime.uiSnapshot();
            const auto clientSnap = clientPeer.runtime.uiSnapshot();
            const uint32_t currentHardResyncCount =
                hostSnap.recoveryStats.hardResyncCount +
                clientSnap.recoveryStats.hardResyncCount;

            if(hostDisconnectTriggered) {
                const bool clientDisconnectedNoReconnect =
                    !clientSnap.connected &&
                    !clientSnap.reconnecting;
                const bool roomClosedObserved =
                    clientSnap.room.state == ConsoleNetplay::SessionState::Ended ||
                    clientSnap.room.participants.empty();
                if(clientDisconnectedNoReconnect && roomClosedObserved) {
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "ok", "", lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.report["startHostFrame"] = startHostFrame;
                    result.report["startClientFrame"] = startClientFrame;
                    result.report["targetHostFrame"] = targetHostFrame;
                    result.report["targetClientFrame"] = targetClientFrame;
                    result.report["desyncInjected"] = desyncInjected;
                    result.report["hardResyncObserved"] = hardResyncObserved;
                    result.report["reconnectTriggered"] = reconnectTriggered;
                    result.report["hostDisconnectTriggered"] = hostDisconnectTriggered;
                    result.report["clientRuntimePauseTriggered"] = clientRuntimePauseTriggered;
                    result.report["clientRuntimePauseRestored"] = clientRuntimePauseRestored;
                    result.exitCode = EXIT_SUCCESS;
                    cleanup();
                    return result;
                }
            }

            hardResyncObserved =
                hardResyncObserved ||
                hostSnap.recoveryStats.hardResyncCount > 0u ||
                clientSnap.recoveryStats.hardResyncCount > 0u ||
                hostSnap.room.activeResyncId != 0u ||
                clientSnap.room.activeResyncId != 0u;
            const uint32_t currentHostForceResyncEvents = static_cast<uint32_t>(
                std::count(
                    hostSnap.eventLog.begin(),
                    hostSnap.eventLog.end(),
                    std::string("Owner forced resync")
                )
            );
            manualResyncObserved =
                manualResyncObserved ||
                (manualResyncTriggered &&
                 (hostSnap.room.activeResyncId != 0u ||
                  clientSnap.room.activeResyncId != 0u ||
                  currentHardResyncCount > manualResyncBaselineHardResyncCount ||
                  currentHostForceResyncEvents > manualResyncBaselineHostForceResyncEvents));

            if(options.reconnectDuringResync &&
               manualResyncTriggered &&
               manualResyncObserved &&
               !manualResyncCompleted &&
               !reconnectTriggered &&
               (hostSnap.room.state == ConsoleNetplay::SessionState::Resyncing ||
                clientSnap.room.state == ConsoleNetplay::SessionState::Resyncing ||
                hostSnap.room.activeResyncId != 0u ||
                clientSnap.room.activeResyncId != 0u ||
                hostSnap.room.state == ConsoleNetplay::SessionState::Running ||
                clientSnap.room.state == ConsoleNetplay::SessionState::Running)) {
                if(!performRuntimeReconnect("disconnect during active or just-observed resync")) {
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "error", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
            }

            if(manualResyncTriggered &&
               manualResyncObserved &&
               !manualResyncCompleted &&
               hostSnap.room.state == ConsoleNetplay::SessionState::Running &&
               clientSnap.room.state == ConsoleNetplay::SessionState::Running &&
               hostSnap.room.activeResyncId == 0u &&
               clientSnap.room.activeResyncId == 0u) {
                manualResyncCompleted = true;
                postResyncCrcCheckStartFrame =
                    std::min(hostPeer.emu.lastFrameReadyFrame(), clientPeer.emu.lastFrameReadyFrame());
            }

            if(manualResyncCompleted &&
               hostPeer.emu.lastFrameReadyFrame() == clientPeer.emu.lastFrameReadyFrame() &&
               hostPeer.emu.lastFrameReadyFrame() >= postResyncCrcCheckStartFrame) {
                if(hostPeer.emu.lastFrameReadyNetplayCrc32() != clientPeer.emu.lastFrameReadyNetplayCrc32()) {
                    if(postResyncCrcMismatchFrame == 0u) {
                        postResyncCrcMismatchFrame = hostPeer.emu.lastFrameReadyFrame();
                    }
                    ++postResyncConsecutiveMismatchCount;
                    if(!options.reconnectDuringResync &&
                       postResyncConsecutiveMismatchCount >= kPostResyncMismatchToleranceFrames) {
                        failureReason =
                            "Host/client netplay CRC diverged after forced resync at frame " +
                            std::to_string(postResyncCrcMismatchFrame) + ".";
                        result.report = buildRuntimeReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                        result.report["startHostFrame"] = startHostFrame;
                        result.report["startClientFrame"] = startClientFrame;
                        result.report["targetHostFrame"] = targetHostFrame;
                        result.report["targetClientFrame"] = targetClientFrame;
                        result.report["desyncInjected"] = desyncInjected;
                        result.report["hardResyncObserved"] = hardResyncObserved;
                        result.report["reconnectTriggered"] = reconnectTriggered;
                        result.report["manualResyncTriggered"] = manualResyncTriggered;
                        result.report["manualResyncObserved"] = manualResyncObserved;
                        result.report["manualResyncCompleted"] = manualResyncCompleted;
                        result.report["postResyncCrcCheckStartFrame"] = postResyncCrcCheckStartFrame;
                        result.report["postResyncCrcMismatchFrame"] = postResyncCrcMismatchFrame;
                        result.report["postResyncConsecutiveMismatchCount"] = postResyncConsecutiveMismatchCount;
                        result.exitCode = RESULT_FAILED;
                        cleanup();
                        return result;
                    }
                } else {
                    postResyncCrcMismatchFrame = 0u;
                    postResyncConsecutiveMismatchCount = 0u;
                }
            }

            if(newHostFrame == hostFrame && newClientFrame == clientFrame) {
                ++stallSteps;
            } else {
                stallSteps = 0;
            }
            const uint32_t previousLastCheckedFrame = lastCheckedFrame;
            lastCheckedFrame = std::min(newHostFrame, newClientFrame);
            if(lastCheckedFrame == previousLastCheckedFrame) {
                ++sharedProgressStallSteps;
            } else {
                sharedProgressStallSteps = 0;
            }
            maxStallSteps = std::max(maxStallSteps, std::max(stallSteps, sharedProgressStallSteps));

            if(stallSteps > 240u || sharedProgressStallSteps > 240u) {
                failureReason =
                    sharedProgressStallSteps > 240u
                        ? "Runtime flow stalled (no shared host/client frame progress)."
                        : "Runtime flow stalled after session start.";
                result.report = buildRuntimeReport(options, hostPeer, clientPeer, "stalled", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                result.report["startHostFrame"] = startHostFrame;
                result.report["startClientFrame"] = startClientFrame;
                result.report["targetHostFrame"] = targetHostFrame;
                result.report["targetClientFrame"] = targetClientFrame;
                result.report["desyncInjected"] = desyncInjected;
                result.report["hardResyncObserved"] = hardResyncObserved;
                result.report["reconnectTriggered"] = reconnectTriggered;
                result.exitCode = RESULT_FAILED;
                cleanup();
                return result;
            }

            if(newHostFrame >= targetHostFrame && newClientFrame >= targetClientFrame) {
                if(options.reconnectAfterFrames > 0 && !reconnectTriggered) {
                    failureReason = "Reconnect scenario never triggered before reaching the target frame.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.report["startHostFrame"] = startHostFrame;
                    result.report["startClientFrame"] = startClientFrame;
                    result.report["targetHostFrame"] = targetHostFrame;
                    result.report["targetClientFrame"] = targetClientFrame;
                    result.report["desyncInjected"] = desyncInjected;
                    result.report["hardResyncObserved"] = hardResyncObserved;
                    result.report["reconnectTriggered"] = reconnectTriggered;
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
                if(options.hostDisconnectFrame > 0 && !hostDisconnectTriggered) {
                    failureReason = "Host-disconnect scenario never triggered before reaching the target frame.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.report["desyncInjected"] = desyncInjected;
                    result.report["hardResyncObserved"] = hardResyncObserved;
                    result.report["reconnectTriggered"] = reconnectTriggered;
                    result.report["hostDisconnectTriggered"] = hostDisconnectTriggered;
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
                if(options.reconnectDuringResync && !reconnectTriggered) {
                    failureReason = "Reconnect-during-resync scenario never triggered before reaching the target frame.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.report["startHostFrame"] = startHostFrame;
                    result.report["startClientFrame"] = startClientFrame;
                    result.report["targetHostFrame"] = targetHostFrame;
                    result.report["targetClientFrame"] = targetClientFrame;
                    result.report["desyncInjected"] = desyncInjected;
                    result.report["hardResyncObserved"] = hardResyncObserved;
                    result.report["reconnectTriggered"] = reconnectTriggered;
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
                if(options.forceDesyncFrame > 0 && (!desyncInjected || !hardResyncObserved)) {
                    failureReason = !desyncInjected
                        ? "Forced desync scenario never injected the divergence."
                        : "Forced desync scenario completed without observing a hard resync.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.report["startHostFrame"] = startHostFrame;
                    result.report["startClientFrame"] = startClientFrame;
                    result.report["targetHostFrame"] = targetHostFrame;
                    result.report["targetClientFrame"] = targetClientFrame;
                    result.report["desyncInjected"] = desyncInjected;
                    result.report["hardResyncObserved"] = hardResyncObserved;
                    result.report["reconnectTriggered"] = reconnectTriggered;
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
                if(options.assignmentPatternCheck && !assignmentPatternVerified) {
                    failureReason = "Assignment swap completed, but the expected post-swap input pattern was not observed in queued netplay frames.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.report["startHostFrame"] = startHostFrame;
                    result.report["startClientFrame"] = startClientFrame;
                    result.report["targetHostFrame"] = targetHostFrame;
                    result.report["targetClientFrame"] = targetClientFrame;
                    result.report["desyncInjected"] = desyncInjected;
                    result.report["hardResyncObserved"] = hardResyncObserved;
                    result.report["reconnectTriggered"] = reconnectTriggered;
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
                if(options.forceManualResyncFrame > 0 &&
                   (!manualResyncTriggered || !manualResyncObserved || !manualResyncCompleted)) {
                    failureReason =
                        !manualResyncTriggered
                            ? "Forced resync scenario never requested the manual resync."
                            : (!manualResyncObserved
                                   ? "Forced resync scenario never observed the room enter resyncing."
                                   : "Forced resync scenario never returned to running after the resync.");
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.report["startHostFrame"] = startHostFrame;
                    result.report["startClientFrame"] = startClientFrame;
                    result.report["targetHostFrame"] = targetHostFrame;
                    result.report["targetClientFrame"] = targetClientFrame;
                    result.report["desyncInjected"] = desyncInjected;
                    result.report["hardResyncObserved"] = hardResyncObserved;
                    result.report["reconnectTriggered"] = reconnectTriggered;
                    result.report["manualResyncTriggered"] = manualResyncTriggered;
                    result.report["manualResyncObserved"] = manualResyncObserved;
                    result.report["manualResyncCompleted"] = manualResyncCompleted;
                    result.report["postResyncCrcCheckStartFrame"] = postResyncCrcCheckStartFrame;
                    result.report["postResyncCrcMismatchFrame"] = postResyncCrcMismatchFrame;
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
                if(!options.hostManualLoadStateFrames.empty() &&
                   hostManualLoadTriggerIndex != options.hostManualLoadStateFrames.size()) {
                    failureReason =
                        "Repeated host load-state scenario never triggered all requested load events.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.report["startHostFrame"] = startHostFrame;
                    result.report["startClientFrame"] = startClientFrame;
                    result.report["targetHostFrame"] = targetHostFrame;
                    result.report["targetClientFrame"] = targetClientFrame;
                    result.report["desyncInjected"] = desyncInjected;
                    result.report["hardResyncObserved"] = hardResyncObserved;
                    result.report["reconnectTriggered"] = reconnectTriggered;
                    result.report["hostManualLoadDuringResyncObserved"] = hostManualLoadDuringResyncObserved;
                    result.report["hostManualLoadTriggerCount"] = hostManualLoadTriggerIndex;
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
                if(options.requireHostManualLoadDuringResync &&
                   !hostManualLoadDuringResyncObserved) {
                    failureReason =
                        "Host load-state scenario never observed the host loading during an active resync.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.report["startHostFrame"] = startHostFrame;
                    result.report["startClientFrame"] = startClientFrame;
                    result.report["targetHostFrame"] = targetHostFrame;
                    result.report["targetClientFrame"] = targetClientFrame;
                    result.report["desyncInjected"] = desyncInjected;
                    result.report["hardResyncObserved"] = hardResyncObserved;
                    result.report["reconnectTriggered"] = reconnectTriggered;
                    result.report["hostManualLoadDuringResyncObserved"] = hostManualLoadDuringResyncObserved;
                    result.report["hostManualLoadTriggerCount"] = hostManualLoadTriggerIndex;
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
                if(options.hostDisconnectFrame > 0 &&
                   !((!clientSnap.connected && !clientSnap.reconnecting) &&
                     (clientSnap.room.state == ConsoleNetplay::SessionState::Ended ||
                      clientSnap.room.participants.empty()))) {
                    ++hostDisconnectCompletionWaitSteps;
                    if(hostDisconnectCompletionWaitSteps >= hostDisconnectCompletionWaitLimit) {
                        failureReason =
                            "Timed out waiting for client to observe host room closure after intentional disconnect.";
                        result.report = buildRuntimeReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                        result.report["startHostFrame"] = startHostFrame;
                        result.report["startClientFrame"] = startClientFrame;
                        result.report["targetHostFrame"] = targetHostFrame;
                        result.report["targetClientFrame"] = targetClientFrame;
                        result.report["desyncInjected"] = desyncInjected;
                        result.report["hardResyncObserved"] = hardResyncObserved;
                        result.report["reconnectTriggered"] = reconnectTriggered;
                        result.report["hostDisconnectTriggered"] = hostDisconnectTriggered;
                        result.exitCode = RESULT_FAILED;
                        cleanup();
                        return result;
                    }
                    continue;
                }
                if(options.clientRuntimePauseAfterFrames > 0 &&
                   (!clientRuntimePauseTriggered ||
                    (options.clientRuntimePauseDurationFrames > 0 && !clientRuntimePauseRestored))) {
                    failureReason =
                        !clientRuntimePauseTriggered
                            ? "Client runtime pause scenario never triggered before reaching the target frame."
                            : "Client runtime pause scenario never restored before reaching the target frame.";
                    result.report = buildRuntimeReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.report["startHostFrame"] = startHostFrame;
                    result.report["startClientFrame"] = startClientFrame;
                    result.report["targetHostFrame"] = targetHostFrame;
                    result.report["targetClientFrame"] = targetClientFrame;
                    result.report["desyncInjected"] = desyncInjected;
                    result.report["hardResyncObserved"] = hardResyncObserved;
                    result.report["reconnectTriggered"] = reconnectTriggered;
                    result.report["hostDisconnectTriggered"] = hostDisconnectTriggered;
                    result.report["clientRuntimePauseTriggered"] = clientRuntimePauseTriggered;
                    result.report["clientRuntimePauseRestored"] = clientRuntimePauseRestored;
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }

                settleRuntimeFinalCrc();
                result.report = buildRuntimeReport(options, hostPeer, clientPeer, "ok", "", lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                result.report["startHostFrame"] = startHostFrame;
                result.report["startClientFrame"] = startClientFrame;
                result.report["targetHostFrame"] = targetHostFrame;
                result.report["targetClientFrame"] = targetClientFrame;
                result.report["desyncInjected"] = desyncInjected;
                result.report["hardResyncObserved"] = hardResyncObserved;
                result.report["reconnectTriggered"] = reconnectTriggered;
                result.report["hostDisconnectTriggered"] = hostDisconnectTriggered;
                result.report["manualResyncTriggered"] = manualResyncTriggered;
                result.report["manualResyncObserved"] = manualResyncObserved;
                result.report["manualResyncCompleted"] = manualResyncCompleted;
                result.report["clientRuntimePauseTriggered"] = clientRuntimePauseTriggered;
                result.report["clientRuntimePauseRestored"] = clientRuntimePauseRestored;
                result.report["postResyncCrcCheckStartFrame"] = postResyncCrcCheckStartFrame;
                result.report["postResyncCrcMismatchFrame"] = postResyncCrcMismatchFrame;
                result.report["postResyncConsecutiveMismatchCount"] = postResyncConsecutiveMismatchCount;
                result.report["hostManualLoadDuringResyncObserved"] = hostManualLoadDuringResyncObserved;
                result.report["hostManualLoadTriggerCount"] = hostManualLoadTriggerIndex;
                result.exitCode = EXIT_SUCCESS;
                cleanup();
                return result;
            }
        }

        failureReason = "Runtime-flow netplay test reached the step limit.";
        result.report = buildRuntimeReport(options, hostPeer, clientPeer, "stalled", failureReason, lastCheckedFrame, maxStallSteps, maxClientAheadOfHostFrames, maxHostAheadOfClientFrames, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
        result.report["startHostFrame"] = startHostFrame;
        result.report["startClientFrame"] = startClientFrame;
        result.report["targetHostFrame"] = targetHostFrame;
        result.report["targetClientFrame"] = targetClientFrame;
        result.report["desyncInjected"] = desyncInjected;
        result.report["hardResyncObserved"] = hardResyncObserved;
        result.report["reconnectTriggered"] = reconnectTriggered;
        result.report["hostDisconnectTriggered"] = hostDisconnectTriggered;
        result.report["postResyncConsecutiveMismatchCount"] = postResyncConsecutiveMismatchCount;
        result.exitCode = RESULT_FAILED;
        cleanup();
        return result;
    }

    static RunArtifacts runSingleCaseRuntimeFlow(const Options& options)
    {
        return options.singleThreadRuntimeFlow
            ? runSingleCaseRuntimeFlowImpl<SingleThreadRuntimePeerState>(options)
            : runSingleCaseRuntimeFlowImpl<RuntimePeerState>(options);
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
        hostPeer.coordinator.setGameplayReceiveDelayMs(options.gameplayReceiveDelayMs);
        clientPeer.coordinator.setGameplayReceiveDelayMs(options.gameplayReceiveDelayMs);

        if(options.preSessionWarmupFrames > 0) {
            const bool warmed = hostPeer.emu.withExclusiveAccess([&](auto& innerEmu) {
                for(uint32_t frame = 1; frame <= options.preSessionWarmupFrames + 2u; ++frame) {
                    innerEmu.queueInputFrame(innerEmu.createInputFrame(frame));
                }
                for(uint32_t step = 0; step < options.preSessionWarmupFrames; ++step) {
                    innerEmu.updateUntilFrame(16);
                }
                return innerEmu.frameCount() >= options.preSessionWarmupFrames;
            });
            if(!warmed) {
                failureReason = "Failed to warm up host emulation before session start.";
                return false;
            }
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
            pumpCoordinators(hostPeer, clientPeer, 1);
            const bool connected =
                hostPeer.coordinator.isConnected() &&
                clientPeer.coordinator.isConnected() &&
                hostPeer.coordinator.localParticipantId() != ConsoleNetplay::kInvalidParticipantId &&
                clientPeer.coordinator.localParticipantId() != ConsoleNetplay::kInvalidParticipantId &&
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

        const ConsoleNetplay::ParticipantId hostId = hostPeer.coordinator.localParticipantId();
        const ConsoleNetplay::ParticipantId clientId = clientPeer.coordinator.localParticipantId();
        hostPeer.coordinator.setInputDelayFrames(static_cast<uint8_t>(options.inputDelayFrames));
        hostPeer.driver.setPrebufferFrames(options.inputDelayFrames);
        clientPeer.driver.setPrebufferFrames(options.inputDelayFrames);

        for(uint32_t i = 0; i < options.startupTimeoutSteps; ++i) {
            const bool hostAssigned = hostPeer.coordinator.assignController(hostId, 0);
            const bool clientAssigned = hostPeer.coordinator.assignController(clientId, 1);
            pumpCoordinators(hostPeer, clientPeer, 1);
            const auto hostSlot = localAssignedSlot(hostPeer.coordinator);
            const auto clientSlot = localAssignedSlot(clientPeer.coordinator);
            if(hostAssigned &&
               clientAssigned &&
               hostSlot == std::optional<ConsoleNetplay::PlayerSlot>(0) &&
               clientSlot == std::optional<ConsoleNetplay::PlayerSlot>(1)) {
                hostPeer.coordinator.setLocalSimulationFrame(hostPeer.emu.exactEmulationFrame());
                clientPeer.coordinator.setLocalSimulationFrame(clientPeer.emu.exactEmulationFrame());
                if(!hostPeer.coordinator.startSession()) {
                    failureReason = "Host failed to start session.";
                    return false;
                }
                if(hostPeer.emu.exactEmulationFrame() > 0u && !beginAppInitialSessionSync(hostPeer)) {
                    failureReason = "Host failed to begin initial session sync.";
                    return false;
                }

                for(uint32_t startStep = 0; startStep < options.startupTimeoutSteps; ++startStep) {
                    pumpCoordinators(hostPeer, clientPeer, 1);
                    processAppHostResync(hostPeer);
                    processAppHostResync(clientPeer);
                    processAppPendingResync(hostPeer);
                    processAppPendingResync(clientPeer);
                    pumpCoordinators(hostPeer, clientPeer, 1);
                    if(hostPeer.coordinator.session().roomState().state == ConsoleNetplay::SessionState::Running &&
                       clientPeer.coordinator.session().roomState().state == ConsoleNetplay::SessionState::Running) {
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

    static void produceAppLocalInputs(AppPeerState& peer,
                                      const Options& options,
                                      uint32_t dtMs,
                                      bool swapped)
    {
        const std::optional<ConsoleNetplay::PlayerSlot> slot = localAssignedSlot(peer.coordinator);
        uint64_t localPrimaryMask = 0;
        if(slot.has_value()) {
            const uint32_t fps = std::max<uint32_t>(1u, peer.emu.getRegionFPS());
            const uint32_t sourceFrame = peer.driver.producedThroughFrame() + 1u;
            const Buttons buttons = runtimeButtonsForPeer(options, peer.generator, peer.host, swapped, sourceFrame, fps);
            localPrimaryMask = buildPadMask(buttons);
        }

        peer.driver.produceLocalBufferedInputs(
            peer.coordinator,
            peer.coordinator.isActive(),
            false,
            peer.coordinator.session().roomState().state,
            slot,
            dtMs,
            localPrimaryMask,
            peer.emu.getRegionFPS(),
            peer.emu.exactEmulationFrame(),
            peer.coordinator.latestConfirmedFrame()
        );
    }

    static void processAppPendingResync(AppPeerState& peer)
    {
        std::optional<ConsoleNetplay::NetplayCoordinator::PendingResyncApply> pending = peer.coordinator.consumePendingResyncApply();
        if(!pending.has_value()) return;

        const bool loaded =
            !pending->payload.empty() &&
            peer.emu.loadStateFromMemory(pending->payload);
        const uint32_t loadedFrame = peer.emu.exactEmulationFrame();
        const bool loadedExpectedFrame =
            loaded &&
            (pending->targetFrame == 0u || loadedFrame == pending->targetFrame);
        const uint32_t loadedCrc32 = loadedExpectedFrame ? peer.emu.canonicalNetplayStateCrc32() : 0u;
        if(loadedExpectedFrame) {
            peer.emu.withExclusiveAccess([&](auto& emu) {
                emu.setInputTimelineEpoch(peer.coordinator.session().roomState().timelineEpoch);
            });
            peer.coordinator.setLocalSimulationFrame(pending->targetFrame);
            peer.emu.seedNetplaySnapshot(pending->targetFrame, pending->payload, loadedCrc32);
            peer.emu.setAuthoritativeFrameReadyState(
                pending->frameReadyFrame != 0u ? pending->frameReadyFrame : pending->targetFrame,
                pending->frameReadyCrc32 != 0u ? pending->frameReadyCrc32 : loadedCrc32
            );
        }
        peer.coordinator.acknowledgeResync(
            pending->resyncId,
            pending->targetFrame,
            loadedCrc32,
            loadedExpectedFrame
        );
    }

    static void processAppHostResync(AppPeerState& peer)
    {
        if(!peer.coordinator.isHosting()) return;

        std::optional<ConsoleNetplay::NetplayCoordinator::PendingHostResyncRequest> pending = peer.coordinator.consumePendingHostResyncFrame();
        if(!pending.has_value() || !peer.emu.valid()) return;

        const bool initialSessionSync =
            peer.coordinator.session().roomState().state == ConsoleNetplay::SessionState::Starting;
        const ConsoleNetplay::FrameNumber emuFrame = peer.emu.exactEmulationFrame();
        const ConsoleNetplay::FrameNumber requestedFrame =
            initialSessionSync
                ? emuFrame
                : pending->frame;
        const ConsoleNetplay::FrameNumber authoritativeFrame =
            std::min<ConsoleNetplay::FrameNumber>(requestedFrame, emuFrame);

        const std::optional<std::shared_ptr<const std::vector<uint8_t>>> confirmedSnapshot =
            initialSessionSync ? std::nullopt : peer.emu.netplaySnapshotForFrame(authoritativeFrame);
        const std::vector<uint8_t> statePayload =
            initialSessionSync
                ? peer.emu.saveNetplayStateToMemory()
                : (confirmedSnapshot.has_value() ? **confirmedSnapshot : peer.emu.saveNetplayStateToMemory());
        if(statePayload.empty()) return;

        const uint32_t payloadCrc32 =
            Crc32::calc(reinterpret_cast<const char*>(statePayload.data()), statePayload.size());
        const uint32_t stateCrc32 =
            (!initialSessionSync && confirmedSnapshot.has_value())
                ? peer.emu.netplaySnapshotCrc32ForFrame(authoritativeFrame).value_or(0u)
                : peer.emu.canonicalNetplayStateCrc32();
        if(peer.coordinator.beginResync(
               authoritativeFrame,
               statePayload,
               payloadCrc32,
               stateCrc32,
               pending->reason,
               pending->participantId
           ) &&
           stateCrc32 != 0u) {
            peer.emu.setAuthoritativeFrameReadyState(authoritativeFrame, stateCrc32);
        }
    }

    static void refreshAppPeerPlayback(AppPeerState& peer)
    {
        processAppHostResync(peer);
        processAppPendingResync(peer);
        peer.driver.preparePlaybackFramesForEmulationThread(
            peer.coordinator,
            peer.coordinator.isActive(),
            false,
            peer.coordinator.session().roomState().state,
            peer.emu.frameCount()
        );
    }

    static bool appPeerHasImmediateNextFrameInput(AppPeerState& peer, uint32_t frame)
    {
        bool hasNextFrameInput = false;
        peer.emu.withExclusiveAccess([&](auto& innerEmu) {
            hasNextFrameInput =
                innerEmu.inputBuffer().findByFrame(frame + 1u, innerEmu.inputTimelineEpoch()) != nullptr;
        });
        return hasNextFrameInput;
    }

    static bool advanceAppPeerExactlyOneFrame(AppPeerState& peer)
    {
        if(!peer.emu.valid()) {
            return false;
        }

        const uint32_t previousFrame = peer.emu.frameCount();
        peer.emu.setSimulationSuspended(false);
        peer.emu.updateUntilFrame(1u);

        for(uint32_t guard = 0; guard < 256u; ++guard) {
            if(peer.emu.frameCount() > previousFrame) {
                return peer.emu.frameCount() == previousFrame + 1u;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return false;
    }

    static void settleAppFrameReadyState(AppPeerState& hostPeer, AppPeerState& clientPeer, uint32_t maxIterations = 256u)
    {
        uint32_t stableCount = 0u;
        uint32_t lastHostReadyFrame = 0u;
        uint32_t lastClientReadyFrame = 0u;
        uint32_t lastHostReadyCrc = 0u;
        uint32_t lastClientReadyCrc = 0u;
        bool lastHostCanAdvance = false;
        bool lastClientCanAdvance = false;

        for(uint32_t i = 0; i < maxIterations; ++i) {
            pumpCoordinators(hostPeer, clientPeer, 1);
            refreshAppPeerPlayback(hostPeer);
            refreshAppPeerPlayback(clientPeer);

            const uint32_t hostFrame = hostPeer.emu.frameCount();
            const uint32_t clientFrame = clientPeer.emu.frameCount();
            const bool hostCanAdvance = appPeerHasImmediateNextFrameInput(hostPeer, hostFrame);
            const bool clientCanAdvance = appPeerHasImmediateNextFrameInput(clientPeer, clientFrame);

            if(hostCanAdvance && clientCanAdvance) {
                if(!advanceAppPeerExactlyOneFrame(hostPeer) || !advanceAppPeerExactlyOneFrame(clientPeer)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
            } else {
                hostPeer.emu.setSimulationSuspended(true);
                clientPeer.emu.setSimulationSuspended(true);
            }

            const uint32_t hostReadyFrame = hostPeer.emu.lastFrameReadyFrame();
            const uint32_t clientReadyFrame = clientPeer.emu.lastFrameReadyFrame();
            const uint32_t hostReadyCrc = hostPeer.emu.lastFrameReadyNetplayCrc32();
            const uint32_t clientReadyCrc = clientPeer.emu.lastFrameReadyNetplayCrc32();

            if(hostReadyFrame == clientReadyFrame &&
               hostReadyCrc == clientReadyCrc &&
               !hostCanAdvance &&
               !clientCanAdvance) {
                ++stableCount;
                if(stableCount >= 2u) {
                    return;
                }
            } else {
                stableCount = 0u;
            }

            const bool unchanged =
                hostReadyFrame == lastHostReadyFrame &&
                clientReadyFrame == lastClientReadyFrame &&
                hostReadyCrc == lastHostReadyCrc &&
                clientReadyCrc == lastClientReadyCrc &&
                hostCanAdvance == lastHostCanAdvance &&
                clientCanAdvance == lastClientCanAdvance;
            lastHostReadyFrame = hostReadyFrame;
            lastClientReadyFrame = clientReadyFrame;
            lastHostReadyCrc = hostReadyCrc;
            lastClientReadyCrc = clientReadyCrc;
            lastHostCanAdvance = hostCanAdvance;
            lastClientCanAdvance = clientCanAdvance;

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            if(unchanged && !hostCanAdvance && !clientCanAdvance && i > 8u) {
                return;
            }
        }
    }

    static bool beginAppInitialSessionSync(AppPeerState& peer)
    {
        if(!peer.coordinator.isHosting()) return false;
        if(!peer.emu.valid()) return false;
        if(peer.coordinator.session().roomState().state != ConsoleNetplay::SessionState::Starting) return false;

        const ConsoleNetplay::FrameNumber authoritativeFrame = peer.emu.exactEmulationFrame();
        const std::vector<uint8_t> statePayload = peer.emu.saveNetplayStateToMemory();
        if(statePayload.empty()) return false;

        const uint32_t payloadCrc32 =
            Crc32::calc(reinterpret_cast<const char*>(statePayload.data()), statePayload.size());
        const uint32_t stateCrc32 = peer.emu.canonicalNetplayStateCrc32();
        const bool started = peer.coordinator.beginResync(authoritativeFrame, statePayload, payloadCrc32, stateCrc32);
        if(started) {
            peer.emu.setAuthoritativeFrameReadyState(authoritativeFrame, stateCrc32);
        }
        return started;
    }

    static RunArtifacts runSingleCaseAppFlow(const Options& options)
    {
        RunArtifacts result;
        AppPeerState hostPeer("Host", true, options.hostInputSeed);
        AppPeerState clientPeer("Client", false, options.clientInputSeed);
        uint32_t lastCheckedFrame = 0;
        uint32_t stallSteps = 0;
        uint32_t maxStallSteps = 0;
        uint32_t maxSimulationFrameSeen = 0;
        bool assignmentSwapTriggered = false;
        bool assignmentSwapVerified = false;
        bool assignmentPatternVerified = !options.assignmentPatternCheck;

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

        const uint32_t startHostFrame = hostPeer.emu.exactEmulationFrame();
        const uint32_t startClientFrame = clientPeer.emu.exactEmulationFrame();
        const uint32_t targetHostFrame = startHostFrame + options.frames;
        const uint32_t targetClientFrame = startClientFrame + options.frames;

        const uint32_t hostLoopDtMs = std::max<uint32_t>(1u, options.hostLoopDtMs);
        const uint32_t clientLoopDtMs = std::max<uint32_t>(1u, options.clientLoopDtMs);
        const uint32_t hostStepStride = std::max<uint32_t>(1u, options.hostStepStride);
        const uint32_t clientStepStride = std::max<uint32_t>(1u, options.clientStepStride);
        auto lastLoopTime = std::chrono::steady_clock::now();
        const bool useSyntheticLoopDt = true;
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
            uint32_t hostStepDtMs = hostLoopDtMs;
            uint32_t clientStepDtMs = clientLoopDtMs;
            if(!useSyntheticLoopDt) {
                const auto now = std::chrono::steady_clock::now();
                const uint32_t observedDtMs = std::max<uint32_t>(
                    1u,
                    static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLoopTime).count())
                );
                lastLoopTime = now;
                hostStepDtMs = observedDtMs;
                clientStepDtMs = observedDtMs;
            }

            hostPeer.coordinator.setLocalSimulationFrame(hostPeer.emu.exactEmulationFrame());
            clientPeer.coordinator.setLocalSimulationFrame(clientPeer.emu.exactEmulationFrame());

            produceAppLocalInputs(hostPeer, options, hostStepDtMs, assignmentSwapVerified);
            produceAppLocalInputs(clientPeer, options, clientStepDtMs, assignmentSwapVerified);
            const uint32_t simulationFrame = std::min(hostPeer.emu.exactEmulationFrame(), clientPeer.emu.exactEmulationFrame());
            maxSimulationFrameSeen = std::max(maxSimulationFrameSeen, simulationFrame);
            const bool withinNetworkHoldWindow = false;
            const bool scheduledPumpNetwork =
                options.networkPumpStride <= 1u ||
                (step % options.networkPumpStride) == 0u;
            const bool shouldPumpNetwork = !withinNetworkHoldWindow && scheduledPumpNetwork;
            if(shouldPumpNetwork) {
                pumpCoordinators(hostPeer, clientPeer, 0);
            }
            processAppHostResync(hostPeer);
            processAppHostResync(clientPeer);
            processAppPendingResync(hostPeer);
            processAppPendingResync(clientPeer);
            if(shouldPumpNetwork) {
                pumpCoordinators(hostPeer, clientPeer, 0);
            }            hostPeer.driver.preparePlaybackFramesForEmulationThread(
                hostPeer.coordinator,
                hostPeer.coordinator.isActive(),
                false,
                hostPeer.coordinator.session().roomState().state,
                hostPeer.emu.exactEmulationFrame()
            );
            clientPeer.driver.preparePlaybackFramesForEmulationThread(
                clientPeer.coordinator,
                clientPeer.coordinator.isActive(),
                false,
                clientPeer.coordinator.session().roomState().state,
                clientPeer.emu.exactEmulationFrame()
            );

            uint32_t hostFrame = hostPeer.emu.exactEmulationFrame();
            uint32_t clientFrame = clientPeer.emu.exactEmulationFrame();
            bool hostCanAdvance = hostFrame + 1u <= hostPeer.driver.queuedThroughFrame();
            bool clientCanAdvance = clientFrame + 1u <= clientPeer.driver.queuedThroughFrame();

            if(withinNetworkHoldWindow && !hostCanAdvance && !clientCanAdvance) {
                pumpCoordinators(hostPeer, clientPeer, 0);
                processAppHostResync(hostPeer);
                processAppHostResync(clientPeer);
                processAppPendingResync(hostPeer);
                processAppPendingResync(clientPeer);
                hostPeer.driver.preparePlaybackFramesForEmulationThread(
                    hostPeer.coordinator,
                    hostPeer.coordinator.isActive(),
                    false,
                    hostPeer.coordinator.session().roomState().state,
                    hostPeer.emu.exactEmulationFrame()
                );
                clientPeer.driver.preparePlaybackFramesForEmulationThread(
                    clientPeer.coordinator,
                    clientPeer.coordinator.isActive(),
                    false,
                    clientPeer.coordinator.session().roomState().state,
                    clientPeer.emu.exactEmulationFrame()
                );

                hostFrame = hostPeer.emu.exactEmulationFrame();
                clientFrame = clientPeer.emu.exactEmulationFrame();
                hostCanAdvance = hostFrame + 1u <= hostPeer.driver.queuedThroughFrame();
                clientCanAdvance = clientFrame + 1u <= clientPeer.driver.queuedThroughFrame();
            }

            hostPeer.emu.setSimulationSuspended(!hostCanAdvance);
            clientPeer.emu.setSimulationSuspended(!clientCanAdvance);
            if(hostCanAdvance && (step % hostStepStride) == 0u) hostPeer.emu.update(hostStepDtMs);
            if(clientCanAdvance && (step % clientStepStride) == 0u) clientPeer.emu.update(clientStepDtMs);

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
                    if(emu.inputBuffer().findByFrame(probe, emu.inputTimelineEpoch()) == nullptr) {
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
                    if(emu.inputBuffer().findByFrame(probe, emu.inputTimelineEpoch()) == nullptr) {
                        break;
                    }
                    ++futureBufferedFrames;
                }
                clientPeer.maxObservedFutureBufferedFrames = std::max(clientPeer.maxObservedFutureBufferedFrames, futureBufferedFrames);
            });
            hostPeer.maxObservedPendingFrameCount = std::max(hostPeer.maxObservedPendingFrameCount, hostPeer.driver.pendingFrameCount());
            clientPeer.maxObservedPendingFrameCount = std::max(clientPeer.maxObservedPendingFrameCount, clientPeer.driver.pendingFrameCount());

            if(newHostFrame == hostFrame && newClientFrame == clientFrame) {
                ++stallSteps;
            } else {
                stallSteps = 0;
            }
            maxStallSteps = std::max(maxStallSteps, stallSteps);

            if(hostPeer.maxObservedFutureBufferedFrames > options.inputDelayFrames + 16u ||
               clientPeer.maxObservedFutureBufferedFrames > options.inputDelayFrames + 16u) {
                failureReason = "Future InputBuffer horizon grew beyond the expected bound under app flow.";
                result.report = buildAppReport(options, hostPeer, clientPeer, "buffer_overfill", failureReason, lastCheckedFrame, maxStallSteps, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                result.exitCode = RESULT_FAILED;
                cleanup();
                return result;
            }

            if(hostPeer.maxObservedPendingFrameCount > options.inputDelayFrames + 64u ||
               clientPeer.maxObservedPendingFrameCount > options.inputDelayFrames + 64u) {
                failureReason = "Pending confirmed frame queue grew beyond the expected bound under app flow.";
                result.report = buildAppReport(options, hostPeer, clientPeer, "pending_overfill", failureReason, lastCheckedFrame, maxStallSteps, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                result.exitCode = RESULT_FAILED;
                cleanup();
                return result;
            }

            if(stallSteps > 240u) {
                failureReason = "App flow stalled while confirmed frames existed.";
                result.report = buildAppReport(options, hostPeer, clientPeer, "stalled", failureReason, lastCheckedFrame, maxStallSteps, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                result.exitCode = RESULT_FAILED;
                cleanup();
                return result;
            }

            if(options.assignmentSwapAfterFrames > 0 &&
               !assignmentSwapTriggered &&
               newHostFrame >= startHostFrame + options.assignmentSwapAfterFrames &&
               newClientFrame >= startClientFrame + options.assignmentSwapAfterFrames) {
                const auto clientId = findParticipantIdByName(hostPeer.coordinator.session().roomState(), clientPeer.name);
                if(clientId.has_value()) {
                    hostPeer.coordinator.assignController(hostPeer.coordinator.localParticipantId(), 1);
                    hostPeer.coordinator.assignController(*clientId, 0);
                    assignmentSwapTriggered = true;
                }
            }

            if(assignmentSwapTriggered && !assignmentSwapVerified) {
                const auto& hostRoom = hostPeer.coordinator.session().roomState();
                const auto& clientRoom = clientPeer.coordinator.session().roomState();
                const auto* hostParticipant = findParticipantInRoom(hostRoom, hostPeer.coordinator.localParticipantId());
                const auto* clientParticipantHostView = findParticipantInRoom(hostRoom, clientPeer.coordinator.localParticipantId());
                const auto* clientLocalParticipant = findParticipantInRoom(clientRoom, clientPeer.coordinator.localParticipantId());
                assignmentSwapVerified =
                    hostParticipant != nullptr &&
                    clientParticipantHostView != nullptr &&
                    clientLocalParticipant != nullptr &&
                    hostParticipant->controllerAssignment == GeraNESNetplay::kPort2PlayerSlot &&
                    clientParticipantHostView->controllerAssignment == GeraNESNetplay::kPort1PlayerSlot &&
                    clientLocalParticipant->controllerAssignment == GeraNESNetplay::kPort1PlayerSlot &&
                    hostRoom.state == ConsoleNetplay::SessionState::Running &&
                    clientRoom.state == ConsoleNetplay::SessionState::Running &&
                    hostRoom.activeResyncId == 0 &&
                    clientRoom.activeResyncId == 0;
            }

            if(options.assignmentPatternCheck && assignmentSwapVerified && !assignmentPatternVerified) {
                const uint32_t probeStartFrame = std::min(hostPeer.driver.queuedThroughFrame(), clientPeer.driver.queuedThroughFrame());
                const uint32_t probeEndFrame = probeStartFrame + 16u;
                assignmentPatternVerified =
                    peerHasBufferedPattern(hostPeer,
                                           hostPeer.generator,
                                           clientPeer.generator,
                                           probeStartFrame,
                                           probeEndFrame,
                                           GeraNESNetplay::kPort2PlayerSlot,
                                           GeraNESNetplay::kPort1PlayerSlot,
                                           options,
                                           true) &&
                    peerHasBufferedPattern(clientPeer,
                                           hostPeer.generator,
                                           clientPeer.generator,
                                           probeStartFrame,
                                           probeEndFrame,
                                           GeraNESNetplay::kPort2PlayerSlot,
                                           GeraNESNetplay::kPort1PlayerSlot,
                                           options,
                                           true);
            }

            if(options.assignmentPatternCheck && assignmentSwapVerified && assignmentPatternVerified) {
                settleAppFrameReadyState(hostPeer, clientPeer);
                freezePeersForInspection();
                result.report = buildAppReport(options, hostPeer, clientPeer, "ok", "", lastCheckedFrame, maxStallSteps, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                result.report["startHostFrame"] = startHostFrame;
                result.report["startClientFrame"] = startClientFrame;
                result.report["targetHostFrame"] = targetHostFrame;
                result.report["targetClientFrame"] = targetClientFrame;
                result.exitCode = EXIT_SUCCESS;
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
                    result.report = buildAppReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }
            }

            if(newHostFrame >= targetHostFrame && newClientFrame >= targetClientFrame) {
                if(options.assignmentPatternCheck && !assignmentPatternVerified) {
                    failureReason = "Assignment swap completed, but the expected post-swap input pattern was not observed in queued netplay frames.";
                    result.report = buildAppReport(options, hostPeer, clientPeer, "failed", failureReason, lastCheckedFrame, maxStallSteps, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                    result.exitCode = RESULT_FAILED;
                    cleanup();
                    return result;
                }

                settleAppFrameReadyState(hostPeer, clientPeer);
                freezePeersForInspection();
                result.report = buildAppReport(options, hostPeer, clientPeer, "ok", "", lastCheckedFrame, maxStallSteps, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
                result.report["startHostFrame"] = startHostFrame;
                result.report["startClientFrame"] = startClientFrame;
                result.report["targetHostFrame"] = targetHostFrame;
                result.report["targetClientFrame"] = targetClientFrame;
                result.exitCode = EXIT_SUCCESS;
                cleanup();
                return result;
            }
        }

        failureReason = "App-flow netplay test reached the step limit.";
        result.report = buildAppReport(options, hostPeer, clientPeer, "stalled", failureReason, lastCheckedFrame, maxStallSteps, assignmentSwapTriggered, assignmentSwapVerified, assignmentPatternVerified);
        result.report["startHostFrame"] = startHostFrame;
        result.report["startClientFrame"] = startClientFrame;
        result.report["targetHostFrame"] = targetHostFrame;
        result.report["targetClientFrame"] = targetClientFrame;
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

    static RunArtifacts runAutoSettingsProbe(const Options& /*options*/)
    {
        RunArtifacts result;
        ConsoleNetplay::NetplayAutoTune autoSettings;
        autoSettings.setEnabled(true);

        auto makeParticipant = [](ConsoleNetplay::ParticipantId id,
                                  ConsoleNetplay::PlayerSlot slot,
                                  uint16_t jitterMs) {
            ConsoleNetplay::ParticipantInfo participant;
            participant.id = id;
            participant.connected = true;
            participant.romLoaded = true;
            participant.romCompatible = true;
            participant.controllerAssignment = slot;
            participant.jitterMs = jitterMs;
            return participant;
        };

        ConsoleNetplay::RoomState room;
        room.sessionId = 1;
        room.timelineEpoch = 1;
        room.inputDelayFrames = 0;
        room.participants = {
            makeParticipant(0, 0, 0),
            makeParticipant(1, 1, 20)
        };

        ConsoleNetplay::NetplayRecoveryStats stats;
        constexpr uint32_t fps = 60;
        nlohmann::json scenarios = nlohmann::json::array();

        auto appendScenario = [&](const std::string& name,
                                  const ConsoleNetplay::NetplayAutoTune::Recommendations& recommendations,
                                  const ConsoleNetplay::NetplayAutoTune::Snapshot& snapshot) {
            scenarios.push_back({
                {"scenario", name},
                {"recommendedDelay", recommendations.inputDelayFrames.has_value()
                    ? nlohmann::json(*recommendations.inputDelayFrames)
                    : nlohmann::json(nullptr)},
                {"currentDelay", snapshot.currentRecommendedDelay},
                {"stableFrameCount", snapshot.stableFrameCount},
                {"lastAdjustmentFrame", snapshot.lastAdjustmentFrame},
                {"lastDecisionReason", snapshot.lastDecisionReason}
            });
        };

        room.state = ConsoleNetplay::SessionState::ReadyCheck;
        auto preSession = autoSettings.update(room, stats, fps);
        appendScenario("pre_session_jitter_bootstrap", preSession, autoSettings.snapshot());
        if(!preSession.inputDelayFrames.has_value() || *preSession.inputDelayFrames != 3u) {
            result.exitCode = RESULT_FAILED;
            result.report = {
                {"status", "failed"},
                {"failureReason", "Auto settings did not derive expected pre-session delay from jitter."},
                {"scenarios", scenarios}
            };
            return result;
        }

        room.inputDelayFrames = *preSession.inputDelayFrames;
        room.state = ConsoleNetplay::SessionState::Running;
        room.currentFrame = 100;
        auto warmup = autoSettings.update(room, stats, fps);
        appendScenario("running_window_init", warmup, autoSettings.snapshot());
        if(warmup.inputDelayFrames.has_value()) {
            result.exitCode = RESULT_FAILED;
            result.report = {
                {"status", "failed"},
                {"failureReason", "Auto settings should not change parameters immediately on first running window."},
                {"scenarios", scenarios}
            };
            return result;
        }

        room.currentFrame = 220;
        stats.playbackStopCount = 1;
        auto increase = autoSettings.update(room, stats, fps);
        appendScenario("increase_after_stop", increase, autoSettings.snapshot());
        if(!increase.inputDelayFrames.has_value() || *increase.inputDelayFrames != 4u) {
            result.exitCode = RESULT_FAILED;
            result.report = {
                {"status", "failed"},
                {"failureReason", "Auto settings did not increase delay after playback stop."},
                {"scenarios", scenarios}
            };
            return result;
        }
        room.inputDelayFrames = *increase.inputDelayFrames;
        stats.playbackStopCount = 1;
        for(uint32_t frame : {340u, 460u, 580u, 700u}) {
            room.currentFrame = frame;
            auto hold = autoSettings.update(room, stats, fps);
            appendScenario("stable_hold_" + std::to_string(frame), hold, autoSettings.snapshot());
            if(hold.inputDelayFrames.has_value()) {
                result.exitCode = RESULT_FAILED;
                result.report = {
                    {"status", "failed"},
                    {"failureReason", "Auto settings reduced delay too early before enough stability."},
                    {"scenarios", scenarios}
                };
                return result;
            }
        }

        room.currentFrame = 820;
        auto decrease = autoSettings.update(room, stats, fps);
        appendScenario("decrease_after_stability", decrease, autoSettings.snapshot());
        if(!decrease.inputDelayFrames.has_value() || *decrease.inputDelayFrames != 3u) {
            result.exitCode = RESULT_FAILED;
            result.report = {
                {"status", "failed"},
                {"failureReason", "Auto settings did not reduce delay after sustained stability."},
                {"scenarios", scenarios}
            };
            return result;
        }

        result.exitCode = EXIT_SUCCESS;
        result.report = {
            {"status", "ok"},
            {"probe", "netplay_auto_settings"},
            {"scenarios", scenarios}
        };
        return result;
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
                hostPeer.emu.inputBuffer().findByFrame(hostPeer.emu.frameCount() + 1u, hostPeer.emu.inputTimelineEpoch()) != nullptr;
            const bool clientCanAdvance =
                clientPeer.emu.frameCount() < options.frames &&
                clientPeer.emu.inputBuffer().findByFrame(clientPeer.emu.frameCount() + 1u, clientPeer.emu.inputTimelineEpoch()) != nullptr;

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

        if(!baseOptions.appFlow || baseOptions.baselineLockstep) {
            Options delay1 = baseOptions;
            delay1.inputDelayFrames = 1;
            addScenario("delay_1", delay1);
        }

        Options delay4 = baseOptions;
        delay4.inputDelayFrames = 4;
        addScenario("delay_4", delay4);

        Options delay10 = baseOptions;
        delay10.inputDelayFrames = 10;
        addScenario("delay_10", delay10);

        if(baseOptions.appFlow && !baseOptions.baselineLockstep) {
            if(baseOptions.runtimeFlow) {
                Options reconnect = baseOptions;
                reconnect.frames = std::max<uint32_t>(baseOptions.frames, 120u);
                reconnect.reconnectAfterFrames = 24u;
                reconnect.inputDelayFrames = std::max<uint32_t>(1u, baseOptions.inputDelayFrames);
                addScenario("reconnect_mid_session", reconnect);

                Options forcedDesync = baseOptions;
                forcedDesync.frames = std::max<uint32_t>(baseOptions.frames, 120u);
                forcedDesync.forceDesyncFrame = 28u;
                forcedDesync.inputDelayFrames = std::max<uint32_t>(1u, baseOptions.inputDelayFrames);
                forcedDesync.desyncAddress = 0x0000u;
                forcedDesync.desyncValueXor = 0x5Au;
                addScenario("forced_desync_resync", forcedDesync);

                Options asymmetricReconnect = reconnect;
                asymmetricReconnect.frames = std::max<uint32_t>(baseOptions.frames, 140u);
                asymmetricReconnect.reconnectAfterFrames = 32u;
                asymmetricReconnect.networkPumpStride = std::max<uint32_t>(2u, baseOptions.networkPumpStride);
                asymmetricReconnect.hostLoopDtMs = 8u;
                asymmetricReconnect.clientLoopDtMs = 33u;
                asymmetricReconnect.hostStepStride = 1u;
                asymmetricReconnect.clientStepStride = 2u;
                addScenario("reconnect_asymmetric_pacing", asymmetricReconnect);

                Options resetThenResync = baseOptions;
                resetThenResync.frames = std::max<uint32_t>(baseOptions.frames, 160u);
                resetThenResync.inputDelayFrames = std::max<uint32_t>(1u, baseOptions.inputDelayFrames);
                resetThenResync.networkPumpStride = std::max<uint32_t>(2u, baseOptions.networkPumpStride);
                resetThenResync.hostLoopDtMs = 8u;
                resetThenResync.clientLoopDtMs = 33u;
                resetThenResync.hostStepStride = 1u;
                resetThenResync.clientStepStride = 2u;
                resetThenResync.forceHostResetFrame = 36u;
                resetThenResync.forceManualResyncFrame = 44u;
                addScenario("reset_then_manual_resync_asymmetric", resetThenResync);

                Options extremeJitter = baseOptions;
                extremeJitter.frames = std::max<uint32_t>(baseOptions.frames, 140u);
                extremeJitter.inputDelayFrames = std::max<uint32_t>(2u, baseOptions.inputDelayFrames);
                extremeJitter.gameplayReceiveDelayMs = std::max<uint32_t>(30u, baseOptions.gameplayReceiveDelayMs);
                extremeJitter.networkPumpStride = std::max<uint32_t>(5u, baseOptions.networkPumpStride);
                extremeJitter.hostLoopDtMs = 7u;
                extremeJitter.clientLoopDtMs = 41u;
                extremeJitter.hostStepStride = 1u;
                extremeJitter.clientStepStride = 3u;
                addScenario("extreme_jitter_asymmetric", extremeJitter);

                Options burstLoss = baseOptions;
                burstLoss.frames = std::max<uint32_t>(baseOptions.frames, 170u);
                burstLoss.inputDelayFrames = std::max<uint32_t>(1u, baseOptions.inputDelayFrames);
                burstLoss.hostLoopDtMs = 8u;
                burstLoss.clientLoopDtMs = 33u;
                burstLoss.hostStepStride = 1u;
                burstLoss.clientStepStride = 2u;
                addScenario("burst_loss_asymmetric", burstLoss);

                Options jitterDesync = extremeJitter;
                jitterDesync.frames = std::max<uint32_t>(baseOptions.frames, 160u);
                jitterDesync.forceDesyncFrame = 52u;
                jitterDesync.desyncAddress = 0x0000u;
                jitterDesync.desyncValueXor = 0x5Au;
                addScenario("forced_desync_extreme_jitter", jitterDesync);

                Options reconnectDuringResync = baseOptions;
                reconnectDuringResync.frames = std::max<uint32_t>(baseOptions.frames, 180u);
                reconnectDuringResync.inputDelayFrames = std::max<uint32_t>(1u, baseOptions.inputDelayFrames);
                reconnectDuringResync.networkPumpStride = std::max<uint32_t>(2u, baseOptions.networkPumpStride);
                reconnectDuringResync.hostLoopDtMs = 8u;
                reconnectDuringResync.clientLoopDtMs = 33u;
                reconnectDuringResync.hostStepStride = 1u;
                reconnectDuringResync.clientStepStride = 2u;
                reconnectDuringResync.forceManualResyncFrame = 42u;
                reconnectDuringResync.reconnectDuringResync = true;
                addScenario("reconnect_during_resync_asymmetric", reconnectDuringResync);

                // Keep packet-loss resync behavior covered by the dedicated test case.
                // The robust matrix stays focused on deterministic always-green scenarios.

                Options reconnectExpiry = baseOptions;
                reconnectExpiry.frames = std::max<uint32_t>(baseOptions.frames, 120u);
                reconnectExpiry.inputDelayFrames = std::max<uint32_t>(1u, baseOptions.inputDelayFrames);
                reconnectExpiry.reconnectAfterFrames = 28u;
                reconnectExpiry.reconnectReservationSecondsForTests = 1u;
                reconnectExpiry.expectReconnectReservationExpiry = true;
                addScenario("reconnect_reservation_expiry", reconnectExpiry);

                Options repeatedLoadState = baseOptions;
                repeatedLoadState.frames = std::max<uint32_t>(baseOptions.frames, 190u);
                repeatedLoadState.inputDelayFrames = std::max<uint32_t>(1u, baseOptions.inputDelayFrames);
                repeatedLoadState.networkPumpStride = std::max<uint32_t>(2u, baseOptions.networkPumpStride);
                repeatedLoadState.hostLoopDtMs = 8u;
                repeatedLoadState.clientLoopDtMs = 33u;
                repeatedLoadState.hostStepStride = 1u;
                repeatedLoadState.clientStepStride = 2u;
                repeatedLoadState.hostSaveStateFrame = 20u;
                repeatedLoadState.hostManualLoadStateFrames = {36u, 37u, 38u};
                addScenario("repeated_host_load_state_asymmetric", repeatedLoadState);

                Options loadStateDuringResync = repeatedLoadState;
                loadStateDuringResync.frames = std::max<uint32_t>(baseOptions.frames, 180u);
                loadStateDuringResync.hostManualLoadStateFrames = {45u};
                loadStateDuringResync.forceManualResyncFrame = 44u;
                loadStateDuringResync.requireHostManualLoadDuringResync = true;
                addScenario("host_load_state_during_resync", loadStateDuringResync);

                Options hostResetBootstrap = baseOptions;
                hostResetBootstrap.frames = std::max<uint32_t>(baseOptions.frames, 170u);
                hostResetBootstrap.inputDelayFrames = std::max<uint32_t>(1u, baseOptions.inputDelayFrames);
                hostResetBootstrap.networkPumpStride = std::max<uint32_t>(2u, baseOptions.networkPumpStride);
                hostResetBootstrap.hostLoopDtMs = 8u;
                hostResetBootstrap.clientLoopDtMs = 33u;
                hostResetBootstrap.hostStepStride = 1u;
                hostResetBootstrap.clientStepStride = 2u;
                hostResetBootstrap.forceHostResetFrame = 36u;
                addScenario("host_reset_authoritative_bootstrap", hostResetBootstrap);
            }

        }

        Options seedMix = baseOptions;
        seedMix.hostInputSeed = 0x00000001u;
        seedMix.clientInputSeed = 0xDEADBEEFu;
        addScenario("seed_mix", seedMix);

        if(baseOptions.appFlow && !baseOptions.baselineLockstep) {
            Options lockstep = baseOptions;
            lockstep.baselineLockstep = true;
            addScenario("baseline_lockstep", lockstep);
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
                << " pumpStride=" << scenario.options.networkPumpStride
                << " hostSeed=" << scenario.options.hostInputSeed
                << " clientSeed=" << scenario.options.clientInputSeed
                << (scenario.options.baselineLockstep ? " lockstep" : "")
                << std::endl;

            RunArtifacts caseResult = scenario.options.appFlow
                ? (scenario.options.runtimeFlow ? runSingleCaseRuntimeFlow(scenario.options)
                                                : runSingleCaseAppFlow(scenario.options))
                : runSingleCase(scenario.options);
            caseResult.report["scenario"] = scenario.name;
            aggregate.report["cases"].push_back(caseResult.report);
            aggregate.report["summary"].push_back({
                {"scenario", scenario.name},
                {"status", caseResult.report.value("status", std::string("unknown"))},
                {"mode", scenario.options.appFlow ? "app-flow" : "direct-core"},
                {"frames", scenario.options.frames},
                {"inputDelayFrames", scenario.options.inputDelayFrames},
                {"gameplayReceiveDelayMs", scenario.options.gameplayReceiveDelayMs},
                {"networkPumpStride", scenario.options.networkPumpStride},
                {"reconnectDuringResync", scenario.options.reconnectDuringResync},
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

        nlohmann::json compatibilityChecks;
        std::string compatibilityFailureReason;
        if(!verifyEmulatorVersionJoinRejection(
               options,
               compatibilityChecks["emulatorVersionMismatchJoinRejected"],
               compatibilityFailureReason
           )) {
            RunArtifacts result;
            result.exitCode = RESULT_FAILED;
            result.report["status"] = "failed";
            result.report["failureReason"] = compatibilityFailureReason;
            result.report["compatibilityChecks"] = compatibilityChecks;
            writeReport(options, result.report);
            return result.exitCode;
        }

        RunArtifacts result = options.robust
            ? runRobustMatrix(options)
            : (options.autoSettingsProbe
                ? runAutoSettingsProbe(options)
                : (options.appFlow
                ? (options.runtimeFlow ? runSingleCaseRuntimeFlow(options) : runSingleCaseAppFlow(options))
                : runSingleCase(options)));
        result.report["compatibilityChecks"] = compatibilityChecks;
        writeReport(options, result.report);
        return result.exitCode;
    }
};

#endif







