#pragma once

#include "GeraNES/defines.h"
#include "GeraNES/Serialization.h"
#include "GeraNES/Settings.h"

#include "GeraNES/IAudioOutput.h"

#include "PulseChannel.h"
#include "TriangleChannel.h"
#include "NoiseChannel.h"
#include "SampleChannel.h"

#include <string>

class APU
{
    IAudioOutput& m_audioOutput;
    Settings& m_settings;

    bool m_mode;
    bool m_interruptInhibitFlag;

    int m_frameStep;

    bool m_frameInterruptFlag;
    uint8_t m_frameInterruptClearDelay;

    bool m_jitter;

    const int mode0DelaysNtsc[6] = {7457, 7456, 7458, 7457, 1, 1 };
    const int mode1DelaysNtsc[5] = {7457, 7456, 7458, 7457, 7453};
    const int mode0DelaysPal[6] = {8313, 8314, 8312, 8313, 1, 1};
    const int mode1DelaysPal[5] = {8313, 8314, 8312, 8314, 8312};
    int m_nextDelay;

    PulseChannel m_pulse1;
    PulseChannel m_pulse2;
    TriangleChannel m_triangle;
    NoiseChannel m_noise;
    SampleChannel m_sample;

    bool m_writeChannelsFlag;
    int m_writeChannelsAddr;
    uint8_t m_writeChannelsData;
    uint8_t m_last4017Value;

    // Audio device parameters only need to be pushed when the generated
    // channel state actually changes.
    bool m_audioOutputStateDirty = true;
    int m_cachedExpansionSourceRateHz = 0;
    int m_cachedPulse1Duty = -1;
    int m_cachedPulse2Duty = -1;
    int m_cachedPulse1Period = -1;
    int m_cachedPulse2Period = -1;
    int m_cachedTrianglePeriod = -1;
    int m_cachedNoisePeriod = -1;
    int m_cachedPulse1Volume = -1;
    int m_cachedPulse2Volume = -1;
    int m_cachedTriangleVolume = -1;
    int m_cachedNoiseVolume = -1;
    int m_cachedNoiseMode = -1;

public:


    GERANES_INLINE const int* mode0Delays() const
    {
        return m_settings.region() == Settings::Region::PAL ? mode0DelaysPal : mode0DelaysNtsc;
    }

    GERANES_INLINE const int* mode1Delays() const
    {
        return m_settings.region() == Settings::Region::PAL ? mode1DelaysPal : mode1DelaysNtsc;
    }

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_mode);
        SERIALIZEDATA(s, m_interruptInhibitFlag);

        SERIALIZEDATA(s, m_frameStep);
        SERIALIZEDATA(s, m_frameInterruptFlag);
        SERIALIZEDATA(s, m_frameInterruptClearDelay);

        SERIALIZEDATA(s, m_jitter);
        SERIALIZEDATA(s, m_nextDelay);

        m_pulse1.serialization(s);
        m_pulse2.serialization(s);
        m_triangle.serialization(s);
        m_noise.serialization(s);
        m_sample.serialization(s);

        SERIALIZEDATA(s, m_writeChannelsFlag);
        SERIALIZEDATA(s, m_writeChannelsAddr);
        SERIALIZEDATA(s, m_writeChannelsData);
        SERIALIZEDATA(s, m_last4017Value);
    }

    APU(IAudioOutput& audioOutput, Settings& settings) :
        m_audioOutput(audioOutput),
        m_settings(settings), m_noise(settings),
        m_sample(settings, audioOutput)
    {

    }

    SampleChannel& getSampleChannel() {
        return m_sample;
    }

    void processDmcControlDelays()
    {
        m_sample.processControlDelays();
    }

    const std::string init()
    {
        m_audioOutput.init();

        powerOnReset();

        return "";
    }

private:
    void invalidateAudioOutputState()
    {
        m_audioOutputStateDirty = true;
        m_cachedExpansionSourceRateHz = 0;
        m_cachedPulse1Duty = -1;
        m_cachedPulse2Duty = -1;
        m_cachedPulse1Period = -1;
        m_cachedPulse2Period = -1;
        m_cachedTrianglePeriod = -1;
        m_cachedNoisePeriod = -1;
        m_cachedPulse1Volume = -1;
        m_cachedPulse2Volume = -1;
        m_cachedTriangleVolume = -1;
        m_cachedNoiseVolume = -1;
        m_cachedNoiseMode = -1;
    }

    void powerOnReset()
    {
        m_pulse1.init();
        m_pulse2.init();
        m_triangle.init();
        m_noise.init();
        m_sample.init();

        m_mode = false;
        m_interruptInhibitFlag = true;

        m_frameStep = 0;
        m_nextDelay = mode0Delays()[0];
        m_jitter = false;

        m_frameInterruptFlag = false;
        m_frameInterruptClearDelay = 0;
        m_writeChannelsFlag = false;
        m_writeChannelsAddr = 0;
        m_writeChannelsData = 0;
        m_last4017Value = 0x00;
        invalidateAudioOutputState();

        //After reset or power-up, APU acts as if $4017 were written with $00 from
        //9 to 12 clocks before first instruction begins.
        write(0x0017, m_last4017Value);
        for(int i = 0; i < 6; i++ ) cycle();

        m_audioOutput.clearAudioBuffers();
        // Mapper expansion audio is fed at CPU-cycle rate.
        m_audioOutput.setExpansionSourceRateHz(m_settings.CPUClockHz());
        m_audioOutput.setExpansionAudioVolume(1.0f);
    }

