#ifndef APU_H
#define APU_H

#include "defines.h"
#include "IBus.h"
#include "Serialization.h"
#include "Settings.h"
#include "CPU2A03.h"

#include "IAudioOutput.h"

#include "signal/SigSlot.h"

#include <string>

const uint8_t LENGTH_TABLE[32] = {
    0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
    0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
    0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
    0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E
};

class Square
{
private:

    bool m_enabled;
    uint16_t m_lengthCounter;
    uint16_t m_period;

    uint8_t m_duty;

    bool m_looping;
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

public:

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_enabled);
        SERIALIZEDATA(s, m_lengthCounter);
        SERIALIZEDATA(s, m_period);

        SERIALIZEDATA(s, m_duty);

        SERIALIZEDATA(s, m_looping);
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
    }

    Square()
    {
        init();
    }

    void init(void)
    {
        m_enabled = false;
        m_lengthCounter = 0;
        m_period = 1;

        m_duty = 0;

        m_looping = false;
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
    }

    void write(int addr, uint8_t data)
    {
        switch(addr)
        {
        case 0x0000:

            m_duty= data >> 6;
            m_constantVolume = data&0x0F;
            m_looping = data & 0x20;
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
            if(m_enabled) m_lengthCounter = LENGTH_TABLE[(data & 0xF8) >> 3];
            m_envelopVolume = 0x0F;
            break;

        }
    }

    void updateLengthCounter(void)
    {
        if ((m_looping == false) && (m_lengthCounter > 0)) --m_lengthCounter;
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


    bool isDisabledBySweep(void)
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
                if (m_looping)
                    m_envelopVolume = (m_envelopVolume - 1) & 0x0F;
                else if (m_envelopVolume > 0)
                    m_envelopVolume--;
            }
        }
    }

    bool isEnabled(void)
    {
        return m_enabled == true && m_lengthCounter > 0 &&  !isDisabledBySweep() && m_period > 0;
    }

    void setEnabled(bool status)
    {
        m_enabled = status;
        if(!status) m_lengthCounter = 0;
    }

    uint8_t getVolume(void)
    {
        if(isEnabled())
            return m_constantVolumeMode ? m_constantVolume : m_envelopVolume;

        return 0;
    }

    uint16_t getPeriod(void)
    {
        return m_period;
    }

    uint8_t getDuty(void)
    {
        return m_duty;
    }

    uint16_t getLengthCounter(void)
    {
        return m_lengthCounter;
    }

};

class Triangle
{

private:

    bool m_enabled;
    uint16_t m_lengthCounter;
    uint16_t m_period;

    bool m_halt;
    bool m_loop;
    uint8_t m_linearLoad;
    uint16_t m_linearCounter;

public:

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_enabled);
        SERIALIZEDATA(s, m_lengthCounter);
        SERIALIZEDATA(s, m_period);

        SERIALIZEDATA(s, m_halt);
        SERIALIZEDATA(s, m_loop);
        SERIALIZEDATA(s, m_linearLoad);
        SERIALIZEDATA(s, m_linearCounter);
    }

    Triangle()
    {
        init();
    }

    void init()
    {
        m_enabled = false;
        m_lengthCounter = 0;
        m_period = 1;

        m_halt = false;
        m_loop = false;
        m_linearLoad = 0;
        m_linearCounter = 0;
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
            if(m_enabled) m_lengthCounter = LENGTH_TABLE[(data & 0xF8) >> 3];
            m_halt = true;
            break;
        }
    }

    void updateLengthCounter(void)
    {
        if ( !m_loop && m_lengthCounter > 0 ) m_lengthCounter--;
    }

    void updateLinearCounter(void)
    {
        if(m_halt) m_linearCounter = m_linearLoad;
        else
        {
            if(m_linearCounter > 0) m_linearCounter--;
        }

        if(!m_loop) m_halt = false;
    }

    bool isEnabled(void)
    {
        return m_enabled && m_lengthCounter > 0 && m_linearCounter > 0 && m_period > 0;
    }

    void setEnabled(bool status)
    {
        m_enabled = status;
        if(!status) m_lengthCounter = 0;
    }

    uint8_t getVolume(void)
    {
        if(isEnabled())
            return (m_enabled && m_lengthCounter > 0 && m_linearCounter > 0) ? 15 : 0;

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

};

class Noise
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

    bool m_looping;
    bool m_constantVolumeMode;
    uint8_t m_envelopVolume;
    uint8_t m_constantVolumeAndEnvelopPeriod;
    uint8_t m_envelopCounter;

    bool m_mode;

