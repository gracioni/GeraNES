#ifndef MAPPER013_H
#define MAPPER013_H

#include "IMapper.h"

class Mapper013 : public IMapper
{

private:

    uint8_t m_CHRReg = 0; 

public:

    Mapper013(ICartridgeData& cd) : IMapper(cd)
    {
    }

    GERANES_HOT void writePRG32k(int addr, uint8_t data) override
    {
        m_CHRReg = (data&readPRG32k(addr))&0x03;
    }

    GERANES_HOT uint8_t readPRG32k(int addr) override
    {
        return m_cd.readPrg<W32K>(0,addr);
    }

    GERANES_HOT uint8_t readCHR8k(int addr) override
    {
        if(addr < 0x1000) addr &= 0x0FFF;
        else addr = (m_CHRReg * 0x1000) + (addr&0x0FFF);
        return *(getVRAM()+addr);
    }

    GERANES_HOT void writeCHR8k(int addr, uint8_t data) override
    {
        if(addr < 0x1000) addr &= 0x0FFF;
        else addr = (m_CHRReg * 0x1000) + (addr&0x0FFF);
        *(getVRAM()+addr) = data;
    }

    void serialization(SerializationBase &s) override
    {
        IMapper::serialization(s);
        SERIALIZEDATA(s, m_CHRReg);     
    }

};

#endif // MAPPER013_H
