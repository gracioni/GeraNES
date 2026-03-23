#pragma once

#include "../Mapper004.h"

class Mmc3ChrRamBase : public Mapper004
{
private:
    uint16_t m_firstRamBank = 0;
    uint16_t m_lastRamBank = 0;
    uint16_t m_chrRamBanks = 0;

    GERANES_INLINE uint16_t currentChrPage1k(int slot) const
    {
        if(!m_chrMode) {
            switch(slot & 0x07) {
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

        switch(slot & 0x07) {
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

protected:
    GERANES_INLINE bool isChrRamPage(uint16_t page) const
    {
        return page >= m_firstRamBank && page <= m_lastRamBank;
    }

    GERANES_INLINE uint16_t mapChrRamPage(uint16_t page) const
    {
        if(m_chrRamBanks == 0) return 0;
        return static_cast<uint16_t>((page - m_firstRamBank) % m_chrRamBanks);
    }

public:
    Mmc3ChrRamBase(ICartridgeData& cd, uint16_t firstRamBank, uint16_t lastRamBank, uint16_t chrRamBanks)
        : Mapper004(cd), m_firstRamBank(firstRamBank), m_lastRamBank(lastRamBank), m_chrRamBanks(chrRamBanks)
    {
        const int size = static_cast<int>(chrRamBanks) * static_cast<int>(BankSize::B1K);
        if(cd.chrRamSize() < size) {
            allocateChrRam(size);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint16_t page = currentChrPage1k((addr >> 10) & 0x07);
        if(isChrRamPage(page)) return readChrRam<BankSize::B1K>(mapChrRamPage(page), addr);
        return cd().readChr<BankSize::B1K>(page & m_chrMask, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        const uint16_t page = currentChrPage1k((addr >> 10) & 0x07);
        if(isChrRamPage(page)) {
            writeChrRam<BankSize::B1K>(mapChrRamPage(page), addr, data);
        }
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_firstRamBank);
        SERIALIZEDATA(s, m_lastRamBank);
        SERIALIZEDATA(s, m_chrRamBanks);
    }
};
