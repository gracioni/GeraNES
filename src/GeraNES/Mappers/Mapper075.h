#pragma once

#include "BaseMapper.h"

// Konami VRC1 (iNES Mapper 75)
class Mapper075 : public BaseMapper
{
private:
    uint8_t m_prgBank[3] = {0, 0, 0}; // $8000, $A000, $C000 (8K each)
    uint8_t m_prgMask = 0;

    uint8_t m_chrBank[2] = {0, 0}; // $0000/$1000 (4K each)
    uint8_t m_chrLowNibble[2] = {0, 0};
    uint8_t m_chrMask = 0;

    bool m_mirroring = false; // 0=V, 1=H

    GERANES_INLINE void updateChrBankFromControl(uint8_t data)
    {
        // $9000: bit 1 = CHR high bit for $0000, bit 2 = CHR high bit for $1000
        m_chrBank[0] = static_cast<uint8_t>(((data & 0x02) << 3) | m_chrLowNibble[0]);
        m_chrBank[1] = static_cast<uint8_t>(((data & 0x04) << 2) | m_chrLowNibble[1]);

        m_chrBank[0] &= m_chrMask;
        m_chrBank[1] &= m_chrMask;
    }

    template<BankSize bs>
    GERANES_INLINE uint8_t readChrBank(int bank, int addr)
    {
        if(hasChrRam()) return readChrRam<bs>(bank, addr);
        return cd().readChr<bs>(bank, addr);
    }

    template<BankSize bs>
    GERANES_INLINE void writeChrBank(int bank, int addr, uint8_t data)
    {
        if(hasChrRam()) writeChrRam<bs>(bank, addr, data);
    }

public:
    Mapper075(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRam()) {
            m_chrMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B4K));
        }
        else {
            m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B4K>());
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const int absolute = addr + 0x8000;

        switch(absolute & 0xF000) {
        case 0x8000:
            m_prgBank[0] = (data & 0x0F) & m_prgMask;
            break;

        case 0x9000:
            m_mirroring = (data & 0x01) != 0;
            updateChrBankFromControl(data);
            break;

        case 0xA000:
            m_prgBank[1] = (data & 0x0F) & m_prgMask;
            break;

        case 0xC000:
            m_prgBank[2] = (data & 0x0F) & m_prgMask;
            break;

        case 0xE000:
            m_chrLowNibble[0] = data & 0x0F;
            m_chrBank[0] = (m_chrBank[0] & 0x10) | m_chrLowNibble[0];
            m_chrBank[0] &= m_chrMask;
            break;

        case 0xF000:
            m_chrLowNibble[1] = data & 0x0F;
            m_chrBank[1] = (m_chrBank[1] & 0x10) | m_chrLowNibble[1];
            m_chrBank[1] &= m_chrMask;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_prgBank[0] & m_prgMask, addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_prgBank[1] & m_prgMask, addr);
        case 2: return cd().readPrg<BankSize::B8K>(m_prgBank[2] & m_prgMask, addr);
        default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(addr < 0x1000) return readChrBank<BankSize::B4K>(m_chrBank[0], addr);
        return readChrBank<BankSize::B4K>(m_chrBank[1], addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(addr < 0x1000) writeChrBank<BankSize::B4K>(m_chrBank[0], addr, data);
        else writeChrBank<BankSize::B4K>(m_chrBank[1], addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        return m_mirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgBank[0] = 0;
        m_prgBank[1] = 0;
        m_prgBank[2] = 0;

        m_chrLowNibble[0] = 0;
        m_chrLowNibble[1] = 0;
        m_chrBank[0] = 0;
        m_chrBank[1] = 0;

        m_mirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        s.array(m_prgBank, 1, 3);
        SERIALIZEDATA(s, m_prgMask);

        s.array(m_chrBank, 1, 2);
        s.array(m_chrLowNibble, 1, 2);
        SERIALIZEDATA(s, m_chrMask);

        SERIALIZEDATA(s, m_mirroring);
    }
};
