#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

#include "GeraNES/InputBuffer.h"

class PendingInputFrames
{
private:
    size_t m_capacity = 64;
    std::map<uint32_t, InputFrame> m_frames;

    void trimToCapacity()
    {
        while(m_frames.size() > m_capacity) {
            m_frames.erase(m_frames.begin());
        }
    }

public:
    explicit PendingInputFrames(size_t capacity = 64)
        : m_capacity(std::max<size_t>(1, capacity))
    {
    }

    void clear()
    {
        m_frames.clear();
    }

    void reconfigureCapacity(size_t capacity)
    {
        m_capacity = std::max<size_t>(1, capacity);
        trimToCapacity();
    }

    size_t capacity() const
    {
        return m_capacity;
    }

    size_t size() const
    {
        return m_frames.size();
    }

    bool empty() const
    {
        return m_frames.empty();
    }

    bool contains(uint32_t frame) const
    {
        return m_frames.find(frame) != m_frames.end();
    }

    const InputFrame* find(uint32_t frame) const
    {
        const auto it = m_frames.find(frame);
        if(it == m_frames.end()) {
            return nullptr;
        }
        return &it->second;
    }

    void set(const InputFrame& frame)
    {
        m_frames[frame.frame] = frame;
        trimToCapacity();
    }

    std::optional<InputFrame> take(uint32_t frame)
    {
        const auto it = m_frames.find(frame);
        if(it == m_frames.end()) {
            return std::nullopt;
        }

        InputFrame value = it->second;
        m_frames.erase(it);
        return value;
    }

    void eraseFramesAfter(uint32_t frame)
    {
        auto it = m_frames.upper_bound(frame);
        while(it != m_frames.end()) {
            it = m_frames.erase(it);
        }
    }

    void eraseFramesBefore(uint32_t frame)
    {
        auto it = m_frames.begin();
        while(it != m_frames.end() && it->first < frame) {
            it = m_frames.erase(it);
        }
    }
};
