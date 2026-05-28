#include "GeraNESApp/ReplayManager.h"

#include <algorithm>

ReplayManager::ReplayState ReplayManager::snapshot() const
{
    std::scoped_lock lock(m_mutex);
    return m_state;
}

void ReplayManager::clear()
{
    std::scoped_lock lock(m_mutex);
    m_state = {};
    m_runtimeSnapshots.clear();
}

void ReplayManager::stopPlayback()
{
    std::scoped_lock lock(m_mutex);
    m_state.playing = false;
    m_state.pendingStopAtEnd = false;
}

void ReplayManager::beginRecording(std::string romName,
                                   std::string romCrc,
                                   const IEmulationHost::InputTopologySnapshot& topology)
{
    std::scoped_lock lock(m_mutex);
    m_state = {};
    m_runtimeSnapshots.clear();
    m_state.mode = ReplayMode::Recording;
    m_state.data.romName = std::move(romName);
    m_state.data.romCrc = std::move(romCrc);
    m_state.data.inputTopology = topology;
    m_state.playing = true;
}

void ReplayManager::appendRecordedFrame(const InputFrame& frame)
{
    std::scoped_lock lock(m_mutex);
    m_state.data.frames.push_back(frame);
    m_state.loadedFrameCount = static_cast<uint32_t>(m_state.data.frames.size());
    m_state.cursorFrame = m_state.loadedFrameCount;
}

void ReplayManager::finalizeRecordingAsPlayback(const fs::path& path)
{
    std::scoped_lock lock(m_mutex);
    m_state.filePath = path;
    m_state.mode = ReplayMode::Playback;
    m_state.loadedReplayActive = true;
    m_state.loadedFrameCount = static_cast<uint32_t>(m_state.data.frames.size());
    m_state.playing = false;
    m_state.pendingStopAtEnd = false;
}

void ReplayManager::setLoadedReplay(const fs::path& path, ReplayData data)
{
    std::scoped_lock lock(m_mutex);
    m_state = {};
    m_runtimeSnapshots.clear();
    m_state.mode = ReplayMode::Playback;
    m_state.filePath = path;
    m_state.data = std::move(data);
    m_state.loadedReplayActive = true;
    m_state.loadedFrameCount = static_cast<uint32_t>(m_state.data.frames.size());
}

uint32_t ReplayManager::inputCount() const
{
    std::scoped_lock lock(m_mutex);
    return static_cast<uint32_t>(m_state.data.frames.size());
}

uint32_t ReplayManager::clampedFrame(uint32_t frame) const
{
    std::scoped_lock lock(m_mutex);
    return std::min(frame, static_cast<uint32_t>(m_state.data.frames.size()));
}

IEmulationHost::InputTopologySnapshot ReplayManager::inputTopology() const
{
    std::scoped_lock lock(m_mutex);
    return m_state.data.inputTopology;
}

std::optional<InputFrame> ReplayManager::playbackFrameForFrame(uint32_t frame) const
{
    std::scoped_lock lock(m_mutex);
    if(frame >= m_state.data.frames.size()) {
        return std::nullopt;
    }
    return m_state.data.frames[static_cast<size_t>(frame)];
}

void ReplayManager::setCursorFrame(uint32_t frame)
{
    std::scoped_lock lock(m_mutex);
    m_state.cursorFrame = frame;
    m_state.cursorCanonicalCrc32.reset();
}

void ReplayManager::setCursorState(uint32_t frame, std::optional<uint32_t> canonicalCrc32)
{
    std::scoped_lock lock(m_mutex);
    m_state.cursorFrame = frame;
    m_state.cursorCanonicalCrc32 = frame == 0 ? std::nullopt : canonicalCrc32;
}

void ReplayManager::beginPlayback()
{
    std::scoped_lock lock(m_mutex);
    m_state.pendingStopAtEnd = false;
    m_state.playing = true;
}

void ReplayManager::markPlaybackReachedEnd()
{
    std::scoped_lock lock(m_mutex);
    m_state.pendingStopAtEnd = true;
}

