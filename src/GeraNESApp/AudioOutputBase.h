#pragma once

#include "AudioGenerator.h"

#include "GeraNES/IAudioOutput.h"
#include <array>
#include <algorithm>
#include <memory>
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
    std::shared_ptr<ExternalAudioMixer> m_externalAudioMixer;
    bool m_rewinding = false;
    static constexpr size_t VISUALIZER_BUFFER_SIZE = 2048;
    std::array<float, VISUALIZER_BUFFER_SIZE> m_visualizerSamples = {};
    size_t m_visualizerWriteIndex = 0;
    size_t m_visualizerSampleCount = 0;

    static float clampVolume(float v);

    float mixExpansionAudio(float& mixWeight);

public:

    bool init() override;
    void initChannels(int sampleRate);
    void clearBuffers();
    void clearAudioBuffers() override;
    void captureMixedSample(float sample);
    std::vector<float> getRecentMixedSamples(size_t maxSamples = 0) const override;
    int outputSampleRate() const override;

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

        if(m_externalAudioMixer) {
            ret += m_externalAudioMixer->mixAudioSample(m_outputSampleRate);
        }

        if(ret > 0.999f) ret = 0.999f;
        else if(ret < -0.999f) ret = -0.999f;

        return ret;
    }

    void setChannelVolume(Channel channel, float volume) override;
    void setChannelFrequency(Channel channel, float frequency) override;
    void setPulseDutyCycle(PulseChannel pulseChannel, float duty) override;
    void setNoiseMetallic(bool state) override;
    void addSample(float sample) override;
    void addSampleDirect(float period, float sample) override;
    void setExpansionSourceRateHz(int rateHz) override;
    void setExpansionAudioVolume(float volume) override;
    void setRewinding(bool rewinding) override;
    void processExpansionAudioSample(float currentSample, float mixWeight) override;
    void setExternalAudioMixer(std::shared_ptr<ExternalAudioMixer> mixer) override;
    std::shared_ptr<ExternalAudioMixer> getExternalAudioMixer() const override;
    std::string getAudioChannelsJson() const override;
    bool setAudioChannelVolumeById(const std::string& id, float volume) override;

};
