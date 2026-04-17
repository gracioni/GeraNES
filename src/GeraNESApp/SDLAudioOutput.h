#pragma once

#include <SDL.h>

#include <deque>
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
    std::deque<std::vector<uint8_t>> m_queuedChunks;
    size_t m_queuedDeviceBytesMirror = 0;

    SDL_AudioSpec spec{};

    uint32_t sampleAcc = 0;
    float m_volume = 1.0f;

    void clearBuffers();
    void turnOff();
    void syncQueuedMirrorToDevice();
    void rebuildDeviceQueueFromMirror();

public:
    SDLAudioOutput();
    ~SDLAudioOutput() override;

    int sampleRate() const;
    int sampleSize() const;

    const std::string& currentDeviceName() const override;
    std::vector<std::string> getAudioList() const override;
    void restart() override;
    bool config(const std::string& deviceName) override;
    bool init() override;
    void render(uint32_t dt, bool silenceFlag) override;
    void discardQueuedAudio() override;
    size_t queuedAudioByteCount() const override;
    bool trimQueuedAudioTailBytes(size_t bytes) override;
    void setVolume(float volume) override;
    float getVolume() const override;
};
