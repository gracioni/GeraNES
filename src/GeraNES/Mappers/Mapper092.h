#pragma once

#include "Mapper072.h"

class Mapper092 : public Mapper072
{
protected:
    uint8_t prgBits(uint8_t data) const override
    {
        return static_cast<uint8_t>(data & 0x1F);
    }

public:
    Mapper092(ICartridgeData& cd) : Mapper072(cd)
    {
    }
};
