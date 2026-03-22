#pragma once

#include "BaseMapper.h"

class Mapper039 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;

public:
    Mapper039(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B32K>());
    }

    GERANES_HOT void writeMapperRegister(int /*addr*/, uint8_t data) override
    {
        m_prgBank = data & m_prgMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank & m_prgMask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    void reset() override
    {
        m_prgBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
    }
};
