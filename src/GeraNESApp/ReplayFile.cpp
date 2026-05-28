#include "GeraNESApp/ReplayFile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <vector>

namespace
{
constexpr char kReplayMagic[] = "GERANES REPLAY";
constexpr uint32_t kReplayBinaryVersion = 1u;
constexpr uint8_t kMissingPortDevice = 0xFFu;

class ByteReader
{
private:
    const std::vector<uint8_t>& m_bytes;
    size_t m_offset = 0;
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

    bool readU16(uint16_t& value)
    {
        if(remaining() < 2u) {
            m_error = "Unexpected end of replay file";
            return false;
        }
        value =
            static_cast<uint16_t>(m_bytes[m_offset]) |
            static_cast<uint16_t>(m_bytes[m_offset + 1u] << 8u);
        m_offset += 2u;
        return true;
    }

    bool readI16(int16_t& value)
    {
        uint16_t raw = 0u;
        if(!readU16(raw)) {
            return false;
        }
        value = static_cast<int16_t>(raw);
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
};

void appendU8(std::vector<uint8_t>& bytes, uint8_t value)
{
    bytes.push_back(value);
}

void appendU16(std::vector<uint8_t>& bytes, uint16_t value)
{
    bytes.push_back(static_cast<uint8_t>(value & 0xFFu));
    bytes.push_back(static_cast<uint8_t>((value >> 8u) & 0xFFu));
}

void appendI16(std::vector<uint8_t>& bytes, int16_t value)
{
    appendU16(bytes, static_cast<uint16_t>(value));
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

void appendBoolBits(std::vector<uint8_t>& bytes, std::initializer_list<bool> bits)
{
    uint8_t packed = 0u;
    size_t bitIndex = 0u;
    for(bool bit : bits) {
        if(bit) {
            packed |= static_cast<uint8_t>(1u << bitIndex);
        }
        ++bitIndex;
        if(bitIndex == 8u) {
            bytes.push_back(packed);
            packed = 0u;
            bitIndex = 0u;
        }
    }
    if(bitIndex != 0u) {
        bytes.push_back(packed);
    }
}

template<size_t N>
void appendBoolArrayBits(std::vector<uint8_t>& bytes, const std::array<bool, N>& bits)
{
    uint8_t packed = 0u;
    size_t bitIndex = 0u;
    for(bool bit : bits) {
        if(bit) {
            packed |= static_cast<uint8_t>(1u << bitIndex);
        }
        ++bitIndex;
        if(bitIndex == 8u) {
            bytes.push_back(packed);
            packed = 0u;
            bitIndex = 0u;
        }
    }
    if(bitIndex != 0u) {
        bytes.push_back(packed);
    }
}

bool readBoolBits(ByteReader& reader, size_t count, std::vector<bool>& bits)
{
    bits.assign(count, false);
    size_t bitIndex = 0u;
    while(bitIndex < count) {
        uint8_t packed = 0u;
        if(!reader.readU8(packed)) {
            return false;
        }
        for(size_t localBit = 0u; localBit < 8u && bitIndex < count; ++localBit, ++bitIndex) {
            bits[bitIndex] = ((packed >> localBit) & 0x01u) != 0u;
        }
    }
    return true;
}

template<size_t N>
bool readBoolArrayBits(ByteReader& reader, std::array<bool, N>& out)
{
    std::vector<bool> bits;
    if(!readBoolBits(reader, N, bits)) {
        return false;
    }
    for(size_t i = 0; i < N; ++i) {
        out[i] = bits[i];
    }
    return true;
}

uint16_t quantizeNormalizedPosition(float position)
{
    const float clamped = std::clamp(position, 0.0f, 1.0f);
    return static_cast<uint16_t>(std::lround(clamped * 65535.0f));
}

float dequantizeNormalizedPosition(uint16_t value)
{
    return static_cast<float>(value) / 65535.0f;
}

std::optional<Settings::Device> portDeviceFromByte(uint8_t value)
{
    if(value == kMissingPortDevice) {
        return std::nullopt;
    }
    return static_cast<Settings::Device>(value);
}

uint8_t portDeviceToByte(const std::optional<Settings::Device>& value)
{
    return value.has_value() ? static_cast<uint8_t>(*value) : kMissingPortDevice;
}

void initializeFrameTopology(InputFrame& frame,
                             const IEmulationHost::InputTopologySnapshot& topology,
                             uint32_t frameNumber)
{
    frame = {};
    frame.frame = frameNumber;
    frame.port1Device = topology.port1Device.value_or(Settings::Device::NONE);
    frame.port2Device = topology.port2Device.value_or(Settings::Device::NONE);
    frame.expansionDevice = topology.expansionDevice;
    frame.nesMultitapDevice = topology.nesMultitapDevice;
    frame.famicomMultitapDevice = topology.famicomMultitapDevice;
}

void appendPortControllerButtons(std::vector<uint8_t>& bytes, const InputFrame& frame, int player, bool includeSnesExtras)
{
    switch(player) {
        case 1:
            if(includeSnesExtras) {
                appendBoolBits(bytes, {frame.p1A, frame.p1B, frame.p1Select, frame.p1Start, frame.p1Up, frame.p1Down, frame.p1Left, frame.p1Right,
                                       frame.p1X, frame.p1Y, frame.p1L, frame.p1R});
            } else {
                appendBoolBits(bytes, {frame.p1A, frame.p1B, frame.p1Select, frame.p1Start, frame.p1Up, frame.p1Down, frame.p1Left, frame.p1Right});
            }
            break;
        case 2:
            if(includeSnesExtras) {
                appendBoolBits(bytes, {frame.p2A, frame.p2B, frame.p2Select, frame.p2Start, frame.p2Up, frame.p2Down, frame.p2Left, frame.p2Right,
                                       frame.p2X, frame.p2Y, frame.p2L, frame.p2R});
            } else {
                appendBoolBits(bytes, {frame.p2A, frame.p2B, frame.p2Select, frame.p2Start, frame.p2Up, frame.p2Down, frame.p2Left, frame.p2Right});
            }
            break;
        case 3:
            appendBoolBits(bytes, {frame.p3A, frame.p3B, frame.p3Select, frame.p3Start, frame.p3Up, frame.p3Down, frame.p3Left, frame.p3Right});
            break;
        case 4:
            appendBoolBits(bytes, {frame.p4A, frame.p4B, frame.p4Select, frame.p4Start, frame.p4Up, frame.p4Down, frame.p4Left, frame.p4Right});
            break;
        default:
            break;
    }
}

bool readPortControllerButtons(ByteReader& reader, InputFrame& frame, int player, bool includeSnesExtras)
{
    std::vector<bool> bits;
    if(!readBoolBits(reader, includeSnesExtras ? 12u : 8u, bits)) {
        return false;
    }

    auto readStandard = [&](bool& a, bool& b, bool& select, bool& start, bool& up, bool& down, bool& left, bool& right,
                            bool* x = nullptr, bool* y = nullptr, bool* l = nullptr, bool* r = nullptr) {
        a = bits[0];
        b = bits[1];
        select = bits[2];
        start = bits[3];
        up = bits[4];
        down = bits[5];
        left = bits[6];
        right = bits[7];
        if(x) *x = includeSnesExtras ? bits[8] : false;
        if(y) *y = includeSnesExtras ? bits[9] : false;
        if(l) *l = includeSnesExtras ? bits[10] : false;
        if(r) *r = includeSnesExtras ? bits[11] : false;
    };

    switch(player) {
        case 1:
            readStandard(frame.p1A, frame.p1B, frame.p1Select, frame.p1Start, frame.p1Up, frame.p1Down, frame.p1Left, frame.p1Right,
                         &frame.p1X, &frame.p1Y, &frame.p1L, &frame.p1R);
            return true;
        case 2:
            readStandard(frame.p2A, frame.p2B, frame.p2Select, frame.p2Start, frame.p2Up, frame.p2Down, frame.p2Left, frame.p2Right,
                         &frame.p2X, &frame.p2Y, &frame.p2L, &frame.p2R);
            return true;
        case 3:
            readStandard(frame.p3A, frame.p3B, frame.p3Select, frame.p3Start, frame.p3Up, frame.p3Down, frame.p3Left, frame.p3Right);
            return true;
        case 4:
            readStandard(frame.p4A, frame.p4B, frame.p4Select, frame.p4Start, frame.p4Up, frame.p4Down, frame.p4Left, frame.p4Right);
            return true;
        default:
            return false;
    }
}

void appendVirtualBoyButtons(std::vector<uint8_t>& bytes, const InputFrame& frame, int player)
{
    if(player == 1) {
        appendBoolBits(bytes, {frame.vbP1A, frame.vbP1B, frame.vbP1Select, frame.vbP1Start,
                               frame.vbP1Up0, frame.vbP1Down0, frame.vbP1Left0, frame.vbP1Right0,
                               frame.vbP1Up1, frame.vbP1Down1, frame.vbP1Left1, frame.vbP1Right1,
                               frame.vbP1L, frame.vbP1R});
    } else if(player == 2) {
        appendBoolBits(bytes, {frame.vbP2A, frame.vbP2B, frame.vbP2Select, frame.vbP2Start,
                               frame.vbP2Up0, frame.vbP2Down0, frame.vbP2Left0, frame.vbP2Right0,
                               frame.vbP2Up1, frame.vbP2Down1, frame.vbP2Left1, frame.vbP2Right1,
                               frame.vbP2L, frame.vbP2R});
    }
}

bool readVirtualBoyButtons(ByteReader& reader, InputFrame& frame, int player)
{
    std::vector<bool> bits;
    if(!readBoolBits(reader, 14u, bits)) {
        return false;
    }

    if(player == 1) {
        frame.vbP1A = bits[0];
        frame.vbP1B = bits[1];
        frame.vbP1Select = bits[2];
        frame.vbP1Start = bits[3];
        frame.vbP1Up0 = bits[4];
        frame.vbP1Down0 = bits[5];
        frame.vbP1Left0 = bits[6];
        frame.vbP1Right0 = bits[7];
        frame.vbP1Up1 = bits[8];
        frame.vbP1Down1 = bits[9];
        frame.vbP1Left1 = bits[10];
        frame.vbP1Right1 = bits[11];
        frame.vbP1L = bits[12];
        frame.vbP1R = bits[13];
        return true;
    }

    if(player == 2) {
        frame.vbP2A = bits[0];
        frame.vbP2B = bits[1];
        frame.vbP2Select = bits[2];
        frame.vbP2Start = bits[3];
        frame.vbP2Up0 = bits[4];
        frame.vbP2Down0 = bits[5];
        frame.vbP2Left0 = bits[6];
        frame.vbP2Right0 = bits[7];
        frame.vbP2Up1 = bits[8];
        frame.vbP2Down1 = bits[9];
        frame.vbP2Left1 = bits[10];
        frame.vbP2Right1 = bits[11];
        frame.vbP2L = bits[12];
        frame.vbP2R = bits[13];
        return true;
    }

    return false;
}

void appendDevicePayload(std::vector<uint8_t>& bytes, const InputFrame& frame, const std::optional<Settings::Device>& device, int port)
{
    if(!device.has_value()) {
        return;
    }

    switch(*device) {
        case Settings::Device::CONTROLLER:
        case Settings::Device::FAMICOM_CONTROLLER:
            appendPortControllerButtons(bytes, frame, port, false);
            break;
        case Settings::Device::SNES_CONTROLLER:
            appendPortControllerButtons(bytes, frame, port, true);
            break;
        case Settings::Device::VIRTUAL_BOY_CONTROLLER:
            appendVirtualBoyButtons(bytes, frame, port);
            break;
        case Settings::Device::ZAPPER:
            if(port == 1) {
                appendI16(bytes, static_cast<int16_t>(frame.zapperP1X));
                appendI16(bytes, static_cast<int16_t>(frame.zapperP1Y));
                appendBoolBits(bytes, {frame.zapperP1Trigger});
            } else {
                appendI16(bytes, static_cast<int16_t>(frame.zapperP2X));
                appendI16(bytes, static_cast<int16_t>(frame.zapperP2Y));
                appendBoolBits(bytes, {frame.zapperP2Trigger});
            }
            break;
        case Settings::Device::ARKANOID_CONTROLLER:
            appendU16(bytes, quantizeNormalizedPosition(port == 1 ? frame.arkanoidP1Position : frame.arkanoidP2Position));
            appendBoolBits(bytes, {port == 1 ? frame.arkanoidP1Button : frame.arkanoidP2Button});
            break;
        case Settings::Device::SNES_MOUSE:
            appendI16(bytes, static_cast<int16_t>(port == 1 ? frame.snesMouseP1DeltaX : frame.snesMouseP2DeltaX));
            appendI16(bytes, static_cast<int16_t>(port == 1 ? frame.snesMouseP1DeltaY : frame.snesMouseP2DeltaY));
            appendBoolBits(bytes, {port == 1 ? frame.snesMouseP1Left : frame.snesMouseP2Left,
                                   port == 1 ? frame.snesMouseP1Right : frame.snesMouseP2Right});
            break;
        case Settings::Device::SUBOR_MOUSE:
            appendI16(bytes, static_cast<int16_t>(port == 1 ? frame.suborMouseP1DeltaX : frame.suborMouseP2DeltaX));
            appendI16(bytes, static_cast<int16_t>(port == 1 ? frame.suborMouseP1DeltaY : frame.suborMouseP2DeltaY));
            appendBoolBits(bytes, {port == 1 ? frame.suborMouseP1Left : frame.suborMouseP2Left,
                                   port == 1 ? frame.suborMouseP1Right : frame.suborMouseP2Right});
            break;
        case Settings::Device::POWER_PAD_SIDE_A:
        case Settings::Device::POWER_PAD_SIDE_B:
            appendBoolArrayBits(bytes, port == 1 ? frame.powerPadP1Buttons : frame.powerPadP2Buttons);
            break;
        case Settings::Device::BANDAI_HYPERSHOT:
        case Settings::Device::NONE:
            break;
    }
}

bool readDevicePayload(ByteReader& reader, InputFrame& frame, const std::optional<Settings::Device>& device, int port)
{
    if(!device.has_value()) {
        return true;
    }

    switch(*device) {
        case Settings::Device::CONTROLLER:
        case Settings::Device::FAMICOM_CONTROLLER:
            return readPortControllerButtons(reader, frame, port, false);
        case Settings::Device::SNES_CONTROLLER:
            return readPortControllerButtons(reader, frame, port, true);
        case Settings::Device::VIRTUAL_BOY_CONTROLLER:
            return readVirtualBoyButtons(reader, frame, port);
        case Settings::Device::ZAPPER:
            if(port == 1) {
                int16_t x = 0;
                int16_t y = 0;
                if(!reader.readI16(x) || !reader.readI16(y)) {
                    return false;
                }
                std::vector<bool> bits;
                if(!readBoolBits(reader, 1u, bits)) {
                    return false;
                }
                frame.zapperP1X = x;
                frame.zapperP1Y = y;
                frame.zapperP1Trigger = bits[0];
            } else {
                int16_t x = 0;
                int16_t y = 0;
                if(!reader.readI16(x) || !reader.readI16(y)) {
                    return false;
                }
                std::vector<bool> bits;
                if(!readBoolBits(reader, 1u, bits)) {
                    return false;
                }
                frame.zapperP2X = x;
                frame.zapperP2Y = y;
                frame.zapperP2Trigger = bits[0];
            }
            return true;
        case Settings::Device::ARKANOID_CONTROLLER: {
            uint16_t position = 0u;
            if(!reader.readU16(position)) {
                return false;
            }
            std::vector<bool> bits;
            if(!readBoolBits(reader, 1u, bits)) {
                return false;
            }
            if(port == 1) {
                frame.arkanoidP1Position = dequantizeNormalizedPosition(position);
                frame.arkanoidP1Button = bits[0];
            } else {
                frame.arkanoidP2Position = dequantizeNormalizedPosition(position);
                frame.arkanoidP2Button = bits[0];
            }
            return true;
        }
        case Settings::Device::SNES_MOUSE: {
            int16_t deltaX = 0;
            int16_t deltaY = 0;
            if(!reader.readI16(deltaX) || !reader.readI16(deltaY)) {
                return false;
            }
            std::vector<bool> bits;
            if(!readBoolBits(reader, 2u, bits)) {
                return false;
            }
            if(port == 1) {
                frame.snesMouseP1DeltaX = deltaX;
                frame.snesMouseP1DeltaY = deltaY;
                frame.snesMouseP1Left = bits[0];
                frame.snesMouseP1Right = bits[1];
            } else {
                frame.snesMouseP2DeltaX = deltaX;
                frame.snesMouseP2DeltaY = deltaY;
                frame.snesMouseP2Left = bits[0];
                frame.snesMouseP2Right = bits[1];
            }
            return true;
        }
        case Settings::Device::SUBOR_MOUSE: {
            int16_t deltaX = 0;
            int16_t deltaY = 0;
            if(!reader.readI16(deltaX) || !reader.readI16(deltaY)) {
                return false;
            }
            std::vector<bool> bits;
            if(!readBoolBits(reader, 2u, bits)) {
                return false;
            }
            if(port == 1) {
                frame.suborMouseP1DeltaX = deltaX;
                frame.suborMouseP1DeltaY = deltaY;
                frame.suborMouseP1Left = bits[0];
                frame.suborMouseP1Right = bits[1];
            } else {
                frame.suborMouseP2DeltaX = deltaX;
                frame.suborMouseP2DeltaY = deltaY;
                frame.suborMouseP2Left = bits[0];
                frame.suborMouseP2Right = bits[1];
            }
            return true;
        }
        case Settings::Device::POWER_PAD_SIDE_A:
        case Settings::Device::POWER_PAD_SIDE_B:
            if(port == 1) {
                return readBoolArrayBits(reader, frame.powerPadP1Buttons);
            }
            return readBoolArrayBits(reader, frame.powerPadP2Buttons);
        case Settings::Device::BANDAI_HYPERSHOT:
        case Settings::Device::NONE:
            return true;
    }

    return true;
}

void appendExpansionPayload(std::vector<uint8_t>& bytes, const InputFrame& frame, Settings::ExpansionDevice device)
{
    switch(device) {
        case Settings::ExpansionDevice::NONE:
        case Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM:
            break;
        case Settings::ExpansionDevice::BANDAI_HYPERSHOT:
            appendBoolBits(bytes, {frame.bandaiA, frame.bandaiB, frame.bandaiSelect, frame.bandaiStart,
                                   frame.bandaiUp, frame.bandaiDown, frame.bandaiLeft, frame.bandaiRight,
                                   frame.bandaiTrigger});
            appendI16(bytes, static_cast<int16_t>(frame.bandaiX));
            appendI16(bytes, static_cast<int16_t>(frame.bandaiY));
            break;
        case Settings::ExpansionDevice::KONAMI_HYPERSHOT:
            appendBoolBits(bytes, {frame.konamiP1Run, frame.konamiP1Jump, frame.konamiP2Run, frame.konamiP2Jump});
            break;
        case Settings::ExpansionDevice::ARKANOID_CONTROLLER:
            appendU16(bytes, quantizeNormalizedPosition(frame.arkanoidFamicomPosition));
            appendBoolBits(bytes, {frame.arkanoidFamicomButton});
            break;
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A:
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B:
            appendBoolArrayBits(bytes, frame.powerPadP2Buttons);
            break;
        case Settings::ExpansionDevice::SUBOR_KEYBOARD:
            appendBoolArrayBits(bytes, frame.suborKeyboardKeys);
            break;
        case Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD:
            appendBoolArrayBits(bytes, frame.familyBasicKeyboardKeys);
            break;
    }
}

bool readExpansionPayload(ByteReader& reader, InputFrame& frame, Settings::ExpansionDevice device)
{
    switch(device) {
        case Settings::ExpansionDevice::NONE:
        case Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM:
            return true;
        case Settings::ExpansionDevice::BANDAI_HYPERSHOT: {
            std::vector<bool> bits;
            int16_t x = 0;
            int16_t y = 0;
            if(!readBoolBits(reader, 9u, bits) || !reader.readI16(x) || !reader.readI16(y)) {
                return false;
            }
            frame.bandaiA = bits[0];
            frame.bandaiB = bits[1];
            frame.bandaiSelect = bits[2];
            frame.bandaiStart = bits[3];
            frame.bandaiUp = bits[4];
            frame.bandaiDown = bits[5];
            frame.bandaiLeft = bits[6];
            frame.bandaiRight = bits[7];
            frame.bandaiTrigger = bits[8];
            frame.bandaiX = x;
            frame.bandaiY = y;
            return true;
        }
        case Settings::ExpansionDevice::KONAMI_HYPERSHOT: {
            std::vector<bool> bits;
            if(!readBoolBits(reader, 4u, bits)) {
                return false;
            }
            frame.konamiP1Run = bits[0];
            frame.konamiP1Jump = bits[1];
            frame.konamiP2Run = bits[2];
            frame.konamiP2Jump = bits[3];
            return true;
        }
        case Settings::ExpansionDevice::ARKANOID_CONTROLLER: {
            uint16_t position = 0u;
            if(!reader.readU16(position)) {
                return false;
            }
            std::vector<bool> bits;
            if(!readBoolBits(reader, 1u, bits)) {
                return false;
            }
            frame.arkanoidFamicomPosition = dequantizeNormalizedPosition(position);
            frame.arkanoidFamicomButton = bits[0];
            return true;
        }
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A:
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B:
            return readBoolArrayBits(reader, frame.powerPadP2Buttons);
        case Settings::ExpansionDevice::SUBOR_KEYBOARD:
            return readBoolArrayBits(reader, frame.suborKeyboardKeys);
        case Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD:
            return readBoolArrayBits(reader, frame.familyBasicKeyboardKeys);
    }

    return true;
}

std::vector<uint8_t> serializeBinaryFramePayload(const InputFrame& frame, const IEmulationHost::InputTopologySnapshot& topology)
{
    std::vector<uint8_t> bytes;
    if(topology.nesMultitapDevice == Settings::NesMultitapDevice::FOUR_SCORE ||
       topology.famicomMultitapDevice == Settings::FamicomMultitapDevice::HORI_ADAPTER) {
        appendPortControllerButtons(bytes, frame, 1, false);
        appendPortControllerButtons(bytes, frame, 2, false);
        appendPortControllerButtons(bytes, frame, 3, false);
        appendPortControllerButtons(bytes, frame, 4, false);
    } else {
        appendDevicePayload(bytes, frame, topology.port1Device, 1);
        appendDevicePayload(bytes, frame, topology.port2Device, 2);
    }
    appendExpansionPayload(bytes, frame, topology.expansionDevice);
    return bytes;
}

bool deserializeBinaryFramePayload(ByteReader& reader,
                                   const IEmulationHost::InputTopologySnapshot& topology,
                                   uint32_t frameNumber,
                                   InputFrame& frame)
{
    initializeFrameTopology(frame, topology, frameNumber);

    if(topology.nesMultitapDevice == Settings::NesMultitapDevice::FOUR_SCORE ||
       topology.famicomMultitapDevice == Settings::FamicomMultitapDevice::HORI_ADAPTER) {
        if(!readPortControllerButtons(reader, frame, 1, false) ||
           !readPortControllerButtons(reader, frame, 2, false) ||
           !readPortControllerButtons(reader, frame, 3, false) ||
           !readPortControllerButtons(reader, frame, 4, false)) {
            return false;
        }
    } else {
        if(!readDevicePayload(reader, frame, topology.port1Device, 1) ||
           !readDevicePayload(reader, frame, topology.port2Device, 2)) {
            return false;
        }
    }

    return readExpansionPayload(reader, frame, topology.expansionDevice);
}

void appendTopologyBytes(std::vector<uint8_t>& bytes, const IEmulationHost::InputTopologySnapshot& topology)
{
    appendU8(bytes, portDeviceToByte(topology.port1Device));
    appendU8(bytes, portDeviceToByte(topology.port2Device));
    appendU8(bytes, static_cast<uint8_t>(topology.expansionDevice));
    appendU8(bytes, static_cast<uint8_t>(topology.nesMultitapDevice));
    appendU8(bytes, static_cast<uint8_t>(topology.famicomMultitapDevice));
}

bool readTopologyBytes(ByteReader& reader, IEmulationHost::InputTopologySnapshot& topology)
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

    topology.port1Device = portDeviceFromByte(port1Device);
    topology.port2Device = portDeviceFromByte(port2Device);
    topology.expansionDevice = static_cast<Settings::ExpansionDevice>(expansionDevice);
    topology.nesMultitapDevice = static_cast<Settings::NesMultitapDevice>(nesMultitapDevice);
    topology.famicomMultitapDevice = static_cast<Settings::FamicomMultitapDevice>(famicomMultitapDevice);
    return true;
}

struct EncodedRun
{
    uint32_t repeatCount = 0u;
    std::vector<uint8_t> payload;
};

bool buildEncodedRuns(const std::vector<InputFrame>& frames,
                      const IEmulationHost::InputTopologySnapshot& topology,
                      std::vector<EncodedRun>& runs,
                      std::string& error)
{
    runs.clear();
    runs.reserve(frames.size());

    for(const InputFrame& frame : frames) {
        std::vector<uint8_t> payload = serializeBinaryFramePayload(frame, topology);
        if(!runs.empty() &&
           runs.back().repeatCount < std::numeric_limits<uint32_t>::max() &&
           runs.back().payload == payload) {
            ++runs.back().repeatCount;
            continue;
        }

        EncodedRun run;
        run.repeatCount = 1u;
        run.payload = std::move(payload);
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

    appendU8(bytes, data.bootstrapFrame.has_value() ? 1u : 0u);
    if(data.bootstrapFrame.has_value()) {
        const std::vector<uint8_t> bootstrapPayload =
            serializeBinaryFramePayload(*data.bootstrapFrame, data.inputTopology);
        bytes.insert(bytes.end(), bootstrapPayload.begin(), bootstrapPayload.end());
    }

    std::vector<EncodedRun> runs;
    if(!buildEncodedRuns(data.frames, data.inputTopology, runs, error)) {
        return false;
    }

    appendU32(bytes, static_cast<uint32_t>(runs.size()));
    for(const EncodedRun& run : runs) {
        appendU32(bytes, run.repeatCount);
        bytes.insert(bytes.end(), run.payload.begin(), run.payload.end());
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
    if(version != kReplayBinaryVersion) {
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

    uint8_t bootstrapPresent = 0u;
    if(!reader.readU8(bootstrapPresent)) {
        error = reader.error();
        return false;
    }
    if(bootstrapPresent != 0u) {
        InputFrame bootstrapFrame;
        if(!deserializeBinaryFramePayload(reader, loadedData.inputTopology, 0u, bootstrapFrame)) {
            error = reader.error();
            return false;
        }
        loadedData.bootstrapFrame = bootstrapFrame;
    }

    uint32_t runCount = 0u;
    if(!reader.readU32(runCount)) {
        error = reader.error();
        return false;
    }

    uint32_t nextFrameNumber = 1u;
    for(uint32_t runIndex = 0u; runIndex < runCount; ++runIndex) {
        uint32_t repeatCount = 0u;
        if(!reader.readU32(repeatCount)) {
            error = reader.error();
            return false;
        }
        if(repeatCount == 0u) {
            error = "Replay file contains an invalid empty input run";
            return false;
        }

        InputFrame decodedFrame;
        if(!deserializeBinaryFramePayload(reader, loadedData.inputTopology, nextFrameNumber, decodedFrame)) {
            error = reader.error();
            return false;
        }

        if(loadedData.frames.size() > (std::numeric_limits<size_t>::max() - repeatCount)) {
            error = "Replay file is too large";
            return false;
        }

        for(uint32_t i = 0u; i < repeatCount; ++i) {
            InputFrame frame = decodedFrame;
            frame.frame = nextFrameNumber++;
            loadedData.frames.push_back(frame);
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
