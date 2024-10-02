#ifndef MAPPER002_H
#define MAPPER002_H

#include "BaseMapper.h"

//UxROM
class Mapper002 : public BaseMapper
{
private:

    int m_selectedBank = 0;

public:

    Mapper002(ICartridgeData& cd) : BaseMapper(cd)
    {
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return m_cd.readPrg<W16K>(m_selectedBank,addr);
        return m_cd.readPrg<W16K>(m_cd.numberOfPRGBanks<W16K>()-1,addr);
    }

    GERANES_HOT void writePrg(int /*addr*/, uint8_t data) override
    {
        m_selectedBank = data;
    };

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_selectedBank);
    }

};

#endif
