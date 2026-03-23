#pragma once

#include "BaseMapper.h"

class Mapper235 : public BaseMapper
{
private:
    bool m_openBus = false;
    uint8_t m_prgLow = 0;
    uint8_t m_prgHigh = 1;
    uint8_t m_mirroringMode = 0;

public:
    Mapper235(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t /*value*/) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        m_mirroringMode = (absolute & 0x0400) ? 2 : ((absolute & 0x2000) ? 1 : 0);

        static const uint8_t config[4][4][2] = {
            { { 0x00, 0 }, { 0x00, 1 }, { 0x00, 1 }, { 0x00, 1 } },
            { { 0x00, 0 }, { 0x00, 1 }, { 0x20, 0 }, { 0x00, 1 } },
            { { 0x00, 0 }, { 0x00, 1 }, { 0x20, 0 }, { 0x40, 0 } },
            { { 0x00, 0 }, { 0x20, 0 }, { 0x40, 0 }, { 0x60, 0 } }
        };

        uint8_t mode = 3;
        switch(cd().numberOfPRGBanks<BankSize::B16K>()) {
        case 64: mode = 0; break;
        case 128: mode = 1; break;
        case 256: mode = 2; break;
        default: break;
        }

        uint8_t bank = static_cast<uint8_t>(config[mode][(absolute >> 8) & 0x03][0] | (absolute & 0x1F));
        m_openBus = false;

        if(config[mode][(absolute >> 8) & 0x03][1]) {
            m_openBus = true;
            return;
        }

        if(absolute & 0x0800) {
            bank = static_cast<uint8_t>((bank << 1) | ((absolute >> 12) & 0x01));
            m_prgLow = bank;
            m_prgHigh = bank;
        } else {
            bank = static_cast<uint8_t>(bank << 1);
            m_prgLow = bank;
            m_prgHigh = static_cast<uint8_t>(bank + 1);
        }
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(m_openBus) return 0;
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgLow, addr);
        return cd().readPrg<BankSize::B16K>(m_prgHigh, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        switch(m_mirroringMode) {
        case 2: return MirroringType::SINGLE_SCREEN_A;
        case 1: return MirroringType::HORIZONTAL;
        default: return MirroringType::VERTICAL;
        }
    }

    void reset() override
    {
        m_openBus = false;
        m_prgLow = 0;
        m_prgHigh = 1;
        m_mirroringMode = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_openBus);
        SERIALIZEDATA(s, m_prgLow);
        SERIALIZEDATA(s, m_prgHigh);
        SERIALIZEDATA(s, m_mirroringMode);
    }
};
