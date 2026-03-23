#pragma once

#include "BaseMapper.h"

class Mapper140 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_chrBank = 0;

public:
    Mapper140(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeSaveRam(int /*addr*/, uint8_t value) override
    {
        m_prgBank = static_cast<uint8_t>((value >> 4) & 0x03);
        m_chrBank = static_cast<uint8_t>(value & 0x0F);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_chrBank);
    }
};
