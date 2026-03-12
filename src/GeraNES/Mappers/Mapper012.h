#pragma once

#include "Mapper004.h"

// MMC3 variant (iNES Mapper 12)
// Reference: https://www.nesdev.org/wiki/INES_Mapper_012
class Mapper012 : public Mapper004
{
private:
    uint8_t m_chrSelection = 0;
    // Mapper 12 adds CHR A17 via $4020-$5FFF latch, so bank index can exceed 8 bits.
    uint16_t m_chrMaskExt = 0;

    GERANES_INLINE uint16_t applyChrSelection(uint8_t slot, uint16_t page1k) const
    {
        if(slot < 4) {
            if(m_chrSelection & 0x01) page1k |= 0x100;
        }
        else {
            if(m_chrSelection & 0x10) page1k |= 0x100;
        }
        return page1k & m_chrMaskExt;
    }

public:
    Mapper012(ICartridgeData& cd) : Mapper004(cd)
    {
        auto calculateMask16 = [](int nBanks) -> uint16_t {
            uint16_t mask = 0;
            int n = nBanks - 1;
            while(n > 0) {
                mask <<= 1;
                mask |= 1;
                n >>= 1;
            }
            return mask;
        };

        if(hasChrRam()) {
            m_chrMaskExt = calculateMask16(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        }
        else {
            m_chrMaskExt = calculateMask16(cd.numberOfCHRBanks<BankSize::B1K>());
        }

        // Mapper 12 uses MMC3 rev A IRQ behavior.
        m_mmc3RevAIrqs = true;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);

        if(!m_chrMode) {
            switch(slot) {
            case 0:
            case 1: {
                const uint16_t page = applyChrSelection(slot, m_chrReg[0]);
                return readChrBank<BankSize::B2K>((page >> 1), addr);
            }
            case 2:
            case 3: {
                const uint16_t page = applyChrSelection(slot, m_chrReg[1]);
                return readChrBank<BankSize::B2K>((page >> 1), addr);
            }
            case 4: return readChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[2]), addr);
            case 5: return readChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[3]), addr);
            case 6: return readChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[4]), addr);
            default: return readChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[5]), addr);
            }
        }
        else {
            switch(slot) {
            case 0: return readChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[2]), addr);
            case 1: return readChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[3]), addr);
            case 2: return readChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[4]), addr);
            case 3: return readChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[5]), addr);
            case 4:
            case 5: {
                const uint16_t page = applyChrSelection(slot, m_chrReg[0]);
                return readChrBank<BankSize::B2K>((page >> 1), addr);
            }
            case 6:
            default: {
                const uint16_t page = applyChrSelection(slot, m_chrReg[1]);
                return readChrBank<BankSize::B2K>((page >> 1), addr);
            }
            }
        }
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;

        const uint8_t slot = static_cast<uint8_t>((addr >> 10) & 0x07);

        if(!m_chrMode) {
            switch(slot) {
            case 0:
            case 1: {
                const uint16_t page = applyChrSelection(slot, m_chrReg[0]);
                writeChrBank<BankSize::B2K>((page >> 1), addr, data);
                break;
            }
            case 2:
            case 3: {
                const uint16_t page = applyChrSelection(slot, m_chrReg[1]);
                writeChrBank<BankSize::B2K>((page >> 1), addr, data);
                break;
            }
            case 4: writeChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[2]), addr, data); break;
            case 5: writeChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[3]), addr, data); break;
            case 6: writeChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[4]), addr, data); break;
            case 7: writeChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[5]), addr, data); break;
            }
        }
        else {
            switch(slot) {
            case 0: writeChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[2]), addr, data); break;
            case 1: writeChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[3]), addr, data); break;
            case 2: writeChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[4]), addr, data); break;
            case 3: writeChrBank<BankSize::B1K>(applyChrSelection(slot, m_chrReg[5]), addr, data); break;
            case 4:
            case 5: {
                const uint16_t page = applyChrSelection(slot, m_chrReg[0]);
                writeChrBank<BankSize::B2K>((page >> 1), addr, data);
                break;
            }
            case 6:
            case 7: {
                const uint16_t page = applyChrSelection(slot, m_chrReg[1]);
                writeChrBank<BankSize::B2K>((page >> 1), addr, data);
                break;
            }
            }
        }
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t data) override
    {
        // CPU $4020-$5FFF arrives here as $0020-$1FFF.
        if(addr >= 0x20 && addr <= 0x1FFF) {
            m_chrSelection = data;
        }
    }

    void reset() override
    {
        Mapper004::reset();
        m_chrSelection = 0;
        m_mmc3RevAIrqs = true;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_chrSelection);
        SERIALIZEDATA(s, m_chrMaskExt);
    }
};
