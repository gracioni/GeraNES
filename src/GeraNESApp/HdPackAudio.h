#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "GeraNES/IAudioOutput.h"

struct HdPackAudioBgmTrack
{
    std::string assetPath;
    uint32_t loopPosition = 0;
};

struct HdPackAudioConfig
{
    bool alternateRegisterRange = false;
    std::unordered_map<int, HdPackAudioBgmTrack> bgmFilesById;
    std::unordered_map<int, std::string> sfxFilesById;
};

class HdPackAudioRuntime : public IAudioOutput::ExternalAudioMixer
{
public:
    using AssetReader = std::function<std::optional<std::vector<uint8_t>>(const std::string&)>;

    explicit HdPackAudioRuntime(AssetReader assetReader);

    void setConfig(const HdPackAudioConfig& config);
    void clearConfig();
    bool hasConfig() const;

    bool handlesCpuWrite(uint16_t addr) const;
    bool handleCpuWrite(uint16_t addr, uint8_t value);
    std::optional<uint8_t> handleCpuRead(uint16_t addr) const;
    bool preloadClip(const std::string& assetPath);
    void pinClip(const std::string& assetPath);
    void setCacheFrame(uint32_t frameCount);
    void rebaseCacheFrame(uint32_t frameCount);
    void evictUnusedDynamicClips(uint32_t maxUnusedFrames);

    void resetRuntime() override;
    void onOutputSampleRateChanged(int sampleRate) override;
    float mixAudioSample(int sampleRate) override;

private:
    struct DecodedClip
    {
        int sampleRate = 44100;
        std::vector<float> monoSamples;
    };

    struct ClipCacheEntry
    {
        std::shared_ptr<const DecodedClip> clip;
        uint32_t lastUsedFrame = 0;
        bool pinned = false;
    };

    struct ActiveClip
    {
        std::shared_ptr<const DecodedClip> clip;
        uint64_t positionFrames = 0;
        uint32_t positionRemainder = 0;
        uint32_t phaseDenominator = 1;
        bool loopEnabled = false;
        uint32_t loopPosition = 0;
        bool finished = false;
    };

    AssetReader m_assetReader;
    HdPackAudioConfig m_config;
    std::unordered_map<std::string, ClipCacheEntry> m_clipCache;

    std::unique_ptr<ActiveClip> m_bgm;
    std::vector<ActiveClip> m_sfx;

    uint8_t m_album = 0;
    uint8_t m_playbackOptions = 0;
    uint8_t m_bgmVolume = 128;
    uint8_t m_sfxVolume = 128;
    bool m_bgmPaused = false;
    bool m_trackError = false;
    int m_currentBgmTrackId = -1;
    int m_outputSampleRate = 44100;
    uint32_t m_cacheFrame = 0;

    std::shared_ptr<const DecodedClip> loadClip(const std::string& assetPath);
    bool playBgmTrack(int trackId);
    bool playSfxTrack(uint8_t sfxNumber);
    float mixClipSample(ActiveClip& clip, uint8_t volume) const;
    int registerIndexForWrite(uint16_t addr) const;
    void resetRuntimeUnlocked();

    mutable std::mutex m_mutex;
};
