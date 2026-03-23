#pragma once

#include "BaseMapper.h"

// iNES Mapper 253
// Waixing board with mixed CHR-ROM / 2KB CHR-RAM windows and CPU-cycle IRQs.
class Mapper253 : public BaseMapper
{
private:
    uint8_t m_chrLow[8] = {0};
    uint8_t m_chrHigh[8] = {0};
    bool m_forceChrRom = false;

    uint8_t m_prgReg[2] = {0};
    uint8_t m_prgMask = 0;
    int m_chrRom1kBanks = 0;

    uint8_t m_irqReloadValue = 0;
    uint8_t m_irqCounter = 0;
    bool m_irqEnabled = false;
    uint16_t m_irqScaler = 0;
    bool m_interruptFlag = false;

    uint8_t m_mirroring = 0;

    GERANES_INLINE uint16_t chrPageForSlot(uint8_t slot) const
    {
        return static_cast<uint16_t>(m_chrLow[slot] | (static_cast<uint16_t>(m_chrHigh[slot]) << 8));
    }

    GERANES_INLINE bool useChrRamPage(uint8_t slot) const
    {
        return (m_chrLow[slot] == 4 || m_chrLow[slot] == 5) && !m_forceChrRom;
    }

public:
    Mapper253(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.chrRamSize() == 0) allocateChrRam(0x800);
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        m_chrRom1kBanks = cd.numberOfCHRBanks<BankSize::B1K>();
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        if(absolute >= 0xB000 && absolute <= 0xE00C) {
            const uint8_t slot = static_cast<uint8_t>((((((absolute & 0x0008) | (absolute >> 8)) >> 3) + 2) & 0x07));
            const uint8_t shift = static_cast<uint8_t>(absolute & 0x0004);
            const uint8_t chrLow = static_cast<uint8_t>((m_chrLow[slot] & (0xF0 >> shift)) | ((value & 0x0F) << shift));
            m_chrLow[slot] = chrLow;

            if(slot == 0) {
                if(chrLow == 0xC8) {
                    m_forceChrRom = false;
                } else if(chrLow == 0x88) {
                    m_forceChrRom = true;
                }
            }

            if(shift != 0) {
                m_chrHigh[slot] = static_cast<uint8_t>(value >> 4);
            }

            return;
        }

        switch(absolute) {
        case 0x8010:
            m_prgReg[0] = value & m_prgMask;
            break;
        case 0xA010:
            m_prgReg[1] = value & m_prgMask;
            break;
        case 0x9400:
            m_mirroring = value & 0x03;
            break;
        case 0xF000:
            m_irqReloadValue = static_cast<uint8_t>((m_irqReloadValue & 0xF0) | (value & 0x0F));
            m_interruptFlag = false;
            break;
        case 0xF004:
            m_irqReloadValue = static_cast<uint8_t>((m_irqReloadValue & 0x0F) | ((value & 0x0F) << 4));
            m_interruptFlag = false;
            break;
        case 0xF008:
            m_irqCounter = m_irqReloadValue;
            m_irqEnabled = (value & 0x02) != 0;
            m_irqScaler = 0;
            m_interruptFlag = false;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_prgReg[0], addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_prgReg[1], addr);
        case 2: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 2, addr);
        default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        const uint16_t page = chrPageForSlot(slot);

        if(useChrRamPage(slot)) {
            return readChrRam<BankSize::B1K>(page & 0x01, addr);
        }

        if(hasChrRam() && m_chrRom1kBanks == 0) {
            return readChrRam<BankSize::B1K>(page & 0x01, addr);
        }

        return cd().readChr<BankSize::B1K>(m_chrRom1kBanks > 0 ? page % m_chrRom1kBanks : 0, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        const uint16_t page = chrPageForSlot(slot);

        if(useChrRamPage(slot) || (hasChrRam() && m_chrRom1kBanks == 0)) {
            writeChrRam<BankSize::B1K>(page & 0x01, addr, data);
        }
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

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    GERANES_HOT void cycle() override
    {
        if(!m_irqEnabled) return;

        ++m_irqScaler;
        if(m_irqScaler < 114) return;

        m_irqScaler = 0;
        ++m_irqCounter;
        if(m_irqCounter == 0) {
            m_irqCounter = m_irqReloadValue;
            m_interruptFlag = true;
        }
    }

    void reset() override
    {
        memset(m_chrLow, 0, sizeof(m_chrLow));
        memset(m_chrHigh, 0, sizeof(m_chrHigh));
        m_forceChrRom = false;
        m_prgReg[0] = 0;
        m_prgReg[1] = 0;
        m_irqReloadValue = 0;
        m_irqCounter = 0;
        m_irqEnabled = false;
        m_irqScaler = 0;
        m_interruptFlag = false;
        m_mirroring = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_chrLow, 1, 8);
        s.array(m_chrHigh, 1, 8);
        SERIALIZEDATA(s, m_forceChrRom);
        s.array(m_prgReg, 1, 2);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrRom1kBanks);
        SERIALIZEDATA(s, m_irqReloadValue);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqEnabled);
        SERIALIZEDATA(s, m_irqScaler);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_mirroring);
    }
};
