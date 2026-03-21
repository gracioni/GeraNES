#pragma once

#include "BaseMapper.h"

class Mapper078 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrBank = 0;
    uint8_t m_chrMask = 0;
    bool m_altMirroring = false;
    bool m_useHolyDiverMirroring = false;

public:
    Mapper078(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());

        // NES 2.0 submapper 3 = Holy Diver, 1 = Cosmo Carrier.
        // For older headers, the "alternative nametables" bit is commonly abused.
        m_useHolyDiverMirroring = cd.subMapperId() == 3 || (cd.subMapperId() == 0 && cd.useFourScreenMirroring());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        data &= readPrg(addr);

        m_prgBank = static_cast<uint8_t>(data & 0x07) & m_prgMask;
        m_altMirroring = (data & 0x08) != 0;
        m_chrBank = static_cast<uint8_t>((data >> 4) & 0x0F) & m_chrMask;
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
        if(m_useHolyDiverMirroring) {
            return m_altMirroring ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
        }

        return m_altMirroring ? MirroringType::SINGLE_SCREEN_B : MirroringType::SINGLE_SCREEN_A;
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBank = 0;
        m_altMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_altMirroring);
        SERIALIZEDATA(s, m_useHolyDiverMirroring);
    }
};
