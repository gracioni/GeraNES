#pragma once

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
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_selectedBank,addr);
        return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>()-1,addr);
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
