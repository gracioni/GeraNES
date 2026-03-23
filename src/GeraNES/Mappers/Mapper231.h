#pragma once

#include "BaseMapper.h"

// iNES Mapper 231
class Mapper231 : public BaseMapper
{
private:
    uint8_t m_prgBank0 = 0;
    uint8_t m_prgBank1 = 0;
    bool m_horizontalMirroring = false;

public:
    Mapper231(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        const uint8_t prgBank = static_cast<uint8_t>(((absolute >> 5) & 0x01) | (absolute & 0x1E));
        m_prgBank0 = static_cast<uint8_t>(prgBank & 0x1E);
        m_prgBank1 = prgBank;
        m_horizontalMirroring = (absolute & 0x80) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBank0, addr);
        return cd().readPrg<BankSize::B16K>(m_prgBank1, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgBank0 = 0;
        m_prgBank1 = 0;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank0);
        SERIALIZEDATA(s, m_prgBank1);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
