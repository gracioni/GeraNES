#include "GeraNESApp/SDLAudioOutput.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

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

IAudioOutput::AudioFormatOptions SDLAudioOutput::getAudioFormatOptions(const std::string& deviceName) const
{
    AudioFormatOptions options;
    auto addUnique = [](std::vector<int>& values, int value) {
        if(value <= 0) return;
        if(std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
    };

    std::string selectedDeviceName = deviceName == "default" ? "" : deviceName;
    if(selectedDeviceName.empty()) {
        char* name = nullptr;
        SDL_AudioSpec defaultSpec{};
        if(SDL_GetDefaultAudioInfo(&name, &defaultSpec, 0) == 0) {
            addUnique(options.sampleRates, defaultSpec.freq);
            addUnique(options.sampleSizes, defaultSpec.format & 0xFF);
        }
        if(name) SDL_free(name);
    } else {
        auto deviceList = getAudioList();
        for(int i = 0; i < static_cast<int>(deviceList.size()); ++i) {
            if(selectedDeviceName == deviceList[i]) {
                SDL_AudioSpec deviceSpec{};
                if(SDL_GetAudioDeviceSpec(i, 0, &deviceSpec) == 0) {
                    addUnique(options.sampleRates, deviceSpec.freq);
                    addUnique(options.sampleSizes, deviceSpec.format & 0xFF);
                }
                break;
            }
        }
    }

    for(int rate : {22050, 32000, 44100, 48000, 88200, 96000}) addUnique(options.sampleRates, rate);
    for(int size : {8, 16, 32}) addUnique(options.sampleSizes, size);

    std::sort(options.sampleRates.begin(), options.sampleRates.end());
    std::sort(options.sampleSizes.begin(), options.sampleSizes.end());

    return options;
}

void SDLAudioOutput::restart()
{
    config(m_currentDeviceName, spec.freq, spec.format & 0xFF);
}

bool SDLAudioOutput::config(const std::string& deviceName)
{
    return config(deviceName, 0, 0);
}

bool SDLAudioOutput::config(const std::string& deviceName, int requestedSampleRate, int requestedSampleSize)
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
    preferredSpec.freq = 48000;
    preferredSpec.format = AUDIO_S16;
    preferredSpec.channels = static_cast<Uint8>(desiredOutputChannels());

    if(deviceIndex >= 0 && SDL_GetAudioDeviceSpec(deviceIndex, 0, &preferredSpec) != 0) {
        Logger::instance().log(std::string("SDL_GetAudioDeviceSpec error: ") + SDL_GetError() + ". Using fallback format.", Logger::Type::WARNING);
        preferredSpec.freq = 48000;
        preferredSpec.format = AUDIO_S16;
        preferredSpec.channels = static_cast<Uint8>(desiredOutputChannels());
    }

    int newSampleRate = requestedSampleRate > 0 ? requestedSampleRate : preferredSpec.freq;
    if(newSampleRate <= 0) {
        newSampleRate = 48000;
    }
    int newSampleSize = 8;
    if(requestedSampleSize > 0) {
        newSampleSize = requestedSampleSize;
    } else {
#ifdef __ANDROID__
        newSampleSize = 16;
#else
        switch(preferredSpec.format) {
            case AUDIO_U8: newSampleSize = 8; break;
            case AUDIO_S16: newSampleSize = 16; break;
            case AUDIO_S32: newSampleSize = 32; break;
        }
#endif
    }

    spec.freq = newSampleRate;
    spec.channels = static_cast<Uint8>(desiredOutputChannels());
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
    Logger::instance().log(std::string("Channels: ") + std::to_string(spec.channels), Logger::Type::INFO);

    m_sampleFormatScale = std::exp2(static_cast<float>(sampleSize()));

    SDL_PauseAudioDevice(m_device, 0);
    initChannels(spec.freq);

    return true;
}

int SDLAudioOutput::currentSampleRate() const
{
    return sampleRate();
}

int SDLAudioOutput::currentSampleSize() const
{
    return sampleSize();
}

bool SDLAudioOutput::init()
{
    AudioOutputBase::init();
    if(m_device == 0) config("");
    return true;
}

void SDLAudioOutput::render(uint32_t dt)
{
    if(m_device == 0){
        turnOff();
        return;
    }

    sampleAcc += static_cast<double>(dt) * static_cast<double>(sampleRate()) / playbackSpeed();

    const int bitsPerSample = sampleSize();
    const int bytesPerSample = bitsPerSample / 8;
    const int outputChannels = std::max(1, static_cast<int>(spec.channels));
    const int bytesPerFrame = bytesPerSample * outputChannels;
    float vol = std::pow(m_volume, 2.0f);
    while(sampleAcc >= 1000.0)
    {
        const auto appendSample = [&](float value) {
            if(bitsPerSample == 8) {
                int temp = static_cast<int>((value / 2.0f + 0.5f) * 255.0f);
                if(temp < 0) temp = 0;
                else if(temp > 255) temp = 255;
                m_buffer.push_back(static_cast<char>(temp));
            } else {
                uint64_t temp = static_cast<uint64_t>(value / 2.0f * m_sampleFormatScale);
                for(int i = 0; i < bytesPerSample; i++ ){
                    m_buffer.push_back(static_cast<char>(temp & 0xFF));
                    temp >>= 8;
                }
            }
        };

        if(outputChannels <= 1) {
            const float mono = mixMono() * vol;
            captureMixedSample(mono);
            appendSample(mono);
        } else {
            const StereoSample mixedFrame = mixFrame();
            const float left = mixedFrame.left * vol;
            const float right = mixedFrame.right * vol;
            captureMixedSample((left + right) * 0.5f);
            appendSample(left);
            appendSample(right);
        }

        sampleAcc -= 1000.0;
    }

    bool playFlag = SDL_GetQueuedAudioSize(m_device) != 0;
    if(playFlag || m_buffer.size() >= static_cast<size_t>(sampleRate() * bytesPerFrame * BUFFER_TIME))
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
