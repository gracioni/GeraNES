#pragma once

#include "BaseMapper.h"

class Mapper166 : public BaseMapper
{
protected:
    uint8_t m_regs[4] = {0};

    GERANES_INLINE virtual bool altMode() const
    {
        return false;
    }

public:
    Mapper166(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        switch(absolute & 0xE000) {
        case 0x8000: m_regs[0] = value & 0x10; break;
        case 0xA000: m_regs[1] = value & 0x1C; break;
        case 0xC000: m_regs[2] = value & 0x1F; break;
        case 0xE000: m_regs[3] = value & 0x1F; break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t outerBank = static_cast<uint8_t>(((m_regs[0] ^ m_regs[1]) & 0x10) << 1);
        const uint8_t innerBank = static_cast<uint8_t>(m_regs[2] ^ m_regs[3]);

        if(m_regs[1] & 0x08) {
            const uint8_t bank = static_cast<uint8_t>((outerBank | innerBank) & 0xFE);
            if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(altMode() ? static_cast<uint8_t>(bank + 1) : bank, addr);
            return cd().readPrg<BankSize::B16K>(altMode() ? bank : static_cast<uint8_t>(bank + 1), addr);
        }

        if(m_regs[1] & 0x04) {
            if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(0x1F, addr);
            return cd().readPrg<BankSize::B16K>(static_cast<uint8_t>(outerBank | innerBank), addr);
        }

        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(static_cast<uint8_t>(outerBank | innerBank), addr);
        return cd().readPrg<BankSize::B16K>(altMode() ? 0x20 : 0x07, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    void reset() override
    {
        memset(m_regs, 0, sizeof(m_regs));
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_regs, 1, 4);
    }
};
