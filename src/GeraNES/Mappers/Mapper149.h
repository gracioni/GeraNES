#pragma once

#include "BaseMapper.h"

class Mapper149 : public BaseMapper
{
private:
    uint8_t m_chrBank = 0;

public:
    Mapper149(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t value) override
    {
        m_chrBank = static_cast<uint8_t>((value >> 7) & 0x01);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(0, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    void reset() override
    {
        m_chrBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_chrBank);
    }
};
