#include "GeraNESApp/ReplayFile.h"

#include <cstring>
#include <fstream>
#include <limits>

namespace
{
constexpr char kReplayMagic[] = "GERANES REPLAY";
constexpr uint32_t kReplayBinaryVersion = 2u;
constexpr uint32_t kReplayBinaryVersionLegacyBootstrap = 1u;
class ByteReader
{
private:
    const std::vector<uint8_t>& m_bytes;
    size_t m_offset = 0u;
    std::string m_error;

public:
    explicit ByteReader(const std::vector<uint8_t>& bytes)
        : m_bytes(bytes)
    {
    }

    size_t remaining() const
    {
        return m_bytes.size() - m_offset;
    }

    const std::string& error() const
    {
        return m_error;
    }

    bool expectLiteral(const char* literal, size_t size)
    {
        if(remaining() < size) {
            m_error = "Replay file header is truncated";
            return false;
        }
        if(std::memcmp(m_bytes.data() + m_offset, literal, size) != 0) {
            m_error = "Invalid replay file header";
            return false;
        }
        m_offset += size;
        return true;
    }

    bool readU8(uint8_t& value)
    {
        if(remaining() < 1u) {
            m_error = "Unexpected end of replay file";
            return false;
        }
        value = m_bytes[m_offset++];
        return true;
    }

    bool readU32(uint32_t& value)
    {
        if(remaining() < 4u) {
            m_error = "Unexpected end of replay file";
            return false;
        }
        value =
            static_cast<uint32_t>(m_bytes[m_offset]) |
            (static_cast<uint32_t>(m_bytes[m_offset + 1u]) << 8u) |
            (static_cast<uint32_t>(m_bytes[m_offset + 2u]) << 16u) |
            (static_cast<uint32_t>(m_bytes[m_offset + 3u]) << 24u);
        m_offset += 4u;
        return true;
    }

    bool readString(std::string& value)
    {
        uint32_t size = 0u;
        if(!readU32(size)) {
            return false;
        }
        if(remaining() < size) {
            m_error = "Unexpected end of replay file";
            return false;
        }
        value.assign(
            reinterpret_cast<const char*>(m_bytes.data() + m_offset),
            reinterpret_cast<const char*>(m_bytes.data() + m_offset + size)
        );
        m_offset += size;
        return true;
    }

