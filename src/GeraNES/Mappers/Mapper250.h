#pragma once

#include "Mapper004.h"

// iNES Mapper 250
// MMC3 variant where address bit A10 becomes MMC3 register select A0,
// and the written value comes from the CPU address low byte.
class Mapper250 : public Mapper004
{
public:
    Mapper250(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const int translatedAddr = (addr & 0x6000) | ((addr & 0x0400) >> 10);
        Mapper004::writePrg(translatedAddr, static_cast<uint8_t>(addr & 0x00FF));
    }
};
