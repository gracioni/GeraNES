#pragma once

#include "Mapper004.h"

// iNES Mapper 249
// MMC3 variant with an extra register at CPU $5000 that scrambles PRG/CHR bits.
class Mapper249 : public Mapper004
{
private:
    uint8_t m_exReg = 0;

    GERANES_INLINE uint16_t mapPrgBank(uint16_t bank) const
    {
        if((m_exReg & 0x02) == 0) return bank;

        if(bank < 0x20) {
            return static_cast<uint16_t>(
                (bank & 0x01) |
                ((bank >> 3) & 0x02) |
                ((bank >> 1) & 0x04) |
                ((bank << 2) & 0x18)
            );
        }

        bank = static_cast<uint16_t>(bank - 0x20);
        return static_cast<uint16_t>(
            (bank & 0x03) |
            ((bank >> 1) & 0x04) |
            ((bank >> 4) & 0x08) |
            ((bank >> 2) & 0x10) |
            ((bank << 3) & 0x20) |
            ((bank << 2) & 0xC0)
        );
    }

    GERANES_INLINE uint16_t mapChrBank(uint16_t bank) const
    {
        if((m_exReg & 0x02) == 0) return bank;

        return static_cast<uint16_t>(
            (bank & 0x03) |
            ((bank >> 1) & 0x04) |
            ((bank >> 4) & 0x08) |
            ((bank >> 2) & 0x10) |
            ((bank << 3) & 0x20) |
            ((bank << 2) & 0xC0)
        );
    }

    GERANES_INLINE uint16_t currentPrgBank8k(uint8_t slot) const
    {
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

    GERANES_INLINE uint16_t currentChrBank1k(uint8_t slot) const
    {
        if(!m_chrMode) {
            switch(slot & 0x07) {
            case 0: return static_cast<uint16_t>(m_chrReg[0] & m_chrMask & 0xFE);
            case 1: return static_cast<uint16_t>((m_chrReg[0] & m_chrMask & 0xFE) + 1);
            case 2: return static_cast<uint16_t>(m_chrReg[1] & m_chrMask & 0xFE);
            case 3: return static_cast<uint16_t>((m_chrReg[1] & m_chrMask & 0xFE) + 1);
            case 4: return m_chrReg[2] & m_chrMask;
            case 5: return m_chrReg[3] & m_chrMask;
            case 6: return m_chrReg[4] & m_chrMask;
            default: return m_chrReg[5] & m_chrMask;
            }
        }

        switch(slot & 0x07) {
        case 0: return m_chrReg[2] & m_chrMask;
        case 1: return m_chrReg[3] & m_chrMask;
        case 2: return m_chrReg[4] & m_chrMask;
        case 3: return m_chrReg[5] & m_chrMask;
        case 4: return static_cast<uint16_t>(m_chrReg[0] & m_chrMask & 0xFE);
        case 5: return static_cast<uint16_t>((m_chrReg[0] & m_chrMask & 0xFE) + 1);
        case 6: return static_cast<uint16_t>(m_chrReg[1] & m_chrMask & 0xFE);
        default: return static_cast<uint16_t>((m_chrReg[1] & m_chrMask & 0xFE) + 1);
        }
    }

public:
    Mapper249(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t data) override
    {
        if(addr == 0x1000) {
            m_exReg = data;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const int totalBanks = cd().numberOfPRGBanks<BankSize::B8K>();
        const uint8_t slot = static_cast<uint8_t>((addr >> 13) & 0x03);
        const uint16_t bank = mapPrgBank(currentPrgBank8k(slot));
        return cd().readPrg<BankSize::B8K>(bank % totalBanks, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        const uint16_t bank = mapChrBank(currentChrBank1k(slot));

        if(hasChrRam()) {
            const int totalBanks = static_cast<int>(cd().chrRamSize() / static_cast<int>(BankSize::B1K));
            return readChrRam<BankSize::B1K>(totalBanks > 0 ? static_cast<int>(bank % totalBanks) : 0, addr);
        }

        const int totalBanks = cd().numberOfCHRBanks<BankSize::B1K>();
        return cd().readChr<BankSize::B1K>(bank % totalBanks, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;

        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        const uint16_t bank = mapChrBank(currentChrBank1k(slot));
        const int totalBanks = static_cast<int>(cd().chrRamSize() / static_cast<int>(BankSize::B1K));
        writeChrRam<BankSize::B1K>(totalBanks > 0 ? static_cast<int>(bank % totalBanks) : 0, addr, data);
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
