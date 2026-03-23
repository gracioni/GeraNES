#pragma once

#include "Mapper004.h"

// iNES Mapper 254
// MMC3 variant with a simple $6000-$7FFF read XOR gate controlled by MMC3 writes.
class Mapper254 : public Mapper004
{
private:
    uint8_t m_exReg0 = 0;
    uint8_t m_exReg1 = 0;

public:
    Mapper254(ICartridgeData& cd) : Mapper004(cd)
    {
    }

    GERANES_HOT void writePrg(int addr, uint8_t data) override
    {
        if(addr == 0x0000) {
            m_exReg0 = 0xFF;
        } else if(addr == 0x2001) {
            m_exReg1 = data;
        }

        Mapper004::writePrg(addr, data);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        const uint8_t value = BaseMapper::readSaveRam(addr);
        return m_exReg0 ? value : static_cast<uint8_t>(value ^ m_exReg1);
    }

    void reset() override
    {
        Mapper004::reset();
        m_exReg0 = 0;
        m_exReg1 = 0;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper004::serialization(s);
        SERIALIZEDATA(s, m_exReg0);
        SERIALIZEDATA(s, m_exReg1);
    }
};
