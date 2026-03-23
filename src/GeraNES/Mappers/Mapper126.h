#pragma once

#include "Mapper004.h"

class Mapper126 : public Mapper004
{
private:
    uint8_t m_exReg[4] = {0};

    GERANES_INLINE uint16_t chrOuterBank() const
    {
        const uint16_t reg = m_exReg[0];
        return static_cast<uint16_t>(
            ((~reg << 0) & 0x0080 & m_exReg[2]) |
            ((reg << 4) & 0x0080 & reg) |
            ((reg << 3) & 0x0100) |
            ((reg << 5) & 0x0200)
        );
    }

    GERANES_INLINE uint16_t mappedPrgBank(uint8_t slot) const
    {
        uint16_t page = 0;

        if(!m_prgMode) {
            switch(slot & 0x03) {
            case 0: page = m_prgReg0; break;
            case 1: page = m_prgReg1; break;
            case 2: page = static_cast<uint16_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 2); break;
            default: page = static_cast<uint16_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1); break;
            }
        } else {
            switch(slot & 0x03) {
            case 0: page = static_cast<uint16_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 2); break;
            case 1: page = m_prgReg1; break;
            case 2: page = m_prgReg0; break;
            default: page = static_cast<uint16_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1); break;
            }
        }

        const uint16_t reg = m_exReg[0];
        page &= static_cast<uint16_t>(((~reg >> 2) & 0x10) | 0x0F);
        page |= static_cast<uint16_t>(((reg & (0x06 | ((reg & 0x40) >> 6))) << 4) | ((reg & 0x10) << 3));

        if((m_exReg[3] & 0x03) == 0x03) {
            return static_cast<uint16_t>((page & ~0x03) | (slot & 0x03));
        }
        if((m_exReg[3] & 0x03) != 0) {
            return static_cast<uint16_t>((page & ~0x01) | (slot & 0x01));
        }
        return page;
    }

    GERANES_INLINE uint16_t mappedChrBank(uint8_t slot) const
    {
        if((m_exReg[3] & 0x10) != 0) {
            return static_cast<uint16_t>(chrOuterBank() | ((m_exReg[2] & 0x0F) << 3) | (slot & 0x07));
        }

        uint16_t page = 0;
        if(!m_chrMode) {
            switch(slot & 0x07) {
            case 0: page = static_cast<uint16_t>((m_chrReg[0] & m_chrMask & 0xFE) + 0); break;
            case 1: page = static_cast<uint16_t>((m_chrReg[0] & m_chrMask & 0xFE) + 1); break;
            case 2: page = static_cast<uint16_t>((m_chrReg[1] & m_chrMask & 0xFE) + 0); break;
            case 3: page = static_cast<uint16_t>((m_chrReg[1] & m_chrMask & 0xFE) + 1); break;
            case 4: page = m_chrReg[2] & m_chrMask; break;
            case 5: page = m_chrReg[3] & m_chrMask; break;
            case 6: page = m_chrReg[4] & m_chrMask; break;
            default: page = m_chrReg[5] & m_chrMask; break;
            }
        } else {
            switch(slot & 0x07) {
            case 0: page = m_chrReg[2] & m_chrMask; break;
            case 1: page = m_chrReg[3] & m_chrMask; break;
            case 2: page = m_chrReg[4] & m_chrMask; break;
            case 3: page = m_chrReg[5] & m_chrMask; break;
            case 4: page = static_cast<uint16_t>((m_chrReg[0] & m_chrMask & 0xFE) + 0); break;
            case 5: page = static_cast<uint16_t>((m_chrReg[0] & m_chrMask & 0xFE) + 1); break;
            case 6: page = static_cast<uint16_t>((m_chrReg[1] & m_chrMask & 0xFE) + 0); break;
            default: page = static_cast<uint16_t>((m_chrReg[1] & m_chrMask & 0xFE) + 1); break;
            }
        }

        return static_cast<uint16_t>(chrOuterBank() | (page & ((m_exReg[0] & 0x80) - 1)));
    }

public:
    Mapper126(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t value) override
    {
        const uint8_t reg = static_cast<uint8_t>(addr & 0x03);
        if(reg == 0x01 || reg == 0x02 || ((reg == 0x00 || reg == 0x03) && (m_exReg[3] & 0x80) == 0)) {
            m_exReg[reg] = value;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(mappedPrgBank(static_cast<uint8_t>((addr >> 13) & 0x03)), addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint16_t bank = mappedChrBank(static_cast<uint8_t>((addr >> 10) & 0x07));
        if(hasChrRam()) return readChrRam<BankSize::B1K>(bank, addr);
        return cd().readChr<BankSize::B1K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint16_t bank = mappedChrBank(static_cast<uint8_t>((addr >> 10) & 0x07));
        writeChrRam<BankSize::B1K>(bank, addr, data);
    }

    void reset() override
    {
        Mapper004::reset();
        memset(m_exReg, 0, sizeof(m_exReg));
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        s.array(m_exReg, 1, 4);
    }
};
