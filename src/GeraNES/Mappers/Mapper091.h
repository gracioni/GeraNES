#pragma once

#include "BaseMapper.h"

// iNES Mapper 91
// Submapper 0: JY830623C/YY840238C (A12 IRQ, hard-wired mirroring, outer bank)
// Submapper 1: EJ-006-1 (M2 IRQ, selectable mirroring)
class Mapper091 : public BaseMapper
{
private:
    uint8_t m_subMapper = 0;

    // 8KB switchable PRG banks at $8000/$A000
    uint8_t m_prgReg[2] = {0, 1};
    // 2KB switchable CHR banks at $0000/$0800/$1000/$1800
    uint8_t m_chrReg[4] = {0, 1, 2, 3};

    uint8_t m_prgMask = 0; // 8KB
    uint8_t m_chrMask = 0; // 2KB

    // Submapper 0 outer bank register: .... .... .... .PPC
    uint8_t m_outerPrg = 0; // PRG A17-A18
    uint8_t m_outerChr = 0; // CHR A19

    // Mirroring for submapper 1 only
    bool m_verticalMirroring = false;

    // IRQ state
    bool m_irqEnabled = false;
    bool m_irqFlag = false;

    // Submapper 0: fixed 64 A12 rises counter
    uint8_t m_irqA12Counter = 64;
    bool m_lastA12 = false;

    // Submapper 1: M2-based down-counter (5 every 4 CPU cycles)
    uint16_t m_irqCounter = 0;
    uint8_t m_m2Div4 = 0;

    GERANES_INLINE uint8_t mapPrg8k(uint8_t reg) const
    {
        if(m_subMapper == 0) {
            const uint8_t inner = static_cast<uint8_t>(reg & 0x0F);
            return static_cast<uint8_t>((m_outerPrg << 4) | inner) & m_prgMask;
        }
        return reg & m_prgMask;
    }

    GERANES_INLINE uint16_t mapChr2k(uint8_t reg) const
    {
        if(m_subMapper == 0) {
            // Outer CHR bit is A19 => bit 8 in 2KB bank units.
            return static_cast<uint16_t>(((m_outerChr & 0x01) << 8) | reg) & m_chrMask;
        }
        return static_cast<uint16_t>(reg) & m_chrMask;
    }

public:
    Mapper091(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_subMapper = static_cast<uint8_t>(cd.subMapperId() & 0x0F);
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B2K));
        } else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B2K>());
        }

        m_verticalMirroring = (cd.mirroringType() == MirroringType::VERTICAL);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) { // 8KB slots
        case 0: return cd().readPrg<BankSize::B8K>(mapPrg8k(m_prgReg[0]), addr);
        case 1: return cd().readPrg<BankSize::B8K>(mapPrg8k(m_prgReg[1]), addr);
        case 2: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 2, addr);
        default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 11) & 0x03); // 2KB slots
        const uint16_t bank = mapChr2k(m_chrReg[slot]);

        if(hasChrRam()) return readChrRam<BankSize::B2K>(bank, addr);
        return cd().readChr<BankSize::B2K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint8_t slot = static_cast<uint8_t>((addr >> 11) & 0x03);
        const uint16_t bank = mapChr2k(m_chrReg[slot]);
        writeChrRam<BankSize::B2K>(bank, addr, data);
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        const bool high = (addr & 0x1000) != 0; // $7000-$7FFF

        if(!high) {
            const uint8_t reg = static_cast<uint8_t>(addr & (m_subMapper == 1 ? 0x07 : 0x03));

            switch(reg) {
            case 0x00: m_chrReg[0] = data; break;
            case 0x01: m_chrReg[1] = data; break;
            case 0x02: m_chrReg[2] = data; break;
            case 0x03: m_chrReg[3] = data; break;
            case 0x04:
                if(m_subMapper == 1) m_verticalMirroring = false; // horizontal
                break;
            case 0x05:
                if(m_subMapper == 1) m_verticalMirroring = true; // vertical
                break;
            case 0x06:
                if(m_subMapper == 1) m_irqCounter = static_cast<uint16_t>((m_irqCounter & 0xFF00) | data);
                break;
            case 0x07:
                if(m_subMapper == 1) m_irqCounter = static_cast<uint16_t>((m_irqCounter & 0x00FF) | (static_cast<uint16_t>(data) << 8));
                break;
            }

            return;
        }

        const uint8_t reg = static_cast<uint8_t>(addr & (m_subMapper == 1 ? 0x07 : 0x03));
        switch(reg) {
        case 0x00: m_prgReg[0] = data; break;
        case 0x01: m_prgReg[1] = data; break;
        case 0x02: // $7006 stop/ack
            m_irqEnabled = false;
            m_irqFlag = false;
            break;
        case 0x03: // $7007 start/reset
            m_irqEnabled = true;
            m_irqFlag = false;
            if(m_subMapper == 0) {
                m_irqA12Counter = 64;
            } else {
                m_m2Div4 = 0;
            }
            break;
        default:
            break;
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        // Submapper 0 outer bank register at $8000-$9FFF.
        if(m_subMapper == 0 && addr < 0x2000) {
            m_outerChr = static_cast<uint8_t>(data & 0x01);
            m_outerPrg = static_cast<uint8_t>((data >> 1) & 0x03);
        }
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(m_subMapper == 0) {
            return cd().mirroringType();
        }
        return m_verticalMirroring ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
    }

    bool getInterruptFlag() override
    {
        return m_irqFlag;
    }

    void setA12State(bool state) override
    {
        if(m_subMapper != 0) {
            m_lastA12 = state;
            return;
        }

        if(!m_lastA12 && state && m_irqEnabled) {
            if(m_irqA12Counter > 0) {
                --m_irqA12Counter;
            }

            if(m_irqA12Counter == 0) {
                m_irqFlag = true;
                m_irqA12Counter = 64;
            }
        }

        m_lastA12 = state;
    }

    void cycle() override
    {
        if(m_subMapper != 1 || !m_irqEnabled) return;

        m_m2Div4 = static_cast<uint8_t>((m_m2Div4 + 1) & 0x03);
        if(m_m2Div4 == 0) {
            const uint16_t prev = m_irqCounter;
            m_irqCounter = static_cast<uint16_t>(m_irqCounter - 5);
            if(prev < 5) {
                m_irqFlag = true;
            }
        }
    }

    void reset() override
    {
        m_prgReg[0] = 0;
        m_prgReg[1] = 1;
        m_chrReg[0] = 0;
        m_chrReg[1] = 1;
        m_chrReg[2] = 2;
        m_chrReg[3] = 3;
        m_outerPrg = 0;
        m_outerChr = 0;
        m_verticalMirroring = (cd().mirroringType() == MirroringType::VERTICAL);
        m_irqEnabled = false;
        m_irqFlag = false;
        m_irqA12Counter = 64;
        m_lastA12 = false;
        m_irqCounter = 0;
        m_m2Div4 = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_subMapper);
        SERIALIZEDATA(s, m_prgReg[0]);
        SERIALIZEDATA(s, m_prgReg[1]);
        SERIALIZEDATA(s, m_chrReg[0]);
        SERIALIZEDATA(s, m_chrReg[1]);
        SERIALIZEDATA(s, m_chrReg[2]);
        SERIALIZEDATA(s, m_chrReg[3]);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_outerPrg);
        SERIALIZEDATA(s, m_outerChr);
        SERIALIZEDATA(s, m_verticalMirroring);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_irqFlag);
        SERIALIZEDATA(s, m_irqA12Counter);
        SERIALIZEDATA(s, m_lastA12);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_m2Div4);
    }
};
