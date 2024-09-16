#ifndef TRIANGLE_CHANNEL_H
#define TRIANGLE_CHANNEL_H

#include "GeraNES/Serialization.h"
#include "APUCommon.h"

class TriangleChannel
{

private:

    bool m_enabled;
    uint16_t m_lengthCounter;
    uint16_t m_period;

    bool m_loop;

    uint8_t m_linearLoad;
    uint16_t m_linearCounter;

    bool m_lengthUpdated;

public:

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_enabled);
        SERIALIZEDATA(s, m_lengthCounter);
        SERIALIZEDATA(s, m_period);

        SERIALIZEDATA(s, m_loop);
        SERIALIZEDATA(s, m_linearLoad);
        SERIALIZEDATA(s, m_linearCounter);

        SERIALIZEDATA(s, m_lengthUpdated);
    }

    TriangleChannel()
    {
        init();
    }

    void init()
    {
        m_enabled = false;
        m_lengthCounter = 0;
        m_period = 1;
  
        m_loop = false;
        m_linearLoad = 0;
        m_linearCounter = 0;

        m_lengthUpdated = false;
    }

    void write(int addr, uint8_t data)
    {
        switch(addr)
        {
        case 0x0000:
            m_linearLoad = data & 0x7F;
            m_loop = data & 0x80;
            break;
        case 0x0001:
            break;
        case 0x0002:
            m_period = data | (m_period & 0x0700);
            break;
        case 0x0003:
            m_period = ((data & 0x07) << 8) | (m_period & 0xFF);

            if(m_enabled)
                m_lengthCounter = LENGTH_TABLE[(data & 0xF8) >> 3];

            break;
        }
    }

    void updateLengthCounter()
    {
        m_lengthUpdated = true;
        if ( !m_loop && m_lengthCounter > 0 ) m_lengthCounter--;
    }

    void updateLinearCounter()
    {
        if(m_loop) m_linearCounter = m_linearLoad;
        else
        {
            if(m_linearCounter > 0) m_linearCounter--;
        }
    }

    bool isEnabled()
    {
        return m_enabled && m_lengthCounter > 0 && m_linearCounter > 0 && m_period > 0;
    }

    void setEnabled(bool status)
    {
        m_enabled = status;
        if(!status) m_lengthCounter = 0;
    }

    uint8_t getVolume()
    {
        if(isEnabled())
            return (m_enabled && m_lengthCounter > 0 && m_linearCounter > 0) ? 15 : 0;

        return 0;
    }

    uint16_t getPeriod()
    {
        return m_period;
    }

    uint16_t getLengthCounter()
    {
        return m_lengthCounter;
    }

    void cycle() {
        m_lengthUpdated = false;
    }

};

#endif
