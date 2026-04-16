#include "GeraNESApp/SDLAudioOutput.h"

#include <cmath>
#include <cstdint>
#include <string>

#include "logger/logger.h"

void SDLAudioOutput::clearBuffers()
{
    m_buffer.clear();
    sampleAcc = 0;

    AudioOutputBase::clearBuffers();
}

void SDLAudioOutput::turnOff()
{
    if(m_device != 0) {
        SDL_PauseAudioDevice(m_device, 1);
        SDL_CloseAudioDevice(m_device);
        m_device = 0;
        m_currentDeviceName = "";
    }
    clearBuffers();
}

SDLAudioOutput::SDLAudioOutput()
{
    SDL_Init(SDL_INIT_AUDIO);

    auto audioDevices = getAudioList();
    for(const auto& d : audioDevices) {
        Logger::instance().log(std::string("Audio device detected: ") + d, Logger::Type::INFO);
    }
}

SDLAudioOutput::~SDLAudioOutput()
{
    turnOff();
}

int SDLAudioOutput::sampleRate() const
{
    return spec.freq;
}

int SDLAudioOutput::sampleSize() const
{
    // SDL formats store the number of bits in the first byte.
    return spec.format & 0xFF;
}

const std::string& SDLAudioOutput::currentDeviceName() const
{
    return m_currentDeviceName;
}

std::vector<std::string> SDLAudioOutput::getAudioList() const
{
    std::vector<std::string> ret;

    int num = SDL_GetNumAudioDevices(0); // 1 input, 0 output
    for(int i = 0; i < num; i++) {
        const char* name = SDL_GetAudioDeviceName(i, 0); // 1 input, 0 output
        ret.push_back(name);
    }

    return ret;
}

void SDLAudioOutput::restart()
{
    config(m_currentDeviceName);
}

bool SDLAudioOutput::config(const std::string& deviceName)
{
    turnOff();

    Logger::instance().log("Initializing audio...", Logger::Type::INFO);

    std::string selectedDeviceName = deviceName;
    if(deviceName.empty()) {
        char* name = nullptr;
        SDL_AudioSpec dummySpec{};
        if(SDL_GetDefaultAudioInfo(&name, &dummySpec, 0) != 0) {
            Logger::instance().log(std::string("SDL_GetDefaultAudioInfo error: ") + SDL_GetError(), Logger::Type::WARNING);
            selectedDeviceName = "";
        } else {
            selectedDeviceName = name ? name : "";
        }
        if(name) SDL_free(name);
    }

    int deviceIndex = -1;
    auto deviceList = getAudioList();
    for(int i = 0; i < static_cast<int>(deviceList.size()); i++) {
        if(selectedDeviceName == deviceList[i]) {
            deviceIndex = i;
            break;
        }
    }

    if(!selectedDeviceName.empty() && deviceIndex < 0) {
        Logger::instance().log(std::string("Requested audio device not found: ") + selectedDeviceName + ". Falling back to SDL default.", Logger::Type::WARNING);
        selectedDeviceName = "";
    }

    SDL_AudioSpec preferredSpec{};
    preferredSpec.freq = 44100;
    preferredSpec.format = AUDIO_S16;
    preferredSpec.channels = 1;

    if(deviceIndex >= 0 && SDL_GetAudioDeviceSpec(deviceIndex, 0, &preferredSpec) != 0) {
        Logger::instance().log(std::string("SDL_GetAudioDeviceSpec error: ") + SDL_GetError() + ". Using fallback format.", Logger::Type::WARNING);
        preferredSpec.freq = 44100;
        preferredSpec.format = AUDIO_S16;
        preferredSpec.channels = 1;
    }

    int newSampleRate = preferredSpec.freq;
    int newSampleSize = 8;
    switch(preferredSpec.format) {
        case AUDIO_U8: newSampleSize = 8; break;
        case AUDIO_S16: newSampleSize = 16; break;
        case AUDIO_S32: newSampleSize = 32; break;
    }

    spec.freq = newSampleRate;
    spec.channels = 1;
    spec.samples = static_cast<uint16_t>(newSampleRate * BUFFER_TIME);
    spec.callback = nullptr;
    spec.userdata = this;

    switch(newSampleSize) {
        case 8:
            spec.format = AUDIO_U8;
            break;
        case 16:
            spec.format = AUDIO_S16;
            break;
        case 32:
            spec.format = AUDIO_S32;
            break;
    }

    SDL_AudioSpec obtained{};
    m_device = SDL_OpenAudioDevice(selectedDeviceName.empty() ? nullptr : selectedDeviceName.c_str(), 0, &spec, &obtained, 0);
    if(m_device == 0) {
        turnOff();
        Logger::instance().log(std::string("Audio init error: ") + SDL_GetError(), Logger::Type::ERROR);
        return false;
    }

    spec = obtained;
    m_currentDeviceName = selectedDeviceName.empty() ? "default" : selectedDeviceName;

    Logger::instance().log(std::string("Audio device: ") + m_currentDeviceName, Logger::Type::INFO);
    Logger::instance().log(std::string("Sample rate: ") + std::to_string(spec.freq), Logger::Type::INFO);
    Logger::instance().log(std::string("Sample size: ") + std::to_string(spec.format & 0xFF), Logger::Type::INFO);

    SDL_PauseAudioDevice(m_device, 0);
    initChannels(spec.freq);

    return true;
}

bool SDLAudioOutput::init()
{
    AudioOutputBase::init();
    if(m_device == 0) config("");
    return true;
}

void SDLAudioOutput::render(uint32_t dt, bool silenceFlag)
{
    if(m_device == 0){
        turnOff();
        return;
    }

    sampleAcc += dt * sampleRate();

    float vol = std::pow(m_volume, 2.0f);
    while(sampleAcc >= 1000)
    {
        float value = silenceFlag ? 0.0f : mix() * vol;
        captureMixedSample(value);

        if(sampleSize() == 8) {
            int temp = static_cast<int>((value / 2.0f + 0.5f) * 255.0f);
            if(temp < 0) temp = 0;
            else if(temp > 255) temp = 255;
            m_buffer.push_back(static_cast<char>(temp));
        }
        else {
            uint64_t temp = static_cast<uint64_t>(value / 2.0f * std::exp2(static_cast<float>(sampleSize())));
            for(int i = 0; i < sampleSize() / 8; i++ ){
                m_buffer.push_back(static_cast<char>(temp & 0xFF));
                temp >>= 8;
            }
        }

        sampleAcc -= 1000;
    }

    bool playFlag = SDL_GetQueuedAudioSize(m_device) != 0;
    if(playFlag || m_buffer.size() >= sampleRate() * (sampleSize() / 8) * BUFFER_TIME)
    {
        SDL_QueueAudio(m_device, static_cast<void*>(m_buffer.data()), static_cast<Uint32>(m_buffer.size()));
        m_buffer.clear();
    }
}

void SDLAudioOutput::discardQueuedAudio()
{
    m_buffer.clear();
    sampleAcc = 0;
    if(m_device != 0) {
        SDL_ClearQueuedAudio(m_device);
    }
}

void SDLAudioOutput::setVolume(float volume)
{
    m_volume = volume;
}

float SDLAudioOutput::getVolume() const
{
    return m_volume;
}
