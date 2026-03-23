#pragma once

#include "BaseMapper.h"

// iNES Mapper 241
// NOTE: This implements the documented base PRG banking behavior.
class Mapper241 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;

public:
    Mapper241(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t value) override
    {
        m_prgBank = value;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank, addr);
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
    }
};
