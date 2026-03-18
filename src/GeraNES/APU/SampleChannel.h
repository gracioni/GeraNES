#pragma once

#include <array>

#include "GeraNES/Serialization.h"
#include "GeraNES/Settings.h"
#include "GeraNES/IAudioOutput.h"

#include "signal/signal.h"

class SampleChannel
{
private:
    static constexpr std::array<uint16_t, 16> NTSC_DMC_PERIOD_TABLE = {
        428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
    };

    static constexpr std::array<uint16_t, 16> PAL_DMC_PERIOD_TABLE = {
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
    bool m_enabled;
    int m_enableReloadDelay;
    int m_disableDelay;
    int m_dmaRequestCooldown;
    bool m_pendingDmaRequest;
    bool m_pendingDmaReload;
    bool m_pendingDmaLoopDisabled;
    bool m_lastLoadedByteCameFromNonLoopingDma;

    void readSample(bool reload)
    {
        if(m_dmaRequestCooldown > 0) {
            m_pendingDmaRequest = true;
            m_pendingDmaReload = m_pendingDmaReload || reload;
            m_pendingDmaLoopDisabled = m_pendingDmaLoopDisabled || ((m_playMode & 0x40) == 0);
            return;
        }

        m_pendingDmaLoopDisabled = ((m_playMode & 0x40) == 0);
        dmcRequest(m_currentAddr | 0x8000, reload);
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

        if(m_settings.region() == Settings::Region::PAL)
            return PAL_DMC_PERIOD_TABLE[m_periodIndex];

        return NTSC_DMC_PERIOD_TABLE[m_periodIndex];
    }

public:

    SigSlot::Signal<uint16_t, bool> dmcRequest;
    SigSlot::Signal<> dmcCancelRequest;
    SigSlot::Signal<> dmcImplicitAbortRequest;

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
        SERIALIZEDATA(s, m_enabled);
        SERIALIZEDATA(s, m_enableReloadDelay);
        SERIALIZEDATA(s, m_disableDelay);
        SERIALIZEDATA(s, m_dmaRequestCooldown);
        SERIALIZEDATA(s, m_pendingDmaRequest);
        SERIALIZEDATA(s, m_pendingDmaReload);
        SERIALIZEDATA(s, m_pendingDmaLoopDisabled);
        SERIALIZEDATA(s, m_lastLoadedByteCameFromNonLoopingDma);

    }

    SampleChannel(Settings& settings, IAudioOutput& audioOutput) : m_settings(settings), m_audioGenerator(audioOutput)
    {
        init();
    }

    void init()
    {

        m_sampleBufferFilled = false;
        m_enabled = false;

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

        m_enableReloadDelay = 0;
        m_disableDelay = 0;
        m_dmaRequestCooldown = 0;
        m_pendingDmaRequest = false;
        m_pendingDmaReload = false;
        m_pendingDmaLoopDisabled = false;
        m_lastLoadedByteCameFromNonLoopingDma = false;

    }

