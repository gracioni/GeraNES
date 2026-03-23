#pragma once

#include "Mapper064.h"

// iNES Mapper 158 (Tengen 800037 / Alien Syndrome)
// RAMBO-1 with TLSROM-style CIRAM A10 wiring, using the live CHR slot layout.
class Mapper158 : public Mapper064
{
public:
    Mapper158(ICartridgeData& cd) : Mapper064(cd)
    {
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return MirroringType::CUSTOM;
    }

    GERANES_HOT uint8_t customMirroring(uint8_t index) override
    {
        return static_cast<uint8_t>((currentChrBank1k(index) >> 7) & 0x01);
    }
};
