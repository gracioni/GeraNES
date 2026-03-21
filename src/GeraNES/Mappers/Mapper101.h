#pragma once

#include "BaseMapper.h"

class Mapper101 : public BaseMapper
{
private:
    uint8_t m_chrBank = 0;
    uint8_t m_chrMask = 0;

public:
    Mapper101(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writeSaveRam(int /*addr*/, uint8_t data) override
    {
        m_chrBank = data & m_chrMask;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    void reset() override
    {
        m_chrBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_chrMask);
    }
};
