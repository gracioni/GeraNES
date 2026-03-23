#pragma once

#include "BaseMapper.h"

class Mapper228 : public BaseMapper
{
private:
    uint8_t m_prgPage = 0;
    uint8_t m_chrPage = 0;
    bool m_prg32Mode = false;
    bool m_horizontalMirroring = false;

public:
    Mapper228(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t value) override
    {
        const uint16_t absolute = static_cast<uint16_t>(addr + 0x8000);

        uint8_t chipSelect = static_cast<uint8_t>((absolute >> 11) & 0x03);
        if(chipSelect == 3) chipSelect = 2;

        m_prgPage = static_cast<uint8_t>(((absolute >> 6) & 0x1F) | (chipSelect << 5));
        m_prg32Mode = (absolute & 0x20) != 0;
        m_chrPage = static_cast<uint8_t>(((absolute & 0x0F) << 2) | (value & 0x03));
        m_horizontalMirroring = (absolute & 0x2000) != 0;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(m_prg32Mode) {
            const uint8_t bank = static_cast<uint8_t>(m_prgPage & 0xFE);
            if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(bank, addr);
            return cd().readPrg<BankSize::B16K>(static_cast<uint8_t>(bank + 1), addr);
        }

        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_prgPage, addr);
        return cd().readPrg<BankSize::B16K>(m_prgPage, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return readChrRam<BankSize::B8K>(m_chrPage, addr);
        return cd().readChr<BankSize::B8K>(m_chrPage, addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(hasChrRam()) writeChrRam<BankSize::B8K>(m_chrPage, addr, data);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return m_horizontalMirroring ? MirroringType::HORIZONTAL : MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_prgPage = 0;
        m_chrPage = 0;
        m_prg32Mode = false;
        m_horizontalMirroring = false;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgPage);
        SERIALIZEDATA(s, m_chrPage);
        SERIALIZEDATA(s, m_prg32Mode);
        SERIALIZEDATA(s, m_horizontalMirroring);
    }
};
