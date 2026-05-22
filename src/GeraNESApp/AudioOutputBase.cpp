#include "GeraNESApp/AudioOutputBase.h"

float AudioOutputBase::clampVolume(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
}

float AudioOutputBase::mixExpansionAudio(float& mixWeight)
{
    return m_expansionChannel.get(mixWeight) * m_expansionVolume * m_userExpansionVolume;
}

bool AudioOutputBase::init()
{
    clearAudioBuffers();
    return true;
}

void AudioOutputBase::initChannels(int sampleRate)
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

    m_hpFilter1.init(sampleRate, 90);
    m_hpFilter2.init(sampleRate, 440);
    m_lpFilter.init(sampleRate, 14000);
    if(m_externalAudioMixer) {
        m_externalAudioMixer->onOutputSampleRateChanged(m_outputSampleRate);
    }
}

void AudioOutputBase::clearBuffers()
{
    m_sampleDirect.clearBuffer();
    m_sample.clearBuffer();
    m_expansionChannel.clearBuffer();
}

void AudioOutputBase::clearAudioBuffers()
{
    clearBuffers();
}

void AudioOutputBase::captureMixedSample(float sample)
{
    m_visualizerSamples[m_visualizerWriteIndex] = sample;
    m_visualizerWriteIndex = (m_visualizerWriteIndex + 1) % VISUALIZER_BUFFER_SIZE;
    if(m_visualizerSampleCount < VISUALIZER_BUFFER_SIZE) ++m_visualizerSampleCount;
}

std::vector<float> AudioOutputBase::getRecentMixedSamples(size_t maxSamples) const
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

int AudioOutputBase::outputSampleRate() const
{
    return m_outputSampleRate;
}

void AudioOutputBase::setChannelVolume(Channel channel, float volume)
{
    switch(channel) {
        case Channel::Pulse_1: m_pulseWave1.setVolume(volume); break;
        case Channel::Pulse_2: m_pulseWave2.setVolume(volume); break;
        case Channel::Triangle: m_triangleWave.setVolume(volume); break;
        case Channel::Noise: m_noise.setVolume(volume); break;
        case Channel::Sample:
            m_sample.setVolume(volume);
            m_sampleDirect.setVolume(volume);
            break;
        case Channel::Expansion:
            m_userExpansionVolume = clampVolume(volume);
            break;
    }
}

void AudioOutputBase::setChannelFrequency(Channel channel, float frequency)
{
    switch(channel) {
        case Channel::Pulse_1: m_pulseWave1.setFrequency(frequency); break;
        case Channel::Pulse_2: m_pulseWave2.setFrequency(frequency); break;
        case Channel::Triangle: m_triangleWave.setFrequency(frequency); break;
        case Channel::Noise: m_noise.setFrequency(frequency); break;
        case Channel::Sample: m_sample.setFrequency(frequency); break;
        case Channel::Expansion: break;
    }
}

void AudioOutputBase::setPulseDutyCycle(PulseChannel pulseChannel, float duty)
{
    switch(pulseChannel) {
        case PulseChannel::Pulse_1: m_pulseWave1.setDuty(duty); break;
        case PulseChannel::Pulse_2: m_pulseWave2.setDuty(duty); break;
    }
}

void AudioOutputBase::setNoiseMetallic(bool state)
{
    m_noise.setMetallic(state);
}

void AudioOutputBase::addSample(float sample)
{
    m_sample.add(sample);
}

void AudioOutputBase::addSampleDirect(float period, float sample)
{
    m_sampleDirect.add(period, sample);
}

void AudioOutputBase::setExpansionSourceRateHz(int rateHz)
{
    if(rateHz > 1) {
        m_expansionChannel.setRawSourceRateHz(static_cast<uint32_t>(rateHz));
    }
}

void AudioOutputBase::setExpansionAudioVolume(float volume)
{
    m_expansionVolume = clampVolume(volume);
}

void AudioOutputBase::setRewinding(bool rewinding)
{
    m_rewinding = rewinding;
}

void AudioOutputBase::processExpansionAudioSample(float currentSample, float mixWeight)
{
    m_expansionChannel.add(currentSample, mixWeight);
}

void AudioOutputBase::setExternalAudioMixer(std::shared_ptr<ExternalAudioMixer> mixer)
{
    m_externalAudioMixer = std::move(mixer);
    if(m_externalAudioMixer) {
        m_externalAudioMixer->onOutputSampleRateChanged(m_outputSampleRate);
    }
}

std::shared_ptr<IAudioOutput::ExternalAudioMixer> AudioOutputBase::getExternalAudioMixer() const
{
    return m_externalAudioMixer;
}

std::string AudioOutputBase::getAudioChannelsJson() const
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

bool AudioOutputBase::setAudioChannelVolumeById(const std::string& id, float volume)
{
    const float v = clampVolume(volume);
    if(id == "nes.pulse1") {
        m_userPulse1Volume = v;
        return true;
    }
    if(id == "nes.pulse2") {
        m_userPulse2Volume = v;
        return true;
    }
    if(id == "nes.triangle") {
        m_userTriangleVolume = v;
        return true;
    }
    if(id == "nes.noise") {
        m_userNoiseVolume = v;
        return true;
    }
    if(id == "nes.sample") {
        m_userSampleVolume = v;
        return true;
    }
    if(id == "nes.expansion") {
        m_userExpansionVolume = v;
        return true;
    }
    return false;
}
