#pragma once

#include "BaseMapper.h"

//NROM
class Mapper000 : public BaseMapper
{

public:

    Mapper000(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    virtual ~Mapper000()
    {
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(0,addr);
        return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>()-1,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        return cd().readChr<BankSize::B8K>(0,addr);
    }   

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);       
    }

};
