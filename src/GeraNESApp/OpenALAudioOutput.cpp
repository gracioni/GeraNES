#include "GeraNESApp/OpenALAudioOutput.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "logger/logger.h"

void OpenALAudioOutput::clearBuffers()
{
    m_bufferData.clear();
    sampleAcc = 0;

    AudioOutputBase::clearBuffers();
}

void OpenALAudioOutput::turnOff()
{
    if(m_device)
    {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(m_context);
        alcCloseDevice(m_device);
        m_device = nullptr;
        m_context = nullptr;
        m_currentDeviceName = "";
    }
    clearBuffers();
}

OpenALAudioOutput::OpenALAudioOutput()
{
    auto audioDevices = getAudioList();
    for(const auto& d : audioDevices) {
        Logger::instance().log(std::string("Audio device detected: ") + d, Logger::Type::INFO);
    }

    m_device = alcOpenDevice(nullptr); // Open default device
    if(!m_device)
    {
        Logger::instance().log("Failed to open default OpenAL device", Logger::Type::ERROR);
        return;
    }

    m_context = alcCreateContext(m_device, nullptr);
    if(!alcMakeContextCurrent(m_context))
    {
        Logger::instance().log("Failed to make default OpenAL context current", Logger::Type::ERROR);
        return;
    }

    Logger::instance().log("OpenAL default device initialized", Logger::Type::INFO);
}

OpenALAudioOutput::~OpenALAudioOutput()
{
    turnOff();
}

int OpenALAudioOutput::sampleRate()
{
    return m_sampleRate;
}

int OpenALAudioOutput::sampleSize()
{
    return m_sampleSize;
}

void OpenALAudioOutput::restart()
{
    config(m_currentDeviceName, m_sampleRate, m_sampleSize);
}

const std::string& OpenALAudioOutput::currentDeviceName() const
{
    return m_currentDeviceName;
}

std::vector<std::string> OpenALAudioOutput::getAudioList() const
{
    std::vector<std::string> ret;
    const ALCchar* devices = alcGetString(nullptr, ALC_DEVICE_SPECIFIER);
    while(*devices)
    {
        ret.push_back(devices);
        devices += std::strlen(devices) + 1;
    }
    return ret;
}

IAudioOutput::AudioFormatOptions OpenALAudioOutput::getAudioFormatOptions(const std::string& deviceName) const
{
    (void)deviceName;
    return AudioFormatOptions{
        {22050, 32000, 44100, 48000, 88200, 96000},
        {16}
    };
}

bool OpenALAudioOutput::config(const std::string& deviceName)
{
    return config(deviceName, 0, 0);
}

bool OpenALAudioOutput::config(const std::string& deviceName, int requestedSampleRate, int requestedSampleSize)
{
    turnOff();

    Logger::instance().log("Initializing audio...", Logger::Type::INFO);

    m_device = alcOpenDevice(deviceName.empty() ? nullptr : deviceName.c_str());
    std::string selectedName = deviceName;
    if(!m_device)
    {
        if(!deviceName.empty()) {
            Logger::instance().log(
                std::string("Requested audio device not available: ") + deviceName + ". Falling back to default.",
                Logger::Type::WARNING);
            m_device = alcOpenDevice(nullptr);
            selectedName = "default";
        }
        if(!m_device) {
            Logger::instance().log("Failed to open OpenAL device", Logger::Type::ERROR);
            return false;
        }
    }
    else if(selectedName.empty()) {
        selectedName = "default";
    }

    m_sampleRate = requestedSampleRate > 0 ? requestedSampleRate : 44100;
    m_sampleSize = requestedSampleSize == 16 ? requestedSampleSize : 16;
    m_outputChannels = desiredOutputChannels();

    m_context = alcCreateContext(m_device, nullptr);
    if(!alcMakeContextCurrent(m_context))
    {
        Logger::instance().log("Failed to make OpenAL context current", Logger::Type::ERROR);
        return false;
    }

    alGenSources(1, &m_source);
    alGenBuffers(N_BUFFERS, m_buffer);

    m_currentBufferIndex = 0;
    m_buffersAvailable = N_BUFFERS;

    m_currentDeviceName = selectedName;

    Logger::instance().log(std::string("Audio device: ") + m_currentDeviceName, Logger::Type::INFO);
    Logger::instance().log(std::string("Sample rate: ") + std::to_string(sampleRate()), Logger::Type::INFO);
    Logger::instance().log(std::string("Sample size: ") + std::to_string(sampleSize()), Logger::Type::INFO);
    Logger::instance().log(std::string("Channels: ") + std::to_string(m_outputChannels), Logger::Type::INFO);

    initChannels(sampleRate());

    return true;
}