    bool readBytes(uint32_t size, std::vector<uint8_t>& bytes)
    {
        if(remaining() < size) {
            m_error = "Unexpected end of replay file";
            return false;
        }
        bytes.assign(m_bytes.begin() + static_cast<std::ptrdiff_t>(m_offset),
                     m_bytes.begin() + static_cast<std::ptrdiff_t>(m_offset + size));
        m_offset += size;
        return true;
    }
};

void appendU8(std::vector<uint8_t>& bytes, uint8_t value)
{
    bytes.push_back(value);
}

void appendU32(std::vector<uint8_t>& bytes, uint32_t value)
{
    bytes.push_back(static_cast<uint8_t>(value & 0xFFu));
    bytes.push_back(static_cast<uint8_t>((value >> 8u) & 0xFFu));
    bytes.push_back(static_cast<uint8_t>((value >> 16u) & 0xFFu));
    bytes.push_back(static_cast<uint8_t>((value >> 24u) & 0xFFu));
}

bool appendString(std::vector<uint8_t>& bytes, const std::string& value, std::string& error)
{
    if(value.size() > std::numeric_limits<uint32_t>::max()) {
        error = "Replay string field is too large";
        return false;
    }
    appendU32(bytes, static_cast<uint32_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
    return true;
}

bool appendBytes(std::vector<uint8_t>& bytes, const std::vector<uint8_t>& payload, std::string& error)
{
    if(payload.size() > std::numeric_limits<uint32_t>::max()) {
        error = "Replay payload is too large";
        return false;
    }
    appendU32(bytes, static_cast<uint32_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return true;
}

void appendTopologyBytes(std::vector<uint8_t>& bytes, const InputTopology& topology)
{
    appendU8(bytes, static_cast<uint8_t>(topology.port1Device));
    appendU8(bytes, static_cast<uint8_t>(topology.port2Device));
    appendU8(bytes, static_cast<uint8_t>(topology.expansionDevice));
    appendU8(bytes, static_cast<uint8_t>(topology.nesMultitapDevice));
    appendU8(bytes, static_cast<uint8_t>(topology.famicomMultitapDevice));
}

bool readTopologyBytes(ByteReader& reader, InputTopology& topology)
{
    uint8_t port1Device = 0u;
    uint8_t port2Device = 0u;
    uint8_t expansionDevice = 0u;
    uint8_t nesMultitapDevice = 0u;
    uint8_t famicomMultitapDevice = 0u;
    if(!reader.readU8(port1Device) ||
       !reader.readU8(port2Device) ||
       !reader.readU8(expansionDevice) ||
       !reader.readU8(nesMultitapDevice) ||
       !reader.readU8(famicomMultitapDevice)) {
        return false;
    }
    topology.port1Device = static_cast<Settings::Device>(port1Device);
    topology.port2Device = static_cast<Settings::Device>(port2Device);
    topology.expansionDevice = static_cast<Settings::ExpansionDevice>(expansionDevice);
    topology.nesMultitapDevice = static_cast<Settings::NesMultitapDevice>(nesMultitapDevice);
    topology.famicomMultitapDevice = static_cast<Settings::FamicomMultitapDevice>(famicomMultitapDevice);
    return true;
}

void initializeFrameTopology(InputFrame& frame,
                             const InputTopology& topology,
                             uint32_t frameNumber)
{
    frame = {};
    frame.frame = frameNumber;
    frame.state.topology = topology;
}

struct EncodedRun
{
    uint32_t repeatCount = 0u;
    std::vector<uint8_t> payload;
};

bool buildEncodedRuns(const std::vector<InputFrame>& frames,
                      std::vector<EncodedRun>& runs,
                      std::string& error)
{
    runs.clear();
    runs.reserve(frames.size());

    for(const InputFrame& frame : frames) {
        if(!runs.empty() &&
           runs.back().repeatCount < std::numeric_limits<uint32_t>::max() &&
           runs.back().payload == frame.state.serializedInputData) {
            ++runs.back().repeatCount;
            continue;
        }

        EncodedRun run;
        run.repeatCount = 1u;
        run.payload = frame.state.serializedInputData;
        runs.push_back(std::move(run));
    }

    if(runs.size() > std::numeric_limits<uint32_t>::max()) {
        error = "Replay contains too many encoded runs";
        return false;
    }
    return true;
}
}

bool ReplayFile::save(const fs::path& path, const Data& data, std::string& error)
{
    if(data.romCrc.empty()) {
        error = "Replay file is missing ROM CRC";
        return false;
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(sizeof(kReplayMagic) - 1u + 64u);
    bytes.insert(bytes.end(), kReplayMagic, kReplayMagic + (sizeof(kReplayMagic) - 1u));
    appendU32(bytes, kReplayBinaryVersion);

    if(!appendString(bytes, data.romName, error) ||
       !appendString(bytes, data.romCrc, error)) {
        return false;
    }

    appendTopologyBytes(bytes, data.inputTopology);

    std::vector<EncodedRun> runs;
    if(!buildEncodedRuns(data.frames, runs, error)) {
        return false;
    }

    appendU32(bytes, static_cast<uint32_t>(runs.size()));
    for(const EncodedRun& run : runs) {
        appendU32(bytes, run.repeatCount);
        if(!appendBytes(bytes, run.payload, error)) {
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if(!file.is_open()) {
        error = "Could not open replay file for writing";
        return false;
    }

    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if(!file.good()) {
        error = "Failed to write replay file";
        return false;
    }
    return true;
}

bool ReplayFile::load(const fs::path& path, Data& data, std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if(!file.is_open()) {
        error = "Could not open replay file";
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if(size <= 0) {
        error = "Replay file is empty";
        return false;
    }
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if(!file.good() && !file.eof()) {
        error = "Failed to read replay file";
        return false;
    }

    ByteReader reader(bytes);
    if(!reader.expectLiteral(kReplayMagic, sizeof(kReplayMagic) - 1u)) {
        error = reader.error();
        return false;
    }

    uint32_t version = 0u;
    if(!reader.readU32(version)) {
        error = reader.error();
        return false;
    }
    if(version != kReplayBinaryVersion &&
       version != kReplayBinaryVersionLegacyBootstrap) {
        error = "Unsupported replay file version";
        return false;
    }

    Data loadedData;
    if(!reader.readString(loadedData.romName) ||
       !reader.readString(loadedData.romCrc) ||
       !readTopologyBytes(reader, loadedData.inputTopology)) {
        error = reader.error();
        return false;
    }

    uint32_t nextFrameNumber = 0u;
    if(version == kReplayBinaryVersionLegacyBootstrap) {
        uint8_t bootstrapPresent = 0u;
        if(!reader.readU8(bootstrapPresent)) {
            error = reader.error();
            return false;
        }

        InputFrame frameZero;
        initializeFrameTopology(frameZero, loadedData.inputTopology, 0u);
        if(bootstrapPresent != 0u) {
            uint32_t bootstrapPayloadSize = 0u;
            if(!reader.readU32(bootstrapPayloadSize)) {
                error = reader.error();
                return false;
            }
            if(!reader.readBytes(bootstrapPayloadSize, frameZero.state.serializedInputData)) {
                error = reader.error();
                return false;
            }
        }
        loadedData.frames.push_back(std::move(frameZero));
        nextFrameNumber = 1u;
    }

    uint32_t runCount = 0u;
    if(!reader.readU32(runCount)) {
        error = reader.error();
        return false;
    }

    for(uint32_t runIndex = 0u; runIndex < runCount; ++runIndex) {
        uint32_t repeatCount = 0u;
        uint32_t payloadSize = 0u;
        if(!reader.readU32(repeatCount) || !reader.readU32(payloadSize)) {
            error = reader.error();
            return false;
        }
        if(repeatCount == 0u) {
            error = "Replay file contains an invalid empty input run";
            return false;
        }

        std::vector<uint8_t> payload;
        if(!reader.readBytes(payloadSize, payload)) {
            error = reader.error();
            return false;
        }

        if(loadedData.frames.size() > (std::numeric_limits<size_t>::max() - repeatCount)) {
            error = "Replay file is too large";
            return false;
        }

        for(uint32_t i = 0u; i < repeatCount; ++i) {
            InputFrame frame;
            initializeFrameTopology(frame, loadedData.inputTopology, nextFrameNumber++);
            frame.state.serializedInputData = payload;
            loadedData.frames.push_back(std::move(frame));
        }
    }

    if(reader.remaining() != 0u) {
        error = "Replay file has unexpected trailing data";
        return false;
    }
    if(loadedData.romCrc.empty()) {
        error = "Replay file is missing ROM CRC";
        return false;
    }

    data = std::move(loadedData);
    return true;
}
