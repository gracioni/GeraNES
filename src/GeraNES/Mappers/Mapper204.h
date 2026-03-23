#pragma once

#include "BaseMapper.h"

// iNES Mapper 204
class Mapper204 : public BaseMapper
{
private:
    uint8_t m_prgBank0 = 0;
    uint8_t m_prgBank1 = 1;
    uint8_t m_chrBank = 0;
    bool m_horizontalMirroring = false;

public:
    Mapper204(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*data*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        const uint8_t bitMask = static_cast<uint8_t>(absolute & 0x0006);
        const uint8_t page = static_cast<uint8_t>(bitMask + (bitMask == 0x06 ? 0 : (absolute & 0x0001)));

        m_prgBank0 = page;
        m_prgBank1 = static_cast<uint8_t>(bitMask + (bitMask == 0x06 ? 1 : (absolute & 0x0001)));
        m_chrBank = page;
        m_horizontalMirroring = (absolute & 0x0010) != 0;
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
        writePrg(0, 0);
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
