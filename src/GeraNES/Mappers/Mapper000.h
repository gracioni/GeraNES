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
        if(addr < 0x4000) return m_cd.readPrg<WindowSize::W16K>(0,addr);
        return m_cd.readPrg<WindowSize::W16K>(m_cd.numberOfPRGBanks<WindowSize::W16K>()-1,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        return m_cd.readChr<WindowSize::W8K>(0,addr);
    }   

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);       
    }

};
