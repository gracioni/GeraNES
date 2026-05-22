#include "GeraNESApp/HdPackAudio.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "logger/logger.h"

#include <stb_vorbis.c>

HdPackAudioRuntime::HdPackAudioRuntime(AssetReader assetReader)
    : m_assetReader(std::move(assetReader))
{
}

void HdPackAudioRuntime::setConfig(const HdPackAudioConfig& config)
{
    std::scoped_lock lock(m_mutex);
    m_config = config;
    m_clipCache.clear();
    resetRuntimeUnlocked();
}

void HdPackAudioRuntime::clearConfig()
{
    std::scoped_lock lock(m_mutex);
    m_config = {};
    m_clipCache.clear();
    resetRuntimeUnlocked();
}

bool HdPackAudioRuntime::hasConfig() const
{
    std::scoped_lock lock(m_mutex);
    return !m_config.bgmFilesById.empty() || !m_config.sfxFilesById.empty();
}

bool HdPackAudioRuntime::handlesCpuWrite(uint16_t addr) const
{
    return registerIndexForWrite(addr) >= 0;
}

bool HdPackAudioRuntime::handleCpuWrite(uint16_t addr, uint8_t value)
{
    const int regNumber = registerIndexForWrite(addr);
    if(regNumber < 0) {
        return false;
    }

    std::scoped_lock lock(m_mutex);
    m_trackError = false;
    switch(regNumber) {
        case 0:
            m_playbackOptions = value;
            if(m_bgm) {
                m_bgm->loopEnabled = (m_playbackOptions & 0x01) != 0;
            }
            break;
        case 1:
            m_bgmPaused = (value & 0x01) != 0;
            if(value & 0x02) {
                m_bgm.reset();
                m_currentBgmTrackId = -1;
            }
            if(value & 0x04) {
                m_sfx.clear();
            }
            break;
        case 2:
            m_bgmVolume = value;
            break;
        case 3:
            m_sfxVolume = value;
            break;
        case 4:
            m_album = value;
            break;
        case 5:
            m_trackError = !playBgmTrack(static_cast<int>(m_album) * 256 + value);
            break;
        case 6:
            m_trackError = !playSfxTrack(value);
            break;
        default:
            break;
    }

    return true;
}

std::optional<uint8_t> HdPackAudioRuntime::handleCpuRead(uint16_t addr) const
{
    std::scoped_lock lock(m_mutex);
    if(m_config.bgmFilesById.empty() && m_config.sfxFilesById.empty()) {
        return std::nullopt;
    }

    if(m_config.alternateRegisterRange) {
        switch(addr) {
            case 0x4018:
                return static_cast<uint8_t>(
                    ((!m_bgmPaused && m_bgm) ? 1 : 0) |
                    (!m_sfx.empty() ? 2 : 0) |
                    (m_trackError ? 4 : 0));
            case 0x4019:
                return static_cast<uint8_t>(1);
            default:
                return std::nullopt;
        }
    }

    switch(addr) {
        case 0x4100:
            return static_cast<uint8_t>(
                ((!m_bgmPaused && m_bgm) ? 1 : 0) |
                (!m_sfx.empty() ? 2 : 0) |
                (m_trackError ? 4 : 0));
        case 0x4101:
            return static_cast<uint8_t>(1);
        case 0x4102:
            return static_cast<uint8_t>('N');
        case 0x4103:
            return static_cast<uint8_t>('E');
        case 0x4104:
            return static_cast<uint8_t>('A');
        default:
            return std::nullopt;
    }
}

void HdPackAudioRuntime::resetRuntime()
{
    std::scoped_lock lock(m_mutex);
    resetRuntimeUnlocked();
}

void HdPackAudioRuntime::resetRuntimeUnlocked()
{
    m_album = 0;
    m_playbackOptions = 0;
    m_bgmVolume = 128;
    m_sfxVolume = 128;
    m_bgmPaused = false;
    m_trackError = false;
    m_bgm.reset();
    m_sfx.clear();
    m_currentBgmTrackId = -1;
}

void HdPackAudioRuntime::onOutputSampleRateChanged(int sampleRate)
{
    std::scoped_lock lock(m_mutex);
    m_outputSampleRate = std::max(1, sampleRate);
}

float HdPackAudioRuntime::mixAudioSample(int sampleRate)
{
    std::scoped_lock lock(m_mutex);
    m_outputSampleRate = std::max(1, sampleRate);

    float mixed = 0.0f;
    if(m_bgm && !m_bgmPaused) {
        mixed += mixClipSample(*m_bgm, m_bgmVolume);
        if(m_bgm->finished) {
            m_bgm.reset();
            m_currentBgmTrackId = -1;
        }
    }

    for(ActiveClip& clip : m_sfx) {
        mixed += mixClipSample(clip, m_sfxVolume);
    }
    m_sfx.erase(
        std::remove_if(m_sfx.begin(), m_sfx.end(), [](const ActiveClip& clip) { return clip.finished; }),
        m_sfx.end());

    return std::clamp(mixed, -1.0f, 1.0f);
}

