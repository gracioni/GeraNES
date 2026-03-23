#pragma once

#include "Mapper004.h"

class Mapper224 : public Mapper004
{
private:
    uint8_t m_outerBank = 0;

    GERANES_INLINE uint8_t mapPrgPage(uint8_t page8k) const
    {
        return static_cast<uint8_t>((page8k & 0x3F) | (m_outerBank << 6)) & m_prgMask;
    }

public:
    Mapper224(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writeMapperRegisterAbsolute(uint16_t addr, uint8_t data) override
    {
        if(addr >= 0x5000 && addr <= 0x5003) {
            if(addr == 0x5000) {
                m_outerBank = static_cast<uint8_t>((data >> 2) & 0x01);
            }
            return;
        }

        BaseMapper::writeMapperRegisterAbsolute(addr, data);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t fixedSecondLast = static_cast<uint8_t>(0x3E | (m_outerBank << 6)) & m_prgMask;
        const uint8_t fixedLast = static_cast<uint8_t>(0x3F | (m_outerBank << 6)) & m_prgMask;

        if(!m_prgMode) {
            switch(addr >> 13) {
            case 0: return cd().readPrg<BankSize::B8K>(mapPrgPage(m_prgReg0), addr);
            case 1: return cd().readPrg<BankSize::B8K>(mapPrgPage(m_prgReg1), addr);
            case 2: return cd().readPrg<BankSize::B8K>(fixedSecondLast, addr);
            default: return cd().readPrg<BankSize::B8K>(fixedLast, addr);
            }
        }

        switch(addr >> 13) {
        case 0: return cd().readPrg<BankSize::B8K>(fixedSecondLast, addr);
        case 1: return cd().readPrg<BankSize::B8K>(mapPrgPage(m_prgReg1), addr);
        case 2: return cd().readPrg<BankSize::B8K>(mapPrgPage(m_prgReg0), addr);
        default: return cd().readPrg<BankSize::B8K>(fixedLast, addr);
        }
    }

    void reset() override
    {
        Mapper004::reset();
        m_outerBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_outerBank);
    }
};
