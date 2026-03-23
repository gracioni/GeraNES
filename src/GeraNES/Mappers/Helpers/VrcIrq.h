#pragma once

#include "GeraNES/Serialization.h"

class VrcIrq
{
private:
    uint8_t m_reloadValue = 0;
    uint8_t m_counter = 0;
    int16_t m_prescaler = 0;
    bool m_enabled = false;
    bool m_enabledAfterAck = false;
    bool m_cycleMode = false;
    bool m_interruptFlag = false;

public:
    void reset()
    {
        m_reloadValue = 0;
        m_counter = 0;
        m_prescaler = 0;
        m_enabled = false;
        m_enabledAfterAck = false;
        m_cycleMode = false;
        m_interruptFlag = false;
    }

    void cycle()
    {
        if(!m_enabled) return;

        m_prescaler -= 3;
        if(!m_cycleMode && m_prescaler > 0) return;

        if(m_counter == 0xFF) {
            m_counter = m_reloadValue;
            m_interruptFlag = true;
        }
        else {
            ++m_counter;
        }

        m_prescaler += 341;
    }

    void setReloadValue(uint8_t value)
    {
        m_reloadValue = value;
    }

    void setControlValue(uint8_t value)
    {
        m_enabledAfterAck = (value & 0x01) != 0;
        m_enabled = (value & 0x02) != 0;
        m_cycleMode = (value & 0x04) != 0;

        if(m_enabled) {
            m_counter = m_reloadValue;
            m_prescaler = 341;
        }

        m_interruptFlag = false;
    }

    void acknowledge()
    {
        m_enabled = m_enabledAfterAck;
        m_interruptFlag = false;
    }

    bool interruptFlag() const
    {
        return m_interruptFlag;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_reloadValue);
        SERIALIZEDATA(s, m_counter);
        SERIALIZEDATA(s, m_prescaler);
        SERIALIZEDATA(s, m_enabled);
        SERIALIZEDATA(s, m_enabledAfterAck);
        SERIALIZEDATA(s, m_cycleMode);
        SERIALIZEDATA(s, m_interruptFlag);
    }
};
