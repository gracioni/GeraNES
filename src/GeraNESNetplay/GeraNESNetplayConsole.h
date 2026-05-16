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
    GeraNESNetplayConsole(IEmulationHost& host,
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

    static bool buildReplayFrameInput(const ConsoleNetplay::NetplayCoordinator::ConfirmedFrameInputs& confirmed,
                                      ConsoleNetplay::FrameNumber frame,
                                      IEmulationHost::ReplayFrameInput& outFrame);
    static bool buildReplayFrameInput(const ConsoleNetplay::NetplayCoordinator::ConfirmedFrameInputs& confirmed,
                                      ConsoleNetplay::FrameNumber frame,
                                      GeraNESEmu& emu,
                                      IEmulationHost::ReplayFrameInput& outFrame);

private:
    IEmulationHost& m_host;
    GeraNESEmu& m_emu;
    const IEmulationHost::InputState& m_latestInputState;
};

} // namespace GeraNESNetplay
