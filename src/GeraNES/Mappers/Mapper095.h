#pragma once

#include "Mapper206.h"

class Mapper095 : public Mapper206
{
public:
    Mapper095(ICartridgeData& cd) : Mapper206(cd)
    {
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return MirroringType::CUSTOM;
    }

    GERANES_HOT uint8_t customMirroring(uint8_t index) override
    {
        switch(index & 0x03) {
        case 0:
        case 1:
            return static_cast<uint8_t>((m_chrReg[0] >> 5) & 0x01);
        case 2:
        case 3:
            return static_cast<uint8_t>((m_chrReg[1] >> 5) & 0x01);
        }

        return 0;
    }
};
