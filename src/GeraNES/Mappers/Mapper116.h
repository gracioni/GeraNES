#pragma once

#include "BaseMapper.h"

// iNES Mapper 116
class Mapper116 : public BaseMapper
{
private:
    uint8_t m_mode = 0;

    uint8_t m_vrc2Chr[8] = {0xFF, 0xFF, 0xFF, 0xFF, 4, 5, 6, 7};
    uint8_t m_vrc2Prg[2] = {0, 1};
    uint8_t m_vrc2Mirroring = 0;

    uint8_t m_mmc3Reg[10] = {0, 2, 4, 5, 6, 7, 0xFC, 0xFD, 0xFE, 0xFF};
    uint8_t m_mmc3Ctrl = 0;
    uint8_t m_mmc3Mirroring = 0;

    uint8_t m_mmc1Reg[4] = {0x0C, 0, 0, 0};
    uint8_t m_mmc1Buffer = 0;
    uint8_t m_mmc1Shift = 0;

    uint8_t m_irqCounter = 0;
    uint8_t m_irqReloadValue = 0;
    bool m_irqReload = false;
    bool m_irqEnabled = false;
    bool m_interruptFlag = false;

    bool m_a12LastState = true;
    uint8_t m_cycleCounter = 0;

    GERANES_INLINE uint16_t mapChr1k(uint16_t bank) const
    {
        return static_cast<uint16_t>(bank | ((m_mode & 0x04) << 6));
    }

    GERANES_INLINE void writeModeRegister(uint8_t value, bool resetMmc1)
    {
        m_mode = value;
        if(resetMmc1) {
            m_mmc1Reg[0] = 0x0C;
            m_mmc1Reg[3] = 0;
            m_mmc1Buffer = 0;
            m_mmc1Shift = 0;
        }
    }

    GERANES_INLINE void writeVrc2Register(uint16_t absolute, uint8_t value)
    {
        if(absolute >= 0xB000 && absolute <= 0xE003) {
            const int regIndex = ((((absolute & 0x0002) | (absolute >> 10)) >> 1) + 2) & 0x07;
            const int lowHighNibble = (absolute & 0x0001) << 2;
            m_vrc2Chr[regIndex] = static_cast<uint8_t>((m_vrc2Chr[regIndex] & (0xF0 >> lowHighNibble)) | ((value & 0x0F) << lowHighNibble));
            return;
        }

        switch(absolute & 0xF000) {
        case 0x8000: m_vrc2Prg[0] = value; break;
        case 0xA000: m_vrc2Prg[1] = value; break;
        case 0x9000: m_vrc2Mirroring = value; break;
        default: break;
        }
    }

    GERANES_INLINE void writeMmc3Register(uint16_t absolute, uint8_t value)
    {
        switch(absolute & 0xE001) {
        case 0x8000: m_mmc3Ctrl = value; break;
        case 0x8001: m_mmc3Reg[m_mmc3Ctrl & 0x07] = value; break;
        case 0xA000: m_mmc3Mirroring = value; break;
        case 0xC000: m_irqReloadValue = value; break;
        case 0xC001: m_irqReload = true; break;
        case 0xE000:
            m_irqEnabled = false;
            m_interruptFlag = false;
            break;
        case 0xE001: m_irqEnabled = true; break;
        default: break;
        }
    }

