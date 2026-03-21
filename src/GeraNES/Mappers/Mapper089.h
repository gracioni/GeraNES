#pragma once

#include "BaseMapper.h"

class Mapper089 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrBank = 0;
    uint8_t m_chrMask = 0;
    bool m_mirroring = false;

public:
    Mapper089(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        data &= readPrg(addr);
        m_chrBank = static_cast<uint8_t>(data & 0x07) & m_chrMask;
        m_mirroring = (data & 0x08) != 0;
        m_prgBank = static_cast<uint8_t>((data >> 4) & 0x07) & m_prgMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        switch((addr >> 14) & 0x01) {
        case 0: return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
        default: return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>() - 1, addr);
        }
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_mirroring ? MirroringType::SINGLE_SCREEN_B : MirroringType::SINGLE_SCREEN_A;
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBank = 0;
        m_mirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_mirroring);
    }
};
