#ifndef APU_H
#define APU_H

#include "GeraNES/defines.h"
#include "GeraNES/Serialization.h"
#include "GeraNES/Settings.h"
#include "GeraNES/CPU2A03.h"

#include "GeraNES/IAudioOutput.h"

#include "SquareChannel.h"
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

    bool m_jitter;

    const int mode0Delays[6] = {7457, 7456, 7458, 7457, 1, 1 };
    const int mode1Delays[5] = {7457, 7456, 7458, 7457, 7453};
    int m_nextDelay;

    SquareChannel m_square1;
    SquareChannel m_square2;
    TriangleChannel m_triangle;
    NoiseChannel m_noise;
    SampleChannel m_sample;

    bool m_writeChannelsFlag;
    int m_writeChannelsAddr;
    uint8_t m_writeChannelsData;

public:   

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_mode);
        SERIALIZEDATA(s, m_interruptInhibitFlag);

        SERIALIZEDATA(s, m_frameStep);
        SERIALIZEDATA(s, m_frameInterruptFlag);

        SERIALIZEDATA(s, m_jitter);
        SERIALIZEDATA(s, m_nextDelay);

        m_square1.serialization(s);
        m_square2.serialization(s);
        m_triangle.serialization(s);
        m_noise.serialization(s);
        m_sample.serialization(s);

        SERIALIZEDATA(s, m_writeChannelsFlag);
        SERIALIZEDATA(s, m_writeChannelsAddr);
        SERIALIZEDATA(s, m_writeChannelsData);
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

    const std::string init()
    {


        m_audioOutput.init();

        m_square1.init();
        m_square2.init();
        m_triangle.init();
        m_noise.init();
        m_sample.init();

        m_mode = false;
        m_interruptInhibitFlag = true;

        m_frameStep = 0;
        m_nextDelay = mode0Delays[0];
        m_jitter = false;

        m_frameInterruptFlag = false;

        //After reset or power-up, APU acts as if $4017 were written with $00 from
        //9 to 12 clocks before first instruction begins.
        write(0x0017, 0x00);
        for(int i = 0; i < 12; i++ ) cycle();

        return "";
    }

    GERANES_INLINE_HOT bool getInterruptFlag()
    {
        return m_frameInterruptFlag || m_sample.getInterruptFlag();
    }

    uint8_t read(int addr)
    {
        uint8_t ret = 0;

        if(addr == 0x15)
        {
            if(m_square1.getLengthCounter() > 0) ret |= 0x01;
            if(m_square2.getLengthCounter() > 0) ret |= 0x02;
            if(m_triangle.getLengthCounter() > 0) ret |= 0x04;
            if(m_noise.getLengthCounter() > 0) ret |= 0x08;
            if(m_sample.getBytesRemaining() > 0) ret |= 0x10;

            //Reading this register clears the frame interrupt flag (but not the DMC interrupt flag).
            if(m_frameInterruptFlag) ret |= 0x40;
            if(m_sample.getInterruptFlag()) ret |= 0x80;

            m_frameInterruptFlag = false;
        }

        return ret;
    }    

    void write(int addr, uint8_t data)
    {
        if(addr < 0x17) {
            m_writeChannelsFlag = true;
            m_writeChannelsAddr = addr;
            m_writeChannelsData = data;
            return;
        }

        switch(addr) {
            /*
        case 0x00 ... 0x03: m_square1.write(addr,data); break;
        case 0x04 ... 0x07: m_square2.write(addr&0x03,data); break;
        case 0x08 ... 0x0B: m_triangle.write(addr&0x03,data); break;
        case 0x0C ... 0x0F: m_noise.write(addr&0x03,data); break;
        case 0x10 ... 0x13: m_sample.write(addr&0x03,data); break;
        case 0x15:
            m_square1.setEnabled(data&0x01);
            m_square2.setEnabled(data&0x02);
            m_triangle.setEnabled(data&0x04);
            m_noise.setEnabled(data&0x08);
            m_sample.setEnabled(data&0x10);
            break;
            */
        case 0x17:
            m_mode = data&0x80;
            m_interruptInhibitFlag = data&0x40;
            m_frameStep = 0;

            if(!m_mode) m_nextDelay = mode0Delays[m_frameStep];
            else m_nextDelay = mode1Delays[m_frameStep];

            m_nextDelay += (m_jitter ? 3 : 4);

            if(m_mode) {
                // Immediately run all units / do not update audio output here
                updateEnvelopsAndLinearCounters();
                updateLengthCountersAndSweeps();
            }

            if(m_interruptInhibitFlag) m_frameInterruptFlag = false;
            break;

        }

    }

    void writeChannels(int addr, uint8_t data) {

        switch(addr) {

            case 0x00 ... 0x03: m_square1.write(addr,data); break;
            case 0x04 ... 0x07: m_square2.write(addr&0x03,data); break;
            case 0x08 ... 0x0B: m_triangle.write(addr&0x03,data); break;
            case 0x0C ... 0x0F: m_noise.write(addr&0x03,data); break;
            case 0x10 ... 0x13: m_sample.write(addr&0x03,data); break;
            case 0x15:
                m_square1.setEnabled(data&0x01);
                m_square2.setEnabled(data&0x02);
                m_triangle.setEnabled(data&0x04);
                m_noise.setEnabled(data&0x08);
                m_sample.setEnabled(data&0x10);
                break;
        }

    }

    void updateEnvelopsAndLinearCounters()
    {
        m_square1.updateEnvelop();
        m_square2.updateEnvelop();
        m_triangle.updateLinearCounter();
        m_noise.updateEnvelop();
    }

    void updateLengthCountersAndSweeps()
    {
        m_square1.updateSweep(0);
        m_square1.updateLengthCounter();

        m_square2.updateSweep(1);
        m_square2.updateLengthCounter();

        m_triangle.updateLengthCounter();

        m_noise.updateLengthCounter();
    }

    void updateAudioOutput()
    {
        switch(m_square1.getDuty())
        {
        case 0: m_audioOutput.setSquare1DutyCycle(0.125); break;
        case 1: m_audioOutput.setSquare1DutyCycle(0.25); break;
        case 2: m_audioOutput.setSquare1DutyCycle(0.5); break;
        case 3: m_audioOutput.setSquare1DutyCycle(0.75); break;
        }

        //f = CPU / (16 * (t + 1))

        const float CPUClock = m_settings.CPUClockHz();

        m_audioOutput.setSquare1Frequency( CPUClock/16.0/(m_square1.getPeriod()+1) );

        //m_audioOutput.setSquare1Volume( (float)m_square1.getVolume()/15.0 );
        m_audioOutput.setSquare1Volume( 0);

        switch(m_square2.getDuty())
        {
        case 0: m_audioOutput.setSquare2DutyCycle(0.125); break;
        case 1: m_audioOutput.setSquare2DutyCycle(0.25); break;
        case 2: m_audioOutput.setSquare2DutyCycle(0.5); break;
        case 3: m_audioOutput.setSquare2DutyCycle(0.75); break;
        }

        m_audioOutput.setSquare2Frequency( CPUClock/16.0/(m_square2.getPeriod()+1) );
         //m_audioOutput.setSquare2Volume( (float)m_square2.getVolume()/15.0 );
        m_audioOutput.setSquare2Volume( 0 );

        m_audioOutput.setTriangleFrequency( (CPUClock/16.0/2.0)/(m_triangle.getPeriod()+1) );
         m_audioOutput.setTriangleVolume( (float)m_triangle.getVolume()/15.0 );
        //m_audioOutput.setTriangleVolume( 0 );


    //     const uint16_t NTSC_NOISE_PERIOD_TABLE[16] = {
    //     4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
    // };

    // 1789773 Hz


        m_audioOutput.setNoiseFrequency( CPUClock/(m_noise.getPeriod()) );
        //m_audioOutput.setNoiseVolume( (float)m_noise.getVolume()/15.0 );
        m_audioOutput.setNoiseVolume( 0);
        m_audioOutput.setNoiseMetallic( m_noise.getMode() );
    }



    void cycle()
    {
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
                    if(!m_interruptInhibitFlag) m_frameInterruptFlag = true;
                    m_frameStep++;
                    break;
                case 4:
                    updateEnvelopsAndLinearCounters();
                    updateLengthCountersAndSweeps();
                    updateAudioOutput();
                    if(!m_interruptInhibitFlag) m_frameInterruptFlag = true;
                    m_frameStep++;
                    break;
                case 5:
                    if(!m_interruptInhibitFlag) m_frameInterruptFlag = true;
                    m_frameStep=0;
                    break;
                }

                m_nextDelay = mode0Delays[m_frameStep];

            }


        }
        else {

            if(--m_nextDelay == 0) {

                switch(m_frameStep) {
                case 0:
                    updateEnvelopsAndLinearCounters();
                    updateAudioOutput();
                    m_frameStep++;
                    m_nextDelay = mode1Delays[m_frameStep];
                    break;
                case 1:
                    updateEnvelopsAndLinearCounters();
                    updateLengthCountersAndSweeps();
                    updateAudioOutput();
                    m_frameStep++;
                    m_nextDelay = mode1Delays[m_frameStep];
                    break;
                case 2:
                    updateEnvelopsAndLinearCounters();
                    updateAudioOutput();
                    m_frameStep++;
                    m_nextDelay = mode1Delays[m_frameStep];
                    break;
                case 3:
                    //do nothing
                    m_frameStep++;
                    m_nextDelay = mode1Delays[m_frameStep];
                    break;
                case 4:
                    updateEnvelopsAndLinearCounters();
                    updateLengthCountersAndSweeps();
                    updateAudioOutput();
                    m_frameStep = 0;
                    m_nextDelay = mode1Delays[m_frameStep] + (m_jitter ? 0 : 1);
                    break;
                }
            }
        }        

        if(m_writeChannelsFlag) {
            m_writeChannelsFlag = false;
            writeChannels(m_writeChannelsAddr, m_writeChannelsData);            
        }

        m_square1.cycle();
        m_square2.cycle();
      
        m_noise.cycle();
        m_sample.cycle();

        m_jitter = !m_jitter; 
    }

};

#endif
