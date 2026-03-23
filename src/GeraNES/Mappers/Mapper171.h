#pragma once

#include "BaseMapper.h"

class Mapper171 : public BaseMapper
{
private:
    uint8_t m_chrBankLow = 0;
    uint8_t m_chrBankHigh = 0;

public:
    Mapper171(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        switch(absolute & 0xF080) {
        case 0xF000: m_chrBankLow = value; break;
        case 0xF080: m_chrBankHigh = value; break;
        default: break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(0, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        const uint8_t bank = (addr < 0x1000) ? m_chrBankLow : m_chrBankHigh;
        return cd().readChr<BankSize::B4K>(bank, addr);
    }

    void reset() override
    {
        m_chrBankLow = 0;
        m_chrBankHigh = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_chrBankLow);
        SERIALIZEDATA(s, m_chrBankHigh);
    }
};
