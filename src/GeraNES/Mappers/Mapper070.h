#pragma once

#include "BaseMapper.h"

// Mapper 70 (Bandai 74161/32)
// $8000-$FFFF: [PPPP CCCC] with bus conflicts
class Mapper070 : public BaseMapper
{
private:
    uint8_t m_prgReg = 0;
    uint8_t m_prgRegMask = 0;
    uint8_t m_chrReg = 0;
    uint8_t m_chrRegMask = 0;

public:
    Mapper070(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgRegMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_chrRegMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        // Mapper 70 has bus conflicts: effective data is CPU data AND ROM data.
        const uint8_t romData = readPrg(addr);
        data &= romData;

        m_prgReg = static_cast<uint8_t>((data >> 4) & 0x0F) & m_prgRegMask;
        m_chrReg = static_cast<uint8_t>(data & 0x0F) & m_chrRegMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch(addr >> 14) { // addr / 16k
        case 0: return cd().readPrg<BankSize::B16K>(m_prgReg, addr);
        default: return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_chrReg, addr);
    }

    void reset() override
    {
        m_prgReg = 0;
        m_chrReg = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgReg);
        SERIALIZEDATA(s, m_prgRegMask);
        SERIALIZEDATA(s, m_chrReg);
        SERIALIZEDATA(s, m_chrRegMask);
    }
};

