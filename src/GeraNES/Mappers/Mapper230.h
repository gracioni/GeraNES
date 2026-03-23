#pragma once

#include "BaseMapper.h"

// iNES Mapper 230
class Mapper230 : public BaseMapper
{
private:
    bool m_contraMode = false;
    uint8_t m_prgBank0 = 0;
    uint8_t m_prgBank1 = 0;
    bool m_verticalMirroring = false;

public:
    Mapper230(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t value) override
    {
        if(m_contraMode) {
            m_prgBank0 = static_cast<uint8_t>(value & 0x07);
            return;
        }

        if((value & 0x20) != 0) {
            m_prgBank0 = static_cast<uint8_t>((value & 0x1F) + 8);
            m_prgBank1 = m_prgBank0;
        } else {
            m_prgBank0 = static_cast<uint8_t>((value & 0x1E) + 8);
            m_prgBank1 = static_cast<uint8_t>((value & 0x1E) + 9);
        }

        m_verticalMirroring = (value & 0x40) != 0;
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
        return m_verticalMirroring ? MirroringType::VERTICAL : MirroringType::HORIZONTAL;
    }

    void reset() override
    {
        m_contraMode = !m_contraMode;
        if(m_contraMode) {
            m_prgBank0 = 0;
            m_prgBank1 = 7;
            m_verticalMirroring = true;
        } else {
            m_prgBank0 = 8;
            m_prgBank1 = 9;
            m_verticalMirroring = false;
        }
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_contraMode);
        SERIALIZEDATA(s, m_prgBank0);
        SERIALIZEDATA(s, m_prgBank1);
        SERIALIZEDATA(s, m_verticalMirroring);
    }
};
