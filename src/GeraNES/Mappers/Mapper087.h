#pragma once

#include "BaseMapper.h"

// iNES Mapper 87 (Jaleco JF-xx)
// - PRG is fixed
// - CHR 8KB bank selected by writes in $6000-$7FFF
// - bank select uses swapped low bits: [b1 b0] -> [b0 b1]
class Mapper087 : public BaseMapper
{
private:
    uint8_t m_chrReg = 0;
    uint8_t m_chrMask = 0; // 8KB CHR bank mask

    GERANES_INLINE uint8_t decodeChrBank(uint8_t data) const
    {
        const uint8_t b0 = static_cast<uint8_t>((data >> 0) & 0x01);
        const uint8_t b1 = static_cast<uint8_t>((data >> 1) & 0x01);
        return static_cast<uint8_t>((b0 << 1) | b1);
    }

public:
    Mapper087(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        // Fixed PRG mapping.
        if(cd().numberOfPRGBanks<BankSize::B16K>() == 1) {
            return cd().readPrg<BankSize::B16K>(0, addr);
        }
        return cd().readPrg<BankSize::B32K>(0, addr);
    }

    GERANES_HOT void writeSaveRam(int /*addr*/, uint8_t data) override
    {
        m_chrReg = decodeChrBank(data) & m_chrMask;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_chrReg, addr);
    }

    void reset() override
    {
        m_chrReg = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_chrReg);
        SERIALIZEDATA(s, m_chrMask);
    }
};
