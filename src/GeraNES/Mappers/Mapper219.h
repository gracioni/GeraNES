#pragma once

#include "Mapper004.h"

class Mapper219 : public Mapper004
{
private:
    uint8_t m_exRegs[3] = {0, 0, 0};
    uint8_t m_prgPages[4] = {0, 1, 2, 3};
    uint16_t m_chrPages[8] = {0, 1, 2, 3, 4, 5, 6, 7};

    GERANES_INLINE void setPrgPage(int slot, uint8_t page8k)
    {
        m_prgPages[slot & 0x03] = page8k & m_prgMask;
    }

    GERANES_INLINE void setChrPage(int slot, uint16_t page1k)
    {
        m_chrPages[slot & 0x07] = page1k & m_chrMask;
    }

public:
    Mapper219(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        if(absolute < 0xA000) {
            switch(absolute & 0xE003) {
            case 0x8000:
                m_exRegs[0] = 0;
                m_exRegs[1] = value;
                return;

            case 0x8001:
                if(m_exRegs[0] >= 0x23 && m_exRegs[0] <= 0x26) {
                    const uint8_t prgBank = static_cast<uint8_t>(((value & 0x20) >> 5) | ((value & 0x10) >> 3) | ((value & 0x08) >> 1) | ((value & 0x04) << 1));
                    setPrgPage(0x26 - m_exRegs[0], prgBank);
                }

                switch(m_exRegs[1]) {
                case 0x08:
                case 0x0A:
                case 0x0E:
                case 0x12:
                case 0x16:
                case 0x1A:
                case 0x1E:
                    m_exRegs[2] = static_cast<uint8_t>(value << 4);
                    break;
                case 0x09: setChrPage(0, static_cast<uint16_t>(m_exRegs[2] | ((value >> 1) & 0x0E))); break;
                case 0x0B: setChrPage(1, static_cast<uint16_t>(m_exRegs[2] | ((value >> 1) | 0x01))); break;
                case 0x0C:
                case 0x0D: setChrPage(2, static_cast<uint16_t>(m_exRegs[2] | ((value >> 1) & 0x0E))); break;
                case 0x0F: setChrPage(3, static_cast<uint16_t>(m_exRegs[2] | ((value >> 1) | 0x01))); break;
                case 0x10:
                case 0x11: setChrPage(4, static_cast<uint16_t>(m_exRegs[2] | ((value >> 1) & 0x0F))); break;
                case 0x14:
                case 0x15: setChrPage(5, static_cast<uint16_t>(m_exRegs[2] | ((value >> 1) & 0x0F))); break;
                case 0x18:
                case 0x19: setChrPage(6, static_cast<uint16_t>(m_exRegs[2] | ((value >> 1) & 0x0F))); break;
                case 0x1C:
                case 0x1D: setChrPage(7, static_cast<uint16_t>(m_exRegs[2] | ((value >> 1) & 0x0F))); break;
                }
                return;

            case 0x8002:
                m_exRegs[0] = value;
                m_exRegs[1] = 0;
                return;
            }
        }

        Mapper004::writePrg(addr, value);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(m_prgPages[(addr >> 13) & 0x03], addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return readChrBank<BankSize::B1K>(m_chrPages[(addr >> 10) & 0x07], addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        writeChrBank<BankSize::B1K>(m_chrPages[(addr >> 10) & 0x07], addr, data);
    }

    void reset() override
    {
        Mapper004::reset();

        m_exRegs[0] = 0;
        m_exRegs[1] = 0;
        m_exRegs[2] = 0;

        const uint8_t lastBase = static_cast<uint8_t>(cd().numberOfPRGBanks<BankSize::B8K>() - 4);
        for(int i = 0; i < 4; i++) {
            m_prgPages[i] = static_cast<uint8_t>(lastBase + i) & m_prgMask;
        }

        for(int i = 0; i < 8; i++) {
            m_chrPages[i] = static_cast<uint16_t>(i) & m_chrMask;
        }
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        s.array(m_exRegs, 1, 3);
        s.array(m_prgPages, 1, 4);
        s.array(reinterpret_cast<uint8_t*>(m_chrPages), 2, 8);
    }
};
