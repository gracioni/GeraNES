#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "GeraNESApp/ReplayFile.h"

namespace fs = std::filesystem;

class ReplayManager
{
public:
    using ReplayData = ReplayFile::Data;
    static constexpr size_t kRuntimeSnapshotCapacity = 10u;

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

    mutable std::mutex m_mutex;
    ReplayState m_state;
    std::vector<RuntimeSnapshot> m_runtimeSnapshots;

    std::vector<uint32_t> scheduledRuntimeSnapshotFramesLocked() const;
    bool hasRuntimeSnapshotForFrameLocked(uint32_t frame) const;

public:
    ReplayState snapshot() const;

    void clear();
    void stopPlayback();

    void beginRecording(std::string romName,
                        std::string romCrc,
                        const InputTopology& topology);
    void beginRecordingFromLoadedReplay(uint32_t continueFromFrame);
    void appendRecordedFrame(const InputFrame& frame);
    void trimRecordedFramesAfter(uint32_t frame);
    void finalizeRecordingAsPlayback(const fs::path& path);
    void setLoadedReplay(const fs::path& path, ReplayData data);
    uint32_t inputCount() const;
    uint32_t clampedFrame(uint32_t frame) const;
    InputTopology inputTopology() const;
    std::optional<InputFrame> playbackFrameForFrame(uint32_t frame) const;
    void setCursorFrame(uint32_t frame);
    void setCursorState(uint32_t frame, std::optional<uint32_t> canonicalCrc32);
    void beginPlayback();
    void markPlaybackReachedEnd();
    void notePlaybackFrame(uint32_t frame);
    bool syncRuntimeFrame(uint32_t emuFrame);
    bool shouldCaptureRuntimeSnapshot(uint32_t frame) const;
    std::vector<uint32_t> pendingRuntimeSnapshotFramesInRange(uint32_t startFrameExclusive,
                                                              uint32_t endFrameInclusive) const;
    void storeRuntimeSnapshot(uint32_t frame, std::vector<uint8_t> state);
    std::optional<std::pair<uint32_t, std::vector<uint8_t>>> runtimeSnapshotAtOrBefore(uint32_t frame) const;

    bool saveToFile(const fs::path& path, std::string& error) const;
    bool loadFromFile(const fs::path& path, std::string& error);

    bool isRecording() const;
    bool isPlayback() const;
    bool hasLoadedReplay() const;
};
