#include "GeraNESApp/ModAudio.h"

#include <algorithm>
#include <cmath>

#include "logger/logger.h"

#include <stb_vorbis.c>

namespace
{
template<typename TClip>
void rescaleClipPhase(TClip& clip, uint32_t newDenominator)
{
    newDenominator = std::max<uint32_t>(1, newDenominator);
    if(clip.phaseDenominator == newDenominator) {
        return;
    }

    const uint32_t oldDenominator = std::max<uint32_t>(1, clip.phaseDenominator);
    const uint64_t scaledRemainder =
        (static_cast<uint64_t>(clip.positionRemainder) * static_cast<uint64_t>(newDenominator) + oldDenominator / 2u)
        / static_cast<uint64_t>(oldDenominator);

    clip.positionFrames += scaledRemainder / newDenominator;
    clip.positionRemainder = static_cast<uint32_t>(scaledRemainder % newDenominator);
    clip.phaseDenominator = newDenominator;
}
}

ModAudioRuntime::ModAudioRuntime(AssetReader assetReader)
    : m_assetReader(std::move(assetReader))
{
}

void ModAudioRuntime::setConfig(const ModAudioConfig& config)
{
    std::scoped_lock lock(m_mutex);
    m_config = config;
    m_clipCache.clear();
    resetRuntimeUnlocked();
}

void ModAudioRuntime::clearConfig()
{
    std::scoped_lock lock(m_mutex);
    m_config = {};
    m_clipCache.clear();
    resetRuntimeUnlocked();
}

bool ModAudioRuntime::hasConfig() const
{
    std::scoped_lock lock(m_mutex);
    return !m_config.bgmFilesById.empty() || !m_config.sfxFilesById.empty();
}

bool ModAudioRuntime::handlesCpuWrite(uint16_t addr) const
{
    return registerIndexForWrite(addr) >= 0;
}

bool ModAudioRuntime::handleCpuWrite(uint16_t addr, uint8_t value)
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

std::optional<uint8_t> ModAudioRuntime::handleCpuRead(uint16_t addr) const
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

bool ModAudioRuntime::preloadClip(const std::string& assetPath)
{
    std::scoped_lock lock(m_mutex);
    const auto clip = loadClip(assetPath);
    return clip && !clip->samples.empty();
}

bool ModAudioRuntime::preloadClipData(const std::string& assetPath, const std::vector<uint8_t>& data)
{
    std::scoped_lock lock(m_mutex);

    const auto cached = m_clipCache.find(assetPath);
    if(cached != m_clipCache.end()) {
        cached->second.lastUsedFrame = m_cacheFrame;
        return cached->second.clip && !cached->second.clip->samples.empty();
    }

    const bool pinned = shouldPinClip(assetPath);
    const auto clip = decodeClipData(assetPath, data);
    storeDecodedClip(assetPath, clip, pinned);
    return clip && !clip->samples.empty();
}

void ModAudioRuntime::setCacheFrame(uint32_t frameCount)
{
    std::scoped_lock lock(m_mutex);
    m_cacheFrame = frameCount;
}

void ModAudioRuntime::rebaseCacheFrame(uint32_t frameCount)
{
    std::scoped_lock lock(m_mutex);
    m_cacheFrame = frameCount;
    for(auto& [_, entry] : m_clipCache) {
        entry.lastUsedFrame = frameCount;
    }
}

void ModAudioRuntime::evictUnusedDynamicClips(uint32_t maxUnusedFrames)
{
    std::scoped_lock lock(m_mutex);

    auto clipIsActive = [&](const std::shared_ptr<const DecodedClip>& clip) {
        if(!clip) {
            return false;
        }
        if(m_bgm && m_bgm->clip == clip && !m_bgm->finished) {
            return true;
        }
        for(const ActiveClip& active : m_sfx) {
            if(active.clip == clip && !active.finished) {
                return true;
            }
        }
        return false;
    };

    for(auto it = m_clipCache.begin(); it != m_clipCache.end(); ) {
        const ClipCacheEntry& entry = it->second;
        if(entry.pinned || clipIsActive(entry.clip) || m_cacheFrame - entry.lastUsedFrame <= maxUnusedFrames) {
            ++it;
        } else {
            it = m_clipCache.erase(it);
        }
    }
}

