#pragma once

#include "BaseMapper.h"

class Mapper057 : public BaseMapper
{
private:
    uint8_t m_registers[2] = {0, 0};
    uint8_t m_prgMask = 0;
    uint8_t m_chrMask = 0;

public:
    Mapper057(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B16K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B8K>());
    }

    void updateState()
    {
        m_registers[0] &= 0xFF;
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);
        switch(absolute & 0x8800) {
        case 0x8000: m_registers[0] = data; break;
        case 0x8800: m_registers[1] = data; break;
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        uint8_t bank = 0;
        if(m_registers[1] & 0x10) {
            bank = static_cast<uint8_t>((m_registers[1] >> 5) & 0x06);
            if(addr >= 0x4000) ++bank;
        } else {
            bank = static_cast<uint8_t>((m_registers[1] >> 5) & 0x07);
        }
        return cd().readPrg<BankSize::B16K>(bank & m_prgMask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        uint8_t bank = static_cast<uint8_t>((((m_registers[0] & 0x40) >> 3) | ((m_registers[0] | m_registers[1]) & 0x07)) & m_chrMask);
        return cd().readChr<BankSize::B8K>(bank, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        if(cd().useFourScreenMirroring()) return MirroringType::FOUR_SCREEN;
        return (m_registers[1] & 0x08) ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_registers[0] = 0;
        m_registers[1] = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        s.array(m_registers, 1, 2);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrMask);
    }
};
