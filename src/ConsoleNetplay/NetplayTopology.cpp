#include "ConsoleNetplay/NetplayTopology.h"

#include <algorithm>

namespace ConsoleNetplay {

std::vector<InputSlotDescriptor> defaultInputTopology()
{
    return {
        {kPort1PlayerSlot, 1, kGenericInputDevice, true, "Input 1", "Input"},
        {kPort2PlayerSlot, 2, kGenericInputDevice, true, "Input 2", "Input"},
    };
}

const InputSlotDescriptor* findInputSlot(const std::vector<InputSlotDescriptor>& topology, PlayerSlot slot)
{
    const auto it = std::find_if(topology.begin(), topology.end(), [slot](const InputSlotDescriptor& descriptor) {
        return descriptor.slot == slot;
    });
    return it == topology.end() ? nullptr : &*it;
}

InputSlotDescriptor* findInputSlot(std::vector<InputSlotDescriptor>& topology, PlayerSlot slot)
{
    const auto it = std::find_if(topology.begin(), topology.end(), [slot](const InputSlotDescriptor& descriptor) {
        return descriptor.slot == slot;
    });
    return it == topology.end() ? nullptr : &*it;
}

bool inputTopologyEquivalent(const std::vector<InputSlotDescriptor>& lhs,
                             const std::vector<InputSlotDescriptor>& rhs)
{
    if(lhs.size() != rhs.size()) return false;
    for(size_t i = 0; i < lhs.size(); ++i) {
        if(lhs[i].slot != rhs[i].slot ||
           lhs[i].groupId != rhs[i].groupId ||
           lhs[i].deviceId != rhs[i].deviceId ||
           lhs[i].assignable != rhs[i].assignable) {
            return false;
        }
    }
    return true;
}

} // namespace ConsoleNetplay
