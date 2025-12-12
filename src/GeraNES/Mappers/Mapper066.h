#pragma once

#include "BaseMapper.h"

class Mapper066 : public BaseMapper
{
private:

    uint8_t m_PRGREG = 0;
    uint8_t m_PRGREGMask = 0;
    uint8_t m_CHRREG = 0;
    uint8_t m_CHRREGMask = 0;

public:

    Mapper066(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_PRGREGMask = calculateMask(m_cd.numberOfPRGBanks<BankSize::B32K>());
        m_CHRREGMask = calculateMask(m_cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t data) override
    {
        m_PRGREG = ((data&0xF0)>>4)&m_PRGREGMask;
        m_CHRREG = (data&0x0F)&m_CHRREGMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return m_cd.readPrg<BankSize::B32K>(m_PRGREG,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        return m_cd.readChr<BankSize::B8K>(m_CHRREG,addr);
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_PRGREG);
        SERIALIZEDATA(s, m_PRGREGMask);
        SERIALIZEDATA(s, m_CHRREG);
        SERIALIZEDATA(s, m_CHRREGMask);
    }

};
