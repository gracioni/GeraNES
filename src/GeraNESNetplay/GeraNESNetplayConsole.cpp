#include "GeraNESNetplay/GeraNESNetplayConsole.h"

#include <algorithm>

#include "GeraNESNetplay/GeraNESInputFrameAdapter.h"
#include "GeraNESNetplay/GeraNESNetplayAdapters.h"

namespace GeraNESNetplay {

using namespace ConsoleNetplay;

namespace {

InputFrame makeEmuTopologyFallbackFrame(const GeraNESEmu& emu,
                                        FrameNumber frame,
                                        uint32_t timelineEpoch)
{
    InputFrame fallback{};
    fallback.frame = frame;
    fallback.timelineEpoch = timelineEpoch;
    fallback.port1Device = emu.getPortDevice(Settings::Port::P_1).value_or(Settings::Device::NONE);
    fallback.port2Device = emu.getPortDevice(Settings::Port::P_2).value_or(Settings::Device::NONE);
    fallback.expansionDevice = emu.getExpansionDevice();
    fallback.nesMultitapDevice = emu.getNesMultitapDevice();
    fallback.famicomMultitapDevice = emu.getFamicomMultitapDevice();
    return fallback;
}

} // namespace

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

uint32_t GeraNESNetplayConsole::canonicalNetplayStateCrc32()
{
    return m_emu.canonicalNetplayStateCrc32();
}

std::optional<NetplayRomSelection> GeraNESNetplayConsole::currentRomSelection() const
{
    return captureRomSelection(m_emu);
}

bool GeraNESNetplayConsole::loadRollbackState(const std::vector<uint8_t>& data)
{
    m_emu.loadStateFromMemoryWithAudioPolicy(
        data,
        GeraNESEmu::StateLoadAudioPolicy::PreserveContinuousOutput
    );
    return m_emu.valid();
}

bool GeraNESNetplayConsole::updateUntilFrame(uint32_t frameDtMs, bool resimulating)
{
    return m_emu.updateUntilFrame(frameDtMs, resimulating);
}

void GeraNESNetplayConsole::applyRemoteInputTopology(const RoomState& room)
{
    m_emu.setPortDevice(Settings::Port::P_1, geraNESPortDeviceFromTopology(room, kPort1PlayerSlot));
    m_emu.setPortDevice(Settings::Port::P_2, geraNESPortDeviceFromTopology(room, kPort2PlayerSlot));
    m_emu.setExpansionDevice(geraNESExpansionDeviceFromTopology(room));
    m_emu.setNesMultitapDevice(geraNESNesMultitapDeviceFromTopology(room));
    m_emu.setFamicomMultitapDevice(geraNESFamicomMultitapDeviceFromTopology(room));
}

void GeraNESNetplayConsole::publishCurrentInputTopology(NetplayCoordinator& coordinator)
{
    const RoomState room = coordinator.session().roomState();
    const Settings::Device roomPort1Device = geraNESPortDeviceFromTopology(room, kPort1PlayerSlot);
    const Settings::Device roomPort2Device = geraNESPortDeviceFromTopology(room, kPort2PlayerSlot);
    const Settings::ExpansionDevice roomExpansionDevice = geraNESExpansionDeviceFromTopology(room);
    const Settings::NesMultitapDevice roomNesMultitapDevice = geraNESNesMultitapDeviceFromTopology(room);
    const Settings::FamicomMultitapDevice roomFamicomMultitapDevice = geraNESFamicomMultitapDeviceFromTopology(room);

    std::optional<Settings::Device> port1Device = m_emu.getPortDevice(Settings::Port::P_1);
    std::optional<Settings::Device> port2Device = m_emu.getPortDevice(Settings::Port::P_2);
    Settings::ExpansionDevice expansionDevice = m_emu.getExpansionDevice();
    Settings::NesMultitapDevice nesMultitapDevice = m_emu.getNesMultitapDevice();
    Settings::FamicomMultitapDevice famicomMultitapDevice = m_emu.getFamicomMultitapDevice();

    if(roomPort1Device != Settings::Device::NONE &&
       port1Device != std::optional<Settings::Device>(roomPort1Device)) {
        port1Device = roomPort1Device;
        if(coordinator.isHosting()) {
            m_emu.setPortDevice(Settings::Port::P_1, *port1Device);
        }
    }
    if(roomPort2Device != Settings::Device::NONE &&
       port2Device != std::optional<Settings::Device>(roomPort2Device)) {
        port2Device = roomPort2Device;
        if(coordinator.isHosting()) {
            m_emu.setPortDevice(Settings::Port::P_2, *port2Device);
        }
    }
    if(roomExpansionDevice != Settings::ExpansionDevice::NONE &&
       expansionDevice != roomExpansionDevice) {
        expansionDevice = roomExpansionDevice;
        if(coordinator.isHosting()) {
            m_emu.setExpansionDevice(expansionDevice);
        }
    }
    if(roomNesMultitapDevice != Settings::NesMultitapDevice::NONE &&
       nesMultitapDevice != roomNesMultitapDevice) {
        nesMultitapDevice = roomNesMultitapDevice;
        if(coordinator.isHosting()) {
            m_emu.setNesMultitapDevice(nesMultitapDevice);
        }
    }
    if(roomFamicomMultitapDevice != Settings::FamicomMultitapDevice::NONE &&
       famicomMultitapDevice != roomFamicomMultitapDevice) {
        famicomMultitapDevice = roomFamicomMultitapDevice;
        if(coordinator.isHosting()) {
            m_emu.setFamicomMultitapDevice(famicomMultitapDevice);
        }
    }
    setGeraNESRoomInputTopology(
        coordinator,
        port1Device,
        port2Device,
        expansionDevice,
        nesMultitapDevice,
        famicomMultitapDevice
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
    const InputFrame* existingFrame = m_emu.inputBuffer().findByFrame(frame, m_emu.inputTimelineEpoch());
    return existingFrame != nullptr && !existingFrame->speculative;
}

void GeraNESNetplayConsole::queueStandaloneBootstrapInputFrame()
{
    const uint32_t currentFrame = m_emu.frameCount();
    if(m_emu.inputBuffer().findByFrame(currentFrame, m_emu.inputTimelineEpoch()) != nullptr) {
        return;
    }

    InputFrame bootstrapFrame = m_emu.createInputFrame(currentFrame);
    (void)m_emu.queueInputFrame(bootstrapFrame);
}

bool GeraNESNetplayConsole::queuePlaybackInputFrame(const NetplayCoordinator::ConfirmedFrameInputs& confirmed)
{
    const InputFrame* existingFrame =
        m_emu.inputBuffer().findByFrame(confirmed.netplayFrame.frame, m_emu.inputTimelineEpoch());
    if(existingFrame != nullptr && existingFrame->speculative && confirmed.predicted) {
        return true;
    }

    InputFrame inputFrame = toGeraNESInputFrame(
        confirmed.netplayFrame,
        makeEmuTopologyFallbackFrame(m_emu, confirmed.netplayFrame.frame, confirmed.netplayFrame.timelineEpoch)
    );
    inputFrame.speculative = confirmed.predicted;
    inputFrame.timelineEpoch = m_emu.inputTimelineEpoch();
    const InputBuffer::EnqueueResult enqueueResult = m_emu.queueInputFrame(inputFrame);
    return enqueueResult == InputBuffer::EnqueueResult::Inserted ||
           enqueueResult == InputBuffer::EnqueueResult::UpdatedPending ||
           (existingFrame != nullptr && enqueueResult == InputBuffer::EnqueueResult::RejectedConsumed);
}

void GeraNESNetplayConsole::discardQueuedInputFramesAfter(FrameNumber frame)
{
    m_emu.discardQueuedInputFramesAfter(frame);
    m_host.discardQueuedNetplayInputsAfter(frame);
}

bool GeraNESNetplayConsole::buildReplayFrameInput(const NetplayCoordinator::ConfirmedFrameInputs& confirmed,
                                                  FrameNumber frame,
                                                  IEmulationHost::ReplayFrameInput& outFrame)
{
    outFrame = {};
    outFrame.speculative = confirmed.predicted;
    outFrame.hasFrameOverride = true;
    outFrame.frameOverride = toGeraNESInputFrame(confirmed.netplayFrame);
    outFrame.frameOverride.frame = frame;
    applyInputFrameToInputState(outFrame.state, outFrame.frameOverride);
    return true;
}

bool GeraNESNetplayConsole::buildReplayFrameInput(const NetplayCoordinator::ConfirmedFrameInputs& confirmed,
                                                  FrameNumber frame,
                                                  GeraNESEmu& emu,
                                                  IEmulationHost::ReplayFrameInput& outFrame)
{
    outFrame = {};
    outFrame.speculative = confirmed.predicted;
    outFrame.hasFrameOverride = true;
    outFrame.frameOverride = toGeraNESInputFrame(
        confirmed.netplayFrame,
        makeEmuTopologyFallbackFrame(emu, confirmed.netplayFrame.frame, confirmed.netplayFrame.timelineEpoch)
    );
    outFrame.frameOverride.frame = frame;
    applyInputFrameToInputState(outFrame.state, outFrame.frameOverride);
    return true;
}

} // namespace GeraNESNetplay
