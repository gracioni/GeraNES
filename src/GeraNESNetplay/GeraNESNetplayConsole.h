#pragma once

#include <optional>
#include <vector>

#include "ConsoleNetplay/INetplayConsole.h"
#include "ConsoleNetplay/NetplayAppRuntime.h"
#include "GeraNES/GeraNESEmu.h"
#include "GeraNESApp/IEmulationHost.h"

namespace GeraNESNetplay {

class GeraNESNetplayConsole final : public ConsoleNetplay::INetplayConsole
{
public:
    struct MenuSnapshot
    {
        bool hosting = false;
        bool inputManaged = false;
        ConsoleNetplay::NetTransportBackend transportBackend = ConsoleNetplay::defaultNetTransportBackend();
        std::vector<ConsoleNetplay::PlayerSlot> localAssignments;
        std::optional<Settings::Device> port1Device;
        std::optional<Settings::Device> port2Device;
        Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
        Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
        Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;
    };

    GeraNESNetplayConsole(IEmulationHost& host,
                          GeraNESEmu& emu,
                          const IEmulationHost::InputState& latestInputState);

    static void configureRuntimeForGeraNES(ConsoleNetplay::NetplayAppRuntime& runtime,
                                           IEmulationHost& host);
    static MenuSnapshot menuSnapshot(const ConsoleNetplay::NetplayAppRuntime& runtime);
    static void configureInputAssignments(ConsoleNetplay::NetplayAppRuntime& runtime,
                                          ConsoleNetplay::ParticipantId participantId,
                                          std::optional<Settings::Device> port1Device,
                                          std::optional<Settings::Device> port2Device,
                                          Settings::ExpansionDevice expansionDevice,
                                          Settings::NesMultitapDevice nesMultitapDevice,
                                          Settings::FamicomMultitapDevice famicomMultitapDevice,
                                          const std::vector<ConsoleNetplay::PlayerSlot>& slots);
    static void runRuntimeOnEmulationThread(ConsoleNetplay::NetplayAppRuntime& runtime,
                                            IEmulationHost& host,
                                            GeraNESEmu& emu,
                                            const IEmulationHost::InputState& latestInputState);
    static std::optional<ConsoleNetplay::NetplayRomSelection> captureRomSelection(GeraNESEmu& emu);

    bool valid() const override;
    uint32_t frameCount() const override;
    uint32_t regionFps() const override;
    uint32_t canonicalNetplayStateCrc32() override;
    std::optional<ConsoleNetplay::NetplayRomSelection> currentRomSelection() const override;
    bool loadRollbackState(const std::vector<uint8_t>& data) override;
    bool updateUntilFrame(uint32_t frameDtMs, bool resimulating) override;

    void applyRemoteInputTopology(const ConsoleNetplay::RoomState& room) override;
    void publishCurrentInputTopology(ConsoleNetplay::NetplayCoordinator& coordinator) override;
    ConsoleNetplay::NetplayInputFrame buildLocalInputContribution(
        ConsoleNetplay::PlayerSlot slot,
        ConsoleNetplay::FrameNumber frame,
        const ConsoleNetplay::RoomState& room) const override;

    bool hasStableQueuedInputFrame(ConsoleNetplay::FrameNumber frame) const override;
    void queueStandaloneBootstrapInputFrame() override;
    bool queuePlaybackInputFrame(const ConsoleNetplay::NetplayCoordinator::ConfirmedFrameInputs& confirmed) override;
    void discardQueuedInputFramesAfter(ConsoleNetplay::FrameNumber frame) override;

    void configureInputTopology(ConsoleNetplay::NetplayCoordinator& coordinator,
                                std::optional<Settings::Device> port1Device,
                                std::optional<Settings::Device> port2Device,
                                Settings::ExpansionDevice expansionDevice,
                                Settings::NesMultitapDevice nesMultitapDevice,
                                Settings::FamicomMultitapDevice famicomMultitapDevice,
                                std::optional<ConsoleNetplay::ParticipantId> preservedParticipantId,
                                ConsoleNetplay::PlayerSlot preservedAssignment);
    static bool buildReplayFrameInput(const ConsoleNetplay::NetplayCoordinator::ConfirmedFrameInputs& confirmed,
                                      ConsoleNetplay::FrameNumber frame,
                                      IEmulationHost::ReplayFrameInput& outFrame);

private:
    IEmulationHost& m_host;
    GeraNESEmu& m_emu;
    const IEmulationHost::InputState& m_latestInputState;
};

} // namespace GeraNESNetplay
