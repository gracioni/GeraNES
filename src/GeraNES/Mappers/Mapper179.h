#pragma once

#include "BaseMapper.h"

class Mapper179 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    bool m_horizontalMirroring = false;

public:
    Mapper179(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writeMapperRegisterAbsolute(uint16_t addr, uint8_t value) override
    {
        if(addr >= 0x5000 && addr <= 0x5FFF) {
            m_prgBank = static_cast<uint8_t>(value >> 1);
        }
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t value) override
    {
        m_horizontalMirroring = (value & 0x01) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank, addr);
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
        m_prgBank = 0;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
