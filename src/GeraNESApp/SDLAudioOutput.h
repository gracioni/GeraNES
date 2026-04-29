#pragma once

#include <SDL.h>

#include <string>
#include <vector>

#include "AudioOutputBase.h"

class SDLAudioOutput : public AudioOutputBase
{
private:
    static constexpr float BUFFER_TIME = 0.1f;

    SDL_AudioDeviceID m_device = 0;
    std::string m_currentDeviceName = "";

    std::vector<char> m_buffer;

    SDL_AudioSpec spec{};

    uint32_t sampleAcc = 0;
    float m_volume = 1.0f;
    float m_sampleFormatScale = 256.0f;

    void clearBuffers();
    void turnOff();

public:
    SDLAudioOutput();
    ~SDLAudioOutput() override;

    int sampleRate() const;
    int sampleSize() const;

    const std::string& currentDeviceName() const override;
    std::vector<std::string> getAudioList() const override;
    AudioFormatOptions getAudioFormatOptions(const std::string& deviceName) const override;
    void restart() override;
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
