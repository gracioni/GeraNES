#include "ConsoleNetplay/SnapshotSystem.h"

#include <algorithm>
#include <utility>

#include "ConsoleNetplay/NetplayCrc32.h"

namespace ConsoleNetplay {

void SnapshotSystem::configure(size_t capacity)
{
    m_capacity = capacity;
    trim();
}

void SnapshotSystem::clear()
{
    m_records.clear();
}

size_t SnapshotSystem::capacity() const
{
    return m_capacity;
}

size_t SnapshotSystem::size() const
{
    return m_records.size();
}

bool SnapshotSystem::empty() const
{
    return m_records.empty();
}

void SnapshotSystem::push(FrameNumber frame, std::vector<uint8_t> data)
{
    if(m_capacity == 0) return;

    // Recovery/resync can regenerate snapshots for an older frame.
    // Once that happens, every newer snapshot becomes stale and must be discarded.
    while(!m_records.empty() && m_records.back().frame >= frame) {
        m_records.pop_back();
    }

    SnapshotRecord record;
    record.frame = frame;
    record.crc32 = calcCrc32(data);
    record.data = std::move(data);
    m_records.push_back(std::move(record));
    trim();
}

const SnapshotRecord* SnapshotSystem::find(FrameNumber frame) const
{
    auto it = std::find_if(m_records.begin(), m_records.end(), [&](const SnapshotRecord& record) {
        return record.frame == frame;
    });

    return it != m_records.end() ? &(*it) : nullptr;
}

const SnapshotRecord* SnapshotSystem::latest() const
{
    return m_records.empty() ? nullptr : &m_records.back();
}

std::optional<uint32_t> SnapshotSystem::crc32ForFrame(FrameNumber frame) const
{
    const SnapshotRecord* record = find(frame);
    if(record == nullptr) return std::nullopt;
    return record->crc32;
}

uint32_t SnapshotSystem::calcCrc32(const std::vector<uint8_t>& data)
{
    if(data.empty()) return 0;
    return crc32(data.data(), data.size());
}

void SnapshotSystem::trim()
{
    while(m_records.size() > m_capacity) {
        m_records.pop_front();
    }
}

} // namespace ConsoleNetplay
