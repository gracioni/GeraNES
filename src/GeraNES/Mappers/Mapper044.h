#pragma once

#include "Mapper004.h"

class Mapper044 : public Mapper004
{
private:
    uint8_t m_block = 0;

    GERANES_INLINE void clampBlock()
    {
        if(m_block == 7) m_block = 6;
    }

    GERANES_INLINE uint8_t mapPrgPage(uint8_t mmc3Page8k) const
    {
        const uint8_t prgAnd = m_block >= 6 ? 0x1F : 0x0F;
        const uint8_t prgOr = static_cast<uint8_t>(m_block >= 6 ? 0x60 : (m_block << 4));
        return static_cast<uint8_t>((mmc3Page8k & prgAnd) | prgOr) & m_prgMask;
    }

    GERANES_INLINE uint16_t mapChrPage(uint16_t page1k) const
    {
        const uint16_t chrAnd = m_block >= 6 ? 0x00FF : 0x007F;
        const uint16_t chrOr = static_cast<uint16_t>(m_block >= 6 ? 0x0300 : (m_block << 7));
        return static_cast<uint16_t>((page1k & chrAnd) | chrOr) & m_chrMask;
    }

    GERANES_INLINE uint16_t currentChrPage1k(int slot) const
    {
        if(!m_chrMode) {
            switch(slot) {
            case 0: return static_cast<uint16_t>(m_chrReg[0] & 0xFE);
            case 1: return static_cast<uint16_t>(m_chrReg[0] | 0x01);
            case 2: return static_cast<uint16_t>(m_chrReg[1] & 0xFE);
            case 3: return static_cast<uint16_t>(m_chrReg[1] | 0x01);
            case 4: return m_chrReg[2];
            case 5: return m_chrReg[3];
            case 6: return m_chrReg[4];
            default: return m_chrReg[5];
            }
        }

        switch(slot) {
        case 0: return m_chrReg[2];
        case 1: return m_chrReg[3];
        case 2: return m_chrReg[4];
        case 3: return m_chrReg[5];
        case 4: return static_cast<uint16_t>(m_chrReg[0] & 0xFE);
        case 5: return static_cast<uint16_t>(m_chrReg[0] | 0x01);
        case 6: return static_cast<uint16_t>(m_chrReg[1] & 0xFE);
        default: return static_cast<uint16_t>(m_chrReg[1] | 0x01);
        }
    }

public:
    Mapper044(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const int reg = addr & 0xF001;
        if(reg == 0x2001) {
            m_enableWRAM = data & 0x80;
            m_writeProtectWRAM = data & 0x40;
            m_block = static_cast<uint8_t>(data & 0x07);
            clampBlock();
            return;
        }

        Mapper004::writePrg(addr, data);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t fixedSecondLast = static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 2);
        const uint8_t fixedLast = static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1);

        if(!m_prgMode) {
            switch(addr >> 13) {
            case 0: return cd().readPrg<BankSize::B8K>(mapPrgPage(m_prgReg0), addr);
            case 1: return cd().readPrg<BankSize::B8K>(mapPrgPage(m_prgReg1), addr);
            case 2: return cd().readPrg<BankSize::B8K>(mapPrgPage(fixedSecondLast), addr);
            default: return cd().readPrg<BankSize::B8K>(mapPrgPage(fixedLast), addr);
            }
        }

        switch(addr >> 13) {
        case 0: return cd().readPrg<BankSize::B8K>(mapPrgPage(fixedSecondLast), addr);
        case 1: return cd().readPrg<BankSize::B8K>(mapPrgPage(m_prgReg1), addr);
        case 2: return cd().readPrg<BankSize::B8K>(mapPrgPage(m_prgReg0), addr);
        default: return cd().readPrg<BankSize::B8K>(mapPrgPage(fixedLast), addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return readChrBank<BankSize::B1K>(mapChrPage(currentChrPage1k((addr >> 10) & 0x07)), addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        writeChrBank<BankSize::B1K>(mapChrPage(currentChrPage1k((addr >> 10) & 0x07)), addr, data);
    }

    void reset() override
    {
        Mapper004::reset();
        m_block = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_block);
    }
};