int OpenALAudioOutput::currentSampleRate() const
{
    return m_sampleRate;
}

int OpenALAudioOutput::currentSampleSize() const
{
    return m_sampleSize;
}

bool OpenALAudioOutput::init()
{
    AudioOutputBase::init();
    if(!m_device) config("");
    return true;
}

void OpenALAudioOutput::render(uint32_t dt, bool silenceFlag)
{
    if(!m_device)
    {
        turnOff();
        return;
    }

    sampleAcc += dt * sampleRate();
    float vol = std::pow(m_volume, 2.0f);

    while(sampleAcc >= 1000)
    {
        if(m_outputChannels <= 1) {
            const float mono = (silenceFlag ? 0.0f : mixMono()) * vol;
            captureMixedSample(mono);
            m_bufferData.push_back(static_cast<ALshort>(mono * 32767.0f));
        } else {
            const StereoSample mixedFrame = silenceFlag ? StereoSample{} : mixFrame();
            const float left = mixedFrame.left * vol;
            const float right = mixedFrame.right * vol;
            captureMixedSample((left + right) * 0.5f);
            m_bufferData.push_back(static_cast<ALshort>(left * 32767.0f));
            m_bufferData.push_back(static_cast<ALshort>(right * 32767.0f));
        }
        sampleAcc -= 1000;
    }

    ALint numProcessed;
    alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &numProcessed);

    if(numProcessed > 0) {
        std::vector<ALuint> processedBuffers(static_cast<size_t>(numProcessed));
        alSourceUnqueueBuffers(m_source, numProcessed, processedBuffers.data());
        m_buffersAvailable = std::min<int>(static_cast<int>(N_BUFFERS), m_buffersAvailable + numProcessed);
    }    

    const size_t prebufferChunkSamples =
        static_cast<size_t>(sampleRate() * BUFFER_TIME / N_BUFFERS) * static_cast<size_t>(std::max(1, m_outputChannels));

    if(m_buffersAvailable > 0 && (m_bufferData.size() >= prebufferChunkSamples)) {
        alBufferData(m_buffer[m_currentBufferIndex],
                     m_outputChannels > 1 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16,
                     m_bufferData.data(),
                     static_cast<ALsizei>(m_bufferData.size() * sizeof(ALshort)),
                     sampleRate());
        alSourceQueueBuffers(m_source, 1, &m_buffer[m_currentBufferIndex]);
        m_currentBufferIndex = (m_currentBufferIndex + 1) % N_BUFFERS;

        m_buffersAvailable--;
        m_bufferData.clear();
    }

    ALint state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);

    ALint queued = 0;    
    alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);

    // Always require a full prebuffer before (re)starting playback.
    if(state != AL_PLAYING && queued >= static_cast<ALint>(N_BUFFERS))
    {
        alSourcePlay(m_source);
    }
}

void OpenALAudioOutput::discardQueuedAudio()
{
    m_bufferData.clear();
    sampleAcc = 0;
    clearBuffers();

    if(m_device) {
        alSourceStop(m_source);

        ALint queued = 0;
        alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);
        while(queued > 0) {
            ALuint bufferId = 0;
            alSourceUnqueueBuffers(m_source, 1, &bufferId);
            --queued;
        }

        m_currentBufferIndex = 0;
        m_buffersAvailable = static_cast<int>(N_BUFFERS);
    }
}

void OpenALAudioOutput::setVolume(float volume)
{
    m_volume = volume;
}

float OpenALAudioOutput::getVolume() const
{
    return m_volume;
}
