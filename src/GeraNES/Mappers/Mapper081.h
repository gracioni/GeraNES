#pragma once

#include "BaseMapper.h"

// iNES Mapper 81 (NTDEC N715021)
// The board latches the low 4 address bits on CPU writes at $8000-$FFFF:
//   PPCC -> PRG 16KB bank @ $8000, CHR 8KB bank @ $0000
class Mapper081 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrBank = 0;
    uint8_t m_chrMask = 0;

public:
    Mapper081(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const uint8_t latch = static_cast<uint8_t>((addr + 0x8000) & 0x0F);
        m_chrBank = static_cast<uint8_t>(latch & 0x03) & m_chrMask;
        m_prgBank = static_cast<uint8_t>((latch >> 2) & 0x03) & m_prgMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
        return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>() - 1, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_chrMask);
    }
};
