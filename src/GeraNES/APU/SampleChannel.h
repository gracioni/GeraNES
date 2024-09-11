#ifndef SAMPLE_CHANNEL_H
#define SAMPLE_CHANNEL_H

#include "GeraNES/Serialization.h"
#include "GeraNES/Settings.h"
#include "GeraNES/IAudioOutput.h"

#include "signal/SigSlot.h"

class SampleChannel
{
private:

    const uint16_t NTSC_DMC_PERIOD_TABLE[16] = {
        428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
    };

    const uint16_t PAL_DMC_PERIOD_TABLE[16] = {
        398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118,  98, 78, 66, 50
    };

    Settings& m_settings;
    IAudioOutput& m_audioGenerator;

    uint8_t m_playMode;

    uint16_t m_currentAddr;

    int m_deltaCounter;
    uint16_t m_sampleAddr;
    uint16_t m_sampleLength;

    uint16_t m_bytesRemaining;
    size_t m_periodIndex;
    int m_periodCounter;
    int m_shiftCounter;

    bool m_interruptFlag;

    uint32_t m_cpuCycleCounter;
    bool m_directControlFlag;

    bool m_sampleBufferFilled;

    void readSample(bool reload)
    {
        dmcRequest(m_currentAddr | 0x8000, reload);
        m_currentAddr = (m_currentAddr + 1) & 0x7FFF;
    }

    void updateDeltaCounter(uint8_t data) {

        uint8_t shift =  data;

        for(int c = 0; c < 8; ++c)
        {
            if (shift & 1)
            {
                m_deltaCounter += 2*8;
                if (m_deltaCounter > 127) m_deltaCounter = 127;
            }
            else {
                m_deltaCounter -= 2*8;
                if(m_deltaCounter < 0) m_deltaCounter = 0;
            }

            shift >>= 1;
        }
    }

    GERANES_INLINE uint16_t getPeriod() {

        if(m_settings.region() == Settings::PAL)
            return PAL_DMC_PERIOD_TABLE[m_periodIndex];

        return NTSC_DMC_PERIOD_TABLE[m_periodIndex];
    }

public:

    SigSlot::Signal<uint16_t, bool> dmcRequest;

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_playMode);

        SERIALIZEDATA(s, m_currentAddr);

        SERIALIZEDATA(s, m_deltaCounter);
        SERIALIZEDATA(s, m_sampleAddr);
        SERIALIZEDATA(s, m_sampleLength);

        SERIALIZEDATA(s, m_bytesRemaining);
        SERIALIZEDATA(s, m_periodIndex);
        SERIALIZEDATA(s, m_periodCounter);
        SERIALIZEDATA(s, m_shiftCounter);

        SERIALIZEDATA(s, m_interruptFlag);

        SERIALIZEDATA(s, m_cpuCycleCounter);
        SERIALIZEDATA(s, m_directControlFlag);

        SERIALIZEDATA(s, m_sampleBufferFilled);   

    }

    SampleChannel(Settings& settings, IAudioOutput& audioOutput) : m_settings(settings), m_audioGenerator(audioOutput)
    {
        init();
    }

    void init()
    {

        m_sampleBufferFilled = false;

        //bit 7 = interrupt enable
        //bit 6 = loop enable
        m_playMode = 0x00;

        m_currentAddr = 0;

        m_deltaCounter = 64;

        m_sampleAddr = 0;
        m_sampleLength = 0;

        m_bytesRemaining = 0;

        m_periodIndex = 0;

        m_periodCounter = getPeriod();

        m_shiftCounter = 1;

        m_interruptFlag = false;

        m_cpuCycleCounter = 0;
        m_directControlFlag = false;

    }

    void write(int addr, uint8_t data)
    {
        switch(addr)
        {
        case 0x0000:
        {
            m_playMode = data & 0xC0;

            m_periodIndex = data&0x0F;

            //f = CPU / (16 * (t + 1))
            //t = (CPU / (16 * f)) - 1

            m_audioGenerator.setSampleFrequency( 2*(m_settings.CPUClockHz()/16.0)/(getPeriod()+1));
            m_audioGenerator.setSampleVolume(1.0);

            if ((data & 0x80) == 0x00) m_interruptFlag = false;
            break;
        }
        case 0x0001:

            m_deltaCounter = data & 0x7F;

            if(m_directControlFlag) {

                float CPUClock = m_settings.CPUClockHz();

                float period =(m_cpuCycleCounter+1)/(8*2*(CPUClock/16.0));

                m_audioGenerator.addSampleDirect(period, (m_deltaCounter-0.5*127.0)/127.0);

            }

            m_cpuCycleCounter = 0;
            m_directControlFlag = true;

            break;

        case 0x0002:
            m_sampleAddr = (data << 6) | 0xC000;
            break;
        case 0x0003:
            m_sampleLength = (data << 4) + 1;
            break;
        }

    }

    void reload(bool reload)
    {
        m_currentAddr = m_sampleAddr;
        m_bytesRemaining = m_sampleLength;

        //if the latch is clear process immediately
        if(!m_sampleBufferFilled && m_bytesRemaining > 0) readSample(reload);
    }

    bool getInterruptFlag(void)
    {
        return m_interruptFlag;
    }

    void setInterruptFlag(bool state)
    {
        m_interruptFlag = state;
    }

    void setEnabled(bool status)
    {
        if(status)
        {
            if(getBytesRemaining() == 0) reload(false);
        }
        else
        {
            m_bytesRemaining  = 0;
            //will silence when stop playing the remaining bits of the shift register
        }

        m_interruptFlag = false;
    }

    void cycle()
    {
        if(m_cpuCycleCounter < NTSC_DMC_PERIOD_TABLE[0])  m_cpuCycleCounter++;
        else m_directControlFlag = false;   

        if( --m_periodCounter == 0)
        {
            m_periodCounter = getPeriod();

            clockDMC();
        }

    }

    void clockDMC() {

        if(--m_shiftCounter == 0) {

            m_shiftCounter = 8;        

            m_sampleBufferFilled = false;

            if(m_bytesRemaining > 0) {
                readSample(true);
            }
            
        }
    }

    int getBytesRemaining(void)
    {
        return m_bytesRemaining;
    }

    void loadSampleBuffer(uint8_t data) {

        m_sampleBufferFilled = true;

        updateDeltaCounter(data);

        m_audioGenerator.addSample( (m_deltaCounter-0.5*127.0)/127.0 );

        if(m_bytesRemaining > 0) --m_bytesRemaining;

        if(m_bytesRemaining == 0) {

            switch(m_playMode)
            {
            case 0x00:
                break;
            case 0x40:
            case 0xC0:
                reload(true);
                break;
            case 0x80:
                m_interruptFlag = true;
                break;
            }
        }


    }

};

#endif
