#pragma once

#include <array>

#include "BaseMapper.h"

// iNES Mapper 55 (BTL-MARIO1-MALEE2)
// - CPU $6000-$6FFF: 2KB PRG-ROM, mirrored once
// - CPU $7000-$7FFF: 2KB PRG-RAM, mirrored once
// - CPU $8000-$FFFF: fixed 32KB PRG-ROM
// - PPU $0000-$1FFF: fixed 8KB CHR
class Mapper055 : public BaseMapper
{
private:
    std::array<uint8_t, 0x0800> m_prgRam{};

public:
    Mapper055(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(0, addr);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(addr < 0x1000) {
            return cd().readPrg<BankSize::B16K>(2, addr & 0x07FF);
        }
        return m_prgRam[addr & 0x07FF];
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        if(addr < 0x1000) return;
        m_prgRam[addr & 0x07FF] = data;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }

    void reset() override
    {
        m_prgRam.fill(0);
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_prgRam);
    }
};
