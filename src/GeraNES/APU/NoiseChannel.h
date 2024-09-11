#ifndef NOISE_CHANNEL_H
#define NOISE_CHANNEL_H

#include "GeraNES/Serialization.h"
#include "APUCommon.h"

class NoiseChannel
{
private:

    const uint16_t NTSC_NOISE_PERIOD_TABLE[16] = {
        4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
    };

    const uint16_t PAL_NOISE_PERIOD_TABLE[16] = {
        4, 7, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778
    };

    Settings& m_settings;

    bool m_enabled;
    uint16_t m_lengthCounter;
    uint16_t m_period;

    bool m_loop;

    bool m_constantVolumeMode;
    uint8_t m_envelopVolume;
    uint8_t m_constantVolumeAndEnvelopPeriod;
    uint8_t m_envelopCounter;

    bool m_mode;

    bool m_lengthUpdated;

public:

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_enabled);
        SERIALIZEDATA(s, m_lengthCounter);
        SERIALIZEDATA(s, m_period);

        SERIALIZEDATA(s, m_loop);
        SERIALIZEDATA(s, m_constantVolumeMode);
        SERIALIZEDATA(s, m_envelopVolume);
        SERIALIZEDATA(s, m_constantVolumeAndEnvelopPeriod);
        SERIALIZEDATA(s, m_envelopCounter);

        SERIALIZEDATA(s, m_mode);

        SERIALIZEDATA(s, m_lengthUpdated);
    }

    NoiseChannel(Settings& settings) : m_settings(settings)
    {
        init();
    }

    void init(void)
    {
        m_enabled = false;
        m_lengthCounter = 0;
        m_period = 1;

        m_loop = false;
        m_constantVolumeMode = false;
        m_envelopVolume = 0;
        m_constantVolumeAndEnvelopPeriod = 1;
        m_envelopCounter = 0;

        m_mode = false;

        m_lengthUpdated = false;
    }

    void write(int addr, uint8_t data)
    {
        switch(addr)
        {
        case 0x0000:
        {
            m_constantVolumeAndEnvelopPeriod = data&0x0F;
            m_loop = data & 0x20;
            m_constantVolumeMode = data & 0x10;
            break;
        }
        case 0x0001:
            break;

        case 0x0002:
        {
            m_mode = data&0x80;

            if(m_settings.region() == Settings::NTSC)
                m_period = NTSC_NOISE_PERIOD_TABLE[data&0x0F];
            else
                m_period = PAL_NOISE_PERIOD_TABLE[data&0x0F];

            break;
        }
        case 0x0003:
        {
            if(m_enabled && (!m_lengthUpdated || (m_lengthUpdated && m_lengthCounter == 0)))
                m_lengthCounter = LENGTH_TABLE[(data & 0xF8) >> 3];

            m_envelopVolume = 0x0F;
            break;
        }
        }
    }

    void updateLengthCounter(void)
    {
        m_lengthUpdated = true;
        if (m_loop == false && m_lengthCounter > 0) --m_lengthCounter;
    }

    void updateEnvelop()
    {
        if(m_envelopCounter > 0) m_envelopCounter--;

        if (m_envelopCounter == 0)
        {
            m_envelopCounter = m_constantVolumeAndEnvelopPeriod+1;

            if (!m_constantVolumeMode)
            {
                if (m_loop)
                    m_envelopVolume = (m_envelopVolume - 1) & 0x0F;
                else if (m_envelopVolume > 0)
                    m_envelopVolume--;
            }
        }
    }

    bool isEnabled(void)
    {
        return m_enabled == true && m_lengthCounter > 0 && m_period > 0;
    }

    void setEnabled(bool status)
    {
        m_enabled = status;
        if(!status) m_lengthCounter = 0;
    }

    uint8_t getVolume(void)
    {
        if(isEnabled())
            return m_constantVolumeMode ? m_constantVolumeAndEnvelopPeriod : m_envelopVolume;

        return 0;
    }

    uint16_t getPeriod(void)
    {
        return m_period;
    }

    uint16_t getLengthCounter(void)
    {
        return m_lengthCounter;
    }

    bool getMode(void)
    {
        return m_mode;
    }

    void cycle() {
        m_lengthUpdated = false;
    }

};

#endif
