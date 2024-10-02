#ifndef MAPPER000_H
#define MAPPER000_H

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
        if(addr < 0x4000) return m_cd.readPrg<W16K>(0,addr);
        return m_cd.readPrg<W16K>(m_cd.numberOfPRGBanks<W16K>()-1,addr);
    }

    GERANES_HOT uint8_t readChr(int addr) override
    {
        if(hasChrRam()) return BaseMapper::readChr(addr);

        return m_cd.readChr<W8K>(0,addr);
    }   

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);       
    }

};

#endif
