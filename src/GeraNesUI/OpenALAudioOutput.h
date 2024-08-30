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

#include "GeraNes/IAudioOutput.h"

#include "GeraNes/Logger.h"

#include "AudioGenerator.h"

class OpenALAudioOutput : public IAudioOutput
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

    PulseWave m_pulseWave1;
    PulseWave m_pulseWave2;
    TriangleWave m_triangleWave;
    NoiseWave m_noise;

    SampleWave m_sample;
    SampleDirect m_sampleDirect;

    FirstOrderHighPassFilter m_hpFilter1;
    FirstOrderHighPassFilter m_hpFilter2;
    FirstOrderLowPassFilter m_lpFilter;

    uint32_t sampleAcc = 0;
    float m_volume = 1.0f;

    void clearBuffers()
    {
        m_bufferData.clear();
        m_sampleDirect.clearBuffer();
        m_sample.clearBuffer();
        sampleAcc = 0;
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

        m_pulseWave1.init(sampleRate());
        m_pulseWave2.init(sampleRate());
        m_triangleWave.init(sampleRate());
        m_noise.init(sampleRate());
        m_sample.init(sampleRate());
        m_sampleDirect.init(sampleRate());

        m_hpFilter1.init(sampleRate(), 90);
        m_hpFilter2.init(sampleRate(), 440);
        m_lpFilter.init(sampleRate(), 14000);

        return true;
    }

    bool init() override
    {
        if (!m_device)
            config("");
        return true;
    }

    GERANES_INLINE_HOT float mix(void)
    {
        float ret = 0;
        const float sum = 0.5 + 0.5 + 0.5 + 1.0 + 1.5 + 1.5;

        ret += 0.5 / sum * m_pulseWave1.get();
        ret += 0.5 / sum * m_pulseWave2.get();
        ret += 0.5 / sum * m_triangleWave.get();
        ret += 1.0 / sum * m_noise.get();
        ret += 1.5 / sum * m_sample.get();
        ret += 1.5 / sum * m_sampleDirect.get();

        ret = m_hpFilter1.apply(ret);
        ret = m_hpFilter2.apply(ret);
        ret = m_lpFilter.apply(ret);

        if (ret > 0.999)
            ret = 0.9999;
        else if (ret < -0.999)
            ret = -0.9999;

        return ret;
    }

    bool IsSourcePlaying(ALuint source) {
        ALenum state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        return (state == AL_PLAYING);
    }

    void render(uint32_t dt, bool silenceFlag) override
    {
        if (!m_device)
        {
            turnOff();
            return;
        }

        sampleAcc = dt * sampleRate();
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
        m_sampleDirect.add(period, sample);
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
