#ifndef MAPPER000_H
#define MAPPER000_H

#include "IMapper.h"

//NROM
class Mapper000 : public IMapper
{

public:

    Mapper000(ICartridgeData& cd) : IMapper(cd)
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
        if(hasVRAM()) return IMapper::readChr(addr);

        return m_cd.readChr<W8K>(0,addr);
    }   

    void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);       
    }

};

#endif
