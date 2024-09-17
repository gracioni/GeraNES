#ifndef MAPPER023_H
#define MAPPER023_H

#include "Mapper021.h"

class Mapper023 : public Mapper021
{
private:

public:

    Mapper023(ICartridgeData& cd) : Mapper021(cd)
    {
    }

    GERANES_HOT virtual void writePrg(int addr, uint8_t data) override
    {

        // VRC4e:    A2, A3    $x000, $x004, $x008, $x00C
        // VRC4f:    A0, A1    $x000, $x001, $x002, $x003

        addr = (addr & 0xF000) | ((addr & 0x000C) >> 2) | (addr & 0x0003);

        writeVRCxx(addr,data,true);

    }
};

#endif
