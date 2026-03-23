#pragma once

#include "BaseMapper.h"

// iNES Mapper 229
class Mapper229 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    bool m_prg32Mode = true;
    uint8_t m_chrBank = 0;
    bool m_horizontalMirroring = false;

public:
    Mapper229(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        m_chrBank = static_cast<uint8_t>(absolute & 0x00FF);
        m_prg32Mode = (absolute & 0x001E) == 0;
        m_prgBank = static_cast<uint8_t>(absolute & 0x001F);
        m_horizontalMirroring = (absolute & 0x0020) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(m_prg32Mode) {
            return cd().readPrg<BankSize::B32K>(0, addr);
        }

        return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(m_chrBank, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgBank = 0;
        m_prg32Mode = true;
        m_chrBank = 0;
        m_horizontalMirroring = false;
        writePrg(0, 0);
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prg32Mode);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
