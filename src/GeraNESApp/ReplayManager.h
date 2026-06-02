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
        uint32_t loadedFrameCount = 0;
        bool playing = false;
        bool pendingStopAtEnd = false;
        bool loadedReplayActive = false;
    };

private:
    mutable std::mutex m_mutex;
    ReplayState m_state;

public:
    ReplayState snapshot() const;

    void clear();
    void stopPlayback();

    void beginRecording(std::string romName,
                        std::string romCrc,
                        const InputTopology& topology);
    void beginRecordingFromLoadedReplay(uint32_t continueFromFrame);
    void appendRecordedFrame(const InputFrame& frame);
    void finalizeRecordingAsPlayback(const fs::path& path);
    void setLoadedReplay(const fs::path& path, ReplayData data);
    uint32_t inputCount() const;
    uint32_t clampedFrame(uint32_t frame) const;
    InputTopology inputTopology() const;
    void setCursorFrame(uint32_t frame);
    void setCursorState(uint32_t frame);
    void beginPlayback();
    bool syncRuntimeFrame(uint32_t emuFrame);

    bool saveToFile(const fs::path& path, std::string& error) const;
    bool loadFromFile(const fs::path& path, std::string& error);

    bool isRecording() const;
    bool isPlayback() const;
    bool hasLoadedReplay() const;
};
