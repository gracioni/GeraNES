#pragma once

#include "Mapper004.h"

class Mapper121 : public Mapper004
{
private:
    uint8_t m_exReg[8] = {0};

    GERANES_INLINE uint8_t reverseLow6(uint8_t value) const
    {
        return static_cast<uint8_t>(
            ((value & 0x01) << 5) |
            ((value & 0x02) << 3) |
            ((value & 0x04) << 1) |
            ((value & 0x08) >> 1) |
            ((value & 0x10) >> 3) |
            ((value & 0x20) >> 5)
        );
    }

    GERANES_INLINE void updateExRegs()
    {
        switch(m_exReg[5] & 0x3F) {
        case 0x20:
        case 0x29:
        case 0x2B:
        case 0x3C:
        case 0x3F:
            m_exReg[7] = 1;
            m_exReg[0] = m_exReg[6];
            break;
        case 0x26:
            m_exReg[7] = 0;
            m_exReg[0] = m_exReg[6];
            break;
        case 0x28:
            m_exReg[7] = 0;
            m_exReg[1] = m_exReg[6];
            break;
        case 0x2A:
            m_exReg[7] = 0;
            m_exReg[2] = m_exReg[6];
            break;
        case 0x2C:
            m_exReg[7] = 1;
            if(m_exReg[6] != 0) m_exReg[0] = m_exReg[6];
            break;
        case 0x2F:
            break;
        default:
            m_exReg[5] = 0;
            break;
        }
    }

    GERANES_INLINE uint16_t currentPrgBank8k(uint8_t slot) const
    {
        const uint8_t orValue = static_cast<uint8_t>((m_exReg[3] & 0x80) >> 2);

        if((m_exReg[5] & 0x3F) != 0) {
            switch(slot & 0x03) {
            case 0: return static_cast<uint16_t>((m_prgReg0 & 0x1F) | orValue);
            case 1: return static_cast<uint16_t>(m_exReg[2] | orValue);
            case 2: return static_cast<uint16_t>(m_exReg[1] | orValue);
            default: return static_cast<uint16_t>(m_exReg[0] | orValue);
            }
        }

        if(!m_prgMode) {
            switch(slot & 0x03) {
            case 0: return static_cast<uint16_t>((m_prgReg0 & 0x1F) | orValue);
            case 1: return static_cast<uint16_t>((m_prgReg1 & 0x1F) | orValue);
            case 2: return static_cast<uint16_t>(((cd().numberOfPRGBanks<BankSize::B8K>() - 2) & 0x1F) | orValue);
            default: return static_cast<uint16_t>(((cd().numberOfPRGBanks<BankSize::B8K>() - 1) & 0x1F) | orValue);
            }
        }

        switch(slot & 0x03) {
        case 0: return static_cast<uint16_t>(((cd().numberOfPRGBanks<BankSize::B8K>() - 2) & 0x1F) | orValue);
        case 1: return static_cast<uint16_t>((m_prgReg1 & 0x1F) | orValue);
        case 2: return static_cast<uint16_t>((m_prgReg0 & 0x1F) | orValue);
        default: return static_cast<uint16_t>(((cd().numberOfPRGBanks<BankSize::B8K>() - 1) & 0x1F) | orValue);
        }
    }

    GERANES_INLINE uint16_t currentChrBank1k(uint8_t slot) const
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

        if(cd().prgSize() == cd().chrSize()) {
            return static_cast<uint16_t>(bank | ((m_exReg[3] & 0x80) << 1));
        }

        if(((slot < 4) && !m_chrMode) || ((slot >= 4) && m_chrMode)) {
            bank |= 0x100;
        }

        return bank;
    }

public:
    Mapper121(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        if(addr >= 0x1000 && addr <= 0x1FFF) {
            static const uint8_t lookup[4] = {0x83, 0x83, 0x42, 0x00};
            m_exReg[4] = lookup[value & 0x03];

            if((addr & 0x1180) == 0x1180) {
                m_exReg[3] = value;
            }
        }
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        if(addr >= 0x1000 && addr <= 0x1FFF) return m_exReg[4];
        return openBusData;
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        if(absolute < 0xA000) {
            if((absolute & 0x0003) == 0x0003) {
                m_exReg[5] = value;
                updateExRegs();
                Mapper004::writePrg(0x0000, value);
            } else if((absolute & 0x0001) != 0) {
                m_exReg[6] = reverseLow6(value);
                if(m_exReg[7] == 0) updateExRegs();
                Mapper004::writePrg(0x0001, value);
            } else {
                Mapper004::writePrg(0x0000, value);
            }
            return;
        }

        Mapper004::writePrg(addr, value);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(currentPrgBank8k(static_cast<uint8_t>((addr >> 13) & 0x03)), addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint16_t bank = currentChrBank1k(static_cast<uint8_t>((addr >> 10) & 0x07));
        if(hasChrRam()) return readChrRam<BankSize::B1K>(bank, addr);
        return cd().readChr<BankSize::B1K>(bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        const uint16_t bank = currentChrBank1k(static_cast<uint8_t>((addr >> 10) & 0x07));
        writeChrRam<BankSize::B1K>(bank, addr, data);
    }

    void reset() override
    {
        Mapper004::reset();
        memset(m_exReg, 0, sizeof(m_exReg));
        m_exReg[3] = 0x80;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        s.array(m_exReg, 1, 8);
    }
};
