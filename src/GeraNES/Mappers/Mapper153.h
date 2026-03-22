#pragma once

#include "Mapper016.h"

class Mapper153 : public Mapper016
{
private:
    bool m_prgRamEnabled = false;

protected:
    GERANES_INLINE bool uses6000WriteRange() const override
    {
        return false;
    }

    void handleRegisterD(uint8_t data) override
    {
        m_prgRamEnabled = (data & 0x20) != 0;
    }

public:
    Mapper153(ICartridgeData& cd) : Mapper016(cd)
    {
    }

    GERANES_HOT void writeSaveRam(int addr, uint8_t data) override
    {
        if(m_prgRamEnabled) BaseMapper::writeSaveRam(addr, data);
    }

    GERANES_HOT uint8_t readSaveRam(int addr) override
    {
        if(m_prgRamEnabled) return BaseMapper::readSaveRam(addr);
        return 0;
    }

    void reset() override
    {
        Mapper016::reset();
        m_prgRamEnabled = false;
    }

    void serialization(SerializationBase& s) override
    {
        Mapper016::serialization(s);
        SERIALIZEDATA(s, m_prgRamEnabled);
    }
};
