#include "GeraNESNetplay/GeraNESNetplayConsole.h"

#include <algorithm>

#include "GeraNESNetplay/GeraNESInputFrameAdapter.h"
#include "GeraNESNetplay/GeraNESNetplayAdapters.h"

namespace GeraNESNetplay {

using namespace ConsoleNetplay;

GeraNESNetplayConsole::GeraNESNetplayConsole(IEmulationHost& host,
                                             GeraNESEmu& emu,
                                             const IEmulationHost::InputState& latestInputState)
    : m_host(host),
      m_emu(emu),
      m_latestInputState(latestInputState)
{
}

std::optional<NetplayRomSelection> GeraNESNetplayConsole::captureRomSelection(GeraNESEmu& emu)
{
    if(!emu.valid()) return std::nullopt;

    Cartridge& cart = emu.getConsole().cartridge();
    NetplayRomSelection selection;
    selection.loaded = true;
    selection.gameName = cart.romFile().fileName();
    selection.validation.romCrc32 = cart.romFile().fileCrc32();
    selection.validation.mapperId = static_cast<uint16_t>(std::max(0, cart.mapperId()));
    selection.validation.subMapperId = static_cast<uint16_t>(std::max(0, cart.subMapperId()));
    selection.validation.prgRomSize = static_cast<uint32_t>(std::max(0, cart.prgSize()));
    selection.validation.chrRomSize = static_cast<uint32_t>(std::max(0, cart.chrSize()));
    selection.validation.chrRamSize = static_cast<uint32_t>(std::max(0, cart.chrRamSize()));
    selection.validation.fileSize = static_cast<uint32_t>(cart.romFile().size());
    selection.validation.contentHash = cart.romFile().contentHash32();
    return selection;
}

bool GeraNESNetplayConsole::valid() const
{
    return m_emu.valid();
}

uint32_t GeraNESNetplayConsole::frameCount() const
{
    return m_emu.frameCount();
}

uint32_t GeraNESNetplayConsole::regionFps() const
{
    return m_emu.getRegionFPS();
}

std::optional<NetplayRomSelection> GeraNESNetplayConsole::currentRomSelection() const
{
    return captureRomSelection(m_emu);
}

bool GeraNESNetplayConsole::updateUntilFrame(uint32_t frameDtMs, bool resimulating)
{
    return m_emu.updateUntilFrame(frameDtMs, resimulating);
}

void GeraNESNetplayConsole::applyRemoteInputTopology(const RoomState& room)
{
    (void)room;
    //topology is set on InputFrame
}

void GeraNESNetplayConsole::publishCurrentInputTopology(NetplayCoordinator& coordinator)
{
    const RoomState room = coordinator.session().roomState();
    InputTopology inputTopology;
    inputTopology.port1Device = geraNESPortDeviceFromTopology(room, kPort1PlayerSlot);
    inputTopology.port2Device = geraNESPortDeviceFromTopology(room, kPort2PlayerSlot);
    inputTopology.expansionDevice = geraNESExpansionDeviceFromTopology(room);
    inputTopology.nesMultitapDevice = geraNESNesMultitapDeviceFromTopology(room);
    inputTopology.famicomMultitapDevice = geraNESFamicomMultitapDeviceFromTopology(room);

    setGeraNESRoomInputTopology(
        coordinator,
        inputTopology
    );
}

NetplayInputFrame GeraNESNetplayConsole::buildLocalInputContribution(PlayerSlot slot,
                                                                     FrameNumber frame,
                                                                     const RoomState& room) const
{
    return buildGeraNESLocalInputContribution(slot, frame, m_latestInputState, room);
}

bool GeraNESNetplayConsole::hasStableQueuedInputFrame(FrameNumber frame) const
{
    (void)frame;
    return false;
}

void GeraNESNetplayConsole::queueStandaloneBootstrapInputFrame()
{
}

bool GeraNESNetplayConsole::queuePlaybackInputFrame(const NetplayCoordinator::ConfirmedFrameInputs& confirmed)
{
    IEmulationHost::ReplayFrameInput replayInput;
    if(!buildReplayFrameInput(confirmed, confirmed.frame, replayInput)) {
        return false;
    }

    m_host.queueReplayInputFrame(replayInput.frameOverride);
    return true;
}

void GeraNESNetplayConsole::discardQueuedInputFramesAfter(FrameNumber frame)
{
    (void)m_emu;
    m_host.discardQueuedNetplayInputsAfter(frame);
}

bool GeraNESNetplayConsole::buildReplayFrameInput(const NetplayCoordinator::ConfirmedFrameInputs& confirmed,
                                                  FrameNumber frame,
                                                  IEmulationHost::ReplayFrameInput& outFrame)
{
    outFrame = {};
    outFrame.hasFrameOverride = true;
    outFrame.frameOverride = toGeraNESInputFrame(confirmed.netplayFrame);
    outFrame.frameOverride.frame = frame;
    applyInputFrameToInputState(outFrame.state, outFrame.frameOverride);
    return true;
}

} // namespace GeraNESNetplay
