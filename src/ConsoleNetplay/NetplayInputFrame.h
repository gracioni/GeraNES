#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "ConsoleNetplay/NetplayTypes.h"
namespace ConsoleNetplay {

struct NetplayInputFrame
{
    FrameNumber frame = 0;
    uint32_t timelineEpoch = 0;
    bool speculative = false;

    std::array<uint64_t, kMaxAssignedPlayerSlot + 1> buttonMaskLo = {};
    std::array<uint64_t, kMaxAssignedPlayerSlot + 1> buttonMaskHi = {};
    std::array<std::vector<uint8_t>, kMaxAssignedPlayerSlot + 1> slotPayloads = {};

    // Console/app-defined metadata. The reusable netplay core treats this as
    // opaque bytes; adapters decide whether they need it.
    std::vector<uint8_t> framePayload;

    bool operator==(const NetplayInputFrame&) const = default;
    bool operator!=(const NetplayInputFrame&) const = default;

    static NetplayInputFrame repeatedFrom(const NetplayInputFrame& previous, FrameNumber targetFrame)
    {
        NetplayInputFrame repeated = previous;
        repeated.frame = targetFrame;
        repeated.speculative = false;
        return repeated;
    }
};

} // namespace ConsoleNetplay
