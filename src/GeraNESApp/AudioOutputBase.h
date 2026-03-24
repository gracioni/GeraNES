#pragma once

#include "AudioGenerator.h"

#include "GeraNES/IAudioOutput.h"
#include <array>
#include <algorithm>
#include <sstream>
#include <vector>

class AudioOutputBase : public IAudioOutput
{

private:

    PulseWave m_pulseWave1;
    PulseWave m_pulseWave2;
    TriangleWave m_triangleWave;
    NoiseWave m_noise;

    SampleWave m_sample;
    SampleDirect m_sampleDirect;
    ExpansionChannel m_expansionChannel;
    float m_expansionVolume = 1.0f;
    int m_outputSampleRate = 44100;

    FirstOrderHighPassFilter m_hpFilter1;
    FirstOrderHighPassFilter m_hpFilter2;
    FirstOrderLowPassFilter m_lpFilter;

    float m_userPulse1Volume = 1.0f;
    float m_userPulse2Volume = 1.0f;
    float m_userTriangleVolume = 1.0f;
    float m_userNoiseVolume = 1.0f;
    float m_userSampleVolume = 1.0f;
    float m_userExpansionVolume = 1.0f;
    bool m_rewinding = false;
    static constexpr size_t VISUALIZER_BUFFER_SIZE = 2048;
    std::array<float, VISUALIZER_BUFFER_SIZE> m_visualizerSamples = {};
    size_t m_visualizerWriteIndex = 0;
    size_t m_visualizerSampleCount = 0;

    static float clampVolume(float v)
    {
        return std::clamp(v, 0.0f, 1.0f);
    }

    float mixExpansionAudio(float& mixWeight)
    {
        return m_expansionChannel.get(mixWeight) * m_expansionVolume * m_userExpansionVolume;
    }

public:

    bool init() override {
        clearAudioBuffers();
        return true;
    }

    void initChannels(int sampleRate)
    {    
        m_outputSampleRate = std::max(1, sampleRate);
        m_pulseWave1.init(sampleRate);
        m_pulseWave2.init(sampleRate);
        m_triangleWave.init(sampleRate);
        m_noise.init(sampleRate);
        m_sample.init(sampleRate);
        m_sampleDirect.init(sampleRate);
        m_expansionChannel.init(sampleRate);
        m_expansionVolume = 1.0f;
        m_visualizerSamples.fill(0.0f);
        m_visualizerWriteIndex = 0;
        m_visualizerSampleCount = 0;

        //from https://www.nesdev.org/wiki/APU_Mixer
        m_hpFilter1.init(sampleRate, 90);
        m_hpFilter2.init(sampleRate, 440);
        m_lpFilter.init(sampleRate, 14000);  
    }

    void clearBuffers()
    {
        m_sampleDirect.clearBuffer();
        m_sample.clearBuffer();
        m_expansionChannel.clearBuffer();
    }

    void clearAudioBuffers() override
    {
        clearBuffers();
    }

    void captureMixedSample(float sample)
    {
        m_visualizerSamples[m_visualizerWriteIndex] = sample;
        m_visualizerWriteIndex = (m_visualizerWriteIndex + 1) % VISUALIZER_BUFFER_SIZE;
        if(m_visualizerSampleCount < VISUALIZER_BUFFER_SIZE) ++m_visualizerSampleCount;
    }

    std::vector<float> getRecentMixedSamples(size_t maxSamples = 0) const override
    {
        const size_t available = m_visualizerSampleCount;
        const size_t count = maxSamples == 0 ? available : std::min(maxSamples, available);

        std::vector<float> out(count);
        if(count == 0) return out;

        const size_t start = (m_visualizerWriteIndex + VISUALIZER_BUFFER_SIZE - count) % VISUALIZER_BUFFER_SIZE;
        for(size_t i = 0; i < count; ++i) {
            out[i] = m_visualizerSamples[(start + i) % VISUALIZER_BUFFER_SIZE];
        }
        return out;
    }

    int outputSampleRate() const override
    {
        return m_outputSampleRate;
    }

    GERANES_INLINE_HOT float mix()
    {
        float ret = 0;
        float expansionMixWeight = 0.0f;
        const float expansionRaw = mixExpansionAudio(expansionMixWeight);
        const float effectiveExpansionMixWeight = m_rewinding ? 0.0f : expansionMixWeight;
        const float sum = 0.5f + 0.5f + 0.5f + 1.0f + 1.5f + 1.5f + expansionMixWeight;

        //empirical values 
        ret += 0.5f/sum*m_pulseWave1.get()*m_userPulse1Volume;
        ret += 0.5f/sum*m_pulseWave2.get()*m_userPulse2Volume;
        ret += 0.5f/sum*m_triangleWave.get()*m_userTriangleVolume;
        ret += 1.0f/sum*m_noise.get()*m_userNoiseVolume;
        if(!m_rewinding) {
            ret += 1.5f/sum*m_sample.get()*m_userSampleVolume;
            ret += 1.5f/sum*m_sampleDirect.get()*m_userSampleVolume;
        }
        if(effectiveExpansionMixWeight > 0.0f) {
            ret += effectiveExpansionMixWeight / sum * expansionRaw;
        }

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
            case Channel::Expansion:
                m_userExpansionVolume = clampVolume(volume); break;
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

    void setExpansionSourceRateHz(int rateHz) override
    {
        if(rateHz > 1) {
            m_expansionChannel.setRawSourceRateHz(static_cast<uint32_t>(rateHz));
        }
    }

    void setExpansionAudioVolume(float volume) override
    {
        m_expansionVolume = clampVolume(volume);
    }

    void setRewinding(bool rewinding) override
    {
        m_rewinding = rewinding;
    }

    void processExpansionAudioSample(float currentSample, float mixWeight) override
    {
        m_expansionChannel.add(currentSample, mixWeight);
    }

    std::string getAudioChannelsJson() const override
    {
        std::ostringstream ss;
        ss << "{\"channels\":["
           << "{\"id\":\"nes.pulse1\",\"label\":\"NES Pulse 1\",\"volume\":" << m_userPulse1Volume << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"nes.pulse2\",\"label\":\"NES Pulse 2\",\"volume\":" << m_userPulse2Volume << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"nes.triangle\",\"label\":\"NES Triangle\",\"volume\":" << m_userTriangleVolume << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"nes.noise\",\"label\":\"NES Noise\",\"volume\":" << m_userNoiseVolume << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"nes.sample\",\"label\":\"NES Sample\",\"volume\":" << m_userSampleVolume << ",\"min\":0.0,\"max\":1.0},"
           << "{\"id\":\"nes.expansion\",\"label\":\"NES Expansion\",\"volume\":" << m_userExpansionVolume << ",\"min\":0.0,\"max\":1.0}"
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
        if(id == "nes.expansion") { m_userExpansionVolume = v; return true; }
        return false;
    }

};
