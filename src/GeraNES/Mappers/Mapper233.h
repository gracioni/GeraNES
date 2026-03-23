#pragma once

#include "Mapper226.h"

class Mapper233 : public Mapper226
{
private:
    uint8_t m_resetBit = 0;

protected:
    uint8_t getPrgPage() const override
    {
        return static_cast<uint8_t>((m_registers[0] & 0x1F)
            | (m_resetBit << 5)
            | ((m_registers[1] & 0x01) << 6));
    }

public:
    Mapper233(ICartridgeData& cd) : Mapper226(cd)
    {
    }

    void reset() override
    {
        Mapper226::reset();
        m_resetBit ^= 0x01;
        updatePrg();
    }

    void serialization(SerializationBase& s) override
    {
        Mapper226::serialization(s);
        SERIALIZEDATA(s, m_resetBit);
    }
};
