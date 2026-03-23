#pragma once

#include "BaseMapper.h"

// iNES Mapper 234
class Mapper234 : public BaseMapper
{
private:
    uint8_t m_reg0 = 0;
    uint8_t m_reg1 = 0;

    GERANES_INLINE void updateState()
    {
        // NINA-03 mode
        if((m_reg0 & 0x40) != 0) {
            m_reg1 &= 0x71;
            return;
        }

        // CNROM mode uses only the low 2 bits of reg1 CHR selector.
        m_reg1 &= 0x31;
    }

    GERANES_INLINE void latchAccess(uint16_t absolute, uint8_t value)
    {
        if(absolute >= 0xFF80 && absolute <= 0xFF9F) {
            if((m_reg0 & 0x3F) == 0) {
                m_reg0 = value;
                updateState();
            }
        } else if(absolute >= 0xFFE8 && absolute <= 0xFFF8) {
            m_reg1 = static_cast<uint8_t>(value & 0x71);
            updateState();
        }
    }

public:
    Mapper234(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        latchAccess(absolute, data);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        const uint8_t value = cd().readPrg<BankSize::B32K>(prgBank(), addr);
        latchAccess(absolute, value);
        return value;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(chrBank(), addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return (m_reg0 & 0x80) != 0 ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    GERANES_INLINE uint8_t prgBank() const
    {
        if((m_reg0 & 0x40) != 0) {
            return static_cast<uint8_t>((m_reg0 & 0x0E) | (m_reg1 & 0x01));
        }

        return static_cast<uint8_t>(m_reg0 & 0x0F);
    }

    GERANES_INLINE uint8_t chrBank() const
    {
        if((m_reg0 & 0x40) != 0) {
            return static_cast<uint8_t>(((m_reg0 << 2) & 0x38) | ((m_reg1 >> 4) & 0x07));
        }

        return static_cast<uint8_t>(((m_reg0 << 2) & 0x3C) | ((m_reg1 >> 4) & 0x03));
    }

    void reset() override
    {
        m_reg0 = 0;
        m_reg1 = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_reg0);
        SERIALIZEDATA(s, m_reg1);
    }
};
