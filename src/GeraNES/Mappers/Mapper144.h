#pragma once

#include "BaseMapper.h"

class Mapper144 : public BaseMapper
{
private:
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;
    uint8_t m_prgBank = 0;
    uint8_t m_chrBank = 0;

public:
    Mapper144(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B32K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const uint8_t romData = cd().readPrg<BankSize::B32K>(m_prgBank, addr);
        const uint8_t effective = static_cast<uint8_t>(romData & (data | 0x01));
        m_prgBank = static_cast<uint8_t>(effective & 0x03) & m_prgMask;
        m_chrBank = static_cast<uint8_t>((effective >> 4) & 0x0F) & m_chrMask;
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
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_chrBank);
    }
};