std::shared_ptr<const HdPackAudioRuntime::DecodedClip> HdPackAudioRuntime::loadClip(const std::string& assetPath)
{
    const auto cached = m_clipCache.find(assetPath);
    if(cached != m_clipCache.end()) {
        return cached->second;
    }

    const std::optional<std::vector<uint8_t>> assetData = m_assetReader ? m_assetReader(assetPath) : std::nullopt;
    if(!assetData.has_value() || assetData->empty()) {
        Logger::instance().log("HD audio asset missing: " + assetPath, Logger::Type::WARNING);
        m_clipCache[assetPath] = nullptr;
        return nullptr;
    }

    int channels = 0;
    int sampleRate = 0;
    short* decoded = nullptr;
    const int sampleFrames = stb_vorbis_decode_memory(
        assetData->data(),
        static_cast<int>(assetData->size()),
        &channels,
        &sampleRate,
        &decoded);

    if(sampleFrames <= 0 || decoded == nullptr || channels <= 0 || sampleRate <= 0) {
        Logger::instance().log("Failed to decode HD audio asset: " + assetPath, Logger::Type::WARNING);
        if(decoded) {
            std::free(decoded);
        }
        m_clipCache[assetPath] = nullptr;
        return nullptr;
    }

    auto clip = std::make_shared<DecodedClip>();
    clip->sampleRate = sampleRate;
    clip->monoSamples.resize(static_cast<size_t>(sampleFrames));
    for(int frame = 0; frame < sampleFrames; ++frame) {
        int64_t acc = 0;
        for(int ch = 0; ch < channels; ++ch) {
            acc += decoded[frame * channels + ch];
        }
        const float mono = static_cast<float>(acc / static_cast<double>(channels)) / 32768.0f;
        clip->monoSamples[static_cast<size_t>(frame)] = mono;
    }

    std::free(decoded);
    m_clipCache[assetPath] = clip;
    return clip;
}

bool HdPackAudioRuntime::playBgmTrack(int trackId)
{
    if(m_bgm && !m_bgm->finished && m_currentBgmTrackId == trackId) {
        m_bgmPaused = false;
        return true;
    }

    const auto it = m_config.bgmFilesById.find(trackId);
    if(it == m_config.bgmFilesById.end()) {
        return false;
    }

    const auto clip = loadClip(it->second.assetPath);
    if(!clip || clip->monoSamples.empty()) {
        return false;
    }

    auto bgm = std::make_unique<ActiveClip>();
    bgm->clip = clip;
    bgm->loopEnabled = (m_playbackOptions & 0x01) != 0;
    bgm->loopPosition = std::min<uint32_t>(it->second.loopPosition, static_cast<uint32_t>(clip->monoSamples.size()));
    m_bgm = std::move(bgm);
    m_currentBgmTrackId = trackId;
    m_bgmPaused = false;
    return true;
}

bool HdPackAudioRuntime::playSfxTrack(uint8_t sfxNumber)
{
    const auto it = m_config.sfxFilesById.find(static_cast<int>(m_album) * 256 + sfxNumber);
    if(it == m_config.sfxFilesById.end()) {
        return false;
    }

    const auto clip = loadClip(it->second);
    if(!clip || clip->monoSamples.empty()) {
        return false;
    }

    ActiveClip sfx;
    sfx.clip = clip;
    m_sfx.push_back(std::move(sfx));
    return true;
}

float HdPackAudioRuntime::mixClipSample(ActiveClip& clip, uint8_t volume) const
{
    if(clip.finished || !clip.clip || clip.clip->monoSamples.empty()) {
        clip.finished = true;
        return 0.0f;
    }

    const std::vector<float>& samples = clip.clip->monoSamples;
    const size_t sampleCount = samples.size();
    if(sampleCount == 0) {
        clip.finished = true;
        return 0.0f;
    }

    auto wrapLoopPosition = [&](double position) {
        if(!clip.loopEnabled || sampleCount <= 1) {
            clip.finished = true;
            return position;
        }

        const size_t loopStart = std::min<size_t>(clip.loopPosition, sampleCount - 1);
        if(loopStart >= sampleCount - 1) {
            clip.finished = true;
            return position;
        }

        const double loopLength = static_cast<double>(sampleCount - loopStart);
        if(loopLength <= 0.0) {
            clip.finished = true;
            return position;
        }

        position = static_cast<double>(loopStart) + std::fmod(position - static_cast<double>(loopStart), loopLength);
        if(position < static_cast<double>(loopStart)) {
            position += loopLength;
        }
        return position;
    };

    if(clip.position >= static_cast<double>(sampleCount)) {
        clip.position = wrapLoopPosition(clip.position);
        if(clip.finished) {
            return 0.0f;
        }
    }

    const size_t sampleIndex = std::min<size_t>(static_cast<size_t>(clip.position), sampleCount - 1);
    const size_t nextIndex = std::min<size_t>(sampleIndex + 1, sampleCount - 1);
    const float frac = static_cast<float>(clip.position - static_cast<double>(sampleIndex));
    const float current = samples[sampleIndex];
    const float next = samples[nextIndex];
    const float value = current + (next - current) * frac;

    const double step = static_cast<double>(clip.clip->sampleRate) / static_cast<double>(std::max(1, m_outputSampleRate));
    clip.position += step;
    if(clip.position >= static_cast<double>(sampleCount) && clip.loopEnabled) {
        clip.position = wrapLoopPosition(clip.position);
    } else if(clip.position >= static_cast<double>(sampleCount)) {
        clip.finished = true;
    }

    return value * (static_cast<float>(volume) / 255.0f);
}

int HdPackAudioRuntime::registerIndexForWrite(uint16_t addr) const
{
    if(!hasConfig()) {
        return -1;
    }

    if(m_config.alternateRegisterRange) {
        for(int i = 0; i < 7; ++i) {
            if(addr == static_cast<uint16_t>(0x3002 + i * 0x10)) {
                return i;
            }
        }
        return -1;
    }

    if(addr >= 0x4100 && addr <= 0x4106) {
        return static_cast<int>(addr - 0x4100);
    }

    return -1;
}
