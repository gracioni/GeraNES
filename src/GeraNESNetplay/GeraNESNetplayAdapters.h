#pragma once

#include <optional>

#include "GeraNES/InputBuffer.h"
#include "GeraNES/Settings.h"
#include "ConsoleNetplay/NetplayInputFrame.h"
#include "ConsoleNetplay/NetplayInputState.h"
#include "ConsoleNetplay/NetSession.h"
#include "ConsoleNetplay/NetplayTopology.h"

namespace ConsoleNetplay {

inline PortDevice toNetplayPortDevice(Settings::Device device)
{
    return static_cast<PortDevice>(static_cast<uint8_t>(device));
}

inline std::optional<PortDevice> toNetplayPortDevice(std::optional<Settings::Device> device)
{
    if(!device.has_value()) return std::nullopt;
    return toNetplayPortDevice(*device);
}

inline ExpansionDevice toNetplayExpansionDevice(Settings::ExpansionDevice device)
{
    return static_cast<ExpansionDevice>(static_cast<uint8_t>(device));
}

inline NesMultitapDevice toNetplayNesMultitapDevice(Settings::NesMultitapDevice device)
{
    return static_cast<NesMultitapDevice>(static_cast<uint8_t>(device));
}

inline FamicomMultitapDevice toNetplayFamicomMultitapDevice(Settings::FamicomMultitapDevice device)
{
    return static_cast<FamicomMultitapDevice>(static_cast<uint8_t>(device));
}

inline Settings::Device toSettingsDevice(PortDevice device)
{
    return static_cast<Settings::Device>(static_cast<uint8_t>(device));
}

inline std::optional<Settings::Device> toSettingsDevice(std::optional<PortDevice> device)
{
    if(!device.has_value()) return std::nullopt;
    return toSettingsDevice(*device);
}

inline Settings::ExpansionDevice toSettingsExpansionDevice(ExpansionDevice device)
{
    return static_cast<Settings::ExpansionDevice>(static_cast<uint8_t>(device));
}

inline Settings::NesMultitapDevice toSettingsNesMultitapDevice(NesMultitapDevice device)
{
    return static_cast<Settings::NesMultitapDevice>(static_cast<uint8_t>(device));
}

inline Settings::FamicomMultitapDevice toSettingsFamicomMultitapDevice(FamicomMultitapDevice device)
{
    return static_cast<Settings::FamicomMultitapDevice>(static_cast<uint8_t>(device));
}

NetplayInputFrame toNetplayInputFrame(const InputFrame& inputFrame);

InputFrame toGeraNESInputFrame(const NetplayInputFrame& inputFrame);

class NetplayCoordinator;
struct InputFrameData;

bool injectInputFrameForTests(NetplayCoordinator& coordinator,
                              const InputFrameData& input,
                              const InputFrame& contribution);
void recordLocalInputFrame(NetplayCoordinator& coordinator,
                           FrameNumber frame,
                           PlayerSlot slot,
                           const InputFrame& contribution);

InputFrame makeRoomTopologyBaseFrame(FrameNumber frame, const RoomState& room);
InputFrame makeContributionBase(const InputFrame& baseFrame);
InputFrame buildAssignedContribution(PlayerSlot slot,
                                      const NetplayInputState& state,
                                      const InputFrame& baseFrame);

uint64_t assignedContributionPrimaryMask(PlayerSlot slot, const InputFrame& contribution);
void applyAssignedContribution(InputFrame& target, PlayerSlot slot, const InputFrame& contribution);

} // namespace ConsoleNetplay
