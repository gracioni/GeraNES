#ifndef MAPPER025_H
#define MAPPER025_H

#include "Mapper025.h"

class Mapper025 : public Mapper021
{
private:

public:

    Mapper025(ICartridgeData& cd) : Mapper021(cd)
    {
    }

    GERANES_HOT virtual void writePrg(int addr, uint8_t data) override
    {

        // VRC4b:    A1, A0    $x000, $x002, $x001, $x003
        // VRC4d:    A3, A2    $x000, $x008, $x004, $x00C

        addr = (addr & 0xF000) |
                ((addr & 0x0002) >> 1) | ((addr & 0x0001) << 1) |
                ((addr & 0x0002) >> 3) | ((addr & 0x0001) >> 1);

        writeVRCxx(addr,data,true);

    }
};

#endif
