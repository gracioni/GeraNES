#pragma once

#include "BaseMapper.h"

class Mapper184 : public BaseMapper
{
private:
    uint8_t m_chrBankLow = 0;
    uint8_t m_chrBankHigh = 0x80;

public:
    Mapper184(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int /*addr*/, uint8_t value) override
    {
        m_chrBankLow = static_cast<uint8_t>(value & 0x07);
        m_chrBankHigh = static_cast<uint8_t>(0x80 | ((value >> 4) & 0x07));
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(0, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        if(addr < 0x1000) return cd().readChr<BankSize::B4K>(m_chrBankLow, addr);
        return cd().readChr<BankSize::B4K>(m_chrBankHigh, addr);
    }

    void reset() override
    {
        m_chrBankLow = 0;
        m_chrBankHigh = 0x80;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_chrBankLow);
        SERIALIZEDATA(s, m_chrBankHigh);
    }
};
