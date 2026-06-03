#include "GeraNESApp/ReplaySession.h"

#include <algorithm>

namespace
{
uint32_t replayTimelineFrameCount(const std::vector<InputFrame>& frames)
{
    uint32_t maxFramePlusOne = 0u;
    for(const InputFrame& frame : frames) {
        maxFramePlusOne = std::max(maxFramePlusOne, frame.frame + 1u);
    }
    return maxFramePlusOne;
}

}

ReplaySession::ReplayState ReplaySession::snapshot() const
{
    std::scoped_lock lock(m_mutex);
    return m_state;
}

void ReplaySession::clear()
{
    std::scoped_lock lock(m_mutex);
    m_state = {};
}

void ReplaySession::stopPlayback()
{
    std::scoped_lock lock(m_mutex);
    m_state.playing = false;
    m_state.pendingStopAtEnd = false;
}

void ReplaySession::beginRecording(std::string romName,
                                   std::string romCrc,
                                   const InputTopology& topology)
{
    std::scoped_lock lock(m_mutex);
    m_state = {};
    m_state.mode = ReplayMode::Recording;
    m_state.data.romName = std::move(romName);
    m_state.data.romCrc = std::move(romCrc);
    m_state.data.inputTopology = topology;
    m_state.playing = true;
}

void ReplaySession::beginRecordingFromLoadedReplay(uint32_t continueFromFrame)
{
    std::scoped_lock lock(m_mutex);
    const uint32_t preservedFrameCount = std::min(
        continueFromFrame + 1u,
        replayTimelineFrameCount(m_state.data.frames));
    m_state.mode = ReplayMode::Recording;
    m_state.filePath.clear();
    m_state.data.frames.resize(preservedFrameCount);
    m_state.cursorFrame = preservedFrameCount;
    m_state.loadedFrameCount = replayTimelineFrameCount(m_state.data.frames);
    m_state.playing = true;
    m_state.pendingStopAtEnd = false;
    m_state.loadedReplayActive = false;
}

void ReplaySession::appendRecordedFrame(const InputFrame& frame)
{
    std::scoped_lock lock(m_mutex);
    if(frame.frame < m_state.data.frames.size()) {
        m_state.data.frames[static_cast<size_t>(frame.frame)] = frame;
    } else {
        m_state.data.frames.push_back(frame);
    }
    m_state.loadedFrameCount = replayTimelineFrameCount(m_state.data.frames);
    m_state.cursorFrame = m_state.loadedFrameCount;
}

void ReplaySession::finalizeRecordingAsPlayback(const fs::path& path)
{
    std::scoped_lock lock(m_mutex);
    m_state.filePath = path;
    m_state.mode = ReplayMode::Playback;
    m_state.loadedReplayActive = true;
    m_state.loadedFrameCount = replayTimelineFrameCount(m_state.data.frames);
    m_state.playing = false;
    m_state.pendingStopAtEnd = false;
}

void ReplaySession::setLoadedReplay(const fs::path& path, ReplayData data)
{
    std::scoped_lock lock(m_mutex);
    m_state = {};
    m_state.mode = ReplayMode::Playback;
    m_state.filePath = path;
    m_state.data = std::move(data);
    m_state.loadedReplayActive = true;
    m_state.loadedFrameCount = replayTimelineFrameCount(m_state.data.frames);
}

uint32_t ReplaySession::inputCount() const
{
    std::scoped_lock lock(m_mutex);
    return replayTimelineFrameCount(m_state.data.frames);
}

uint32_t ReplaySession::clampedFrame(uint32_t frame) const
{
    std::scoped_lock lock(m_mutex);
    return std::min(frame, replayTimelineFrameCount(m_state.data.frames));
}

InputTopology ReplaySession::inputTopology() const
{
    std::scoped_lock lock(m_mutex);
    return m_state.data.inputTopology;
}

void ReplaySession::setCursorFrame(uint32_t frame)
{
    std::scoped_lock lock(m_mutex);
    m_state.cursorFrame = frame;
}

void ReplaySession::setCursorState(uint32_t frame)
{
    std::scoped_lock lock(m_mutex);
    m_state.cursorFrame = frame;
}

void ReplaySession::beginPlayback()
{
    std::scoped_lock lock(m_mutex);
    m_state.pendingStopAtEnd = false;
    m_state.playing = true;
}

bool ReplaySession::syncRuntimeFrame(uint32_t emuFrame)
{
    std::scoped_lock lock(m_mutex);
    if(m_state.mode == ReplayMode::None) {
        return false;
    }

    if(m_state.mode == ReplayMode::Recording) {
        if(emuFrame < m_state.cursorFrame) {
            const uint32_t preservedFrameCount = std::min(
                emuFrame + 1u,
                replayTimelineFrameCount(m_state.data.frames));
            m_state.data.frames.resize(preservedFrameCount);
        }
        m_state.loadedFrameCount = replayTimelineFrameCount(m_state.data.frames);
        m_state.cursorFrame = std::min(emuFrame, m_state.loadedFrameCount);
        return false;
    }

    m_state.loadedFrameCount = replayTimelineFrameCount(m_state.data.frames);
    m_state.cursorFrame = std::min(emuFrame, m_state.loadedFrameCount);
    return m_state.pendingStopAtEnd || m_state.cursorFrame >= m_state.loadedFrameCount;
}

bool ReplaySession::saveToFile(const fs::path& path, std::string& error) const
{
    ReplayState snapshotState;
    {
        std::scoped_lock lock(m_mutex);
        snapshotState = m_state;
    }
    return ReplayFile::save(path, snapshotState.data, error);
}

bool ReplaySession::saveToBytes(std::vector<uint8_t>& bytes, std::string& error) const
{
    ReplayState snapshotState;
    {
        std::scoped_lock lock(m_mutex);
        snapshotState = m_state;
    }
    return ReplayFile::saveToBytes(snapshotState.data, bytes, error);
}

bool ReplaySession::loadFromFile(const fs::path& path, std::string& error)
{
    ReplayData data;
    if(!ReplayFile::load(path, data, error)) {
        return false;
    }
    setLoadedReplay(path, std::move(data));
    return true;
}

bool ReplaySession::isRecording() const
{
    std::scoped_lock lock(m_mutex);
    return m_state.mode == ReplayMode::Recording;
}

bool ReplaySession::isPlayback() const
{
    std::scoped_lock lock(m_mutex);
    return m_state.mode == ReplayMode::Playback;
}

bool ReplaySession::hasLoadedReplay() const
{
    std::scoped_lock lock(m_mutex);
    return m_state.loadedReplayActive;
}
