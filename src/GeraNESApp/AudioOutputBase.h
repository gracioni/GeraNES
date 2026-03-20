#pragma once

#include "AudioGenerator.h"

#include "GeraNES/IAudioOutput.h"
#include "GeraNES/util/CircularBuffer.h"
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
    // Expansion audio: CPU-domain samples are downsampled and queued to FIFO.
    CircularBuffer<float> m_expansionFifo { 4096, CircularBuffer<float>::GROW };
    float m_expansionVolume = 1.0f;
    float m_expansionLastSample = 0.0f;
    bool m_expansionPlaybackStarted = false;
    static constexpr size_t EXPANSION_PREBUFFER_MS = 3;
    static constexpr size_t EXPANSION_TARGET_BUFFER_MS = 6;
    double m_expansionConsumeRate = 1.0;
    double m_expansionConsumeAcc = 0.0;
    uint32_t m_expansionSourceRateHz = 1789773;
    uint64_t m_expansionPhaseAcc = 0; // [0, m_expansionSourceRateHz)
    int64_t m_expansionWindowWeight = 0; // weighted by output-rate units
    int64_t m_expansionWindowSumQ = 0;   // fixed-point weighted sum
    static constexpr int64_t EXPANSION_Q_SCALE = 1 << 20;
    int m_outputSampleRate = 44100;

    FirstOrderHighPassFilter m_hpFilter1;
    FirstOrderHighPassFilter m_hpFilter2;
    FirstOrderLowPassFilter m_lpFilter;

    float m_userPulse1Volume = 1.0f;
    float m_userPulse2Volume = 1.0f;
    float m_userTriangleVolume = 1.0f;
    float m_userNoiseVolume = 1.0f;
    float m_userSampleVolume = 1.0f;
    static constexpr size_t VISUALIZER_BUFFER_SIZE = 2048;
    std::array<float, VISUALIZER_BUFFER_SIZE> m_visualizerSamples = {};
    size_t m_visualizerWriteIndex = 0;
    size_t m_visualizerSampleCount = 0;

    static float clampVolume(float v)
    {
        return std::clamp(v, 0.0f, 1.0f);
    }

    size_t expansionPrebufferSamples() const
    {
        const uint64_t rate = static_cast<uint64_t>(std::max(1, m_outputSampleRate));
        const uint64_t samples = (rate * static_cast<uint64_t>(EXPANSION_PREBUFFER_MS) + 999ULL) / 1000ULL;
        return static_cast<size_t>(std::max<uint64_t>(1ULL, samples));
    }

    size_t expansionTargetBufferSamples() const
    {
        const uint64_t rate = static_cast<uint64_t>(std::max(1, m_outputSampleRate));
        const uint64_t samples = (rate * static_cast<uint64_t>(EXPANSION_TARGET_BUFFER_MS) + 999ULL) / 1000ULL;
        return static_cast<size_t>(std::max<uint64_t>(1ULL, samples));
    }

    float mixExpansionAudio()
    {
        if(!m_expansionPlaybackStarted) {
            if(m_expansionFifo.size() >= expansionPrebufferSamples()) {
                m_expansionPlaybackStarted = true;
            }
        }

        if(!m_expansionPlaybackStarted) {
            return 0.0f;
        }

        const double fifoError = static_cast<double>(
            static_cast<int64_t>(m_expansionFifo.size()) - static_cast<int64_t>(expansionTargetBufferSamples()));

        // Keep FIFO around target size to reduce starvation/overfill over long runs.
        m_expansionConsumeRate = std::clamp(1.0 + fifoError * 0.00002, 0.995, 1.005);
        m_expansionConsumeAcc += m_expansionConsumeRate;

        while(m_expansionConsumeAcc >= 1.0) {
            if(!m_expansionFifo.empty()) {
                m_expansionLastSample = m_expansionFifo.read();
            }
            m_expansionConsumeAcc -= 1.0;
        }

        // If FIFO starves, hold last value to avoid sharp discontinuity.
        return m_expansionLastSample * m_expansionVolume;
    }

public:

    AudioOutputBase()
    {    
    }

    ~AudioOutputBase() override
    {
    }

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
        m_expansionFifo.clear();
        m_expansionVolume = 1.0f;
        m_expansionLastSample = 0.0f;
        m_expansionPlaybackStarted = false;
        m_expansionConsumeRate = 1.0;
        m_expansionConsumeAcc = 0.0;
        m_expansionPhaseAcc = 0;
        m_expansionWindowWeight = 0;
        m_expansionWindowSumQ = 0;
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
        m_expansionFifo.clear();
        m_expansionLastSample = 0.0f;
        m_expansionPlaybackStarted = false;
        m_expansionConsumeRate = 1.0;
        m_expansionConsumeAcc = 0.0;
        m_expansionPhaseAcc = 0;
        m_expansionWindowWeight = 0;
        m_expansionWindowSumQ = 0;
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

        const float sum = 0.5f+0.5f+0.5f+1.0f+1.5f+1.5f+1.0f;

        //empirical values 
        ret += 0.5f/sum*m_pulseWave1.get()*m_userPulse1Volume;
        ret += 0.5f/sum*m_pulseWave2.get()*m_userPulse2Volume;
        ret += 0.5f/sum*m_triangleWave.get()*m_userTriangleVolume;
        ret += 1.0f/sum*m_noise.get()*m_userNoiseVolume;
        ret += 1.5f/sum*m_sample.get()*m_userSampleVolume;
        ret += 1.5f/sum*m_sampleDirect.get()*m_userSampleVolume;
        ret += 1.0f/sum*mixExpansionAudio();

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

    void setExpansionSourceRateHz(int rateHz) override
    {
        if(rateHz > 1) {
            m_expansionSourceRateHz = static_cast<uint32_t>(std::max(1, rateHz));
        }
    }

    void setExpansionAudioVolume(float volume) override
    {
        m_expansionVolume = clampVolume(volume);
    }

    void processExpansionAudioSample(float currentSample) override
    {
        const int64_t sampleQ = static_cast<int64_t>(currentSample * static_cast<float>(EXPANSION_Q_SCALE));
        const uint64_t outRate = static_cast<uint64_t>(std::max(1, m_outputSampleRate));
        const uint64_t srcRate = static_cast<uint64_t>(m_expansionSourceRateHz);

        // Advance one source tick (CPU cycle) in rational clock space.
        m_expansionPhaseAcc += outRate;

        if(m_expansionPhaseAcc < srcRate) {
            m_expansionWindowWeight += static_cast<int64_t>(outRate);
            m_expansionWindowSumQ += sampleQ * static_cast<int64_t>(outRate);
            return;
        }

        // Boundary crossed inside this source tick. Split this tick proportionally.
        const uint64_t excess = m_expansionPhaseAcc - srcRate;
        const uint64_t inWindowUnits = outRate - excess;

        m_expansionWindowWeight += static_cast<int64_t>(inWindowUnits);
        m_expansionWindowSumQ += sampleQ * static_cast<int64_t>(inWindowUnits);

        if(m_expansionWindowWeight > 0) {
            const float averaged = static_cast<float>(
                static_cast<double>(m_expansionWindowSumQ) /
                static_cast<double>(m_expansionWindowWeight) /
                static_cast<double>(EXPANSION_Q_SCALE));
            m_expansionFifo.write(averaged);
        }

        // Keep only leftover proportional time/value for next output sample window.
        m_expansionWindowWeight = static_cast<int64_t>(excess);
        m_expansionWindowSumQ = sampleQ * static_cast<int64_t>(excess);
        m_expansionPhaseAcc = excess;
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
