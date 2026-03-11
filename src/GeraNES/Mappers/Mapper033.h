#pragma once

#include "BaseMapper.h"

// Taito TC0190 / subset of TC0350 (iNES Mapper 33)
class Mapper033 : public BaseMapper
{
private:
    uint8_t m_prgReg[2] = {0, 0};
    uint8_t m_prgMask = 0;

    uint8_t m_chrReg2k[2] = {0, 0};
    uint8_t m_chrReg1k[4] = {0, 0, 0, 0};
    uint8_t m_chr2kMask = 0;
    uint8_t m_chr1kMask = 0;

    bool m_mirroring = false;

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
    Mapper033(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());

        if(hasChrRam()) {
            m_chr2kMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B2K));
            m_chr1kMask = calculateMask(cd.chrRamSize() / static_cast<int>(BankSize::B1K));
        }
        else {
            m_chr2kMask = calculateMask(cd.numberOfCHRBanks<BankSize::B2K>());
            m_chr1kMask = calculateMask(cd.numberOfCHRBanks<BankSize::B1K>());
        }
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const int absolute = addr + 0x8000;
        if(absolute < 0x8000 || absolute > 0xBFFF) return;

        switch(absolute & 0xA003) {
        case 0x8000:
            m_mirroring = (data & 0x40) != 0;
            m_prgReg[0] = (data & 0x3F) & m_prgMask;
            break;
        case 0x8001:
            m_prgReg[1] = (data & 0x3F) & m_prgMask;
            break;
        case 0x8002:
            m_chrReg2k[0] = data & m_chr2kMask;
            break;
        case 0x8003:
            m_chrReg2k[1] = data & m_chr2kMask;
            break;
        case 0xA000:
            m_chrReg1k[0] = data & m_chr1kMask;
            break;
        case 0xA001:
            m_chrReg1k[1] = data & m_chr1kMask;
            break;
        case 0xA002:
            m_chrReg1k[2] = data & m_chr1kMask;
            break;
        case 0xA003:
            m_chrReg1k[3] = data & m_chr1kMask;
            break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 13) & 0x03) {
        case 0: return cd().readPrg<BankSize::B8K>(m_prgReg[0] & m_prgMask, addr);
        case 1: return cd().readPrg<BankSize::B8K>(m_prgReg[1] & m_prgMask, addr);
        case 2: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 2, addr);
        default: return cd().readPrg<BankSize::B8K>(cd().numberOfPRGBanks<BankSize::B8K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(addr < 0x0800) return readChrBank<BankSize::B2K>(m_chrReg2k[0] & m_chr2kMask, addr);
        if(addr < 0x1000) return readChrBank<BankSize::B2K>(m_chrReg2k[1] & m_chr2kMask, addr);

        const uint8_t slot = static_cast<uint8_t>((addr >> 10) - 4); // $1000-$1FFF => 0..3
        return readChrBank<BankSize::B1K>(m_chrReg1k[slot] & m_chr1kMask, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(addr < 0x0800) {
            writeChrBank<BankSize::B2K>(m_chrReg2k[0] & m_chr2kMask, addr, data);
            return;
        }
        if(addr < 0x1000) {
            writeChrBank<BankSize::B2K>(m_chrReg2k[1] & m_chr2kMask, addr, data);
            return;
        }

        const uint8_t slot = static_cast<uint8_t>((addr >> 10) - 4);
        writeChrBank<BankSize::B1K>(m_chrReg1k[slot] & m_chr1kMask, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_mirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgReg[0] = 0;
        m_prgReg[1] = 0;
        m_chrReg2k[0] = 0;
        m_chrReg2k[1] = 0;
        m_chrReg1k[0] = 0;
        m_chrReg1k[1] = 0;
        m_chrReg1k[2] = 0;
        m_chrReg1k[3] = 0;
        m_mirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);

        s.array(m_prgReg, 1, 2);
        SERIALIZEDATA(s, m_prgMask);

        s.array(m_chrReg2k, 1, 2);
        s.array(m_chrReg1k, 1, 4);
        SERIALIZEDATA(s, m_chr2kMask);
        SERIALIZEDATA(s, m_chr1kMask);

        SERIALIZEDATA(s, m_mirroring);
    }
};
