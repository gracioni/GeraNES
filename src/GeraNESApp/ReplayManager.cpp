#include "GeraNESApp/ReplayManager.h"

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

std::optional<InputFrame> replayFrameForNumber(const std::vector<InputFrame>& frames, uint32_t frameNumber)
{
    if(frameNumber < frames.size()) {
        const InputFrame& indexed = frames[static_cast<size_t>(frameNumber)];
        if(indexed.frame == frameNumber) {
            return indexed;
        }
    }

    const auto it = std::lower_bound(
        frames.begin(),
        frames.end(),
        frameNumber,
        [](const InputFrame& frame, uint32_t targetFrame) {
            return frame.frame < targetFrame;
        });
    if(it == frames.end() || it->frame != frameNumber) {
        return std::nullopt;
    }
    return *it;
}
}

std::vector<uint32_t> ReplayManager::scheduledRuntimeSnapshotFramesLocked() const
{
    std::vector<uint32_t> frames;
    if(m_state.mode != ReplayMode::Playback || !m_state.loadedReplayActive) {
        return frames;
    }

    const uint32_t replayFrameCount = replayTimelineFrameCount(m_state.data.frames);
    if(replayFrameCount <= 1u) {
        return frames;
    }

    frames.reserve(kRuntimeSnapshotCapacity);
    for(uint32_t i = 1; i <= kRuntimeSnapshotCapacity; ++i) {
        const uint32_t frame = (i * replayFrameCount) / (static_cast<uint32_t>(kRuntimeSnapshotCapacity) + 1u);
        if(frame == 0 || frame >= replayFrameCount || (!frames.empty() && frames.back() == frame)) {
            continue;
        }
        frames.push_back(frame);
    }

    return frames;
}

bool ReplayManager::hasRuntimeSnapshotForFrameLocked(uint32_t frame) const
{
    return std::any_of(m_runtimeSnapshots.begin(), m_runtimeSnapshots.end(), [frame](const RuntimeSnapshot& snapshot) {
        return snapshot.frame == frame;
    });
}

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
                                   const InputTopology& topology)
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

void ReplayManager::beginRecordingFromLoadedReplay(uint32_t continueFromFrame)
{
    std::scoped_lock lock(m_mutex);
    const uint32_t preservedFrameCount = std::min(
        continueFromFrame + 1u,
        replayTimelineFrameCount(m_state.data.frames));
    m_runtimeSnapshots.clear();
    m_state.mode = ReplayMode::Recording;
    m_state.filePath.clear();
    m_state.data.frames.resize(preservedFrameCount);
    m_state.cursorFrame = preservedFrameCount;
    m_state.cursorCanonicalCrc32.reset();
    m_state.loadedFrameCount = replayTimelineFrameCount(m_state.data.frames);
    m_state.playing = true;
    m_state.pendingStopAtEnd = false;
    m_state.loadedReplayActive = false;
}

void ReplayManager::appendRecordedFrame(const InputFrame& frame)
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

void ReplayManager::trimRecordedFramesAfter(uint32_t frame)
{
    std::scoped_lock lock(m_mutex);
    const uint32_t preservedFrameCount = std::min(
        frame + 1u,
        replayTimelineFrameCount(m_state.data.frames));
    m_state.data.frames.resize(preservedFrameCount);
    m_state.loadedFrameCount = replayTimelineFrameCount(m_state.data.frames);
    m_state.cursorFrame = std::min(frame, m_state.loadedFrameCount);
    m_state.cursorCanonicalCrc32.reset();
}

void ReplayManager::finalizeRecordingAsPlayback(const fs::path& path)
{
    std::scoped_lock lock(m_mutex);
    m_state.filePath = path;
    m_state.mode = ReplayMode::Playback;
    m_state.loadedReplayActive = true;
    m_state.loadedFrameCount = replayTimelineFrameCount(m_state.data.frames);
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
    m_state.loadedFrameCount = replayTimelineFrameCount(m_state.data.frames);
}

uint32_t ReplayManager::inputCount() const
{
    std::scoped_lock lock(m_mutex);
    return replayTimelineFrameCount(m_state.data.frames);
}

uint32_t ReplayManager::clampedFrame(uint32_t frame) const
{
    std::scoped_lock lock(m_mutex);
    return std::min(frame, replayTimelineFrameCount(m_state.data.frames));
}

InputTopology ReplayManager::inputTopology() const
{
    std::scoped_lock lock(m_mutex);
    return m_state.data.inputTopology;
}

std::optional<InputFrame> ReplayManager::playbackFrameForFrame(uint32_t frame) const
{
    std::scoped_lock lock(m_mutex);
    return replayFrameForNumber(m_state.data.frames, frame);
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

bool ReplayManager::shouldCaptureRuntimeSnapshot(uint32_t frame) const
{
    std::scoped_lock lock(m_mutex);
    if(m_state.mode != ReplayMode::Playback || !m_state.loadedReplayActive || frame == 0) {
        return false;
    }

    const std::vector<uint32_t> scheduledFrames = scheduledRuntimeSnapshotFramesLocked();
    return std::find(scheduledFrames.begin(), scheduledFrames.end(), frame) != scheduledFrames.end() &&
           !hasRuntimeSnapshotForFrameLocked(frame);
}

std::vector<uint32_t> ReplayManager::pendingRuntimeSnapshotFramesInRange(uint32_t startFrameExclusive,
                                                                         uint32_t endFrameInclusive) const
{
    std::scoped_lock lock(m_mutex);
    std::vector<uint32_t> pendingFrames;
    if(m_state.mode != ReplayMode::Playback || !m_state.loadedReplayActive || endFrameInclusive == 0 ||
       startFrameExclusive >= endFrameInclusive) {
        return pendingFrames;
    }

    const std::vector<uint32_t> scheduledFrames = scheduledRuntimeSnapshotFramesLocked();
    for(const uint32_t frame : scheduledFrames) {
        if(frame <= startFrameExclusive || frame > endFrameInclusive || hasRuntimeSnapshotForFrameLocked(frame)) {
            continue;
        }
        pendingFrames.push_back(frame);
    }
    return pendingFrames;
}

void ReplayManager::storeRuntimeSnapshot(uint32_t frame, std::vector<uint8_t> state)
{
    std::scoped_lock lock(m_mutex);
    if(m_state.mode != ReplayMode::Playback || !m_state.loadedReplayActive || frame == 0 || state.empty()) {
        return;
    }

    const std::vector<uint32_t> scheduledFrames = scheduledRuntimeSnapshotFramesLocked();
    if(std::find(scheduledFrames.begin(), scheduledFrames.end(), frame) == scheduledFrames.end()) {
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
    while(m_runtimeSnapshots.size() > kRuntimeSnapshotCapacity) {
        m_runtimeSnapshots.erase(m_runtimeSnapshots.begin());
    }
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
