#pragma once

#include "BaseMapper.h"

class Mapper097 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;
    bool m_verticalMirroring = false;

public:
    Mapper097(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t data) override
    {
        m_prgBank = static_cast<uint8_t>(data & 0x1F) & m_prgMask;
        m_verticalMirroring = (data & 0x80) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 14) & 0x01) {
        case 0: return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>() - 1, addr);
        default: return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
        }
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_verticalMirroring ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
    }

    void reset() override
    {
        m_prgBank = 0;
        m_verticalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_verticalMirroring);
    }
};
