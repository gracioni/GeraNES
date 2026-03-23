#pragma once

#include "BaseMapper.h"

// iNES Mapper 225
class Mapper225 : public BaseMapper
{
private:
    uint8_t m_prgBank0 = 0;
    uint8_t m_prgBank1 = 1;
    uint8_t m_chrBank = 0;
    bool m_horizontalMirroring = false;

public:
    Mapper225(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        const uint8_t highBit = static_cast<uint8_t>((absolute >> 8) & 0x40);
        const uint8_t prgPage = static_cast<uint8_t>(((absolute >> 6) & 0x3F) | highBit);

        if((absolute & 0x1000) != 0) {
            m_prgBank0 = prgPage;
            m_prgBank1 = prgPage;
        } else {
            m_prgBank0 = static_cast<uint8_t>(prgPage & 0xFE);
            m_prgBank1 = static_cast<uint8_t>((prgPage & 0xFE) + 1);
        }

        m_chrBank = static_cast<uint8_t>((absolute & 0x3F) | highBit);
        m_horizontalMirroring = (absolute & 0x2000) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBank0, addr);
        return cd().readPrg<BankSize::B16K>(m_prgBank1, addr);
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
        m_prgBank0 = 0;
        m_prgBank1 = 1;
        m_chrBank = 0;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank0);
        SERIALIZEDATA(s, m_prgBank1);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
