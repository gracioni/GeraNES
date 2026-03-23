#pragma once

#include "Mapper004.h"

class Mapper187 : public Mapper004
{
private:
    uint8_t m_overrideReg = 0;

    GERANES_INLINE bool overrideEnabled() const
    {
        return (m_overrideReg & 0x80) != 0;
    }

    GERANES_INLINE uint8_t currentChrPage1k(int slot) const
    {
        if(!m_chrMode) {
            switch(slot) {
            case 0: return static_cast<uint8_t>(m_chrReg[0] & 0xFE);
            case 1: return static_cast<uint8_t>(m_chrReg[0] | 0x01);
            case 2: return static_cast<uint8_t>(m_chrReg[1] & 0xFE);
            case 3: return static_cast<uint8_t>(m_chrReg[1] | 0x01);
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
        case 4: return static_cast<uint8_t>(m_chrReg[0] & 0xFE);
        case 5: return static_cast<uint8_t>(m_chrReg[0] | 0x01);
        case 6: return static_cast<uint8_t>(m_chrReg[1] & 0xFE);
        default: return static_cast<uint8_t>(m_chrReg[1] | 0x01);
        }
    }

    GERANES_INLINE uint16_t mappedChrPage(int addr) const
    {
        uint16_t page = currentChrPage1k((addr >> 10) & 0x07);
        if(cd().numberOfCHRBanks<BankSize::B1K>() > 0x100) {
            page |= static_cast<uint16_t>((addr >> 12) & 0x01) << 8;
        }
        return page & m_chrMask;
    }

public:
    Mapper187(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        if((addr & 0x1001) == 0x1000) {
            m_overrideReg = value;
        }
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        if((addr & 0x1000) != 0) {
            return static_cast<uint8_t>(openBusData | 0x80);
        }
        return openBusData;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(!overrideEnabled()) {
            return Mapper004::readPrg(addr);
        }

        const uint8_t baseBank = static_cast<uint8_t>((m_overrideReg >> 1) & 0x0F);
        uint8_t bank = baseBank;

        if((m_overrideReg & 0x20) != 0) {
            bank = static_cast<uint8_t>((baseBank & 0x0E) | ((addr >> 14) & 0x01));
        }

        return cd().readPrg<BankSize::B16K>(bank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return readChrBank<BankSize::B1K>(mappedChrPage(addr), addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        writeChrBank<BankSize::B1K>(mappedChrPage(addr), addr, data);
    }

    void reset() override
    {
        Mapper004::reset();
        m_overrideReg = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_overrideReg);
    }
};
