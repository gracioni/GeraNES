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

    writer.writePod(header);
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
        writer.writePod(slotHeader);
        writer.writeBytes(std::span<const uint8_t>(frame.slotPayloads[slot].data(), frame.slotPayloads[slot].size()));
    }
    return writer.data();
}

size_t serializedNetplayInputFrameSize(const NetplayInputFrame& frame)
{
    size_t size = sizeof(NetplayInputFrameWireHeader) + frame.framePayload.size();
    for(PlayerSlot slot : frame.activeSlots()) {
        if(slotHasPayload(frame, slot)) {
            size += sizeof(NetplayInputSlotWireHeader) + frame.slotPayloads[slot].size();
        }
    }
    return size;
}

bool deserializeNetplayInputFrame(const uint8_t* data, size_t size, NetplayInputFrame& frame)
{
    PacketReader reader(data, size);
    NetplayInputFrameWireHeader header;
    if(!reader.readPod(header)) return false;
    if(header.version != kNetplayInputFrameWireVersion) return false;

    NetplayInputFrame decoded;
    decoded.frame = header.frame;
    decoded.timelineEpoch = header.timelineEpoch;
    decoded.speculative = header.speculative != 0u;
    if(!reader.readBytes(decoded.framePayload, header.framePayloadSize)) return false;
    for(uint8_t index = 0; index < header.slotCount; ++index) {
        NetplayInputSlotWireHeader slotHeader;
        if(!reader.readPod(slotHeader)) return false;
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
