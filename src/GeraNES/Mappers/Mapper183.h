#pragma once

#include "BaseMapper.h"

class Mapper183 : public BaseMapper
{
private:
    uint8_t m_chrRegs[8] = {0};
    uint8_t m_prgReg = 0;
    uint8_t m_irqCounter = 0;
    uint8_t m_irqScaler = 0;
    bool m_irqEnabled = false;
    bool m_needIrq = false;
    bool m_irqFlag = false;

public:
    Mapper183(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        if((absolute & 0xF800) == 0x6800) {
            m_prgReg = static_cast<uint8_t>(absolute & 0x3F);
            return;
        }

        if((absolute & 0xF80C) >= 0xB000 && (absolute & 0xF80C) <= 0xE00C) {
            const int slot = (((absolute >> 11) - 6) | (absolute >> 3)) & 0x07;
            m_chrRegs[slot] = static_cast<uint8_t>((m_chrRegs[slot] & (0xF0 >> (absolute & 0x04))) | ((value & 0x0F) << (absolute & 0x04)));
            return;
        }

        switch(absolute & 0xF80C) {
        case 0x8800: m_prgLow[0] = value; break;
        case 0xA800: m_prgLow[1] = value; break;
        case 0xA000: m_prgLow[2] = value; break;
        case 0x9800: m_mirroring = static_cast<uint8_t>(value & 0x03); break;
        case 0xF000: m_irqCounter = static_cast<uint8_t>((m_irqCounter & 0xF0) | (value & 0x0F)); break;
        case 0xF004: m_irqCounter = static_cast<uint8_t>((m_irqCounter & 0x0F) | ((value & 0x0F) << 4)); break;
        case 0xF008:
            m_irqEnabled = value > 0;
            if(!m_irqEnabled) m_irqScaler = 0;
            m_irqFlag = false;
            break;
        default:
            break;
        }
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(m_prgReg, addr);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_prgLow[0], addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_prgLow[1], addr);
        case 2: return cd().readPrg<BankSize::B8K>(m_prgLow[2], addr);
        default: return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1), addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        const uint8_t bank = m_chrRegs[slot];
        if(hasChrRam()) return readChrRam<BankSize::B1K>(bank, addr);
        return cd().readChr<BankSize::B1K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        writeChrRam<BankSize::B1K>(m_chrRegs[slot], addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_mirroring) {
        case 0: return MirroringType::VERTICAL;
        case 1: return MirroringType::HORIZONTAL;
        case 2: return MirroringType::SINGLE_SCREEN_A;
        case 3: return MirroringType::SINGLE_SCREEN_B;
        default: return MirroringType::VERTICAL;
        }
    }

    GERANES_HOT void cycle() override
    {
        if(m_needIrq) {
            m_irqFlag = true;
            m_needIrq = false;
        }

        ++m_irqScaler;
        if(m_irqScaler == 114) {
            m_irqScaler = 0;
            if(m_irqEnabled) {
                ++m_irqCounter;
                if(m_irqCounter == 0) m_needIrq = true;
            }
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_irqFlag;
    }

    void reset() override
    {
        memset(m_chrRegs, 0, sizeof(m_chrRegs));
        m_prgReg = 0;
        m_prgLow[0] = 0;
        m_prgLow[1] = 1;
        m_prgLow[2] = 2;
        m_mirroring = 0;
        m_irqCounter = 0;
        m_irqScaler = 0;
        m_irqEnabled = false;
        m_needIrq = false;
        m_irqFlag = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_chrRegs, 1, 8);
        SERIALIZEDATA(s, m_prgReg);
        SERIALIZEDATA(s, m_prgLow[0]);
        SERIALIZEDATA(s, m_prgLow[1]);
        SERIALIZEDATA(s, m_prgLow[2]);
        SERIALIZEDATA(s, m_mirroring);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqScaler);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_needIrq);
        SERIALIZEDATA(s, m_irqFlag);
    }

private:
    uint8_t m_prgLow[3] = {0, 1, 2};
    uint8_t m_mirroring = 0;
};