void ModAudioRuntime::resetRuntime()
{
    std::scoped_lock lock(m_mutex);
    resetRuntimeUnlocked();
}

void ModAudioRuntime::resetRuntimeUnlocked()
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

void ModAudioRuntime::onOutputSampleRateChanged(int sampleRate)
{
    std::scoped_lock lock(m_mutex);
    const uint32_t newSampleRate = static_cast<uint32_t>(std::max(1, sampleRate));
    if(static_cast<uint32_t>(m_outputSampleRate) == newSampleRate) {
        return;
    }

    m_outputSampleRate = static_cast<int>(newSampleRate);
    if(m_bgm) {
        rescaleClipPhase(*m_bgm, newSampleRate);
    }
    for(ActiveClip& clip : m_sfx) {
        rescaleClipPhase(clip, newSampleRate);
    }
}

float ModAudioRuntime::mixMonoSample(int sampleRate)
{
    const IAudioOutput::StereoSample mixed = mixStereoFrame(sampleRate);
    return std::clamp((mixed.left + mixed.right) * 0.5f, -1.0f, 1.0f);
}

IAudioOutput::StereoSample ModAudioRuntime::mixStereoFrame(int sampleRate)
{
    std::scoped_lock lock(m_mutex);
    const uint32_t newSampleRate = static_cast<uint32_t>(std::max(1, sampleRate));
    if(static_cast<uint32_t>(m_outputSampleRate) != newSampleRate) {
        m_outputSampleRate = static_cast<int>(newSampleRate);
        if(m_bgm) {
            rescaleClipPhase(*m_bgm, newSampleRate);
        }
        for(ActiveClip& clip : m_sfx) {
            rescaleClipPhase(clip, newSampleRate);
        }
    }

    IAudioOutput::StereoSample mixed;
    if(m_bgm && !m_bgmPaused) {
        const IAudioOutput::StereoSample bgmSample = mixClipSample(*m_bgm, m_bgmVolume);
        mixed.left += bgmSample.left;
        mixed.right += bgmSample.right;
        if(m_bgm->finished) {
            m_bgm.reset();
            m_currentBgmTrackId = -1;
        }
    }

    for(ActiveClip& clip : m_sfx) {
        const IAudioOutput::StereoSample sfxSample = mixClipSample(clip, m_sfxVolume);
        mixed.left += sfxSample.left;
        mixed.right += sfxSample.right;
    }
    m_sfx.erase(
        std::remove_if(m_sfx.begin(), m_sfx.end(), [](const ActiveClip& clip) { return clip.finished; }),
        m_sfx.end());

    mixed.left = std::clamp(mixed.left, -1.0f, 1.0f);
    mixed.right = std::clamp(mixed.right, -1.0f, 1.0f);
    return mixed;
}

int ModAudioRuntime::preferredOutputChannels() const
{
    std::scoped_lock lock(m_mutex);
    return (!m_config.bgmFilesById.empty() || !m_config.sfxFilesById.empty()) ? 2 : 1;
}

std::shared_ptr<const ModAudioRuntime::DecodedClip> ModAudioRuntime::loadClip(const std::string& assetPath)
{
    const auto cached = m_clipCache.find(assetPath);
    if(cached != m_clipCache.end()) {
        cached->second.lastUsedFrame = m_cacheFrame;
        return cached->second.clip;
    }

    const std::optional<std::vector<uint8_t>> assetData = m_assetReader ? m_assetReader(assetPath) : std::nullopt;
    if(!assetData.has_value() || assetData->empty()) {
        Logger::instance().log("HD audio asset missing: " + assetPath, Logger::Type::WARNING);
        m_clipCache[assetPath] = { nullptr, m_cacheFrame, shouldPinClip(assetPath) };
        return nullptr;
    }

    const bool pinned = shouldPinClip(assetPath);
    const auto clip = decodeClipData(assetPath, *assetData);
    return storeDecodedClip(assetPath, clip, pinned);
}

std::shared_ptr<const ModAudioRuntime::DecodedClip> ModAudioRuntime::storeDecodedClip(
    const std::string& assetPath,
    std::shared_ptr<const DecodedClip> clip,
    bool pinned)
{
    m_clipCache[assetPath] = { clip, m_cacheFrame, pinned };
    return clip;
}

