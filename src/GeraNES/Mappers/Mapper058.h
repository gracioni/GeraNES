#pragma once

#include "BaseMapper.h"

class Mapper058 : public BaseMapper
{
private:
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;
    uint8_t m_prgBank = 0;
    bool m_prg32Mode = false;
    uint8_t m_chrBank = 0;
    bool m_horizontalMirroring = false;

public:
    Mapper058(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        m_prgBank = static_cast<uint8_t>(absolute & 0x07);
        m_prg32Mode = (absolute & 0x40) == 0;
        m_chrBank = static_cast<uint8_t>((absolute >> 3) & 0x07);
        m_horizontalMirroring = (absolute & 0x80) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(m_prg32Mode) {
            return cd().readPrg<BankSize::B32K>(static_cast<uint8_t>((m_prgBank & 0x06) >> 1), addr);
        }
        return cd().readPrg<BankSize::B16K>(m_prgBank & m_prgMask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_chrBank & m_chrMask, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgBank = 0;
        m_prg32Mode = false;
        m_chrBank = 0;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prg32Mode);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
