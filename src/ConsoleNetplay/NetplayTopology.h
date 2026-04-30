#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ConsoleNetplay/NetplayTypes.h"

namespace ConsoleNetplay {

using InputDeviceId = uint16_t;
using InputGroupId = uint16_t;

inline constexpr InputDeviceId kNoInputDevice = 0;
inline constexpr InputDeviceId kGenericInputDevice = 1;

struct InputSlotDescriptor
{
    PlayerSlot slot = kObserverPlayerSlot;
    InputGroupId groupId = 0;
    InputDeviceId deviceId = kNoInputDevice;
    bool assignable = false;
    std::string groupLabel;
    std::string inputLabel;
};

std::vector<InputSlotDescriptor> defaultInputTopology();
const InputSlotDescriptor* findInputSlot(const std::vector<InputSlotDescriptor>& topology, PlayerSlot slot);
InputSlotDescriptor* findInputSlot(std::vector<InputSlotDescriptor>& topology, PlayerSlot slot);
bool inputTopologyEquivalent(const std::vector<InputSlotDescriptor>& lhs,
                             const std::vector<InputSlotDescriptor>& rhs);

} // namespace ConsoleNetplay
