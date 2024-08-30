#ifndef INCLUDE_MAPPER000
#define INCLUDE_MAPPER000

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

    GERANES_INLINE_HOT uint8_t readPRG32k(int addr) override
    {
        if(addr < 0x4000) return m_cartridgeData.readPRG<W16K>(0,addr);
        return m_cartridgeData.readPRG<W16K>(m_cartridgeData.numberOfPRGBanks<W16K>()-1,addr);
    }

    GERANES_INLINE_HOT uint8_t readCHR8k(int addr) override
    {
        if(has8kVRAM()) return IMapper::readCHR8k(addr);

        return m_cartridgeData.readCHR<W8K>(0,addr);
    }   

    void serialization(SerializationBase& s) override
    {
        IMapper::serialization(s);       
    }

};

#endif
