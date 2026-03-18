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

    GERANES_INLINE void processExpansionAudioSample(float currentSample)
    {
        m_audioOutput.processExpansionAudioSample(currentSample);
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

            case 0x00 ... 0x03: m_pulse1.write(addr,data); break;
            case 0x04 ... 0x07: m_pulse2.write(addr&0x03,data); break;
            case 0x08 ... 0x0B: m_triangle.write(addr&0x03,data); break;
            case 0x0C ... 0x0F: m_noise.write(addr&0x03,data); break;
            case 0x10 ... 0x13: m_sample.write(addr&0x03,data); break;
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
        m_audioOutput.setExpansionSourceRateHz(m_settings.CPUClockHz());
        m_audioOutput.setExpansionAudioVolume(1.0f);

        switch(m_pulse1.getDuty())
        {
        case 0: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_1, 0.125f); break;
        case 1: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_1, 0.25f); break;
        case 2: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_1, 0.5f); break;
        case 3: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_1, 0.75f); break;
        }        

        const float CPUClock = m_settings.CPUClockHz();

        //f = CPU / (16 * (t + 1))
        m_audioOutput.setChannelFrequency(IAudioOutput::Channel::Pulse_1, CPUClock/16.0f/(m_pulse1.getPeriod()+1) );
        m_audioOutput.setChannelVolume(IAudioOutput::Channel::Pulse_1, (float)m_pulse1.getVolume()/15.0f );

        switch(m_pulse2.getDuty())
        {
        case 0: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_2, 0.125f); break;
        case 1: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_2, 0.25f); break;
        case 2: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_2, 0.5f); break;
        case 3: m_audioOutput.setPulseDutyCycle(IAudioOutput::PulseChannel::Pulse_2, 0.75f); break;
        }

        m_audioOutput.setChannelFrequency(IAudioOutput::Channel::Pulse_2, CPUClock/16.0f/(m_pulse2.getPeriod()+1) );
        m_audioOutput.setChannelVolume(IAudioOutput::Channel::Pulse_2, (float)m_pulse2.getVolume()/15.0f );

        m_audioOutput.setChannelFrequency(IAudioOutput::Channel::Triangle, (CPUClock/16.0f/2.0f)/(m_triangle.getPeriod()+1) );
        m_audioOutput.setChannelVolume(IAudioOutput::Channel::Triangle, (float)m_triangle.getVolume()/15.0f );

        m_audioOutput.setChannelFrequency(IAudioOutput::Channel::Noise, CPUClock/(m_noise.getPeriod()) );
        m_audioOutput.setChannelVolume(IAudioOutput::Channel::Noise, (float)m_noise.getVolume()/15.0f );
        m_audioOutput.setNoiseMetallic( m_noise.getMode() );
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
