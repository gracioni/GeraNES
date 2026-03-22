#pragma once

#include "BaseMapper.h"

class Mapper086 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_chrBank = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;

public:
    Mapper086(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B32K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t value) override
    {
        switch((addr + 0x6000) & 0x7000) {
        case 0x6000:
            m_prgBank = static_cast<uint8_t>((value & 0x30) >> 4) & m_prgMask;
            m_chrBank = static_cast<uint8_t>(((value & 0x03) | ((value >> 4) & 0x04)) & m_chrMask);
            break;
        case 0x7000:
            break;
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
