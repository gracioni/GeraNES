#pragma once

#include "BaseMapper.h"

class Mapper164 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0x0F;

public:
    Mapper164(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegisterAbsolute(uint16_t addr, uint8_t value) override
    {
        switch(addr & 0x7300) {
        case 0x5000:
            m_prgBank = static_cast<uint8_t>((m_prgBank & 0xF0) | (value & 0x0F));
            break;
        case 0x5100:
            m_prgBank = static_cast<uint8_t>((m_prgBank & 0x0F) | ((value & 0x0F) << 4));
            break;
        default:
            break;
        }
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
        m_prgBank = 0x0F;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
    }
};
