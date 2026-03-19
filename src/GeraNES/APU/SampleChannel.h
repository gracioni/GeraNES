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
        398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118, 98, 78, 66, 50
    };

    Settings& m_settings;
    IAudioOutput& m_audioGenerator;

    uint8_t m_playMode = 0;
    uint16_t m_currentAddr = 0;
    int m_deltaCounter = 64;
    uint16_t m_sampleAddr = 0;
    uint16_t m_sampleLength = 0;
    uint16_t m_bytesRemaining = 0;
    size_t m_periodIndex = 0;
    int m_periodCounter = 0;
    int m_bitsRemaining = 1;
    uint8_t m_shiftRegister = 0;
    uint8_t m_readBuffer = 0;
    bool m_readBufferFilled = false;
    bool m_silenceFlag = true;

    bool m_interruptFlag = false;

    uint32_t m_cpuCycleCounter = 0;
    bool m_directControlFlag = false;

    bool m_enabled = false;
    int m_enableReloadDelay = 0;
    int m_disableDelay = 0;

    float m_sample = m_deltaCounter;

    GERANES_INLINE bool loopEnabled() const
    {
        return (m_playMode & 0x40) != 0;
    }

    GERANES_INLINE bool irqEnabled() const
    {
        return (m_playMode & 0x80) != 0;
    }

    GERANES_INLINE uint16_t getPeriod()
    {
        if(m_settings.region() == Settings::Region::PAL) {
            return PAL_DMC_PERIOD_TABLE[m_periodIndex];
        }
        return NTSC_DMC_PERIOD_TABLE[m_periodIndex];
    }

    GERANES_INLINE float normalizedOutput() const
    {
        return (m_sample - 0.5f * 127.0f) / 127.0f;
    }

    void requestSample(bool reload)
    {
        if(m_bytesRemaining == 0) {
            return;
        }

        dmcRequest(m_currentAddr | 0x8000, reload);
    }

    void restartSample(bool reload)
    {
        m_currentAddr = m_sampleAddr;
        m_bytesRemaining = m_sampleLength;

        if(m_enabled && !m_readBufferFilled && m_enableReloadDelay == 0 && m_bytesRemaining > 0) {
            requestSample(reload);
        }
    }

    void clockOutputUnit()
    {
        if(!m_silenceFlag) {
            if((m_shiftRegister & 0x01) != 0) {
                if(m_deltaCounter <= 125) {
                    m_deltaCounter += 2;
                }
            } else if(m_deltaCounter >= 2) {
                m_deltaCounter -= 2;
            }
        }

        m_shiftRegister >>= 1;

        if(--m_bitsRemaining != 0) {
            return;
        }

        m_bitsRemaining = 8;

        if(m_readBufferFilled) {
            m_shiftRegister = m_readBuffer;
            m_readBufferFilled = false;
            m_silenceFlag = false;

            if(m_enabled && m_enableReloadDelay == 0 && m_bytesRemaining > 0) {
                requestSample(true);
            }
        } else {
            m_silenceFlag = true;
        }
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
        SERIALIZEDATA(s, m_bitsRemaining);
        SERIALIZEDATA(s, m_shiftRegister);
        SERIALIZEDATA(s, m_readBuffer);
        SERIALIZEDATA(s, m_readBufferFilled);
        SERIALIZEDATA(s, m_silenceFlag);
        SERIALIZEDATA(s, m_interruptFlag);
        SERIALIZEDATA(s, m_cpuCycleCounter);
        SERIALIZEDATA(s, m_directControlFlag);
        SERIALIZEDATA(s, m_enabled);
        SERIALIZEDATA(s, m_enableReloadDelay);
        SERIALIZEDATA(s, m_disableDelay);
        SERIALIZEDATA(s, m_sample);
    }

    SampleChannel(Settings& settings, IAudioOutput& audioOutput)
        : m_settings(settings), m_audioGenerator(audioOutput)
    {
        init();
    }

    void init()
    {
        m_playMode = 0x00;
        m_currentAddr = 0;
        m_deltaCounter = 64;
        m_sampleAddr = 0;
        m_sampleLength = 0;
        m_bytesRemaining = 0;
        m_periodIndex = 0;
        m_periodCounter = getPeriod();
        m_bitsRemaining = 1;
        m_shiftRegister = 0;
        m_readBuffer = 0;
        m_readBufferFilled = false;
        m_silenceFlag = true;
        m_interruptFlag = false;
        m_cpuCycleCounter = 0;
        m_directControlFlag = false;
        m_enabled = false;
        m_enableReloadDelay = 0;
        m_disableDelay = 0;
    }

    void reset()
    {
        m_currentAddr = m_sampleAddr;
        m_bytesRemaining = 0;
        m_periodCounter = getPeriod();
        m_bitsRemaining = 1;
        m_shiftRegister = 0;
        m_readBuffer = 0;
        m_readBufferFilled = false;
        m_silenceFlag = true;
        m_interruptFlag = false;
        m_cpuCycleCounter = 0;
        m_directControlFlag = false;
        m_enabled = false;
        m_enableReloadDelay = 0;
        m_disableDelay = 0;
    }

    void write(int addr, uint8_t data)
    {
        switch(addr)
        {
        case 0x0000:
            m_playMode = data & 0xC0;
            m_periodIndex = data & 0x0F;
            m_audioGenerator.setChannelFrequency(
                IAudioOutput::Channel::Sample,
                2 * (m_settings.CPUClockHz() / 16.0f) / (getPeriod() + 1)
            );
            m_audioGenerator.setChannelVolume(IAudioOutput::Channel::Sample, 1.0f);

            if((data & 0x80) == 0x00) {
                m_interruptFlag = false;
            }
            break;

        case 0x0001:
            m_deltaCounter = data & 0x7F;

            if(m_directControlFlag) {
                float cpuClock = m_settings.CPUClockHz();
                float period = (m_cpuCycleCounter + 1) / (8 * 2 * (cpuClock / 16.0f));
                
                m_sample = m_deltaCounter;
                m_audioGenerator.addSampleDirect(period, normalizedOutput());
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
        restartSample(reload);
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
        if(status) {
            const bool disableWasPending = m_disableDelay > 0;
            const bool wasEnabled = m_enabled;

            m_enabled = true;
            m_disableDelay = 0;

            const bool hadNoBytesRemaining = m_bytesRemaining == 0;
            if(hadNoBytesRemaining) {
                m_currentAddr = m_sampleAddr;
                m_bytesRemaining = m_sampleLength;
            }

            if(!wasEnabled || hadNoBytesRemaining || disableWasPending) {
                m_enableReloadDelay = cpuOddCycle ? 3 : 2;
            }
        } else {
            m_enableReloadDelay = 0;
            if(m_disableDelay == 0) {
                m_disableDelay = cpuOddCycle ? 3 : 2;
            }
        }

        m_interruptFlag = false;
    }

    void processControlDelays()
    {
        if(m_disableDelay > 0 && --m_disableDelay == 0) {
            m_enabled = false;
            m_bytesRemaining = 0;
            dmcCancelRequest();
        }

        if(m_enableReloadDelay > 0 && --m_enableReloadDelay == 0) {
            if(m_enabled && !m_readBufferFilled && m_bytesRemaining > 0) {
                requestSample(false);
            }
        }
    }

    void cycle()
    {
        if(m_cpuCycleCounter < NTSC_DMC_PERIOD_TABLE[0]) {
            m_cpuCycleCounter++;
        } else {
            m_directControlFlag = false;
        }

        if(--m_periodCounter == 0) {
            m_periodCounter = getPeriod();
            clockOutputUnit();
        }
    }

    int getBytesRemaining()
    {
        return m_bytesRemaining;
    }

    void loadSampleBuffer(uint8_t data)
    {
        updateSample(data);
        m_audioGenerator.addSample(normalizedOutput());

        const uint16_t bytesRemainingBeforeLoad = m_bytesRemaining;

        m_readBuffer = data;
        m_readBufferFilled = true;
        m_currentAddr = (m_currentAddr + 1) & 0x7FFF;

        if(m_bytesRemaining > 0) {
            --m_bytesRemaining;
        }

        const bool sampleEnded = m_bytesRemaining == 0;
        const bool lateOneByteNonLoopingLoad =
            sampleEnded &&
            bytesRemainingBeforeLoad == 1 &&
            m_sampleLength == 1 &&
            !loopEnabled() &&
            m_bitsRemaining == 1 &&
            m_periodCounter == 2;

        if(sampleEnded) {
            if(loopEnabled()) {
                restartSample(true);
            } else if(irqEnabled()) {
                m_interruptFlag = true;
            }
        }

        if(lateOneByteNonLoopingLoad) {
            dmcImplicitAbortRequest();
        }
    }

    void updateSample(uint8_t data) {

        uint8_t shift =  data;

        for(int c = 0; c < 8; ++c)
        {
            if (shift & 1)
            {
                m_sample += 2*8;
                if (m_sample > 127) m_sample = 127;
            }
            else {
                m_sample -= 2*8;
                if(m_sample < 0) m_sample = 0;
            }

            shift >>= 1;
        }
    }
};
