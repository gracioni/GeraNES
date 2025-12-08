#pragma once

#include "GeraNES/Serialization.h"
#include "APUCommon.h"

class SquareChannel
{
private:

    bool m_enabled;
    uint16_t m_lengthCounter;
    uint16_t m_period;

    uint8_t m_duty;

    bool m_loop;

    bool m_constantVolumeMode;
    uint8_t m_envelopPeriod;
    uint8_t m_envelopVolume;
    uint8_t m_constantVolume;
    uint8_t m_envelopCounter;

    bool m_sweepEnabled;
    uint8_t m_sweepPeriod;
    bool m_sweepNegative;
    uint8_t m_sweepShift;
    bool m_sweepWritten;

    uint16_t m_sweepCounter;
    uint16_t m_sweepResult;

    bool m_lengthUpdated;

public:

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_enabled);
        SERIALIZEDATA(s, m_lengthCounter);
        SERIALIZEDATA(s, m_period);

        SERIALIZEDATA(s, m_duty);

        SERIALIZEDATA(s, m_loop);
        SERIALIZEDATA(s, m_constantVolumeMode);
        SERIALIZEDATA(s, m_envelopPeriod);
        SERIALIZEDATA(s, m_envelopVolume);
        SERIALIZEDATA(s, m_constantVolume);
        SERIALIZEDATA(s, m_envelopCounter);

        SERIALIZEDATA(s, m_sweepEnabled);
        SERIALIZEDATA(s, m_sweepPeriod);
        SERIALIZEDATA(s, m_sweepNegative);
        SERIALIZEDATA(s, m_sweepShift);
        SERIALIZEDATA(s, m_sweepWritten);

        SERIALIZEDATA(s, m_sweepCounter);
        SERIALIZEDATA(s, m_sweepResult);

        SERIALIZEDATA(s, m_lengthUpdated);
    }

    SquareChannel()
    {
        init();
    }

    void init()
    {
        m_enabled = false;
        m_lengthCounter = 0;
        m_period = 1;

        m_duty = 0;

        m_loop = false;
        m_constantVolumeMode = false;
        m_envelopPeriod = 1;
        m_envelopVolume = 0;
        m_constantVolume = 0;
        m_envelopCounter = 0;

        m_sweepEnabled = false;
        m_sweepPeriod = 1;
        m_sweepNegative = false;
        m_sweepShift = 0;
        m_sweepWritten = false;

        m_sweepCounter = 0;
        m_sweepResult = 0;

        m_lengthUpdated = false;
    }

    void write(int addr, uint8_t data)
    {
        switch(addr)
        {
            case 0x0000:

                m_duty = data >> 6;
                m_constantVolume = data&0x0F;
                m_loop = data & 0x20;
                m_constantVolumeMode = data & 0x10;
                m_envelopPeriod = (data & 0x0F);

                break;

            case 0x0001:

                m_sweepEnabled = data & 0x80;
                m_sweepPeriod = ((data >> 4) & 0x07);
                m_sweepNegative	 = data & 0x08;
                m_sweepShift	 = data & 0x07;
                m_sweepWritten	 = true;
                break;

            case 0x0002:

                m_period = data | (m_period & 0x0700);
                break;

            case 0x0003:

                m_period = ((uint16_t)(data & 0x07) << 8) | (m_period & 0x00FF);

                if(m_enabled && (!m_lengthUpdated || (m_lengthUpdated && m_lengthCounter == 0)))
                    m_lengthCounter = LENGTH_TABLE[(data & 0xF8) >> 3];

                m_envelopVolume = 0x0F;
                break;
        }
    }

    

    void updateLengthCounter()
    {
        m_lengthUpdated = true;
        if (m_loop == false && m_lengthCounter > 0) --m_lengthCounter;
    }

    void updateSweep(int channel)
    {
        if(--m_sweepCounter == 0)
        {
            m_sweepCounter = m_sweepPeriod+1;

            if (m_sweepEnabled && m_sweepShift > 0 && m_period > 8)
            {
                int delta = m_period >> m_sweepShift;

                if(m_sweepNegative)
                {
                    m_period -= delta;
                    if(channel == 0) m_period--;
                }
                else if( (m_period+delta) < 0x800)
                {
                    m_period += delta;
                }
            }
        }

        if (m_sweepWritten)
        {
            m_sweepWritten = false;
            m_sweepCounter = m_sweepPeriod+1;
        }
    }


    bool isDisabledBySweep()
    {
        if(m_period < 8) return true;
        if(m_period > 0x7FF) return true;

        if(!m_sweepNegative)
        {
            if( (m_period + (m_period >> m_sweepShift)) > 0x7FF) return true;
        }

        return false;
    }

    void updateEnvelop()
    {
        if(m_envelopCounter > 0) m_envelopCounter--;

        if (m_envelopCounter == 0)
        {
            m_envelopCounter = m_envelopPeriod+1;

            if (!m_constantVolumeMode)
            {
                if (m_loop)
                    m_envelopVolume = (m_envelopVolume - 1) & 0x0F;
                else if (m_envelopVolume > 0)
                    m_envelopVolume--;
            }
        }
    }

    bool isEnabled()
    {
        return m_enabled == true && m_lengthCounter > 0 &&  !isDisabledBySweep() && m_period > 0;
    }

    void setEnabled(bool status)
    {
        m_enabled = status;
        if(!status) m_lengthCounter = 0;
    }

    uint8_t getVolume()
    {
        if(isEnabled())
            return m_constantVolumeMode ? m_constantVolume : m_envelopVolume;

        return 0;
    }

    uint16_t getPeriod()
    {
        return m_period;
    }

    uint8_t getDuty()
    {
        return m_duty;
    }

    uint16_t getLengthCounter()
    {
        return m_lengthCounter;
    }

    void cycle() {
        m_lengthUpdated = false;
    }

};
