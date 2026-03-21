#pragma once

#include "BaseMapper.h"

class Mapper029 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_chrBank = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;

public:
    Mapper029(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.chrRamSize() == 0) allocateChrRam(static_cast<int>(BankSize::B32K));
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_chrMask = calculateMask(std::max(1, (cd.chrRamSize() > 0 ? cd.chrRamSize() : static_cast<int>(BankSize::B32K)) / static_cast<int>(BankSize::B8K)));
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t data) override
    {
        m_chrBank = static_cast<uint8_t>(data & 0x03) & m_chrMask;
        m_prgBank = static_cast<uint8_t>((data >> 2) & 0x07) & m_prgMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
        return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>() - 1, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return readChrRam<BankSize::B8K>(m_chrBank, addr);
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(hasChrRam()) writeChrRam<BankSize::B8K>(m_chrBank, addr, data);
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
