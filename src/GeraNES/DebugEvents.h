#pragma once

#include <cstdint>

namespace GeraNES {

enum class DebugEvent : uint8_t
{
    CpuNmiAccepted,
    CpuIrqAccepted,
    CpuNmiHijacksBrk,
    CpuNmiHijacksIrq,
    PpuVBlankStart,
    PpuVBlankEnd,
    PpuSpriteOverflow,
    PpuOam2OverflowEvaluation,
    PpuOam2OverflowFetch,
    PpuOam2OverflowClear,
    PpuRenderingEnabled,
    PpuRenderingDisabled,
    ApuFrameIrqStart,
    ApuFrameIrqEnd,
    DmcIrqStart,
    DmcIrqEnd,
    MapperIrqStart,
    MapperIrqEnd,
    OamDmaEnd,
    DmcDmaEnd,
    DmcDmaAbort,
    PpuPosition
};

constexpr uint32_t debugEventBit(DebugEvent event)
{
    return uint32_t{1} << static_cast<uint8_t>(event);
}

} // namespace GeraNES
