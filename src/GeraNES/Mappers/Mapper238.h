#pragma once

#include "Mapper004.h"

class Mapper238 : public Mapper004
{
private:
    uint8_t m_exReg = 0;

public:
    Mapper238(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT uint8_t readMapperRegisterAbsolute(uint16_t addr, uint8_t openBusData) override
    {
        if(addr >= 0x4020 && addr <= 0x7FFF) {
            return m_exReg;
        }
        return Mapper004::readMapperRegisterAbsolute(addr, openBusData);
    }

    GERANES_HOT void writeMapperRegisterAbsolute(uint16_t addr, uint8_t value) override
    {
        if(addr >= 0x4020 && addr <= 0x7FFF) {
            static const uint8_t securityLut[4] = {0x00, 0x02, 0x02, 0x03};
            m_exReg = securityLut[value & 0x03];
            return;
        }
        Mapper004::writeMapperRegisterAbsolute(addr, value);
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_exReg);
    }
};
