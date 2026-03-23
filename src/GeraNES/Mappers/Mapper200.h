#pragma once

#include "BaseMapper.h"

class Mapper200 : public BaseMapper
{
private:
    uint8_t m_bank = 0;
    bool m_verticalMirroring = false;

public:
    Mapper200(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*value*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        m_bank = static_cast<uint8_t>(absolute & 0x07);
        m_verticalMirroring = (absolute & 0x08) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_bank, addr);
        return cd().readPrg<BankSize::B16K>(m_bank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return readChrRam<BankSize::B8K>(m_bank, addr);
        return cd().readChr<BankSize::B8K>(m_bank, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(hasChrRam()) writeChrRam<BankSize::B8K>(m_bank, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_verticalMirroring ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
    }

    void reset() override
    {
        m_bank = 0;
        m_verticalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_bank);
        SERIALIZEDATA(s, m_verticalMirroring);
    }
};
