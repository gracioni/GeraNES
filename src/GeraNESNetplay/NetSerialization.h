#pragma once

#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace Netplay {

class PacketWriter
{
private:
    std::vector<uint8_t> m_data;

public:
    template<typename T>
    void writePod(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        const size_t offset = m_data.size();
        m_data.resize(offset + sizeof(T));
        std::memcpy(m_data.data() + offset, &value, sizeof(T));
    }

    void writeBytes(std::span<const uint8_t> bytes)
    {
        const size_t offset = m_data.size();
        m_data.resize(offset + bytes.size());
        if(!bytes.empty()) {
            std::memcpy(m_data.data() + offset, bytes.data(), bytes.size());
        }
    }

    void writeString(const std::string& value)
    {
        const uint16_t size = static_cast<uint16_t>(value.size());
        writePod(size);
        writeBytes(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(value.data()), value.size()));
    }

    const std::vector<uint8_t>& data() const
    {
        return m_data;
    }
};

class PacketReader
{
private:
    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    size_t m_offset = 0;

public:
    PacketReader(const uint8_t* data, size_t size)
        : m_data(data)
        , m_size(size)
    {
    }

    template<typename T>
    bool readPod(T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        if(m_offset + sizeof(T) > m_size) return false;
        std::memcpy(&value, m_data + m_offset, sizeof(T));
        m_offset += sizeof(T);
        return true;
    }

    bool readString(std::string& value)
    {
        uint16_t size = 0;
        if(!readPod(size)) return false;
        if(m_offset + size > m_size) return false;
        value.assign(reinterpret_cast<const char*>(m_data + m_offset), size);
        m_offset += size;
        return true;
    }

    bool readBytes(std::vector<uint8_t>& value, size_t size)
    {
        if(m_offset + size > m_size) return false;
        value.resize(size);
        if(size > 0) {
            std::memcpy(value.data(), m_data + m_offset, size);
        }
        m_offset += size;
        return true;
    }

    bool skip(size_t size)
    {
        if(m_offset + size > m_size) return false;
        m_offset += size;
        return true;
    }

    size_t remaining() const
    {
        return m_offset <= m_size ? (m_size - m_offset) : 0;
    }
};

} // namespace Netplay
