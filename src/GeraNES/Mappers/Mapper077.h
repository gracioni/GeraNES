#pragma once

#include <array>

#include "BaseMapper.h"

// iNES Mapper 77 (Napoleon Senki)
// - CPU $8000-$FFFF: [CCCC PPPP] with bus conflicts
//   - PPPP selects the 32KB PRG bank
//   - CCCC selects the 2KB CHR-ROM bank at $0000
// - PPU $0800-$2FFF behaves like 10KB of RAM in practice for emulation.
class Mapper077 : public BaseMapper
{
private:
    std::array<uint8_t, 0x2800> m_chrNtRam = {};
    uint8_t m_prgBank = 0;
    uint8_t m_prgMask = 0;
    uint8_t m_chrBank = 0;
    uint8_t m_chrMask = 0;

public:
    Mapper077(ICartridgeData& cd) : BaseMapper(cd)
    {
        m_prgMask = calculateMask(cd.numberOfPRGBanks<BankSize::B32K>());
        m_chrMask = calculateMask(cd.numberOfCHRBanks<BankSize::B2K>());
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        data &= readPrg(addr);
        m_prgBank = static_cast<uint8_t>(data & 0x0F) & m_prgMask;
        m_chrBank = static_cast<uint8_t>((data >> 4) & 0x0F) & m_chrMask;
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        return cd().readPrg<BankSize::B32K>(m_prgBank, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(addr < 0x0800) {
            return cd().readChr<BankSize::B2K>(m_chrBank, addr);
        }
        return m_chrNtRam[addr - 0x0800];
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        if(addr < 0x0800) return;
        m_chrNtRam[addr - 0x0800] = data;
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return MirroringType::FOUR_SCREEN;
    }

    GERANES_HOT bool useCustomNameTable(uint8_t /*index*/) override
    {
        return true;
    }

    GERANES_HOT uint8_t readCustomNameTable(uint8_t index, uint16_t addr) override
    {
        return m_chrNtRam[0x1800 + (index << 10) + addr];
    }

    GERANES_HOT void writeCustomNameTable(uint8_t index, uint16_t addr, uint8_t data) override
    {
        m_chrNtRam[0x1800 + (index << 10) + addr] = data;
    }

    void reset() override
    {
        m_chrNtRam.fill(0);
        m_prgBank = 0;
        m_chrBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_chrNtRam);
        SERIALIZEDATA(s, m_prgBank);
        SERIALIZEDATA(s, m_prgMask);
        SERIALIZEDATA(s, m_chrBank);
        SERIALIZEDATA(s, m_chrMask);
    }
};