    void reset()
    {
        m_sampleBufferFilled = false;
        m_enabled = false;
        m_currentAddr = m_sampleAddr;
        m_bytesRemaining = 0;
        m_shiftCounter = 1;
        m_interruptFlag = false;
        m_cpuCycleCounter = 0;
        m_directControlFlag = false;
        m_enableReloadDelay = 0;
        m_disableDelay = 0;
        m_dmaRequestCooldown = 0;
        m_pendingDmaRequest = false;
        m_pendingDmaReload = false;
        m_pendingDmaLoopDisabled = false;
        m_lastLoadedByteCameFromNonLoopingDma = false;
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

            m_audioGenerator.setChannelFrequency(IAudioOutput::Channel::Sample, 2*(m_settings.CPUClockHz()/16.0)/(getPeriod()+1));
            m_audioGenerator.setChannelVolume(IAudioOutput::Channel::Sample, 1.0);

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

    bool getInterruptFlag()
    {
        return m_interruptFlag;
    }

    void setInterruptFlag(bool state)
    {
        m_interruptFlag = state;
    }

    void setEnabled(bool status, bool cpuOddCycle)
    {
        if(status)
        {
            const bool disableWasPending = m_disableDelay > 0;
            const bool wasEnabled = m_enabled;
            m_enabled = true;
            m_disableDelay = 0;

            if(!wasEnabled || getBytesRemaining() == 0 || disableWasPending) {
                m_enableReloadDelay = cpuOddCycle ? 3 : 2;
            }

            if(getBytesRemaining() == 0) {
                m_currentAddr = m_sampleAddr;
                m_bytesRemaining = m_sampleLength;
            }
        }
        else
        {
            m_enableReloadDelay = 0;

            if(m_disableDelay == 0) {
                m_disableDelay = cpuOddCycle ? 3 : 2;
            }
            //will silence when stop playing the remaining bits of the shift register
        }

        m_interruptFlag = false;
    }

    void processControlDelays()
    {
        if(m_dmaRequestCooldown > 0 && --m_dmaRequestCooldown == 0) {
            if(m_pendingDmaRequest && m_bytesRemaining > 0) {
                const bool reload = m_pendingDmaReload;
                const bool loopDisabled = m_pendingDmaLoopDisabled;
                m_pendingDmaRequest = false;
                m_pendingDmaReload = false;
                m_pendingDmaLoopDisabled = false;
                m_pendingDmaLoopDisabled = loopDisabled;
                readSample(reload);
            } else {
                m_pendingDmaRequest = false;
                m_pendingDmaReload = false;
                m_pendingDmaLoopDisabled = false;
            }
        }

        if(m_disableDelay > 0 && --m_disableDelay == 0) {
            m_enabled = false;
            m_bytesRemaining = 0;
            dmcCancelRequest();
        }

        if(m_enableReloadDelay > 0 && --m_enableReloadDelay == 0) {
            if(m_enabled && !m_sampleBufferFilled && m_bytesRemaining > 0) {
                readSample(false);
            }
        }
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

            // A $4015 enable write delays DMC activation by 2/3 CPU cycles.
            if(m_enabled && m_bytesRemaining > 0 && m_enableReloadDelay == 0) {
                readSample(true);
            }
            
        }
    }

    int getBytesRemaining()
    {
        return m_bytesRemaining;
    }

    void loadSampleBuffer(uint8_t data) {
        const uint16_t bytesRemainingBeforeLoad = m_bytesRemaining;

        m_sampleBufferFilled = true;
        m_dmaRequestCooldown = 4;
        m_lastLoadedByteCameFromNonLoopingDma = m_pendingDmaLoopDisabled;
        m_pendingDmaLoopDisabled = false;

        m_currentAddr = (m_currentAddr + 1) & 0x7FFF;

        updateDeltaCounter(data);

        m_audioGenerator.addSample( (m_deltaCounter-0.5*127.0)/127.0 );

        if(m_bytesRemaining > 0) --m_bytesRemaining;

        const bool sampleEnded = m_bytesRemaining == 0;

        const bool implicitAbortSpecialCase =
            sampleEnded &&
            bytesRemainingBeforeLoad == 1 &&
            m_sampleLength == 1 &&
            m_shiftCounter == 1 &&
            m_lastLoadedByteCameFromNonLoopingDma;
        if(sampleEnded) {
            if(m_enabled) {
                switch(m_playMode)
                {
                case 0x00:
                    break;
                case 0x40:
                case 0xC0:
                    if(!implicitAbortSpecialCase) {
                        reload(true);
                    }
                    break;
                case 0x80:
                    m_interruptFlag = true;
                    break;
                }
            }
        }

        if(implicitAbortSpecialCase)
        {
            m_currentAddr = m_sampleAddr;
            m_bytesRemaining = m_sampleLength;
            dmcImplicitAbortRequest();
        }


    }

};
