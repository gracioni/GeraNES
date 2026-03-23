#pragma once

#include "Mapper004.h"

class Mapper217 : public Mapper004
{
private:
    uint8_t m_exRegs[4] = {0, 0xFF, 0x03, 0};

    static constexpr uint8_t LUT[8] = { 0, 6, 3, 7, 5, 2, 4, 1 };

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

    GERANES_INLINE uint16_t mapChrPage(uint16_t page1k) const
    {
        if((m_exRegs[1] & 0x08) == 0) {
            page1k = static_cast<uint16_t>(((m_exRegs[1] << 3) & 0x80) | (page1k & 0x7F));
        }
        return static_cast<uint16_t>(((m_exRegs[1] << 8) & 0x0300) | page1k) & m_chrMask;
    }

    GERANES_INLINE uint8_t mapPrgPage(int slot, uint8_t page8k) const
    {
        if(m_exRegs[0] & 0x80) {
            uint8_t bank = static_cast<uint8_t>((m_exRegs[0] & 0x0F) | ((m_exRegs[1] << 4) & 0x30));
            bank = static_cast<uint8_t>(bank << 1);
            return static_cast<uint8_t>(bank + (slot & 0x01)) & m_prgMask;
        }

        if(m_exRegs[1] & 0x08) {
            page8k &= 0x1F;
        }
        else {
            page8k = static_cast<uint8_t>((page8k & 0x0F) | (m_exRegs[1] & 0x10));
        }

        return static_cast<uint8_t>((((m_exRegs[1] << 5) & 0x60) | page8k)) & m_prgMask;
    }

public:
    Mapper217(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writeMapperRegisterAbsolute(uint16_t addr, uint8_t value) override
    {
        switch(addr) {
        case 0x5000:
            m_exRegs[0] = value;
            break;
        case 0x5001:
            if(m_exRegs[1] != value) {
                m_exRegs[1] = value;
            }
            break;
        case 0x5007:
            m_exRegs[2] = value;
            break;
        default:
            return;
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        switch(absolute & 0xE001) {
        case 0x8000:
            Mapper004::writePrg((m_exRegs[2] ? 0xC000 : 0x8000) - 0x8000, value);
            break;
        case 0x8001:
            if(m_exRegs[2]) {
                value = static_cast<uint8_t>((value & 0xC0) | LUT[value & 0x07]);
                m_exRegs[3] = 1;
                Mapper004::writePrg(0x0000, value);
            }
            else {
                Mapper004::writePrg(0x0001, value);
            }
            break;
        case 0xA000:
            if(m_exRegs[2]) {
                if(m_exRegs[3] && (((m_exRegs[0] & 0x80) == 0) || m_addrReg < 6)) {
                    m_exRegs[3] = 0;
                    Mapper004::writePrg(0x0001, value);
                }
            }
            else {
                m_mirroring = (value & 0x01) != 0;
            }
            break;
        case 0xA001:
            if(m_exRegs[2]) {
                m_mirroring = (value & 0x01) != 0;
            }
            else {
                Mapper004::writePrg(0x2001, value);
            }
            break;
        default:
            Mapper004::writePrg(addr, value);
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t fixedSecondLast = static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 2);
        const uint8_t fixedLast = static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 1);

        if(!m_prgMode) {
            switch(addr >> 13) {
            case 0: return cd().readPrg<BankSize::B8K>(mapPrgPage(0, m_prgReg0), addr);
            case 1: return cd().readPrg<BankSize::B8K>(mapPrgPage(1, m_prgReg1), addr);
            case 2: return cd().readPrg<BankSize::B8K>(mapPrgPage(2, fixedSecondLast), addr);
            default: return cd().readPrg<BankSize::B8K>(mapPrgPage(3, fixedLast), addr);
            }
        }

        switch(addr >> 13) {
        case 0: return cd().readPrg<BankSize::B8K>(mapPrgPage(0, fixedSecondLast), addr);
        case 1: return cd().readPrg<BankSize::B8K>(mapPrgPage(1, m_prgReg1), addr);
        case 2: return cd().readPrg<BankSize::B8K>(mapPrgPage(2, m_prgReg0), addr);
        default: return cd().readPrg<BankSize::B8K>(mapPrgPage(3, fixedLast), addr);
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
        m_exRegs[0] = 0;
        m_exRegs[1] = 0xFF;
        m_exRegs[2] = 0x03;
        m_exRegs[3] = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        s.array(m_exRegs, 1, 4);
    }
};
