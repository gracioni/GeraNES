#pragma once

#include "BaseMapper.h"

class Mapper162 : public BaseMapper
{
private:
    uint8_t m_regs[4] = {3, 0, 0, 7};

    GERANES_INLINE uint8_t currentBank() const
    {
        switch(m_regs[3] & 0x05) {
        case 0x00: return static_cast<uint8_t>((m_regs[0] & 0x0C) | (m_regs[1] & 0x02) | ((m_regs[2] & 0x0F) << 4));
        case 0x01: return static_cast<uint8_t>((m_regs[0] & 0x0C) | ((m_regs[2] & 0x0F) << 4));
        case 0x04: return static_cast<uint8_t>((m_regs[0] & 0x0E) | ((m_regs[1] >> 1) & 0x01) | ((m_regs[2] & 0x0F) << 4));
        case 0x05: return static_cast<uint8_t>((m_regs[0] & 0x0F) | ((m_regs[2] & 0x0F) << 4));
        default: return 0;
        }
    }

public:
    Mapper162(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegisterAbsolute(uint16_t addr, uint8_t value) override
    {
        if(addr >= 0x5000 && addr <= 0x5FFF) {
            m_regs[(addr >> 8) & 0x03] = value;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(currentBank(), addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    void reset() override
    {
        m_regs[0] = 3;
        m_regs[1] = 0;
        m_regs[2] = 0;
        m_regs[3] = 7;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_regs, 1, 4);
    }
};
