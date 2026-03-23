#pragma once

#include "Mapper004.h"

// iNES Mapper 115
class Mapper115 : public Mapper004
{
private:
    uint8_t m_prgRegExt = 0;
    uint8_t m_chrRegExt = 0;
    uint8_t m_protectionReg = 0;

    GERANES_INLINE uint16_t mappedPrgBank(uint8_t slot) const
    {
        if((m_prgRegExt & 0x80) != 0) {
            if((m_prgRegExt & 0x20) != 0) {
                return static_cast<uint16_t>((((m_prgRegExt & 0x0F) >> 1) << 2) + slot);
            }

            return static_cast<uint16_t>(((m_prgRegExt & 0x0F) << 1) + ((slot >> 1) & 0x01));
        }

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

    GERANES_INLINE uint16_t mappedChrBank(uint8_t slot) const
    {
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

        return static_cast<uint16_t>(page | (static_cast<uint16_t>(m_chrRegExt) << 8));
    }

public:
    Mapper115(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        if(addr == 0x1080) {
            m_protectionReg = value;
        } else if((addr & 0x01) != 0) {
            m_chrRegExt = value & 0x01;
        } else {
            m_prgRegExt = value;
        }
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        if(addr >= 0x1000 && addr <= 0x1FFF) return m_protectionReg;
        return openBusData;
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t value) override
    {
        if((addr + 0x6000) == 0x7080) {
            m_protectionReg = value;
        } else if((addr & 0x01) != 0) {
            m_chrRegExt = value & 0x01;
        } else {
            m_prgRegExt = value;
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
        m_prgRegExt = 0;
        m_chrRegExt = 0;
        m_protectionReg = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_prgRegExt);
        SERIALIZEDATA(s, m_chrRegExt);
        SERIALIZEDATA(s, m_protectionReg);
    }
};
