#pragma once

#include "BaseMapper.h"

//CNROM
class Mapper003 : public BaseMapper
{
private:

    uint8_t m_CHRREG = 0;
    uint8_t m_CHRREGMask = 0;

public:

    Mapper003(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_CHRREGMask = calculateMask(m_cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t data) override
    {
        m_CHRREG = data&m_CHRREGMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return m_cd.readPrg<BankSize::B16K>(0,addr);
        return m_cd.readPrg<BankSize::B16K>(m_cd.numberOfPRGBanks<BankSize::B16K>()==2?1:0,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        return m_cd.readChr<BankSize::B8K>(m_CHRREG,addr);
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_CHRREG);
        SERIALIZEDATA(s, m_CHRREGMask);
    }

};
