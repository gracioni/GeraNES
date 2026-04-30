#pragma once

#include <cstddef>
#include <utility>

#include "Diagnostics.h"
#include "InputTimeline.h"
#include "SnapshotSystem.h"

namespace ConsoleNetplay {

class NetplayRuntime
{
private:
    FrameNumber m_currentFrame = 0;
    SnapshotSystem m_snapshots;
    InputTimeline m_localInputs;
    InputTimeline m_remoteInputs;
    RollbackStats m_stats;

public:
    void configureRollbackWindow(size_t snapshotCapacity);
    void reset();
    void setCurrentFrame(FrameNumber frame);
    FrameNumber currentFrame() const;
    SnapshotSystem& snapshots();
    const SnapshotSystem& snapshots() const;
    RollbackStats& stats();
    const RollbackStats& stats() const;
    InputTimeline& localInputs();
    const InputTimeline& localInputs() const;
    InputTimeline& remoteInputs();
    const InputTimeline& remoteInputs() const;

    template<typename Saver>
    void captureSnapshot(FrameNumber frame, Saver&& saver)
    {
        m_snapshots.push(frame, std::forward<Saver>(saver)());
    }

    template<typename Loader>
    bool rollbackTo(FrameNumber frame, Loader&& loader)
    {
        if(!m_snapshots.restore(frame, std::forward<Loader>(loader))) {
            return false;
        }

        m_stats.recordRollback(frame, m_currentFrame);
        m_currentFrame = frame;
        return true;
    }
};

} // namespace ConsoleNetplay
