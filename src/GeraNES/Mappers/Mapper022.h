#pragma once

#include "Mapper021.h"

//TODO: data eeprom

//VRC2a
class Mapper022 : public Mapper021
{

private:

public:

    Mapper022(ICartridgeData& cd) : Mapper021(cd)
    {

    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        //A1 A0 - $x000, $x002, $x001, $x003

        addr = (addr&0xF000) | ((addr&0x0001)<<1) | ((addr&0x0002)>>1);
        writeVRCxx(addr,data,false);

        m_PRGMode = false; // VRC2 has 1 mode only
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        size_t index = addr >> 10;
        uint8_t bank = m_CHRReg[index];

        bank >>= 1; //VRC2a only

        return m_cd.readChr<W1K>(bank&m_CHRREGMask,addr); // addr/1024
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return false;
    }

};
