#pragma once

#include "ConsoleNetplay/INetplayConsole.h"
#include "GeraNES/GeraNESEmu.h"
#include "GeraNESApp/IEmulationHost.h"

namespace GeraNESNetplay {

class GeraNESNetplayConsole final : public ConsoleNetplay::INetplayConsole
{
public:
    GeraNESNetplayConsole(IEmulationHost& host,
                          GeraNESEmu& emu,
                          const IEmulationHost::InputState& latestInputState);

    static std::optional<ConsoleNetplay::NetplayRomSelection> captureRomSelection(GeraNESEmu& emu);

    std::optional<ConsoleNetplay::NetplayRomSelection> currentRomSelection() const override;
    uint32_t frameCount() const override;
    uint32_t inputTimelineEpoch() const override;
    uint32_t regionFps() const override;

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
