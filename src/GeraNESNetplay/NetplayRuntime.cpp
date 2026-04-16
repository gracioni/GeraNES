#include "GeraNESNetplay/NetplayRuntime.h"

namespace Netplay {

void NetplayRuntime::configureRollbackWindow(size_t snapshotCapacity)
{
    m_snapshots.configure(snapshotCapacity);
    m_localInputs.configure(snapshotCapacity * 4);
    m_remoteInputs.configure(snapshotCapacity * 4);
}

void NetplayRuntime::reset()
{
    m_currentFrame = 0;
    m_snapshots.clear();
    m_localInputs.clear();
    m_remoteInputs.clear();
    m_stats = {};
}

void NetplayRuntime::setCurrentFrame(FrameNumber frame)
{
    m_currentFrame = frame;
}

FrameNumber NetplayRuntime::currentFrame() const
{
    return m_currentFrame;
}

SnapshotSystem& NetplayRuntime::snapshots()
{
    return m_snapshots;
}

const SnapshotSystem& NetplayRuntime::snapshots() const
{
    return m_snapshots;
}

RollbackStats& NetplayRuntime::stats()
{
    return m_stats;
}

const RollbackStats& NetplayRuntime::stats() const
{
    return m_stats;
}

InputTimeline& NetplayRuntime::localInputs()
{
    return m_localInputs;
}

const InputTimeline& NetplayRuntime::localInputs() const
{
    return m_localInputs;
}

InputTimeline& NetplayRuntime::remoteInputs()
{
    return m_remoteInputs;
}

const InputTimeline& NetplayRuntime::remoteInputs() const
{
    return m_remoteInputs;
}

} // namespace Netplay
