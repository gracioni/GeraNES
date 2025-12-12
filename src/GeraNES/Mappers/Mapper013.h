#pragma once

#include "BaseMapper.h"

class Mapper013 : public BaseMapper
{

private:

    uint8_t m_CHRReg = 0; 

public:

    Mapper013(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        m_CHRReg = (data&readPrg(addr))&0x03;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(0,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(addr < 0x1000) addr &= 0x0FFF;
        else addr = (m_CHRReg * 0x1000) + (addr&0x0FFF);
        return *(chrRam()+addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(addr < 0x1000) addr &= 0x0FFF;
        else addr = (m_CHRReg * 0x1000) + (addr&0x0FFF);
        *(chrRam()+addr) = data;
    }

    void serialization(SerializationBase &s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_CHRReg);     
    }

};
