#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include "GeraNES/InputFrame.h"

class ReplayPlaybackController
{
public:
    struct StoredSnapshot
    {
        uint32_t frame = 0;
        std::shared_ptr<std::vector<uint8_t>> data;
    };

private:
    static constexpr size_t kSnapshotCapacity = 10u;

public:
    bool loaded = false;
    bool playing = false;
    bool seeking = false;
    uint32_t cursorFrame = 0;
    std::vector<InputFrame> frames;
    std::vector<uint32_t> scheduledSnapshotFrames;
    std::deque<StoredSnapshot> snapshots;

    static uint32_t frameCountFromFrames(const std::vector<InputFrame>& frames)
    {
        uint32_t maxFramePlusOne = 0u;
        for(const InputFrame& frame : frames) {
            maxFramePlusOne = std::max(maxFramePlusOne, frame.frame + 1u);
        }
        return maxFramePlusOne;
    }

    static const InputFrame* findFrameByNumber(const std::vector<InputFrame>& frames, uint32_t targetFrame)
    {
        if(targetFrame < frames.size()) {
            const InputFrame& indexed = frames[static_cast<size_t>(targetFrame)];
            if(indexed.frame == targetFrame) {
                return &indexed;
            }
        }

        const auto it = std::lower_bound(
            frames.begin(),
            frames.end(),
            targetFrame,
            [](const InputFrame& frame, uint32_t frameNumber) {
                return frame.frame < frameNumber;
            });
        if(it == frames.end() || it->frame != targetFrame) {
            return nullptr;
        }
        return &*it;
    }

    uint32_t loadedFrameCount() const
    {
        return frameCountFromFrames(frames);
    }

    void clear()
    {
        *this = {};
    }

    void loadFrames(std::vector<InputFrame> loadedFrames, uint32_t currentFrame)
    {
        loaded = true;
        playing = false;
        seeking = false;
        frames = std::move(loadedFrames);
        std::stable_sort(
            frames.begin(),
            frames.end(),
            [](const InputFrame& lhs, const InputFrame& rhs) {
                return lhs.frame < rhs.frame;
            });
        resetSnapshots(currentFrame);
    }

    void resetSnapshots(uint32_t currentFrame)
    {
        cursorFrame = currentFrame;
        scheduledSnapshotFrames.clear();
        snapshots.clear();

        const uint32_t replayFrameCount = loadedFrameCount();
        if(replayFrameCount <= 1u) {
            return;
        }

        for(uint32_t i = 1; i <= kSnapshotCapacity; ++i) {
            const uint32_t frame = (i * replayFrameCount) / (static_cast<uint32_t>(kSnapshotCapacity) + 1u);
            if(frame == 0 || frame >= replayFrameCount) {
                continue;
            }
            if(!scheduledSnapshotFrames.empty() && scheduledSnapshotFrames.back() == frame) {
                continue;
            }
            scheduledSnapshotFrames.push_back(frame);
        }
    }

    void storeSnapshot(uint32_t frame, const std::vector<uint8_t>& state)
    {
        if(state.empty()) {
            return;
        }

        const auto existing = std::find_if(
            snapshots.begin(),
            snapshots.end(),
            [frame](const StoredSnapshot& snapshot) {
                return snapshot.frame == frame;
        });
        if(existing != snapshots.end()) {
            existing->data = std::make_shared<std::vector<uint8_t>>(state);
            return;
        }

        snapshots.push_back(StoredSnapshot{
            frame,
            std::make_shared<std::vector<uint8_t>>(state)
        });
    }

    bool shouldCaptureScheduledSnapshot(uint32_t frame) const
    {
        return std::find(
                   scheduledSnapshotFrames.begin(),
                   scheduledSnapshotFrames.end(),
                   frame) != scheduledSnapshotFrames.end();
    }

    void initializeBaselineSnapshot(uint32_t currentFrame, const std::vector<uint8_t>& state)
    {
        resetSnapshots(currentFrame);
        storeSnapshot(currentFrame, state);
    }

    bool resolveInput(uint32_t targetFrame, InputFrame& frame)
    {
        if(!loaded) {
            return false;
        }

        if(!playing && !seeking) {
            return false;
        }

        const InputFrame* replayFrame = findFrameByNumber(frames, targetFrame);
        if(replayFrame == nullptr) {
            playing = false;
            seeking = false;
            return false;
        }

        frame = *replayFrame;
        frame.frame = targetFrame;
        return true;
    }

    bool blocksNonReplayInput() const
    {
        return loaded && !playing && !seeking;
    }

    uint32_t clampTargetFrame(uint32_t targetFrame) const
    {
        return std::min(targetFrame, loadedFrameCount());
    }

    const StoredSnapshot* bestSnapshotAtOrBefore(uint32_t targetFrame) const
    {
        const auto bestSnapshot = std::find_if(
            snapshots.rbegin(),
            snapshots.rend(),
            [targetFrame](const StoredSnapshot& snapshot) {
                return snapshot.frame <= targetFrame && snapshot.data && !snapshot.data->empty();
            });
        return bestSnapshot == snapshots.rend() ? nullptr : &*bestSnapshot;
    }
};
