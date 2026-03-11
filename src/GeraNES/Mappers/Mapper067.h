#pragma once

#include "BaseMapper.h"

// Sunsoft-3 (iNES Mapper 67)
class Mapper067 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;

    uint8_t m_chrBank[4] = {0, 1, 2, 3};
    uint8_t m_chrMask = 0;

    uint8_t m_mirroring = 0;

    uint16_t m_irqCounter = 0;
    bool m_irqCounterEnabled = false;
    bool m_irqHighByteNext = true;
    bool m_interruptFlag = false;

    template<BankSize bs>
    GERANES_INLINE uint8_t readChrBank(int bank, int addr)
    {
        if(hasChrRam()) return readChrRam<bs>(bank, addr);
        return cd().readChr<bs>(bank, addr);
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrBank(int bank, int addr, uint8_t data)
    {
        if(hasChrRam()) writeChrRam<bs>(bank, addr, data);
    }

public:
    Mapper067(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B2K));
        }
        else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B2K>());
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const int fullAddr = addr + 0x8000;

        // IRQ acknowledge ($8000, mask $8800)
        if((fullAddr & 0x8800) == 0x8000) {
            m_interruptFlag = false;
            return;
        }

        switch(fullAddr & 0xF800) {
        case 0x8800: m_chrBank[0] = (data & 0x3F) & m_chrMask; break;
        case 0x9800: m_chrBank[1] = (data & 0x3F) & m_chrMask; break;
        case 0xA800: m_chrBank[2] = (data & 0x3F) & m_chrMask; break;
        case 0xB800: m_chrBank[3] = (data & 0x3F) & m_chrMask; break;

        case 0xC800:
            if(m_irqHighByteNext) {
                m_irqCounter = (m_irqCounter & 0x00FF) | (static_cast<uint16_t>(data) << 8);
            }
            else {
                m_irqCounter = (m_irqCounter & 0xFF00) | data;
            }
            m_irqHighByteNext = !m_irqHighByteNext;
            break;

        case 0xD800:
            m_irqCounterEnabled = (data & 0x10) != 0;
            m_irqHighByteNext = true;
            break;

        case 0xE800:
            m_mirroring = data & 0x03;
            break;

        case 0xF800:
            m_prgBank = (data & 0x0F) & m_prgMask;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBank, addr);

        return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>() - 1, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 11) & 0x03);
        return readChrBank<BankSize::B2K>(m_chrBank[slot] & m_chrMask, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 11) & 0x03);
        writeChrBank<BankSize::B2K>(m_chrBank[slot] & m_chrMask, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;

        switch(m_mirroring) {
        case 0: return MirroringType::VERTICAL;
        case 1: return MirroringType::HORIZONTAL;
        case 2: return MirroringType::SINGLE_SCREEN_A;
        case 3: return MirroringType::SINGLE_SCREEN_B;
        }

        return MirroringType::VERTICAL;
    }

    GERANES_HOT void cycle() override
    {
        if(!m_irqCounterEnabled) return;

        if(m_irqCounter == 0) {
            m_irqCounter = 0xFFFF;
            m_interruptFlag = true;
            m_irqCounterEnabled = false;
            return;
        }

        --m_irqCounter;
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return m_interruptFlag;
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBank[0] = 0;
        m_chrBank[1] = 1;
        m_chrBank[2] = 2;
        m_chrBank[3] = 3;

        m_mirroring = (cd().mirroringType() == MirroringType::HORIZONTAL) ? 1 : 0;

        m_irqCounter = 0;
        m_irqCounterEnabled = false;
        m_irqHighByteNext = true;
        m_interruptFlag = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        s.array(m_chrBank, 1, 4);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_mirroring);
        SERIALIZEDATA(s, m_irqCounter);
        SERIALIZEDATA(s, m_irqCounterEnabled);
        SERIALIZEDATA(s, m_irqHighByteNext);
        SERIALIZEDATA(s, m_interruptFlag);
    }
};
