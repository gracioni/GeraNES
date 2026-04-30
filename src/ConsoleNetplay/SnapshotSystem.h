#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "NetplayTypes.h"

namespace ConsoleNetplay {

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
    void configure(size_t capacity);
    void clear();
    size_t capacity() const;
    size_t size() const;
    bool empty() const;
    void push(FrameNumber frame, std::vector<uint8_t> data);
    const SnapshotRecord* find(FrameNumber frame) const;
    const SnapshotRecord* latest() const;
    std::optional<uint32_t> crc32ForFrame(FrameNumber frame) const;

    template<typename Loader>
    bool restore(FrameNumber frame, Loader&& loader) const
    {
        const SnapshotRecord* record = find(frame);
        if(record == nullptr) return false;

        std::forward<Loader>(loader)(record->data);
        return true;
    }

private:
    static uint32_t calcCrc32(const std::vector<uint8_t>& data);
    void trim();
};

} // namespace ConsoleNetplay
