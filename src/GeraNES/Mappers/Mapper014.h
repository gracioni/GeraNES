#pragma once

#include "Mapper004.h"

class Mapper014 : public Mapper004
{
private:
    uint8_t m_vrcChrReg[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t m_vrcPrgReg[2] = {0, 0};
    bool m_vrcMirroring = false;
    uint8_t m_mode = 0;
    uint8_t m_prgMask8k = 0;
    uint16_t m_chrMask1k = 0;

    static uint16_t calculateMask16(int nBanks)
    {
        uint16_t mask = 0;
        int n = nBanks - 1;
        while(n > 0) {
            mask <<= 1;
            mask |= 1;
            n >>= 1;
        }
        return mask;
    }

    GERANES_INLINE bool useMmc3Mode() const
    {
        return (m_mode & 0x01) != 0;
    }

    GERANES_INLINE uint16_t outerChrBitForSlot(int slot) const
    {
        if(slot < 4) return (m_mode & 0x08) ? 0x100 : 0x000;
        if(slot < 6) return (m_mode & 0x20) ? 0x100 : 0x000;
        return (m_mode & 0x80) ? 0x100 : 0x000;
    }

    GERANES_INLINE uint16_t currentMmc3ChrPage1k(int slot) const
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

    GERANES_INLINE uint8_t mapPrg8k(uint8_t bank) const
    {
        return static_cast<uint8_t>(bank & m_prgMask8k);
    }

    GERANES_INLINE uint16_t mapChr1k(uint16_t bank) const
    {
        return static_cast<uint16_t>(bank & m_chrMask1k);
    }

    GERANES_INLINE void writeVrcChrNibble(int addr, uint8_t data)
    {
        const int absolute = addr + 0x8000;
        if(absolute < 0xB000 || absolute > 0xEFFF) return;

        const uint8_t regNumber = static_cast<uint8_t>(((((absolute >> 12) & 0x07) - 3) << 1) + ((absolute >> 1) & 0x01));
        if((absolute & 0x01) == 0) {
            m_vrcChrReg[regNumber] = static_cast<uint8_t>((m_vrcChrReg[regNumber] & 0xF0) | (data & 0x0F));
        }
        else {
            m_vrcChrReg[regNumber] = static_cast<uint8_t>((m_vrcChrReg[regNumber] & 0x0F) | ((data & 0x0F) << 4));
        }
    }

public:
    Mapper014(ICartridgeData& cd) : Mapper004(cd)
    {
        m_prgMask8k = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
        if(hasChrRam()) {
            m_chrMask1k = calculateMask16(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        }
        else {
            m_chrMask1k = calculateMask16(cd.numberOfCHRBanks<BankSize::B1K>());
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const int absolute = addr + 0x8000;

        if(absolute == 0xA131) {
            m_mode = data;
        }

        if(useMmc3Mode()) {
            Mapper004::writePrg(addr, data);
            return;
        }

        if(absolute >= 0xB000 && absolute <= 0xEFFF) {
            writeVrcChrNibble(addr, data);
            return;
        }

        switch(absolute & 0xF003) {
        case 0x8000:
            m_vrcPrgReg[0] = data & m_prgMask8k;
            break;
        case 0x9000:
            m_vrcMirroring = (data & 0x01) != 0;
            break;
        case 0xA000:
            m_vrcPrgReg[1] = data & m_prgMask8k;
            break;
        default:
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(useMmc3Mode()) return Mapper004::readPrg(addr);

        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(mapPrg8k(m_vrcPrgReg[0]), addr);
        case 1: return cd().readPrg<BankSize::B8K>(mapPrg8k(m_vrcPrgReg[1]), addr);
        case 2: return cd().readPrg<BankSize::B8K>(mapPrg8k(cd().numberOfPRGBanks<BankSize::B8K>() - 2), addr);
        default: return cd().readPrg<BankSize::B8K>(mapPrg8k(cd().numberOfPRGBanks<BankSize::B8K>() - 1), addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(useMmc3Mode()) {
            const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
            const uint16_t page = mapChr1k(static_cast<uint16_t>(currentMmc3ChrPage1k(slot) | outerChrBitForSlot(slot)));
            return readChrBank<BankSize::B1K>(page, addr);
        }

        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        return readChrBank<BankSize::B1K>(mapChr1k(m_vrcChrReg[slot]), addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;

        if(useMmc3Mode()) {
            const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
            const uint16_t page = mapChr1k(static_cast<uint16_t>(currentMmc3ChrPage1k(slot) | outerChrBitForSlot(slot)));
            writeChrBank<BankSize::B1K>(page, addr, data);
            return;
        }

        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);
        writeChrBank<BankSize::B1K>(mapChr1k(m_vrcChrReg[slot]), addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(useMmc3Mode()) return Mapper004::mirroringType();
        return m_vrcMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    GERANES_HOT bool getInterruptFlag() override
    {
        return useMmc3Mode() ? Mapper004::getInterruptFlag() : false;
    }

    void reset() override
    {
        Mapper004::reset();
        memset(m_vrcChrReg, 0, sizeof(m_vrcChrReg));
        m_vrcPrgReg[0] = 0;
        m_vrcPrgReg[1] = 0;
        m_vrcMirroring = false;
        m_mode = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        s.array(m_vrcChrReg, 1, 8);
        s.array(m_vrcPrgReg, 1, 2);
        SERIALIZEDATA(s, m_vrcMirroring);
        SERIALIZEDATA(s, m_mode);
        SERIALIZEDATA(s, m_prgMask8k);
        SERIALIZEDATA(s, m_chrMask1k);
    }
};
