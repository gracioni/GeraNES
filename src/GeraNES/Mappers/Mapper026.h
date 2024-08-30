#ifndef MAPPER026_H
#define MAPPER026_H

#include "Mapper024.h"

//VRC6b
class Mapper026 : public Mapper024
{

private:

public:

    Mapper026(ICartridgeData& cd) : Mapper024(cd)
    {

    }

    void writePRG32k(int addr, uint8_t data) override
    {
        //VRC6b:    A1, A0    $x000, $x002, $x001, $x003

        addr = (addr&0xF000) | ((addr&0x0001)<<1) | ((addr&0x0002)>>1);
        writeVRC6x(addr,data,false);
    }

};

#endif
