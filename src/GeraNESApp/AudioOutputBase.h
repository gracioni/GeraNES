#pragma once

#include "AudioGenerator.h"

#include "GeraNES/IAudioOutput.h"
#include <algorithm>
#include <sstream>

class AudioOutputBase : public IAudioOutput
{

private:

    PulseWave m_pulseWave1;
    PulseWave m_pulseWave2;
    TriangleWave m_triangleWave;
    NoiseWave m_noise;

    SampleWave m_sample;
    SampleDirect m_sampleDirect;
    // Expansion audio (mappers) uses timestamped samples to avoid jitter/cutouts.
    SampleDirect m_expansionSampleDirect;
    float m_expansionSamplePeriodSec = 1.0f / 1789773.0f;

    FirstOrderHighPassFilter m_hpFilter1;
    FirstOrderHighPassFilter m_hpFilter2;
    FirstOrderLowPassFilter m_lpFilter;

    float m_userPulse1Volume = 1.0f;
    float m_userPulse2Volume = 1.0f;
    float m_userTriangleVolume = 1.0f;
    float m_userNoiseVolume = 1.0f;
    float m_userSampleVolume = 1.0f;

    static float clampVolume(float v)
    {
        return std::clamp(v, 0.0f, 1.0f);
    }

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
        m_expansionSampleDirect.init(sampleRate);

        //from https://www.nesdev.org/wiki/APU_Mixer
        m_hpFilter1.init(sampleRate, 90);
        m_hpFilter2.init(sampleRate, 440);
        m_lpFilter.init(sampleRate, 14000);  
    }

    void clearBuffers()
    {
        m_sampleDirect.clearBuffer();
        m_sample.clearBuffer();
        m_expansionSampleDirect.clearBuffer();
    }

    GERANES_INLINE_HOT float mix()
    {
        float ret = 0;

        const float sum = 0.5f+0.5f+0.5f+1.0f+1.5f+1.5f+1.0f;

        //empirical values 
        ret += 0.5f/sum*m_pulseWave1.get()*m_userPulse1Volume;
        ret += 0.5f/sum*m_pulseWave2.get()*m_userPulse2Volume;
        ret += 0.5f/sum*m_triangleWave.get()*m_userTriangleVolume;
        ret += 1.0f/sum*m_noise.get()*m_userNoiseVolume;
        ret += 1.5f/sum*m_sample.get()*m_userSampleVolume;
        ret += 1.5f/sum*m_sampleDirect.get()*m_userSampleVolume;
        ret += 1.0f/sum*m_expansionSampleDirect.get();

        ret = m_hpFilter1.apply(ret);
        ret = m_hpFilter2.apply(ret);
        ret = m_lpFilter.apply(ret);

        if(ret > 0.999f) ret = 0.999f;
        else if(ret < -0.999f) ret = -0.999f;

        return ret;
    }

    void setChannelVolume(Channel channel, float volume) override {
        switch(channel) {
            case Channel::Pulse_1: m_pulseWave1.setVolume(volume); break;
            case Channel::Pulse_2: m_pulseWave2.setVolume(volume); break;
            case Channel::Triangle: m_triangleWave.setVolume(volume); break;
            case Channel::Noise: m_noise.setVolume(volume); break;
            case Channel::Sample:
                m_sample.setVolume(volume);
                m_sampleDirect.setVolume(volume); break;
        }
    }

    void setChannelFrequency(Channel channel, float frequency) override {
        switch(channel) {
            case Channel::Pulse_1: m_pulseWave1.setFrequency(frequency); break;
            case Channel::Pulse_2: m_pulseWave2.setFrequency(frequency); break;
            case Channel::Triangle: m_triangleWave.setFrequency(frequency); break;
            case Channel::Noise: m_noise.setFrequency(frequency); break;
            case Channel::Sample: m_sample.setFrequency(frequency); break;
        }
    }
    
    virtual void setPulseDutyCycle(PulseChannel pulseChannel, float duty) override {
        switch(pulseChannel) {
            case PulseChannel::Pulse_1: m_pulseWave1.setDuty(duty); break;
            case PulseChannel::Pulse_2: m_pulseWave2.setDuty(duty); break;
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

    void setExpansionAudioSampleRate(float rateHz) override
    {
        if(rateHz > 1.0f) {
            m_expansionSamplePeriodSec = 1.0f / rateHz;
        }
    }

    void setExpansionAudioVolume(float volume) override
    {
        // SampleDirect applies an internal gain curve; compensate to keep expansion level close.
        m_expansionSampleDirect.setVolume(volume * 0.4f);
    }

    void addExpansionAudioSample(float sample) override
    {
        m_expansionSampleDirect.add(m_expansionSamplePeriodSec, sample);
    }

    std::string getAudioChannelsJson() const override
    {
        std::ostringstream ss;
        ss << "{\"channels\":["
           << "{\"id\":\"nes.pulse1\",\"label\":\"NES Pulse 1\",\"volume\":" << m_userPulse1Volume << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"nes.pulse2\",\"label\":\"NES Pulse 2\",\"volume\":" << m_userPulse2Volume << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"nes.triangle\",\"label\":\"NES Triangle\",\"volume\":" << m_userTriangleVolume << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"nes.noise\",\"label\":\"NES Noise\",\"volume\":" << m_userNoiseVolume << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"nes.sample\",\"label\":\"NES Sample\",\"volume\":" << m_userSampleVolume << ",\"min\":0.0,\"max\":1.0}"
           << "]}";
        return ss.str();
    }

    bool setAudioChannelVolumeById(const std::string& id, float volume) override
    {
        const float v = clampVolume(volume);
        if(id == "nes.pulse1") { m_userPulse1Volume = v; return true; }
        if(id == "nes.pulse2") { m_userPulse2Volume = v; return true; }
        if(id == "nes.triangle") { m_userTriangleVolume = v; return true; }
        if(id == "nes.noise") { m_userNoiseVolume = v; return true; }
        if(id == "nes.sample") { m_userSampleVolume = v; return true; }
        return false;
    }

};
