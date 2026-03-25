#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

#include "GeraNES/util/Crc32.h"
#include "NetplayTypes.h"

namespace Netplay {

struct SnapshotRecord
{
    FrameNumber frame = 0;
    uint32_t crc32 = 0;
    std::vector<uint8_t> data;
};

class SnapshotSystem
{
private:
    size_t m_capacity = 0;
    std::deque<SnapshotRecord> m_records;

public:
    void configure(size_t capacity)
    {
        m_capacity = capacity;
        trim();
    }

    void clear()
    {
        m_records.clear();
    }

    size_t capacity() const
    {
        return m_capacity;
    }

    size_t size() const
    {
        return m_records.size();
    }

    bool empty() const
    {
        return m_records.empty();
    }

    void push(FrameNumber frame, std::vector<uint8_t> data)
    {
        if(m_capacity == 0) return;

        if(!m_records.empty() && m_records.back().frame == frame) {
            m_records.back().data = std::move(data);
            m_records.back().crc32 = calcCrc32(m_records.back().data);
            return;
        }

        SnapshotRecord record;
        record.frame = frame;
        record.crc32 = calcCrc32(data);
        record.data = std::move(data);
        m_records.push_back(std::move(record));
        trim();
    }

    const SnapshotRecord* find(FrameNumber frame) const
    {
        auto it = std::find_if(m_records.begin(), m_records.end(), [&](const SnapshotRecord& record) {
            return record.frame == frame;
        });

        return it != m_records.end() ? &(*it) : nullptr;
    }

    const SnapshotRecord* latest() const
    {
        return m_records.empty() ? nullptr : &m_records.back();
    }

    std::optional<uint32_t> crc32ForFrame(FrameNumber frame) const
    {
        const SnapshotRecord* record = find(frame);
        if(record == nullptr) return std::nullopt;
        return record->crc32;
    }

    template<typename Loader>
    bool restore(FrameNumber frame, Loader&& loader) const
    {
        const SnapshotRecord* record = find(frame);
        if(record == nullptr) return false;

        std::forward<Loader>(loader)(record->data);
        return true;
    }

private:
    static uint32_t calcCrc32(const std::vector<uint8_t>& data)
    {
        if(data.empty()) return 0;
        return Crc32::calc(reinterpret_cast<const char*>(data.data()), data.size());
    }

    void trim()
    {
        while(m_records.size() > m_capacity) {
            m_records.pop_front();
        }
    }
};

} // namespace Netplay