public:
    void reset()
    {
        m_pulse1.setEnabled(false);
        m_pulse2.setEnabled(false);
        m_triangle.setEnabled(false);
        m_noise.setEnabled(false);
        m_sample.reset();

        m_frameStep = 0;
        m_nextDelay = mode0Delays()[0];
        m_jitter = false;

        m_frameInterruptFlag = false;
        m_frameInterruptClearDelay = 0;
        m_writeChannelsFlag = false;
        m_writeChannelsAddr = 0;
        m_writeChannelsData = 0;
        invalidateAudioOutputState();

        // On reset, the frame counter behaves as if the last mode value were
        // written again shortly before execution resumes. The IRQ inhibit bit
        // is not reliably preserved by hardware, so only the mode bit is
        // restored here.
        write(0x0017, m_last4017Value & 0x80);
        for(int i = 0; i < 3; i++ ) cycle();

        m_audioOutput.clearAudioBuffers();
        m_audioOutput.setExpansionSourceRateHz(m_settings.CPUClockHz());
        m_audioOutput.setExpansionAudioVolume(1.0f);
    }

    GERANES_INLINE void processExpansionAudioSample(float currentSample, float mixWeight)
    {
        m_audioOutput.processExpansionAudioSample(currentSample, mixWeight);
    }

    GERANES_INLINE_HOT bool getInterruptFlag()
    {
        return ((!m_interruptInhibitFlag) && m_frameInterruptFlag) || m_sample.getInterruptFlag();
    }

    uint8_t getActiveChannelMask()
    {
        uint8_t mask = 0;
        if(m_pulse1.getVolume() > 0) mask |= 0x01;
        if(m_pulse2.getVolume() > 0) mask |= 0x02;
        if(m_triangle.getVolume() > 0) mask |= 0x04;
        if(m_noise.getVolume() > 0) mask |= 0x08;
        if(m_sample.getBytesRemaining() > 0) mask |= 0x10;
        return mask;
    }

    uint8_t read(int addr, bool cpuOddCycle = false)
    {
        uint8_t ret = 0;

        if(addr == 0x15)
        {
            if(m_pulse1.getLengthCounter() > 0) ret |= 0x01;
            if(m_pulse2.getLengthCounter() > 0) ret |= 0x02;
            if(m_triangle.getLengthCounter() > 0) ret |= 0x04;
            if(m_noise.getLengthCounter() > 0) ret |= 0x08;
            if(m_sample.getBytesRemaining() > 0) ret |= 0x10;

            //Reading this register clears the frame interrupt flag (but not the DMC interrupt flag).
            if(m_frameInterruptFlag) ret |= 0x40;
            if(m_sample.getInterruptFlag()) ret |= 0x80;

            if(m_frameInterruptFlag) {
                // AccuracyCoin test 6/7: the flag clears on the next APU "get" cycle,
                // not immediately on the CPU read that observed it.
                m_frameInterruptClearDelay = cpuOddCycle ? 2 : 1;
            }
        }

        return ret;
    }    

    void write(int addr, uint8_t data, bool cpuOddCycle = false)
    {
        if(addr == 0x15) {
            writeChannels(addr, data, cpuOddCycle);
            return;
        }

        if(addr < 0x17) {
            m_writeChannelsFlag = true;
            m_writeChannelsAddr = addr;
            m_writeChannelsData = data;
            return;
        }

        switch(addr) {

        case 0x17:
            m_last4017Value = data & 0xC0;
            m_mode = data&0x80;
            m_interruptInhibitFlag = data&0x40;
            m_frameStep = 0;

            if(!m_mode) m_nextDelay = mode0Delays()[m_frameStep];
            else m_nextDelay = mode1Delays()[m_frameStep];

            m_nextDelay += (m_jitter ? 3 : 4);

            if(m_mode) {
                // Immediately run all units / do not update audio output here
                updateEnvelopsAndLinearCounters();
                updateLengthCountersAndSweeps();
            }

            if(m_interruptInhibitFlag) {
                m_frameInterruptFlag = false;
                m_frameInterruptClearDelay = 0;
            }
            break;

        }

    }

    void writeChannels(int addr, uint8_t data, bool cpuOddCycle = false) {

        switch(addr) {
            case 0x00:
            case 0x01:
            case 0x02:
            case 0x03:
                m_pulse1.write(addr, data);
                break;
            case 0x04:
            case 0x05:
            case 0x06:
            case 0x07:
                m_pulse2.write(addr & 0x03, data);
                break;
            case 0x08:
            case 0x09:
            case 0x0A:
            case 0x0B:
                m_triangle.write(addr & 0x03, data);
                break;
            case 0x0C:
            case 0x0D:
            case 0x0E:
            case 0x0F:
                m_noise.write(addr & 0x03, data);
                break;
            case 0x10:
            case 0x11:
            case 0x12:
            case 0x13:
                m_sample.write(addr & 0x03, data);
                break;
            case 0x15:
                m_pulse1.setEnabled(data&0x01);
                m_pulse2.setEnabled(data&0x02);
                m_triangle.setEnabled(data&0x04);
                m_noise.setEnabled(data&0x08);
                m_sample.setEnabled(data&0x10, cpuOddCycle);
                break;
        }

    }

    void updateEnvelopsAndLinearCounters()
    {
        m_pulse1.updateEnvelop();
        m_pulse2.updateEnvelop();
        m_triangle.updateLinearCounter();
        m_noise.updateEnvelop();
    }

    void updateLengthCountersAndSweeps()
    {
        m_pulse1.updateSweep(0);
        m_pulse1.updateLengthCounter();

        m_pulse2.updateSweep(1);
        m_pulse2.updateLengthCounter();

        m_triangle.updateLengthCounter();

        m_noise.updateLengthCounter();
    }

    void updateAudioOutput()
    {
        const int expansionSourceRateHz = m_settings.CPUClockHz();
        if(m_audioOutputStateDirty || m_cachedExpansionSourceRateHz != expansionSourceRateHz) {
            m_audioOutput.setExpansionSourceRateHz(expansionSourceRateHz);
            m_audioOutput.setExpansionAudioVolume(1.0f);
            m_cachedExpansionSourceRateHz = expansionSourceRateHz;
        }

        const int pulse1Duty = m_pulse1.getDuty();
        if(m_audioOutputStateDirty || m_cachedPulse1Duty != pulse1Duty) {
            switch(pulse1Duty)
            {
            case 0: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_1, 0.125f); break;
            case 1: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_1, 0.25f); break;
            case 2: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_1, 0.5f); break;
            case 3: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_1, 0.75f); break;
            }
            m_cachedPulse1Duty = pulse1Duty;
        }

        const int pulse2Duty = m_pulse2.getDuty();
        if(m_audioOutputStateDirty || m_cachedPulse2Duty != pulse2Duty) {
            switch(pulse2Duty)
            {
            case 0: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_2, 0.125f); break;
            case 1: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_2, 0.25f); break;
            case 2: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_2, 0.5f); break;
            case 3: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_2, 0.75f); break;
            }
            m_cachedPulse2Duty = pulse2Duty;
        }

        const float cpuClock = static_cast<float>(expansionSourceRateHz);
        const int pulse1Period = m_pulse1.getPeriod();
        if(m_audioOutputStateDirty || m_cachedPulse1Period != pulse1Period) {
            m_audioOutput.setChannelFrequency(IAudioOutput::Channel::Pulse_1, cpuClock / 16.0f / (pulse1Period + 1));
            m_cachedPulse1Period = pulse1Period;
        }

        const int pulse1Volume = m_pulse1.getVolume();
        if(m_audioOutputStateDirty || m_cachedPulse1Volume != pulse1Volume) {
            m_audioOutput.setChannelVolume(IAudioOutput::Channel::Pulse_1, static_cast<float>(pulse1Volume) / 15.0f);
            m_cachedPulse1Volume = pulse1Volume;
        }

        const int pulse2Period = m_pulse2.getPeriod();
        if(m_audioOutputStateDirty || m_cachedPulse2Period != pulse2Period) {
            m_audioOutput.setChannelFrequency(IAudioOutput::Channel::Pulse_2, cpuClock / 16.0f / (pulse2Period + 1));
            m_cachedPulse2Period = pulse2Period;
        }

        const int pulse2Volume = m_pulse2.getVolume();
        if(m_audioOutputStateDirty || m_cachedPulse2Volume != pulse2Volume) {
            m_audioOutput.setChannelVolume(IAudioOutput::Channel::Pulse_2, static_cast<float>(pulse2Volume) / 15.0f);
            m_cachedPulse2Volume = pulse2Volume;
        }

        const int trianglePeriod = m_triangle.getPeriod();
        if(m_audioOutputStateDirty || m_cachedTrianglePeriod != trianglePeriod) {
            m_audioOutput.setChannelFrequency(IAudioOutput::Channel::Triangle, (cpuClock / 16.0f / 2.0f) / (trianglePeriod + 1));
            m_cachedTrianglePeriod = trianglePeriod;
        }

        const int triangleVolume = m_triangle.getVolume();
        if(m_audioOutputStateDirty || m_cachedTriangleVolume != triangleVolume) {
            m_audioOutput.setChannelVolume(IAudioOutput::Channel::Triangle, static_cast<float>(triangleVolume) / 15.0f);
            m_cachedTriangleVolume = triangleVolume;
        }

        const int noisePeriod = m_noise.getPeriod();
        if(m_audioOutputStateDirty || m_cachedNoisePeriod != noisePeriod) {
            m_audioOutput.setChannelFrequency(IAudioOutput::Channel::Noise, cpuClock / noisePeriod);
            m_cachedNoisePeriod = noisePeriod;
        }

        const int noiseVolume = m_noise.getVolume();
        if(m_audioOutputStateDirty || m_cachedNoiseVolume != noiseVolume) {
            m_audioOutput.setChannelVolume(IAudioOutput::Channel::Noise, static_cast<float>(noiseVolume) / 15.0f);
            m_cachedNoiseVolume = noiseVolume;
        }

        const int noiseMode = m_noise.getMode() ? 1 : 0;
        if(m_audioOutputStateDirty || m_cachedNoiseMode != noiseMode) {
            m_audioOutput.setNoiseMetallic(m_noise.getMode());
            m_cachedNoiseMode = noiseMode;
        }

        m_audioOutputStateDirty = false;
    }



    void cycle()
    {
        if(m_frameInterruptClearDelay > 0 && --m_frameInterruptClearDelay == 0) {
            m_frameInterruptFlag = false;
        }

        if(m_mode == 0) {

            if(--m_nextDelay == 0) {

                switch(m_frameStep) {
                case 0:
                    updateEnvelopsAndLinearCounters();
                    updateAudioOutput();
                    m_frameStep++;
                    break;
                case 1:
                    updateEnvelopsAndLinearCounters();
                    updateLengthCountersAndSweeps();
                    updateAudioOutput();
                    m_frameStep++;
                    break;
                case 2:
                    updateEnvelopsAndLinearCounters();
                    updateAudioOutput();
                    m_frameStep++;
                    break;
                case 3:
                    m_frameInterruptFlag = true;
                    m_frameStep++;
                    break;
                case 4:
                    updateEnvelopsAndLinearCounters();
                    updateLengthCountersAndSweeps();
                    updateAudioOutput();
                    m_frameInterruptFlag = true;
                    m_frameStep++;
                    break;
                case 5:
                    m_frameInterruptFlag = !m_interruptInhibitFlag;
                    m_frameStep=0;
                    break;
                }

                m_nextDelay = mode0Delays()[m_frameStep];

            }


        }
        else {

            if(--m_nextDelay == 0) {

                switch(m_frameStep) {
                case 0:
                    updateEnvelopsAndLinearCounters();
                    updateAudioOutput();
                    m_frameStep++;
                    m_nextDelay = mode1Delays()[m_frameStep];
                    break;
                case 1:
                    updateEnvelopsAndLinearCounters();
                    updateLengthCountersAndSweeps();
                    updateAudioOutput();
                    m_frameStep++;
                    m_nextDelay = mode1Delays()[m_frameStep];
                    break;
                case 2:
                    updateEnvelopsAndLinearCounters();
                    updateAudioOutput();
                    m_frameStep++;
                    m_nextDelay = mode1Delays()[m_frameStep];
                    break;
                case 3:
                    //do nothing
                    m_frameStep++;
                    m_nextDelay = mode1Delays()[m_frameStep];
                    break;
                case 4:
                    updateEnvelopsAndLinearCounters();
                    updateLengthCountersAndSweeps();
                    updateAudioOutput();
                    m_frameStep = 0;
                    m_nextDelay = mode1Delays()[m_frameStep] + (m_jitter ? 0 : 1);
                    break;
                }
            }
        }        

        if(m_writeChannelsFlag) {
            m_writeChannelsFlag = false;
            writeChannels(m_writeChannelsAddr, m_writeChannelsData);            
        }

        m_pulse1.cycle();
        m_pulse2.cycle();
      
        m_noise.cycle();
        m_sample.cycle();

        m_jitter = !m_jitter; 
    }

};