std::shared_ptr<const ModAudioRuntime::DecodedClip> ModAudioRuntime::decodeClipData(
    const std::string& assetPath,
    const std::vector<uint8_t>& assetData)
{
    int error = 0;
    stb_vorbis* vorbis = stb_vorbis_open_memory(
        assetData.data(),
        static_cast<int>(assetData.size()),
        &error,
        nullptr);

    if(vorbis == nullptr) {
        Logger::instance().log(
            "Failed to decode HD audio asset: " + assetPath + " (stb_vorbis error " + std::to_string(error) + ")",
            Logger::Type::WARNING);
        return nullptr;
    }

    const stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    const int channels = info.channels;
    const int sampleRate = static_cast<int>(info.sample_rate);
    const unsigned int totalFrames = stb_vorbis_stream_length_in_samples(vorbis);

    if(channels <= 0 || sampleRate <= 0) {
        Logger::instance().log("Failed to decode HD audio asset: " + assetPath, Logger::Type::WARNING);
        stb_vorbis_close(vorbis);
        return nullptr;
    }

    auto clip = std::make_shared<DecodedClip>();
    clip->sampleRate = sampleRate;
    clip->channelCount = 1;
    if(totalFrames > 0) {
        clip->samples.reserve(
            static_cast<size_t>(totalFrames) * 2u);
    }

    constexpr int DecodeChunkFrames = 1024;
    std::vector<float> interleavedBuffer(static_cast<size_t>(DecodeChunkFrames) * static_cast<size_t>(channels));

    while(true) {
        const int framesDecoded = stb_vorbis_get_samples_float_interleaved(
            vorbis,
            channels,
            interleavedBuffer.data(),
            static_cast<int>(interleavedBuffer.size()));
        if(framesDecoded <= 0) {
            break;
        }

        clip->channelCount = 2;
        const size_t baseIndex = clip->samples.size();
        clip->samples.resize(baseIndex + static_cast<size_t>(framesDecoded) * 2u);
        for(int frame = 0; frame < framesDecoded; ++frame) {
            const size_t inputBase = static_cast<size_t>(frame * channels);
            float left = 0.0f;
            float right = 0.0f;
            if(channels == 1) {
                left = interleavedBuffer[inputBase];
                right = left;
            } else {
                left = interleavedBuffer[inputBase];
                right = interleavedBuffer[inputBase + 1];
                for(int ch = 2; ch < channels; ++ch) {
                    const float extra = interleavedBuffer[inputBase + static_cast<size_t>(ch)];
                    left += extra;
                    right += extra;
                }
                if(channels > 2) {
                    const float extraScale = 1.0f / static_cast<float>(channels - 1);
                    left *= extraScale;
                    right *= extraScale;
                }
            }
            const size_t outputBase = baseIndex + static_cast<size_t>(frame) * 2u;
            clip->samples[outputBase] = std::clamp(left, -1.0f, 1.0f);
            clip->samples[outputBase + 1] = std::clamp(right, -1.0f, 1.0f);
        }
    }

    stb_vorbis_close(vorbis);

    if(clip->samples.empty()) {
        Logger::instance().log("Failed to decode HD audio asset: " + assetPath, Logger::Type::WARNING);
        return nullptr;
    }

    return clip;
}

bool ModAudioRuntime::shouldPinClip(const std::string& assetPath) const
{
    return std::any_of(
        m_config.sfxFilesById.begin(),
        m_config.sfxFilesById.end(),
        [&](const auto& entry) { return entry.second == assetPath; });
}

bool ModAudioRuntime::playBgmTrack(int trackId)
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
    if(!clip || clip->samples.empty()) {
        return false;
    }

    auto bgm = std::make_unique<ActiveClip>();
    bgm->clip = clip;
    bgm->phaseDenominator = static_cast<uint32_t>(std::max(1, m_outputSampleRate));
    bgm->loopEnabled = (m_playbackOptions & 0x01) != 0;
    bgm->loopPosition = std::min<uint32_t>(
        it->second.loopPosition,
        static_cast<uint32_t>(clip->samples.size() / static_cast<size_t>(std::max(1, clip->channelCount))));
    m_bgm = std::move(bgm);
    m_currentBgmTrackId = trackId;
    m_bgmPaused = false;
    return true;
}

