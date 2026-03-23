#pragma once

#include "Mapper004.h"

class Mapper134 : public Mapper004
{
private:
    uint8_t m_exReg = 0;

    GERANES_INLINE uint16_t mappedPrgBank(uint8_t slot) const
    {
        uint16_t bank = 0;
        if(!m_prgMode) {
            switch(slot & 0x03) {
            case 0: bank = m_prgReg0; break;
            case 1: bank = m_prgReg1; break;
            case 2: bank = static_cast<uint16_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 2); break;
            default: bank = static_cast<uint16_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1); break;
            }
        } else {
            switch(slot & 0x03) {
            case 0: bank = static_cast<uint16_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 2); break;
            case 1: bank = m_prgReg1; break;
            case 2: bank = m_prgReg0; break;
            default: bank = static_cast<uint16_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1); break;
            }
        }
        return static_cast<uint16_t>((bank & 0x1F) | ((m_exReg & 0x02) << 4));
    }

    GERANES_INLINE uint16_t mappedChrBank(uint8_t slot) const
    {
        uint16_t bank = 0;
        if(!m_chrMode) {
            switch(slot & 0x07) {
            case 0: bank = static_cast<uint16_t>((m_chrReg[0] & m_chrMask & 0xFE) + 0); break;
            case 1: bank = static_cast<uint16_t>((m_chrReg[0] & m_chrMask & 0xFE) + 1); break;
            case 2: bank = static_cast<uint16_t>((m_chrReg[1] & m_chrMask & 0xFE) + 0); break;
            case 3: bank = static_cast<uint16_t>((m_chrReg[1] & m_chrMask & 0xFE) + 1); break;
            case 4: bank = m_chrReg[2] & m_chrMask; break;
            case 5: bank = m_chrReg[3] & m_chrMask; break;
            case 6: bank = m_chrReg[4] & m_chrMask; break;
            default: bank = m_chrReg[5] & m_chrMask; break;
            }
        } else {
            switch(slot & 0x07) {
            case 0: bank = m_chrReg[2] & m_chrMask; break;
            case 1: bank = m_chrReg[3] & m_chrMask; break;
            case 2: bank = m_chrReg[4] & m_chrMask; break;
            case 3: bank = m_chrReg[5] & m_chrMask; break;
            case 4: bank = static_cast<uint16_t>((m_chrReg[0] & m_chrMask & 0xFE) + 0); break;
            case 5: bank = static_cast<uint16_t>((m_chrReg[0] & m_chrMask & 0xFE) + 1); break;
            case 6: bank = static_cast<uint16_t>((m_chrReg[1] & m_chrMask & 0xFE) + 0); break;
            default: bank = static_cast<uint16_t>((m_chrReg[1] & m_chrMask & 0xFE) + 1); break;
            }
        }
        return static_cast<uint16_t>((bank & 0xFF) | ((m_exReg & 0x20) << 3));
    }

public:
    Mapper134(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t value) override
    {
        if(addr == 0x0001) m_exReg = value;
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
        m_exReg = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_exReg);
    }
};
