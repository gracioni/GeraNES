#pragma once

#include "BaseMapper.h"

// iNES Mapper 120 (LH15)
// - CPU $6000-$7FFF: switchable 8KB PRG-ROM bank
// - CPU $8000-$FFFF: fixed to PRG-ROM banks 2-5
// - PPU $0000-$1FFF: unbanked CHR-RAM
// - Write register decoded at CPU $41xx with A8 set (NESdev mask $E100)
class Mapper120 : public BaseMapper
{
private:
    uint8_t m_saveBank = 0;
    uint8_t m_prg8kMask = 0;

public:
    Mapper120(ICartridgeData& cd) : BaseMapper(cd)
    {
        if(cd.chrRamSize() == 0) allocateChrRam(static_cast<int>(BankSize::B8K));
        m_prg8kMask = calculateMask(cd.numberOfPRGBanks<BankSize::B8K>());
    }

    GERANES_HOT void writeMapperRegisterAbsolute(uint16_t addr, uint8_t data) override
    {
        if((addr & 0xE100) != 0x4100) return;
        m_saveBank = static_cast<uint8_t>(data & 0x07) & m_prg8kMask;
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        return cd().readPrg<BankSize::B8K>(m_saveBank, addr);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        const uint8_t slot = static_cast<uint8_t>((addr >> 13) & 0x03);
        return cd().readPrg<BankSize::B8K>(static_cast<uint8_t>(2 + slot) & m_prg8kMask, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return BaseMapper::readChr(addr);
    }

    GERANES_HOT MirroringType mirroringType() override
    {
        return MirroringType::VERTICAL;
    }

    void reset() override
    {
        m_saveBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_saveBank);
        SERIALIZEDATA(s, m_prg8kMask);
    }
};
