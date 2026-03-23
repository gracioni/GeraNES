#pragma once

#include "Mapper004.h"

class Mapper182 : public Mapper004
{
private:
    static GERANES_INLINE uint8_t remapAddrReg(uint8_t value)
    {
        switch(value & 0x07) {
        case 0: return 0;
        case 1: return 3;
        case 2: return 1;
        case 3: return 5;
        case 4: return 6;
        case 5: return 7;
        case 6: return 2;
        default: return 4;
        }
    }

public:
    Mapper182(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        switch(addr & 0x6001) {
        case 0x0001:
            Mapper004::writePrg(0x2000, value);
            break;
        case 0x2000:
            Mapper004::writePrg(0x0000, static_cast<uint8_t>((value & 0xF8) | remapAddrReg(value)));
            break;
        case 0x4000:
            Mapper004::writePrg(0x0001, value);
            break;
        case 0x4001:
            Mapper004::writePrg(0x4000, value);
            Mapper004::writePrg(0x4001, value);
            break;
        case 0x6000:
            Mapper004::writePrg(0x6000, value);
            break;
        case 0x6001:
            Mapper004::writePrg(0x6001, value);
            break;
        default:
            break;
        }
    }
};
