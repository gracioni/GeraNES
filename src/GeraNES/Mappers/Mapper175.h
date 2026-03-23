#pragma once

#include "BaseMapper.h"

class Mapper175 : public BaseMapper
{
private:
    uint8_t m_reg = 0;
    bool m_horizontalMirroring = false;

public:
    Mapper175(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        switch(absolute) {
        case 0x8000:
            m_horizontalMirroring = (value & 0x04) != 0;
            break;
        case 0xA000:
            m_reg = static_cast<uint8_t>(value & 0x0F);
            break;
        default:
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_reg, addr);
        return cd().readPrg<BankSize::B16K>(m_reg, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_reg, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_reg = 0;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_reg);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
