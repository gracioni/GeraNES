#ifndef MAPPER034_H
#define MAPPER034_H

#include "IMapper.h"

//BxROM or NINA-001, 2 incompatible mappers
class Mapper034 : public IMapper
{
private:

    uint8_t m_PRGMask = 0;
    uint8_t m_PRGReg = 0;

    uint8_t m_CHRReg[2] = {0};
    uint8_t m_CHRMask = 0;

public:

    Mapper034(ICartridgeData& cd) : IMapper(cd)
    {
        m_PRGMask = calculateMask(m_cartridgeData.numberOfPRGBanks<W32K>());

        m_CHRMask = calculateMask(m_cartridgeData.numberOfCHRBanks<W4K>());
    }

    bool isNINA() {
        return !has8kVRAM();
    }

    void writePRG32k(int addr, uint8_t data) override
    {
        if(!isNINA()) m_PRGReg = data & m_PRGMask;
        else IMapper::writePRG32k(addr,data);
    }

    uint8_t readPRG32k(int addr) override
    {
        return m_cartridgeData.readPrg<W32K>(m_PRGReg,addr);
    }

    uint8_t readCHR8k(int addr) override
    {
        if(has8kVRAM()) return IMapper::readCHR8k(addr);
        else {

            switch(addr>>12){
            case 0: return m_cartridgeData.readChr<W4K>(m_CHRReg[0],addr);
            case 1: return m_cartridgeData.readChr<W4K>(m_CHRReg[1],addr);
            }

        }

        return 0;
    }

    void writeSRAM8k(int addr, uint8_t data) override
    {
        if(isNINA()){
            switch(addr){
            case 0x1FFD: m_PRGReg = data & m_PRGMask; break;
            case 0x1FFE: m_CHRReg[0] = data & m_CHRMask; break;
            case 0x1FFF: m_CHRReg[1] = data & m_CHRMask; break;
            }
        }

        IMapper::writeSRAM8k(addr,data);
    }

    void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);    

        SERIALIZEDATA(s, m_PRGMask);
        SERIALIZEDATA(s, m_PRGReg);

        s.array(m_CHRReg,1,2);
        SERIALIZEDATA(s, m_CHRMask);
    }

};

#endif
