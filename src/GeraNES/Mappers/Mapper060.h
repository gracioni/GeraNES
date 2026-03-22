#pragma once

#include "BaseMapper.h"

class Mapper060 : public BaseMapper
{
private:
    uint8_t m_resetCounter = 0;
    bool m_firstReset = true;
    uint8_t m_prgMask16 = 0;
    uint8_t m_chrMask = 0;

public:
    Mapper060(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask16 = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B16K>(m_resetCounter & m_prgMask16, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_resetCounter & m_chrMask, addr);
    }

    void reset() override
    {
        if(m_firstReset) {
            m_resetCounter = 0;
            m_firstReset = false;
        } else {
            m_resetCounter = static_cast<uint8_t>((m_resetCounter + 1) & 0x03);
        }
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_resetCounter);
        SERIALIZEDATA(s, m_firstReset);
        SERIALIZEDATA(s, m_prgMask16);
        SERIALIZEDATA(s, m_chrMask);
    }
};
