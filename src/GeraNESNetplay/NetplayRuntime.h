#pragma once

#include <cstddef>

#include "Diagnostics.h"
#include "InputTimeline.h"
#include "SnapshotSystem.h"

namespace Netplay {

class NetplayRuntime
{
private:
    FrameNumber m_currentFrame = 0;
    SnapshotSystem m_snapshots;
    InputTimeline m_localInputs;
    InputTimeline m_remoteInputs;
    RollbackStats m_stats;

public:
    void configureRollbackWindow(size_t snapshotCapacity)
    {
        m_snapshots.configure(snapshotCapacity);
        m_localInputs.configure(snapshotCapacity * 4);
        m_remoteInputs.configure(snapshotCapacity * 4);
    }

    void reset()
    {
        m_currentFrame = 0;
        m_snapshots.clear();
        m_localInputs.clear();
        m_remoteInputs.clear();
        m_stats = {};
    }

    void setCurrentFrame(FrameNumber frame)
    {
        m_currentFrame = frame;
    }

    FrameNumber currentFrame() const
    {
        return m_currentFrame;
    }

    SnapshotSystem& snapshots()
    {
        return m_snapshots;
    }

    const SnapshotSystem& snapshots() const
    {
        return m_snapshots;
    }

    RollbackStats& stats()
    {
        return m_stats;
    }

    const RollbackStats& stats() const
    {
        return m_stats;
    }

    InputTimeline& localInputs()
    {
        return m_localInputs;
    }

    const InputTimeline& localInputs() const
    {
        return m_localInputs;
    }

    InputTimeline& remoteInputs()
    {
        return m_remoteInputs;
    }

    const InputTimeline& remoteInputs() const
    {
        return m_remoteInputs;
    }

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

} // namespace Netplay
