#pragma once

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

    GERANES_INLINE_HOT float mix()
    {
        float ret = 0;

        const float sum = 0.5f+0.5f+0.5f+1.0f+1.5f+1.5f;

        //empirical values 
        ret += 0.5f/sum*m_pulseWave1.get();
        ret += 0.5f/sum*m_pulseWave2.get();
        ret += 0.5f/sum*m_triangleWave.get();
        ret += 1.0f/sum*m_noise.get();
        ret += 1.5f/sum*m_sample.get();
        ret += 1.5f/sum*m_sampleDirect.get();

        ret = m_hpFilter1.apply(ret);
        ret = m_hpFilter2.apply(ret);
        ret = m_lpFilter.apply(ret);

        if(ret > 0.999f) ret = 0.999f;
        else if(ret < -0.999f) ret = -0.999f;

        return ret;
    }

    void setChannelVolume(Channel channel, float volume) override {
        switch(channel) {
            case Channel::Square_1: m_pulseWave1.setVolume(volume); break;
            case Channel::Square_2: m_pulseWave2.setVolume(volume); break;
            case Channel::Triangle: m_triangleWave.setVolume(volume); break;
            case Channel::Noise: m_noise.setVolume(volume); break;
            case Channel::Sample:
                m_sample.setVolume(volume);
                m_sampleDirect.setVolume(volume); break;
        }
    }

    void setChannelFrequency(Channel channel, float frequency) override {
        switch(channel) {
            case Channel::Square_1: m_pulseWave1.setFrequency(frequency); break;
            case Channel::Square_2: m_pulseWave2.setFrequency(frequency); break;
            case Channel::Triangle: m_triangleWave.setFrequency(frequency); break;
            case Channel::Noise: m_noise.setFrequency(frequency); break;
            case Channel::Sample: m_sample.setFrequency(frequency); break;
        }
    }
    
    virtual void setSquareDutyCycle(SquareChannel squareChannel, float duty) override {
        switch(squareChannel) {
            case SquareChannel::Square_1: m_pulseWave1.setDuty(duty); break;
            case SquareChannel::Square_2: m_pulseWave2.setDuty(duty); break;
        }
    }

    void setNoiseMetallic(bool state) override
    {
        m_noise.setMetallic(state);
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
