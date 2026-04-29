#pragma once

#include <AL/al.h>
#include <AL/alc.h>

#include <string>
#include <vector>

#include "AudioOutputBase.h"

class OpenALAudioOutput : public AudioOutputBase
{
private:
    static const size_t N_BUFFERS = 3;
    static constexpr float BUFFER_TIME = 0.1f;

    ALCdevice* m_device = nullptr;
    ALCcontext* m_context = nullptr;
    ALuint m_source = 0;
    ALuint m_buffer[N_BUFFERS] = {};

    int m_currentBufferIndex = 0;
    int m_buffersAvailable = 0;

    std::string m_currentDeviceName = "";
    std::vector<ALshort> m_bufferData;

    int m_sampleRate = 44100;
    int m_sampleSize = 16;
    uint32_t sampleAcc = 0;
    float m_volume = 1.0f;

    void clearBuffers();
    void turnOff();

public:
    OpenALAudioOutput();
    ~OpenALAudioOutput() override;

    int sampleRate();
    int sampleSize();

    void restart() override;
    const std::string& currentDeviceName() const override;
    std::vector<std::string> getAudioList() const override;
    AudioFormatOptions getAudioFormatOptions(const std::string& deviceName) const override;
    bool config(const std::string& deviceName) override;
    bool config(const std::string& deviceName, int sampleRate, int sampleSize) override;
    int currentSampleRate() const override;
    int currentSampleSize() const override;
    bool init() override;
    void render(uint32_t dt, bool silenceFlag) override;
    void discardQueuedAudio() override;
    void setVolume(float volume) override;
    float getVolume() const override;
};