bool ModAudioRuntime::playSfxTrack(uint8_t sfxNumber)
{
    const auto it = m_config.sfxFilesById.find(static_cast<int>(m_album) * 256 + sfxNumber);
    if(it == m_config.sfxFilesById.end()) {
        return false;
    }

    const auto clip = loadClip(it->second);
    if(!clip || clip->samples.empty()) {
        return false;
    }

    ActiveClip sfx;
    sfx.clip = clip;
    sfx.phaseDenominator = static_cast<uint32_t>(std::max(1, m_outputSampleRate));
    m_sfx.push_back(std::move(sfx));
    return true;
}



IAudioOutput::StereoSample ModAudioRuntime::mixClipSample(ActiveClip& clip, uint8_t volume) const
{
    if(clip.finished || !clip.clip || clip.clip->samples.empty()) {
        clip.finished = true;
        return {};
    }

    const std::vector<float>& samples = clip.clip->samples;
    const size_t channelCount = static_cast<size_t>(std::max(1, clip.clip->channelCount));
    const size_t sampleCount = samples.size() / channelCount;
    if(sampleCount == 0) {
        clip.finished = true;
        return {};
    }

    const uint32_t outputRate = static_cast<uint32_t>(std::max(1, m_outputSampleRate));
    if(clip.phaseDenominator != outputRate) {
        rescaleClipPhase(clip, outputRate);
    }

    auto wrapLoopPosition = [&]() {
        if(!clip.loopEnabled || sampleCount <= 1) {
            clip.finished = true;
            return;
        }

        const size_t loopStart = std::min<size_t>(clip.loopPosition, sampleCount - 1);
        if(loopStart >= sampleCount - 1) {
            clip.finished = true;
            return;
        }

        const uint64_t loopStartNumerator = static_cast<uint64_t>(loopStart) * outputRate;
        const uint64_t loopLengthNumerator = static_cast<uint64_t>(sampleCount - loopStart) * outputRate;
        if(loopLengthNumerator == 0) {
            clip.finished = true;
            return;
        }

        const uint64_t positionNumerator =
            static_cast<uint64_t>(clip.positionFrames) * outputRate + static_cast<uint64_t>(clip.positionRemainder);
        const uint64_t wrappedNumerator =
            loopStartNumerator + (positionNumerator - loopStartNumerator) % loopLengthNumerator;

        clip.positionFrames = wrappedNumerator / outputRate;
        clip.positionRemainder = static_cast<uint32_t>(wrappedNumerator % outputRate);
    };

    if(clip.positionFrames >= sampleCount) {
        wrapLoopPosition();
        if(clip.finished) {
            return {};
        }
    }

    const size_t sampleIndex = std::min<size_t>(static_cast<size_t>(clip.positionFrames), sampleCount - 1);
    const size_t nextIndex = std::min<size_t>(sampleIndex + 1, sampleCount - 1);
    const float frac = static_cast<float>(clip.positionRemainder) / static_cast<float>(outputRate);
    const size_t currentBase = sampleIndex * channelCount;
    const size_t nextBase = nextIndex * channelCount;
    const float currentLeft = samples[currentBase];
    const float nextLeft = samples[nextBase];
    const float left = currentLeft + (nextLeft - currentLeft) * frac;
    const float currentRight = samples[currentBase + (channelCount > 1 ? 1u : 0u)];
    const float nextRight = samples[nextBase + (channelCount > 1 ? 1u : 0u)];
    const float right = currentRight + (nextRight - currentRight) * frac;

    const uint64_t advancedRemainder =
        static_cast<uint64_t>(clip.positionRemainder) + static_cast<uint64_t>(clip.clip->sampleRate);
    clip.positionFrames += advancedRemainder / outputRate;
    clip.positionRemainder = static_cast<uint32_t>(advancedRemainder % outputRate);

    if(clip.positionFrames >= sampleCount && clip.loopEnabled) {
        wrapLoopPosition();
    } else if(clip.positionFrames >= sampleCount) {
        clip.finished = true;
    }

    const float gain = static_cast<float>(volume) / 255.0f;
    return { left * gain, right * gain };
}

int ModAudioRuntime::registerIndexForWrite(uint16_t addr) const
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
