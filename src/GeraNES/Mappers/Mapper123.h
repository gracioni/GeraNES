#pragma once

#include "Mapper004.h"

class Mapper123 : public Mapper004
{
private:
    static constexpr uint8_t SECURITY[8] = {0, 3, 1, 5, 6, 7, 2, 4};
    uint8_t m_exReg[2] = {0};

    GERANES_INLINE uint16_t activePrgBank(uint8_t slot) const
    {
        if((m_exReg[0] & 0x40) != 0) {
            const uint8_t bank = static_cast<uint8_t>((m_exReg[0] & 0x05) | ((m_exReg[0] & 0x08) >> 2) | ((m_exReg[0] & 0x20) >> 2));
            if((m_exReg[0] & 0x02) != 0) return static_cast<uint16_t>(((bank & 0xFE) << 1) + (slot & 0x03));
            return static_cast<uint16_t>((bank << 1) + ((slot >> 1) & 0x01));
        }

        if(!m_prgMode) {
            switch(slot & 0x03) {
            case 0: return m_prgReg0;
            case 1: return m_prgReg1;
            case 2: return static_cast<uint16_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 2);
            default: return static_cast<uint16_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1);
            }
        }

        switch(slot & 0x03) {
        case 0: return static_cast<uint16_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 2);
        case 1: return m_prgReg1;
        case 2: return m_prgReg0;
        default: return static_cast<uint16_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1);
        }
    }

public:
    Mapper123(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        if(addr >= 0x0801) m_exReg[addr & 0x01] = value;
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        if(absolute < 0xA000) {
            switch(absolute & 0x8001) {
            case 0x8000:
                Mapper004::writePrg(0x0000, static_cast<uint8_t>((value & 0xC0) | SECURITY[value & 0x07]));
                break;
            case 0x8001:
                Mapper004::writePrg(0x0001, value);
                break;
            }
            return;
        }

        Mapper004::writePrg(addr, value);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(activePrgBank(static_cast<uint8_t>((addr >> 13) & 0x03)), addr);
    }

    void reset() override
    {
        Mapper004::reset();
        m_exReg[0] = 0;
        m_exReg[1] = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        s.array(m_exReg, 1, 2);
    }
};
