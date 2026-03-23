#pragma once

#include "BaseMapper.h"

class Mapper143 : public BaseMapper
{
public:
    Mapper143(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT uint8_t readMapperRegister(int addr, uint8_t /*openBusData*/) override
    {
        return static_cast<uint8_t>((~addr & 0x3F) | 0x40);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(0, addr);
        return cd().readPrg<BankSize::B16K>(1, addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);
        return cd().readChr<BankSize::B8K>(0, addr);
    }
};