void ReplayManager::notePlaybackFrame(uint32_t frame)
{
    std::scoped_lock lock(m_mutex);
    m_state.cursorFrame = frame;
}

bool ReplayManager::syncRuntimeFrame(uint32_t emuFrame)
{
    std::scoped_lock lock(m_mutex);
    if(m_state.mode == ReplayMode::None) {
        return false;
    }

    if(m_state.mode == ReplayMode::Recording) {
        m_state.cursorFrame = std::min(emuFrame, static_cast<uint32_t>(m_state.data.frames.size()));
        m_state.loadedFrameCount = static_cast<uint32_t>(m_state.data.frames.size());
        return false;
    }

    m_state.loadedFrameCount = static_cast<uint32_t>(m_state.data.frames.size());
    m_state.cursorFrame = std::min(emuFrame, m_state.loadedFrameCount);
    return m_state.pendingStopAtEnd || m_state.cursorFrame >= m_state.loadedFrameCount;
}

bool ReplayManager::shouldCaptureRuntimeSnapshot(uint32_t frame) const
{
    std::scoped_lock lock(m_mutex);
    if(m_state.mode != ReplayMode::Playback || !m_state.loadedReplayActive || frame == 0 ||
       (frame % kSnapshotIntervalFrames) != 0) {
        return false;
    }
    return std::none_of(m_runtimeSnapshots.begin(), m_runtimeSnapshots.end(), [frame](const RuntimeSnapshot& snapshot) {
        return snapshot.frame == frame;
    });
}

void ReplayManager::storeRuntimeSnapshot(uint32_t frame, std::vector<uint8_t> state)
{
    std::scoped_lock lock(m_mutex);
    if(m_state.mode != ReplayMode::Playback || !m_state.loadedReplayActive || frame == 0 || state.empty()) {
        return;
    }
    const auto it = std::find_if(m_runtimeSnapshots.begin(), m_runtimeSnapshots.end(), [frame](const RuntimeSnapshot& snapshot) {
        return snapshot.frame == frame;
    });
    if(it != m_runtimeSnapshots.end()) {
        it->state = std::move(state);
        return;
    }
    m_runtimeSnapshots.push_back(RuntimeSnapshot{frame, std::move(state)});
    std::sort(m_runtimeSnapshots.begin(), m_runtimeSnapshots.end(), [](const RuntimeSnapshot& lhs, const RuntimeSnapshot& rhs) {
        return lhs.frame < rhs.frame;
    });
}

std::optional<std::pair<uint32_t, std::vector<uint8_t>>> ReplayManager::runtimeSnapshotAtOrBefore(uint32_t frame) const
{
    std::scoped_lock lock(m_mutex);
    std::optional<std::pair<uint32_t, std::vector<uint8_t>>> best;
    for(const RuntimeSnapshot& snapshot : m_runtimeSnapshots) {
        if(snapshot.frame > frame) {
            break;
        }
        best = std::make_pair(snapshot.frame, snapshot.state);
    }
    return best;
}

bool ReplayManager::saveToFile(const fs::path& path, std::string& error) const
{
    ReplayState snapshotState;
    {
        std::scoped_lock lock(m_mutex);
        snapshotState = m_state;
    }
    return ReplayFile::save(path, snapshotState.data, error);
}

bool ReplayManager::loadFromFile(const fs::path& path, std::string& error)
{
    ReplayData data;
    if(!ReplayFile::load(path, data, error)) {
        return false;
    }
    setLoadedReplay(path, std::move(data));
    return true;
}

bool ReplayManager::isRecording() const
{
    std::scoped_lock lock(m_mutex);
    return m_state.mode == ReplayMode::Recording;
}

bool ReplayManager::isPlayback() const
{
    std::scoped_lock lock(m_mutex);
    return m_state.mode == ReplayMode::Playback;
}

bool ReplayManager::hasLoadedReplay() const
{
    std::scoped_lock lock(m_mutex);
    return m_state.loadedReplayActive;
}
