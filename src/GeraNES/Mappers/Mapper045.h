#pragma once

#include "Mapper004.h"

// iNES Mapper 45 (GA23C multicart, MMC3 clone + 4 outer bank registers)
class Mapper045 : public Mapper004
{
private:
    uint8_t m_outerRegIndex = 0;
    uint8_t m_outerReg[4] = {0, 0, 0x0F, 0};
    uint16_t m_chrMaskExt = 0;
    uint8_t m_menuDipPosition = 0;

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

    GERANES_INLINE bool outerRegsLocked() const
    {
        return (m_outerReg[3] & 0x40) != 0;
    }

    GERANES_INLINE void resetOuterRegs()
    {
        m_outerRegIndex = 0;
        m_outerReg[0] = 0x00;
        m_outerReg[1] = 0x00;
        m_outerReg[2] = 0x0F;
        m_outerReg[3] = 0x00;
    }

    GERANES_INLINE uint16_t mapChrPage(uint16_t mmc3Page1k) const
    {
        if(hasChrRam()) {
            return mmc3Page1k & m_chrMask;
        }

        // CHR page = (MMC3 page & CHR-AND) | CHR-OR
        uint16_t page = mmc3Page1k;
        page &= static_cast<uint16_t>(0xFF >> (0x0F - (m_outerReg[2] & 0x0F)));
        page |= static_cast<uint16_t>(m_outerReg[0]);
        page |= static_cast<uint16_t>((m_outerReg[2] & 0xF0) << 4);
        return page & m_chrMaskExt;
    }

