#pragma once

#include "BaseMapper.h"

class Mapper244 : public BaseMapper
{
private:
    static constexpr uint8_t LUT_PRG[4][4] = {
        { 0, 1, 2, 3 },
        { 3, 2, 1, 0 },
        { 0, 2, 1, 3 },
        { 3, 1, 2, 0 }
    };

    static constexpr uint8_t LUT_CHR[8][8] = {
        { 0, 1, 2, 3, 4, 5, 6, 7 },
        { 0, 2, 1, 3, 4, 6, 5, 7 },
        { 0, 1, 4, 5, 2, 3, 6, 7 },
        { 0, 4, 1, 5, 2, 6, 3, 7 },
        { 0, 4, 2, 6, 1, 5, 3, 7 },
        { 0, 2, 4, 6, 1, 3, 5, 7 },
        { 7, 6, 5, 4, 3, 2, 1, 0 },
        { 7, 6, 5, 4, 3, 2, 1, 0 }
    };

    uint8_t m_prgBank = 0;
    uint8_t m_chrBank = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;

public:
    Mapper244(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B32K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t value) override
    {
        if((value & 0x08) != 0) {
            m_chrBank = LUT_CHR[(value >> 4) & 0x07][value & 0x07] & m_chrMask;
        }
        else {
            m_prgBank = LUT_PRG[(value >> 4) & 0x03][value & 0x03] & m_prgMask;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank, addr);
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
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
    }
};
