#pragma once

#include "BaseMapper.h"

class Mapper145 : public BaseMapper
{
private:
    uint8_t m_chrBank = 0;

    GERANES_INLINE void applyWrite(uint16_t absolute, uint8_t value)
    {
        if((absolute & 0x4100) == 0x4100) {
            m_chrBank = static_cast<uint8_t>((value >> 7) & 0x01);
        }
    }

public:
    Mapper145(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        applyWrite(static_cast<uint16_t>(addr + 0x4000), value);
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t value) override
    {
        applyWrite(static_cast<uint16_t>(addr + 0x6000), value);
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
