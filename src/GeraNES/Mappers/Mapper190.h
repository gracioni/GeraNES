#pragma once

#include "BaseMapper.h"

class Mapper190 : public BaseMapper
{
private:
    uint8_t m_prgBank = 0;
    uint8_t m_chrBank[4] = {0, 0, 0, 0};

public:
    Mapper190(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        if(absolute < 0xA000) {
            m_prgBank = static_cast<uint8_t>((value & 0x07) | ((absolute >> 11) & 0x08));
            return;
        }

        if(absolute < 0xC000) {
            m_chrBank[(absolute >> 11) & 0x03] = value;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgBank, addr);
        return cd().readPrg<BankSize::B16K>(0, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B2K>(m_chrBank[(addr >> 11) & 0x03], addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(!hasChrRam()) return;
        writeChrRam<BankSize::B2K>(m_chrBank[(addr >> 11) & 0x03], addr, data);
    }

    void reset() override
    {
        m_prgBank = 0;
        m_chrBank[0] = 0;
        m_chrBank[1] = 0;
        m_chrBank[2] = 0;
        m_chrBank[3] = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgBank);
        s.array(m_chrBank, 1, 4);
    }
};
