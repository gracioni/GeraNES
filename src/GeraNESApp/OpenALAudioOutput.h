#ifndef OPEN_AL_AUDIO_OUTPUT_H
#define OPEN_AL_AUDIO_OUTPUT_H

#include <AL/al.h>
#include <AL/alc.h>

#include <set>
#include <string>
#include <vector>
#include <queue>
#include <limits>
#include <algorithm>

#define BUFFER_TIME (0.1) //seconds

#include "AudioOutputBase.h"

#include "GeraNES/Logger.h"

#include "AudioGenerator.h"

class OpenALAudioOutput : public AudioOutputBase
{

private:

    static const size_t N_BUFFERS = 2;

    ALCdevice* m_device = nullptr;
    ALCcontext* m_context = nullptr;
    ALuint m_source;
    ALuint m_buffer[N_BUFFERS];

    int m_currentBufferIndex;
    int m_buffersAvailable;

    std::string m_currentDeviceName = "";
    std::vector<ALshort> m_bufferData;  

    uint32_t sampleAcc = 0;
    float m_volume = 1.0f;

    void clearBuffers()
    {
        m_bufferData.clear();
        sampleAcc = 0;

        AudioOutputBase::clearBuffers();
    }

    void turnOff()
    {
        if (m_device)
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

public:

    OpenALAudioOutput()
    {
        m_device = alcOpenDevice(nullptr); // Open default device
        if (!m_device)
        {
            std::cerr << "Failed to open default OpenAL device." << std::endl;
            return;
        }

        m_context = alcCreateContext(m_device, nullptr);
        if (!alcMakeContextCurrent(m_context))
        {
            std::cerr << "Failed to make OpenAL context current." << std::endl;
            return;
        }
    }

    ~OpenALAudioOutput() override
    {
        turnOff();
    }

    int sampleRate()
    {
        // OpenAL assumes a sample rate of 44100 by default
        return 44100;
    }

    int sampleSize()
    {
        return 16; // OpenAL typically uses 16-bit samples
    }

    const std::string& currentDeviceName()
    {
        return m_currentDeviceName;
    }

    std::vector<std::string> getAudioList()
    {
        std::vector<std::string> ret;
        const ALCchar* devices = alcGetString(nullptr, ALC_DEVICE_SPECIFIER);
        while (*devices)
        {
            ret.push_back(devices);
            devices += strlen(devices) + 1;
        }
        return ret;
    }

    bool config(const std::string& deviceName)
    {
        turnOff();

        m_device = alcOpenDevice(deviceName.empty() ? nullptr : deviceName.c_str());
        if (!m_device)
        {
            Logger::instance().log("Failed to open OpenAL device", Logger::ERROR);
            return false;
        }

        m_context = alcCreateContext(m_device, nullptr);
        if (!alcMakeContextCurrent(m_context))
        {
            Logger::instance().log("Failed to make OpenAL context current", Logger::ERROR);
            return false;
        }

        alGenSources(1, &m_source);
        alGenBuffers(N_BUFFERS, m_buffer);

        m_currentBufferIndex = 0;
        m_buffersAvailable = N_BUFFERS;

        m_currentDeviceName = deviceName;

        initChannels(sampleRate());

        return true;
    }

    bool init() override
    {
        if (!m_device)
            config("");
        return true;
    }

    void render(uint32_t dt, bool silenceFlag) override
    {
        if (!m_device)
        {
            turnOff();
            return;
        }

        sampleAcc += dt * sampleRate();
        float vol = std::pow(m_volume, 2);

        while (sampleAcc >= 1000)
        {
            float value = silenceFlag ? 0 : mix() * vol;
            ALshort sample = static_cast<ALshort>(value * 32767);
            m_bufferData.push_back(sample);
            sampleAcc -= 1000;
        }

        ALint numProcessed;
        alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &numProcessed);

        if(numProcessed > 0) {
            alSourceUnqueueBuffers(m_source, numProcessed, nullptr);
            m_buffersAvailable += numProcessed;
        }        

        if(m_buffersAvailable > 0 && m_bufferData.size() >= sampleRate()*(sampleSize()/8)*BUFFER_TIME/N_BUFFERS)  {
            alBufferData(m_buffer[m_currentBufferIndex], AL_FORMAT_MONO16, m_bufferData.data(), m_bufferData.size() * sizeof(ALshort), sampleRate());
            alSourceQueueBuffers(m_source, 1, &m_buffer[m_currentBufferIndex]);
            m_currentBufferIndex = (m_currentBufferIndex+1)%N_BUFFERS;

            m_buffersAvailable--;  
            
            m_bufferData.clear();     
        }

        
        ALint state;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);        

        if (state != AL_PLAYING && m_buffersAvailable == 0)
        {
            alSourcePlay(m_source);
        }        
        
    }    

    void setVolume(float volume)
    {
        m_volume = volume;
    }

    float getVolume()
    {
        return m_volume;
    }
};


#endif
