#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "GeraNESApp/IEmulationHost.h"

namespace fs = std::filesystem;

class ReplayManager
{
public:
    struct ReplayData {
        std::string romName;
        std::string romCrc;
        IEmulationHost::InputTopologySnapshot inputTopology = {};
        std::optional<InputFrame> bootstrapFrame;
        std::vector<InputFrame> frames;
    };

    enum class ReplayMode {
        None,
        Recording,
        Playback
    };

    struct ReplayState {
        ReplayMode mode = ReplayMode::None;
        fs::path filePath;
        ReplayData data;
        uint32_t cursorFrame = 0;
        std::optional<uint32_t> cursorCanonicalCrc32;
        uint32_t loadedFrameCount = 0;
        bool playing = false;
        bool pendingStopAtEnd = false;
        bool loadedReplayActive = false;
    };

private:
    struct RuntimeSnapshot {
        uint32_t frame = 0;
        std::vector<uint8_t> state;
    };

    static constexpr uint32_t kSnapshotIntervalFrames = 10000u;

    mutable std::mutex m_mutex;
    ReplayState m_state;
    std::vector<RuntimeSnapshot> m_runtimeSnapshots;

public:
    ReplayState snapshot() const;

    void clear();
    void stopPlayback();

    void beginRecording(std::string romName,
                        std::string romCrc,
                        const IEmulationHost::InputTopologySnapshot& topology);
    void appendRecordedFrame(const InputFrame& frame);
    void setBootstrapFrame(const InputFrame& frame);
    void finalizeRecordingAsPlayback(const fs::path& path);
    void setLoadedReplay(const fs::path& path, ReplayData data);
    uint32_t inputCount() const;
    uint32_t clampedFrame(uint32_t frame) const;
    IEmulationHost::InputTopologySnapshot inputTopology() const;
    std::optional<InputFrame> playbackFrameForFrame(uint32_t frame) const;
    void setCursorFrame(uint32_t frame);
    void setCursorState(uint32_t frame, std::optional<uint32_t> canonicalCrc32);
    void beginPlayback();
    void markPlaybackReachedEnd();
    void notePlaybackFrame(uint32_t frame);
    bool syncRuntimeFrame(uint32_t emuFrame);
    bool shouldCaptureRuntimeSnapshot(uint32_t frame) const;
    void storeRuntimeSnapshot(uint32_t frame, std::vector<uint8_t> state);
    std::optional<std::pair<uint32_t, std::vector<uint8_t>>> runtimeSnapshotAtOrBefore(uint32_t frame) const;
    static constexpr uint32_t snapshotIntervalFrames() { return kSnapshotIntervalFrames; }

    bool saveToFile(const fs::path& path, std::string& error) const;
    bool loadFromFile(const fs::path& path, std::string& error);

    bool isRecording() const;
    bool isPlayback() const;
    bool hasLoadedReplay() const;
};
