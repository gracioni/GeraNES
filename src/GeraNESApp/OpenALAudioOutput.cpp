#include "GeraNESApp/OpenALAudioOutput.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "logger/logger.h"

void OpenALAudioOutput::clearBuffers()
{
    m_bufferData.clear();
    m_queuedChunks.clear();
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
    // OpenAL assumes a sample rate of 44100 by default
    return 44100;
}

int OpenALAudioOutput::sampleSize()
{
    return 16; // OpenAL typically uses 16-bit samples
}

void OpenALAudioOutput::restart()
{
    config(m_currentDeviceName);
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

bool OpenALAudioOutput::config(const std::string& deviceName)
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

    initChannels(sampleRate());

    return true;
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
        float value = silenceFlag ? 0.0f : mix() * vol;
        captureMixedSample(value);
        ALshort sample = static_cast<ALshort>(value * 32767.0f);
        m_bufferData.push_back(sample);
        sampleAcc -= 1000;
    }

    popProcessedQueuedChunks();

    if(m_buffersAvailable > 0 && m_bufferData.size() >= sampleRate() * (sampleSize() / 8) * BUFFER_TIME / N_BUFFERS) {
        alBufferData(m_buffer[m_currentBufferIndex],
                     AL_FORMAT_MONO16,
                     m_bufferData.data(),
                     static_cast<ALsizei>(m_bufferData.size() * sizeof(ALshort)),
                     sampleRate());
        alSourceQueueBuffers(m_source, 1, &m_buffer[m_currentBufferIndex]);
        m_queuedChunks.push_back(m_bufferData);
        m_currentBufferIndex = (m_currentBufferIndex + 1) % N_BUFFERS;

        m_buffersAvailable--;
        m_bufferData.clear();
    }

    ALint state;
    ALint queued = 0;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
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
    m_queuedChunks.clear();
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

void OpenALAudioOutput::popProcessedQueuedChunks()
{
    ALint numProcessed = 0;
    alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &numProcessed);

    if(numProcessed > 0) {
        std::vector<ALuint> processedBuffers(static_cast<size_t>(numProcessed));
        alSourceUnqueueBuffers(m_source, numProcessed, processedBuffers.data());
        m_buffersAvailable = std::min<int>(static_cast<int>(N_BUFFERS), m_buffersAvailable + numProcessed);
        while(numProcessed-- > 0 && !m_queuedChunks.empty()) {
            m_queuedChunks.pop_front();
        }
    }
}

void OpenALAudioOutput::rebuildQueuedChunksOnSource()
{
    if(!m_device) return;
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
    for(const std::vector<ALshort>& chunk : m_queuedChunks) {
        if(chunk.empty() || m_buffersAvailable <= 0) continue;
        alBufferData(
            m_buffer[m_currentBufferIndex],
            AL_FORMAT_MONO16,
            chunk.data(),
            static_cast<ALsizei>(chunk.size() * sizeof(ALshort)),
            sampleRate()
        );
        alSourceQueueBuffers(m_source, 1, &m_buffer[m_currentBufferIndex]);
        m_currentBufferIndex = (m_currentBufferIndex + 1) % N_BUFFERS;
        --m_buffersAvailable;
    }

    ALint state = 0;
    ALint queuedBuffers = 0;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queuedBuffers);
    if(state != AL_PLAYING && queuedBuffers >= static_cast<ALint>(N_BUFFERS)) {
        alSourcePlay(m_source);
    }
}

size_t OpenALAudioOutput::queuedAudioByteCount() const
{
    auto* self = const_cast<OpenALAudioOutput*>(this);
    if(!self->m_device) return 0;
    self->popProcessedQueuedChunks();

    size_t queuedBytes = self->m_bufferData.size() * sizeof(ALshort);
    for(const auto& chunk : self->m_queuedChunks) {
        queuedBytes += chunk.size() * sizeof(ALshort);
    }
    return queuedBytes;
}

bool OpenALAudioOutput::trimQueuedAudioTailBytes(size_t bytes)
{
    if(bytes == 0) return true;
    if(!m_device) return false;

    popProcessedQueuedChunks();
    size_t available = queuedAudioByteCount();
    if(available == 0) return false;
    bytes = std::min(bytes, available);

    // Quantize to sample granularity (16-bit mono) to guarantee loop progress.
    bytes &= ~static_cast<size_t>(sizeof(ALshort) - 1);
    size_t pendingBytes = m_bufferData.size() * sizeof(ALshort);
    size_t removePending = std::min(bytes, pendingBytes);
    if(removePending > 0) {
        const size_t removeSamples = removePending / sizeof(ALshort);
        m_bufferData.resize(m_bufferData.size() - removeSamples);
        bytes -= removeSamples * sizeof(ALshort);
    }

    while(bytes > 0 && !m_queuedChunks.empty()) {
        std::vector<ALshort>& back = m_queuedChunks.back();
        size_t backBytes = back.size() * sizeof(ALshort);
        size_t remove = std::min(bytes, backBytes);
        size_t removeSamples = remove / sizeof(ALshort);
        if(removeSamples == 0) break;
        back.resize(back.size() - removeSamples);
        bytes -= removeSamples * sizeof(ALshort);
        if(back.empty()) {
            m_queuedChunks.pop_back();
        }
    }

    rebuildQueuedChunksOnSource();
    return true;
}

void OpenALAudioOutput::setVolume(float volume)
{
    m_volume = volume;
}

float OpenALAudioOutput::getVolume() const
{
    return m_volume;
}
