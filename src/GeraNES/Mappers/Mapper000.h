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

    GERANES_HOT uint8_t readPRG32k(int addr) override
    {
        if(addr < 0x4000) return m_cartridgeData.readPrg<W16K>(0,addr);
        return m_cartridgeData.readPrg<W16K>(m_cartridgeData.numberOfPRGBanks<W16K>()-1,addr);
    }

    GERANES_HOT uint8_t readCHR8k(int addr) override
    {
        if(has8kVRAM()) return IMapper::readCHR8k(addr);

        return m_cartridgeData.readChr<W8K>(0,addr);
    }   

    void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);       
    }

};

#endif
