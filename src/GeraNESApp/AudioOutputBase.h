#ifndef AUDIO_OUTPUT_BASE_H
#define AUDIO_OUTPUT_BASE_H

#include "AudioGenerator.h"

#include "GeraNES/IAudioOutput.h"

class AudioOutputBase : public IAudioOutput
{

private:

    PulseWave m_pulseWave1;
    PulseWave m_pulseWave2;
    TriangleWave m_triangleWave;
    NoiseWave m_noise;

    SampleWave m_sample;
    SampleDirect m_sampleDirect;

    FirstOrderHighPassFilter m_hpFilter1;
    FirstOrderHighPassFilter m_hpFilter2;
    FirstOrderLowPassFilter m_lpFilter;

public:

    AudioOutputBase()
    {    
    }

    ~AudioOutputBase() override
    {
    }

    void initChannels(int sampleRate)
    {    
        m_pulseWave1.init(sampleRate);
        m_pulseWave2.init(sampleRate);
        m_triangleWave.init(sampleRate);
        m_noise.init(sampleRate);
        m_sample.init(sampleRate);
        m_sampleDirect.init(sampleRate);

        //from https://www.nesdev.org/wiki/APU_Mixer
        m_hpFilter1.init(sampleRate, 90);
        m_hpFilter2.init(sampleRate, 440);
        m_lpFilter.init(sampleRate, 14000);  
    }

    void clearBuffers()
    {
        m_sampleDirect.clearBuffer();
        m_sample.clearBuffer();   
    }

    GERANES_INLINE_HOT float mix(void)
    {
        float ret = 0;

        const float sum = 0.5+0.5+0.5+1.0+1.5+1.5;

        //empirical values 
        ret += 0.5/sum*m_pulseWave1.get();
        ret += 0.5/sum*m_pulseWave2.get();
        ret += 0.5/sum*m_triangleWave.get();
        ret += 1.0/sum*m_noise.get();
        ret += 1.5/sum*m_sample.get();
        ret += 1.5/sum*m_sampleDirect.get();

        ret = m_hpFilter1.apply(ret);
        ret = m_hpFilter2.apply(ret);
        ret = m_lpFilter.apply(ret);

        if(ret > 0.999) ret = 0.9999;
        else if(ret < -0.999) ret = -0.9999;

        return ret;
    }  
    
    void setSquare1Frequency(float f) override
    {
        m_pulseWave1.setFrequency(f);
    }

    void setSquare1DutyCycle(float d) override
    {
        m_pulseWave1.setDuty(d);
    }

    void setSquare1Volume(float v) override
    {
        m_pulseWave1.setVolume(v);
    }

    void setSquare2Frequency(float f) override
    {
        m_pulseWave2.setFrequency(f);
    }

    void setSquare2DutyCycle(float d) override
    {
        m_pulseWave2.setDuty(d);
    }

    void setSquare2Volume(float v) override
    {
        m_pulseWave2.setVolume(v);
    }

    void setTriangleFrequency(float f) override
    {
        m_triangleWave.setFrequency(f);
    }

    void setTriangleVolume(float v) override
    {
        m_triangleWave.setVolume(v);
    }

    void setNoiseFrequency(float f) override
    {
        m_noise.setFrequency(f);
    }

    void setNoiseMetallic(bool state) override
    {
        m_noise.setMetallic(state);
    }

    void setNoiseVolume(float v) override
    {
        m_noise.setVolume(v);
    }

    void setSampleVolume(float v) override
    {
        m_sample.setVolume(v);
    }

    void setSampleFrequency(float f) override
    {
        m_sample.setFrequency(f);
    }

    void addSample(float sample) override
    {
        m_sample.add(sample);
    }

    void addSampleDirect(float period, float sample) override
    {
        m_sampleDirect.add(period,sample);
    }

};

#endif
