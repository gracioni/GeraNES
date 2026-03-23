#pragma once

#include "Mapper004.h"

// iNES Mapper 199 (Waixing FS309)
// MMC3-like board with fixed 8KB CHR-RAM and 12KB of mapped PRG-RAM at $5000-$7FFF.
class Mapper199 : public Mapper004
{
private:
    GERANES_INLINE size_t ramOffset5000(int addr) const
    {
        return static_cast<size_t>(addr & 0x0FFF);
    }

    GERANES_INLINE size_t ramOffset6000(int addr) const
    {
        return static_cast<size_t>(0x1000 + (addr & 0x1FFF));
    }

public:
    Mapper199(ICartridgeData& cd) : Mapper004(cd)
    {
        if(cd.chrRamSize() == 0) {
            allocateChrRam(static_cast<int>(BankSize::B8K));
        }
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t openBusData) override
    {
        uint8_t* ram = saveRamData();
        if(ram == nullptr || saveRamSize() == 0) return openBusData;
        return ram[ramOffset5000(addr) % saveRamSize()];
    }

    GERANES_HOT void writeMapperRegister(int addr, uint8_t value) override
    {
        uint8_t* ram = saveRamData();
        if(ram == nullptr || saveRamSize() == 0) return;
        ram[ramOffset5000(addr) % saveRamSize()] = value;
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        uint8_t* ram = saveRamData();
        if(ram == nullptr || saveRamSize() == 0) return 0;
        return ram[ramOffset6000(addr) % saveRamSize()];
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t value) override
    {
        uint8_t* ram = saveRamData();
        if(ram == nullptr || saveRamSize() == 0) return;
        ram[ramOffset6000(addr) % saveRamSize()] = value;
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        return BaseMapper::readChr(addr);
    }

    GERANES_HOT void writeChr(int addr, uint8_t data) override
    {
        BaseMapper::writeChr(addr, data);
    }
};
