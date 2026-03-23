#pragma once

#include "BaseMapper.h"

class Mapper133 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_chrBank = 0;

    GERANES_INLINE void applyValue(uint8_t value)
    {
        m_prgBank = static_cast<uint8_t>((value >> 2) & 0x01);
        m_chrBank = static_cast<uint8_t>(value & 0x03);
    }

public:
    Mapper133(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        if((addr & 0x0100) != 0) applyValue(value);
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
