#pragma once

#include "Mapper004.h"

// iNES Mapper 114
class Mapper114 : public Mapper004
{
private:
    static constexpr uint8_t SECURITY[8] = {0, 3, 1, 5, 6, 7, 2, 4};
    uint8_t m_exReg0 = 0;
    uint8_t m_exReg1 = 0;

    GERANES_INLINE uint8_t currentPrgBank8k(uint8_t slot) const
    {
        if((m_exReg0 & 0x80) != 0) {
            const uint8_t bank = static_cast<uint8_t>((m_exReg0 & 0x0F) << 1);
            return static_cast<uint8_t>(bank + ((slot >> 1) & 0x01));
        }

        if(!m_prgMode) {
            switch(slot & 0x03) {
            case 0: return m_prgReg0;
            case 1: return m_prgReg1;
            case 2: return static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 2);
            default: return static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1);
            }
        }

        switch(slot & 0x03) {
        case 0: return static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 2);
        case 1: return m_prgReg1;
        case 2: return m_prgReg0;
        default: return static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1);
        }
    }

public:
    Mapper114(ICartridgeData& cd) : Mapper004(cd)
    {
        m_mmc3RevAIrqs = true;
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        if(addr >= 0x1000) {
            m_exReg0 = value;
        }
    }

    GERANES_HOT void writeSaveRam(int /*addr*/, uint8_t value) override
    {
        m_exReg0 = value;
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const int absolute = addr + 0x8000;

        switch(absolute & 0xE001) {
        case 0x8001:
            Mapper004::writePrg(0x2000, value);
            break;
        case 0xA000:
            Mapper004::writePrg(0x0000, static_cast<uint8_t>((value & 0xC0) | SECURITY[value & 0x07]));
            m_exReg1 = 1;
            break;
        case 0xA001:
            m_reloadValue = value;
            break;
        case 0xC000:
            if(m_exReg1 != 0) {
                m_exReg1 = 0;
                Mapper004::writePrg(0x0001, value);
            }
            break;
        case 0xC001:
            m_irqClearFlag = true;
            m_irqCounter = 0;
            break;
        case 0xE000:
            m_interruptFlag = false;
            m_enableInterrupt = false;
            break;
        case 0xE001:
            m_enableInterrupt = true;
            break;
        default:
            Mapper004::writePrg(addr, value);
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(currentPrgBank8k(static_cast<uint8_t>((addr >> 13) & 0x03)), addr);
    }

    void reset() override
    {
        Mapper004::reset();
        m_exReg0 = 0;
        m_exReg1 = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_exReg0);
        SERIALIZEDATA(s, m_exReg1);
    }
};
