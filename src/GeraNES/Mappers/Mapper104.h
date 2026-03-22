#pragma once

#include "BaseMapper.h"

class Mapper104 : public BaseMapper
{
private:
    uint8_t m_prgReg = 0;
    uint8_t m_prgMask = 0;

public:
    Mapper104(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        if(absolute >= 0xC000) {
            m_prgReg = static_cast<uint8_t>((m_prgReg & 0xF0) | (value & 0x0F));
        } else if(absolute <= 0x9FFF && (value & 0x08)) {
            m_prgReg = static_cast<uint8_t>((m_prgReg & 0x0F) | ((value << 4) & 0x70));
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgReg & m_prgMask, addr);
        return cd().readPrg<BankSize::B16K>(static_cast<uint8_t>((m_prgReg & 0x70) | 0x0F) & m_prgMask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    void reset() override
    {
        m_prgReg = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgReg);
        SERIALIZEDATA(s, m_prgMask);
    }
};
