#pragma once

#include "BaseMapper.h"

class Mapper180 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;

public:
    Mapper180(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        data &= readPrg(addr);
        m_prgBank = static_cast<uint8_t>(data & 0x07) & m_prgMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 14) & 0x01) {
        case 0: return cd().readPrg<BankSize::B16K>(0, addr);
        default: return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
        }
    }

    void reset() override
    {
        m_prgBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
    }
};
