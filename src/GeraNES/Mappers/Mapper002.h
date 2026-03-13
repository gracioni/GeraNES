#pragma once

#include "BaseMapper.h"

//UxROM
class Mapper002 : public BaseMapper
{
private:

    int m_selectedBank = 0;
    bool m_hasBusConflicts = false;

public:

    Mapper002(ICartridgeData& cd) : BaseMapper(cd)
    {
        // NES 2.0 Mapper 2:
        // submapper 1 = no bus conflicts, submapper 2 = bus conflicts.
        m_hasBusConflicts = (cd.subMapperId() == 2);
    }

    GERANES_HOT uint8_t readPrg(int addr) override
    {
        if(addr < 0x4000) return cd().readPrg<BankSize::B16K>(m_selectedBank,addr);
        return cd().readPrg<BankSize::B16K>(cd().numberOfPRGBanks<BankSize::B16K>()-1,addr);
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(m_hasBusConflicts) {
            // Effective write value is CPU data AND ROM data currently on bus.
            data &= readPrg(addr);
        }

        m_selectedBank = data;
    };

    void reset() override
    {
        m_selectedBank = 0;
    }

    void serialization(SerializationBase& s) override
    {
        BaseMapper::serialization(s);
        SERIALIZEDATA(s, m_selectedBank);
    }

};