    GERANES_INLINE void writeMmc1Register(uint16_t absolute, uint8_t value)
    {
        if(value & 0x80) {
            m_mmc1Reg[0] |= 0x0C;
            m_mmc1Buffer = 0;
            m_mmc1Shift = 0;
            return;
        }

        const uint8_t regIndex = static_cast<uint8_t>((absolute >> 13) - 4);
        m_mmc1Buffer |= static_cast<uint8_t>((value & 0x01) << m_mmc1Shift++);
        if(m_mmc1Shift == 5) {
            m_mmc1Reg[regIndex] = m_mmc1Buffer;
            m_mmc1Buffer = 0;
            m_mmc1Shift = 0;
        }
    }

public:
    Mapper116(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        if((addr & 0x0100) != 0) {
            writeModeRegister(value, (addr & 0x0001) != 0);
        }
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t value) override
    {
        if((addr & 0x0100) != 0) {
            writeModeRegister(value, (addr & 0x0001) != 0);
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        switch(m_mode & 0x03) {
        case 0: writeVrc2Register(absolute, value); break;
        case 1: writeMmc3Register(absolute, value); break;
        case 2:
        case 3: writeMmc1Register(absolute, value); break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch(m_mode & 0x03) {
        case 0:
            switch((addr >> 13) & 0x03) {
            case 0: return cd().readPrg<BankSize::B8K>(m_vrc2Prg[0], addr);
            case 1: return cd().readPrg<BankSize::B8K>(m_vrc2Prg[1], addr);
            case 2: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 2, addr);
            default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
            }

        case 1:
        {
            const uint8_t prgMode = static_cast<uint8_t>((m_mmc3Ctrl >> 5) & 0x02);
            switch((addr >> 13) & 0x03) {
            case 0: return cd().readPrg<BankSize::B8K>(m_mmc3Reg[6 + prgMode], addr);
            case 1: return cd().readPrg<BankSize::B8K>(m_mmc3Reg[7], addr);
            case 2: return cd().readPrg<BankSize::B8K>(m_mmc3Reg[6 + (prgMode ^ 0x02)], addr);
            default: return cd().readPrg<BankSize::B8K>(m_mmc3Reg[9], addr);
            }
        }

        default:
        {
            const uint8_t bank = static_cast<uint8_t>(m_mmc1Reg[3] & 0x0F);
            if((m_mmc1Reg[0] & 0x08) != 0) {
                if((m_mmc1Reg[0] & 0x04) != 0) {
                    if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(bank, addr);
                    return cd().readPrg<BankSize::B16K>(0x0F, addr);
                }

                if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(0, addr);
                return cd().readPrg<BankSize::B16K>(bank, addr);
            }

            return cd().readPrg<BankSize::B32K>(static_cast<uint8_t>((bank & 0x0E) >> 1), addr);
        }
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        uint16_t bank = 0;

        switch(m_mode & 0x03) {
        case 0:
            bank = mapChr1k(m_vrc2Chr[(addr >> 10) & 0x07]);
            break;

        case 1:
        {
            const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
            const uint8_t slotSwap = (m_mmc3Ctrl & 0x80) ? 4 : 0;
            const uint8_t physical = static_cast<uint8_t>(slot ^ slotSwap);

            if(physical < 2) bank = mapChr1k(static_cast<uint16_t>((m_mmc3Reg[0] & 0xFE) + physical));
            else if(physical < 4) bank = mapChr1k(static_cast<uint16_t>((m_mmc3Reg[1] & 0xFE) + (physical - 2)));
            else bank = mapChr1k(m_mmc3Reg[physical - 2]);
            break;
        }

        default:
            if((m_mmc1Reg[0] & 0x10) != 0) {
                if(addr < 0x1000) return cd().readChr<BankSize::B4K>(m_mmc1Reg[1], addr);
                return cd().readChr<BankSize::B4K>(m_mmc1Reg[2], addr);
            }

            return cd().readChr<BankSize::B8K>(static_cast<uint8_t>((m_mmc1Reg[1] & 0xFE) >> 1), addr);
        }

        if(hasChrRam()) return readChrRam<BankSize::B1K>(bank, addr);
        return cd().readChr<BankSize::B1K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;

        uint16_t bank = 0;
        switch(m_mode & 0x03) {
        case 0:
            bank = mapChr1k(m_vrc2Chr[(addr >> 10) & 0x07]);
            break;
        case 1:
        {
            const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
            const uint8_t slotSwap = (m_mmc3Ctrl & 0x80) ? 4 : 0;
            const uint8_t physical = static_cast<uint8_t>(slot ^ slotSwap);
            if(physical < 2) bank = mapChr1k(static_cast<uint16_t>((m_mmc3Reg[0] & 0xFE) + physical));
            else if(physical < 4) bank = mapChr1k(static_cast<uint16_t>((m_mmc3Reg[1] & 0xFE) + (physical - 2)));
            else bank = mapChr1k(m_mmc3Reg[physical - 2]);
            break;
        }
        default:
            return;
        }

        writeChrRam<BankSize::B1K>(bank, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_mode & 0x03) {
        case 0: return (m_vrc2Mirroring & 0x01) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
        case 1: return (m_mmc3Mirroring & 0x01) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
        default:
            switch(m_mmc1Reg[0] & 0x03) {
            case 0: return MirroringType::SINGLE_SCREEN_A;
            case 1: return MirroringType::SINGLE_SCREEN_B;
            case 2: return MirroringType::VERTICAL;
            default: return MirroringType::HORIZONTAL;
            }
        }
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    void setA12State(bool state) override
    {
        if((m_mode & 0x03) != 1) {
            m_a12LastState = state;
            return;
        }

        if(!m_a12LastState && state) {
            if(m_cycleCounter > 3) {
                if(m_irqCounter == 0 || m_irqReload) {
                    m_irqCounter = m_irqReloadValue;
                } else {
                    --m_irqCounter;
                }

                if(m_irqCounter == 0 && m_irqEnabled) {
                    m_interruptFlag = true;
                }
                m_irqReload = false;
            }
        } else if(m_a12LastState && !state) {
            m_cycleCounter = 0;
        }

        m_a12LastState = state;
    }

    void cycle() override
    {
        if(static_cast<uint8_t>(m_cycleCounter + 1) != 0) {
            ++m_cycleCounter;
        }
    }

    void reset() override
    {
        m_mode = 0;
        uint8_t vrc2Chr[8] = {0xFF, 0xFF, 0xFF, 0xFF, 4, 5, 6, 7};
        memcpy(m_vrc2Chr, vrc2Chr, sizeof(m_vrc2Chr));
        m_vrc2Prg[0] = 0;
        m_vrc2Prg[1] = 1;
        m_vrc2Mirroring = 0;

        uint8_t mmc3Reg[10] = {0, 2, 4, 5, 6, 7, 0xFC, 0xFD, 0xFE, 0xFF};
        memcpy(m_mmc3Reg, mmc3Reg, sizeof(m_mmc3Reg));
        m_mmc3Ctrl = 0;
        m_mmc3Mirroring = 0;

        m_mmc1Reg[0] = 0x0C;
        m_mmc1Reg[1] = 0;
        m_mmc1Reg[2] = 0;
        m_mmc1Reg[3] = 0;
        m_mmc1Buffer = 0;
        m_mmc1Shift = 0;

        m_irqCounter = 0;
        m_irqReloadValue = 0;
        m_irqReload = false;
        m_irqEnabled = false;
        m_interruptFlag = false;
        m_a12LastState = true;
        m_cycleCounter = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_mode);
        s.array(m_vrc2Chr, 1, 8);
        s.array(m_vrc2Prg, 1, 2);
        SERIALIZEDATA(s, m_vrc2Mirroring);
        s.array(m_mmc3Reg, 1, 10);
        SERIALIZEDATA(s, m_mmc3Ctrl);
        SERIALIZEDATA(s, m_mmc3Mirroring);
        s.array(m_mmc1Reg, 1, 4);
        SERIALIZEDATA(s, m_mmc1Buffer);
        SERIALIZEDATA(s, m_mmc1Shift);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqReloadValue);
        SERIALIZEDATA(s, m_irqReload);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_a12LastState);
        SERIALIZEDATA(s, m_cycleCounter);
    }
};
