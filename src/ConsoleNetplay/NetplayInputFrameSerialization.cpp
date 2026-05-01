#include "ConsoleNetplay/NetplayInputFrameSerialization.h"

#include <span>
#include <utility>

#include "ConsoleNetplay/NetSerialization.h"

namespace ConsoleNetplay {

namespace {

constexpr uint8_t kNetplayInputFrameWireVersion = 3;

struct NetplayInputFrameWireHeader
{
    uint8_t version = kNetplayInputFrameWireVersion;
    FrameNumber frame = 0;
    uint32_t timelineEpoch = 0;
    uint8_t speculative = 0;
    uint8_t slotCount = 0;
    uint16_t framePayloadSize = 0;
};

struct NetplayInputSlotWireHeader
{
    PlayerSlot slot = kObserverPlayerSlot;
    uint64_t buttonMaskLo = 0;
    uint64_t buttonMaskHi = 0;
    uint16_t payloadSize = 0;
};

bool slotHasPayload(const NetplayInputFrame& frame, PlayerSlot slot)
{
    return frame.buttonMaskLo[slot] != 0u ||
           frame.buttonMaskHi[slot] != 0u ||
           !frame.slotPayloads[slot].empty();
}

uint8_t activeSlotCount(const NetplayInputFrame& frame)
{
    return static_cast<uint8_t>(frame.activeSlots().size());
}

void writeNetplayInputFrameWireHeader(PacketWriter& writer, const NetplayInputFrameWireHeader& header)
{
    writer.writePod(header.version);
    writer.writePod(header.frame);
    writer.writePod(header.timelineEpoch);
    writer.writePod(header.speculative);
    writer.writePod(header.slotCount);
    writer.writePod(header.framePayloadSize);
}

bool readNetplayInputFrameWireHeader(PacketReader& reader, NetplayInputFrameWireHeader& header)
{
    return reader.readPod(header.version) &&
           reader.readPod(header.frame) &&
           reader.readPod(header.timelineEpoch) &&
           reader.readPod(header.speculative) &&
           reader.readPod(header.slotCount) &&
           reader.readPod(header.framePayloadSize);
}

void writeNetplayInputSlotWireHeader(PacketWriter& writer, const NetplayInputSlotWireHeader& header)
{
    writer.writePod(header.slot);
    writer.writePod(header.buttonMaskLo);
    writer.writePod(header.buttonMaskHi);
    writer.writePod(header.payloadSize);
}

bool readNetplayInputSlotWireHeader(PacketReader& reader, NetplayInputSlotWireHeader& header)
{
    return reader.readPod(header.slot) &&
           reader.readPod(header.buttonMaskLo) &&
           reader.readPod(header.buttonMaskHi) &&
           reader.readPod(header.payloadSize);
}

} // namespace

std::vector<uint8_t> serializeNetplayInputFrame(const NetplayInputFrame& frame)
{
    PacketWriter writer;
    writer.reserve(serializedNetplayInputFrameSize(frame));

    NetplayInputFrameWireHeader header;
    header.frame = frame.frame;
    header.timelineEpoch = frame.timelineEpoch;
    header.speculative = frame.speculative ? 1u : 0u;
    header.slotCount = activeSlotCount(frame);
    header.framePayloadSize = static_cast<uint16_t>(frame.framePayload.size());

    writeNetplayInputFrameWireHeader(writer, header);
    writer.writeBytes(std::span<const uint8_t>(frame.framePayload.data(), frame.framePayload.size()));
    for(PlayerSlot slot : frame.activeSlots()) {
        if(!slotHasPayload(frame, slot)) {
            continue;
        }
        NetplayInputSlotWireHeader slotHeader;
        slotHeader.slot = slot;
        slotHeader.buttonMaskLo = frame.buttonMaskLo[slot];
        slotHeader.buttonMaskHi = frame.buttonMaskHi[slot];
        slotHeader.payloadSize = static_cast<uint16_t>(frame.slotPayloads[slot].size());
        writeNetplayInputSlotWireHeader(writer, slotHeader);
        writer.writeBytes(std::span<const uint8_t>(frame.slotPayloads[slot].data(), frame.slotPayloads[slot].size()));
    }
    return writer.data();
}

size_t serializedNetplayInputFrameSize(const NetplayInputFrame& frame)
{
    size_t size =
        sizeof(uint8_t) + sizeof(FrameNumber) + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t) +
        frame.framePayload.size();
    for(PlayerSlot slot : frame.activeSlots()) {
        if(slotHasPayload(frame, slot)) {
            size += sizeof(PlayerSlot) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint16_t) + frame.slotPayloads[slot].size();
        }
    }
    return size;
}

bool deserializeNetplayInputFrame(const uint8_t* data, size_t size, NetplayInputFrame& frame)
{
    PacketReader reader(data, size);
    NetplayInputFrameWireHeader header;
    if(!readNetplayInputFrameWireHeader(reader, header)) return false;
    if(header.version != kNetplayInputFrameWireVersion) return false;

    NetplayInputFrame decoded;
    decoded.frame = header.frame;
    decoded.timelineEpoch = header.timelineEpoch;
    decoded.speculative = header.speculative != 0u;
    if(!reader.readBytes(decoded.framePayload, header.framePayloadSize)) return false;
    for(uint8_t index = 0; index < header.slotCount; ++index) {
        NetplayInputSlotWireHeader slotHeader;
        if(!readNetplayInputSlotWireHeader(reader, slotHeader)) return false;
        if(slotHeader.buttonMaskLo != 0u) {
            decoded.buttonMaskLo[slotHeader.slot] = slotHeader.buttonMaskLo;
        }
        if(slotHeader.buttonMaskHi != 0u) {
            decoded.buttonMaskHi[slotHeader.slot] = slotHeader.buttonMaskHi;
        }
        if(slotHeader.payloadSize != 0) {
            if(!reader.readBytes(decoded.slotPayloads[slotHeader.slot], slotHeader.payloadSize)) return false;
        }
    }
    if(reader.remaining() != 0) return false;

    frame = std::move(decoded);
    return true;
}

} // namespace ConsoleNetplay