    GERANES_INLINE uint8_t mapPrgPage(uint8_t mmc3Page8k) const
    {
        // PRG page = (MMC3 page & PRG-AND) | PRG-OR
        uint8_t page = mmc3Page8k;
        page &= static_cast<uint8_t>(0x3F ^ (m_outerReg[3] & 0x3F));
        page |= m_outerReg[1];
        return page & m_prgMask;
    }

public:
    Mapper045(ICartridgeData& cd) : Mapper004(cd)
    {
        if(hasChrRam()) {
            m_chrMaskExt = calculateMask16(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        }
        else {
            m_chrMaskExt = calculateMask16(cd.numberOfCHRBanks<BankSize::B1K>());
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        addr &= 0xF001;

        switch(addr)
        {
        case 0x0000:
            m_chrMode = data & 0x80;
            m_prgMode = data & 0x40;
            m_addrReg = data & 0x07;
            break;

        case 0x0001:
            // MMC3 behavior: regs 0/1 ignore bit 0 (2KB CHR banks).
            if(m_addrReg <= 1) data &= 0xFE;

            switch(m_addrReg)
            {
            case 0: m_chrReg[0] = data; break;
            case 1: m_chrReg[1] = data; break;
            case 2: m_chrReg[2] = data; break;
            case 3: m_chrReg[3] = data; break;
            case 4: m_chrReg[4] = data; break;
            case 5: m_chrReg[5] = data; break;
            case 6: m_prgReg0 = data & m_prgMask; break;
            case 7: m_prgReg1 = data & m_prgMask; break;
            }
            break;

        case 0x2000:
            m_mirroring = data & 0x01;
            break;
        case 0x2001:
            m_enableWRAM = data & 0x80;
            m_writeProtectWRAM = data & 0x40;
            break;
        case 0x4000:
            m_reloadValue = data;
            break;
        case 0x4001:
            m_irqClearFlag = true;
            m_irqCounter = 0;
            break;
        case 0x6000:
            m_interruptFlag = false;
            m_enableInterrupt = false;
            break;
        case 0x6001:
            m_enableInterrupt = true;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        // MMC3 fixed slots are virtual $3E/$3F before outer masking.
        const uint8_t fixedSecondLast = 0x3E;
        const uint8_t fixedLast = 0x3F;

        if(!m_prgMode)
        {
            switch(addr >> 13) {
            case 0: return cd().readPrg<BankSize::B8K>(mapPrgPage(m_prgReg0), addr);
            case 1: return cd().readPrg<BankSize::B8K>(mapPrgPage(m_prgReg1), addr);
            case 2: return cd().readPrg<BankSize::B8K>(mapPrgPage(fixedSecondLast), addr);
            default: return cd().readPrg<BankSize::B8K>(mapPrgPage(fixedLast), addr);
            }
        }
        else
        {
            switch(addr >> 13) {
            case 0: return cd().readPrg<BankSize::B8K>(mapPrgPage(fixedSecondLast), addr);
            case 1: return cd().readPrg<BankSize::B8K>(mapPrgPage(m_prgReg1), addr);
            case 2: return cd().readPrg<BankSize::B8K>(mapPrgPage(m_prgReg0), addr);
            default: return cd().readPrg<BankSize::B8K>(mapPrgPage(fixedLast), addr);
            }
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        uint16_t page = 0;

        if(!m_chrMode)
        {
            switch((addr >> 10) & 0x07) {
            case 0: page = static_cast<uint16_t>(m_chrReg[0] & 0xFE); break;
            case 1: page = static_cast<uint16_t>(m_chrReg[0] | 0x01); break;
            case 2: page = static_cast<uint16_t>(m_chrReg[1] & 0xFE); break;
            case 3: page = static_cast<uint16_t>(m_chrReg[1] | 0x01); break;
            case 4: page = m_chrReg[2]; break;
            case 5: page = m_chrReg[3]; break;
            case 6: page = m_chrReg[4]; break;
            default: page = m_chrReg[5]; break;
            }
        }
        else
        {
            switch((addr >> 10) & 0x07) {
            case 0: page = m_chrReg[2]; break;
            case 1: page = m_chrReg[3]; break;
            case 2: page = m_chrReg[4]; break;
            case 3: page = m_chrReg[5]; break;
            case 4: page = static_cast<uint16_t>(m_chrReg[0] & 0xFE); break;
            case 5: page = static_cast<uint16_t>(m_chrReg[0] | 0x01); break;
            case 6: page = static_cast<uint16_t>(m_chrReg[1] & 0xFE); break;
            default: page = static_cast<uint16_t>(m_chrReg[1] | 0x01); break;
            }
        }

        return readChrBank<BankSize::B1K>(mapChrPage(page), addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;

        uint16_t page = 0;

        if(!m_chrMode)
        {
            switch((addr >> 10) & 0x07) {
            case 0: page = static_cast<uint16_t>(m_chrReg[0] & 0xFE); break;
            case 1: page = static_cast<uint16_t>(m_chrReg[0] | 0x01); break;
            case 2: page = static_cast<uint16_t>(m_chrReg[1] & 0xFE); break;
            case 3: page = static_cast<uint16_t>(m_chrReg[1] | 0x01); break;
            case 4: page = m_chrReg[2]; break;
            case 5: page = m_chrReg[3]; break;
            case 6: page = m_chrReg[4]; break;
            default: page = m_chrReg[5]; break;
            }
        }
        else
        {
            switch((addr >> 10) & 0x07) {
            case 0: page = m_chrReg[2]; break;
            case 1: page = m_chrReg[3]; break;
            case 2: page = m_chrReg[4]; break;
            case 3: page = m_chrReg[5]; break;
            case 4: page = static_cast<uint16_t>(m_chrReg[0] & 0xFE); break;
            case 5: page = static_cast<uint16_t>(m_chrReg[0] | 0x01); break;
            case 6: page = static_cast<uint16_t>(m_chrReg[1] & 0xFE); break;
            default: page = static_cast<uint16_t>(m_chrReg[1] | 0x01); break;
            }
        }

        writeChrBank<BankSize::B1K>(mapChrPage(page), addr, data);
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        // $6001 (mask $F001): reset outer registers and release lock.
        if((addr & 0x1001) == 0x0001) {
            resetOuterRegs();
            return;
        }

        // $6000 (mask $F001): sequential writes to outer regs #0..#3.
        if((addr & 0x1001) == 0x0000) {
            if(!outerRegsLocked()) {
                m_outerReg[m_outerRegIndex] = data;
                m_outerRegIndex = (m_outerRegIndex + 1) & 0x03;
            }
            return;
        }

        // When locked, behave as normal WRAM for non-register addresses.
        if(outerRegsLocked()) {
            Mapper004::writeSaveRam(addr, data);
        }
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData = 0) override
    {
        // Optional menu DIP behavior on $5000-$5FFF (default DIP position = 0).
        if(addr >= 0x1000 && addr <= 0x1FFF) {
            const uint8_t d0 = static_cast<uint8_t>((addr >> (m_menuDipPosition + 4)) & 0x01);
            return static_cast<uint8_t>((openBusData & 0xFE) | d0);
        }
        return openBusData;
    }

    void reset() override
    {
        Mapper004::reset();

        resetOuterRegs();

        // Match MMC3 power-on CHR register defaults expected by some menus.
        m_chrReg[0] = 0;
        m_chrReg[1] = 2;
        m_chrReg[2] = 4;
        m_chrReg[3] = 5;
        m_chrReg[4] = 6;
        m_chrReg[5] = 7;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_outerRegIndex);
        SERIALIZEDATA(s, m_outerReg[0]);
        SERIALIZEDATA(s, m_outerReg[1]);
        SERIALIZEDATA(s, m_outerReg[2]);
        SERIALIZEDATA(s, m_outerReg[3]);
        SERIALIZEDATA(s, m_chrMaskExt);
        SERIALIZEDATA(s, m_menuDipPosition);
    }
};

