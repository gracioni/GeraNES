#pragma once

#include "BaseMapper.h"

class Mapper170 : public BaseMapper
{
private:
    uint8_t m_reg = 0;

public:
    Mapper170(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegisterAbsolute(uint16_t addr, uint8_t value) override
    {
        if(addr == 0x6502 || addr == 0x7000) {
            m_reg = static_cast<uint8_t>((value << 1) & 0x80);
        }
    }

    GERANES_HOT uint8_t readMapperRegisterAbsolute(uint16_t addr, uint8_t openBusData) override
    {
        if(addr == 0x7001 || addr == 0x7777) {
            return static_cast<uint8_t>(m_reg | ((addr >> 8) & 0x7F));
        }
        return openBusData;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(0, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    void reset() override
    {
        m_reg = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_reg);
    }
};