public:

    void serialization(SerializationBase& s)
    {
        SERIALIZEDATA(s, m_enabled);
        SERIALIZEDATA(s, m_lengthCounter);
        SERIALIZEDATA(s, m_period);

        SERIALIZEDATA(s, m_looping);
        SERIALIZEDATA(s, m_constantVolumeMode);
        SERIALIZEDATA(s, m_envelopVolume);
        SERIALIZEDATA(s, m_constantVolumeAndEnvelopPeriod);
        SERIALIZEDATA(s, m_envelopCounter);

        SERIALIZEDATA(s, m_mode);
    }

    Noise(Settings& settings) : m_settings(settings)
    {
        init();
    }

    void init(void)
    {
        m_enabled = false;
        m_lengthCounter = 0;
        m_period = 1;

        m_looping = false;
        m_constantVolumeMode = false;
        m_envelopVolume = 0;
        m_constantVolumeAndEnvelopPeriod = 1;
        m_envelopCounter = 0;

        m_mode = false;
    }

    void write(int addr, uint8_t data)
    {
        switch(addr)
        {
        case 0x0000:
        {
            m_constantVolumeAndEnvelopPeriod = data&0x0F;
            m_looping = data & 0x20;
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
            if(m_enabled) m_lengthCounter = LENGTH_TABLE[(data & 0xF8) >> 3];
            m_envelopVolume = 0x0F;
            break;
        }
        }
    }

    void updateLengthCounter(void)
    {
        if ((m_looping == false) && (m_lengthCounter > 0)) --m_lengthCounter;
    }

    void updateEnvelop()
    {
        if(m_envelopCounter > 0) m_envelopCounter--;

        if (m_envelopCounter == 0)
        {
            m_envelopCounter = m_constantVolumeAndEnvelopPeriod+1;

            if (!m_constantVolumeMode)
            {
                if (m_looping)
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

};

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

    bool m_silenceFlag;

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

    GERANES_INLINE uint16_t _getPeriod() {

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

        SERIALIZEDATA(s, m_sampleBufferFilled);

        SERIALIZEDATA(s, m_silenceFlag);

    }

    SampleChannel(Settings& settings, Ibus& m, IAudioOutput& audioOutput) : m_settings(settings), m_audioGenerator(audioOutput)
    {
        init();
    }

    void init()
    {
        m_silenceFlag = true;

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

        m_periodCounter = _getPeriod();

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

            m_audioGenerator.setSampleFrequency( 2*(m_settings.CPUClockHz()/16.0)/(_getPeriod()+1));
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

    uint16_t getPeriod(void)
    {
        return _getPeriod();
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
            m_periodCounter = _getPeriod();

            clockDMC();
        }

    }

    void clockDMC() {

        if(--m_shiftCounter == 0) {

            m_shiftCounter = 8;

            m_silenceFlag = !m_sampleBufferFilled;

            if(!m_silenceFlag) {

                m_sampleBufferFilled = false;

                if(m_bytesRemaining > 0) {
                    readSample(true);
                }
            }
        }
    }

    int getBytesRemaining(void)
    {
        return m_bytesRemaining;
    }

    void loadSampleBuffer(uint8_t data) {

        m_sampleBufferFilled = true;

        m_silenceFlag = false;

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
                //m_currentAddr = m_sampleAddr;
                //m_bytesRemaining = m_sampleLength;
                break;
            case 0x80:
                m_interruptFlag = true;
                break;
            }
        }


    }

};

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

    Square m_square1;
    Square m_square2;
    Triangle m_triangle;
    Noise m_noise;
    SampleChannel m_sample;

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
    }

    APU(IAudioOutput& audioOutput, Settings& settings, Ibus& m) :
        m_audioOutput(audioOutput),
        m_settings(settings), m_noise(settings),
        m_sample(settings,m,audioOutput)
    {

    }

    SampleChannel& getSampleChannel() {
        return m_sample;
    }

    const std::string init(void)
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
        //write(0x0017, 0x00); //this LINE hangs cobra triangle
        //for(int i = 0; i < 10; i++ ) process();

        return "";
    }

    GERANES_INLINE_HOT bool getInterruptFlag(void)
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

    void updateEnvelopsAndLinearCounters(void)
    {
        m_square1.updateEnvelop();
        m_square2.updateEnvelop();
        m_triangle.updateLinearCounter();
        m_noise.updateEnvelop();
    }

    void updateLengthCountersAndSweeps(void)
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

        m_audioOutput.setSquare1Volume( (float)m_square1.getVolume()/15.0 );

        switch(m_square2.getDuty())
        {
        case 0: m_audioOutput.setSquare2DutyCycle(0.125); break;
        case 1: m_audioOutput.setSquare2DutyCycle(0.25); break;
        case 2: m_audioOutput.setSquare2DutyCycle(0.5); break;
        case 3: m_audioOutput.setSquare2DutyCycle(0.75); break;
        }

        m_audioOutput.setSquare2Frequency( CPUClock/16.0/(m_square2.getPeriod()+1) );
        m_audioOutput.setSquare2Volume( (float)m_square2.getVolume()/15.0 );

        m_audioOutput.setTriangleFrequency( (CPUClock/16.0/2.0)/(m_triangle.getPeriod()+1) );
        m_audioOutput.setTriangleVolume( (float)m_triangle.getVolume()/15.0 );


        m_audioOutput.setNoiseFrequency( 2*CPUClock/(m_noise.getPeriod()+1) );
        m_audioOutput.setNoiseVolume( (float)m_noise.getVolume()/15.0 );
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

        m_sample.cycle();

        m_jitter = !m_jitter;
    }

};

#endif
