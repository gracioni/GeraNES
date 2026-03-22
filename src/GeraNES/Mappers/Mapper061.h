#pragma once

#include "BaseMapper.h"

class Mapper061 : public BaseMapper
{
private:
    uint8_t m_prgPage = 0;
    bool m_prg16Mode = false;
    bool m_horizontalMirroring = false;
    uint8_t m_prgMask16 = 0;
    uint8_t m_prgMask32 = 0;

public:
    Mapper061(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask16 = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_prgMask32 = calculateMask(cd.numberOfPRGBanks<BankSize::B32K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        m_prgPage = static_cast<uint8_t>(((absolute & 0x0F) << 1) | ((absolute >> 5) & 0x01));
        m_prg16Mode = (absolute & 0x10) != 0;
        m_horizontalMirroring = (absolute & 0x80) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(m_prg16Mode) {
            return cd().readPrg<BankSize::B16K>(m_prgPage & m_prgMask16, addr);
        }
        return cd().readPrg<BankSize::B32K>(static_cast<uint8_t>((m_prgPage & 0xFE) >> 1) & m_prgMask32, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgPage = 0;
        m_prg16Mode = false;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgPage);
        SERIALIZEDATA(s, m_prg16Mode);
        SERIALIZEDATA(s, m_horizontalMirroring);
        SERIALIZEDATA(s, m_prgMask16);
        SERIALIZEDATA(s, m_prgMask32);
    }
};
