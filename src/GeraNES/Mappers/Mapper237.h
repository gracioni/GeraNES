#pragma once

#include "BaseMapper.h"

class Mapper237 : public BaseMapper
{
private:
    uint8_t m_outerBank = 0;
    uint8_t m_innerBank = 0;
    uint8_t m_mode = 0;
    bool m_horizontalMirroring = true;
    bool m_type = false;
    bool m_lock = false;

    GERANES_INLINE uint8_t lowBank() const
    {
        const uint8_t base = static_cast<uint8_t>((m_outerBank << 3) | m_innerBank);
        switch(m_mode) {
        case 0: return base;
        case 1: return static_cast<uint8_t>((m_outerBank << 3) | (m_innerBank & 0x06));
        case 2: return base;
        default: return static_cast<uint8_t>((m_outerBank << 3) | (m_innerBank & 0x06));
        }
    }

    GERANES_INLINE uint8_t highBank() const
    {
        const uint8_t base = static_cast<uint8_t>((m_outerBank << 3) | m_innerBank);
        switch(m_mode) {
        case 0: return static_cast<uint8_t>((m_outerBank << 3) | 0x07);
        case 1: return static_cast<uint8_t>((m_outerBank << 3) | 0x07);
        case 2: return base;
        default: return static_cast<uint8_t>(lowBank() + 1);
        }
    }

public:
    Mapper237(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.chrRamSize() == 0) {
            allocateChrRam(static_cast<int>(BankSize::B8K));
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        if(m_lock) {
            m_innerBank = static_cast<uint8_t>(value & 0x07);
            return;
        }

        m_innerBank = static_cast<uint8_t>(value & 0x07);
        m_outerBank = static_cast<uint8_t>((((absolute >> 2) & 0x01) << 2) | ((value >> 3) & 0x03));
        m_horizontalMirroring = (value & 0x20) == 0;
        m_mode = static_cast<uint8_t>((value >> 6) & 0x03);
        m_type = (absolute & 0x01) != 0;
        m_lock = (absolute & 0x02) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        (void)m_type;
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(lowBank(), addr);
        return cd().readPrg<BankSize::B16K>(highBank(), addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return BaseMapper::readChr(addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        BaseMapper::writeChr(addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_outerBank = 0;
        m_innerBank = 0;
        m_mode = 0;
        m_horizontalMirroring = true;
        m_type = false;
        m_lock = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_outerBank);
        SERIALIZEDATA(s, m_innerBank);
        SERIALIZEDATA(s, m_mode);
        SERIALIZEDATA(s, m_horizontalMirroring);
        SERIALIZEDATA(s, m_type);
        SERIALIZEDATA(s, m_lock);
    }
};
