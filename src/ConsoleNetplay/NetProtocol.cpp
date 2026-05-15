#include "ConsoleNetplay/NetProtocol.h"

#include <algorithm>
#include <limits>
#include <span>

#include "ConsoleNetplay/NetSerialization.h"

namespace ConsoleNetplay {

void PacketHeader::serialize(PacketWriter& writer) const
{
    writer.writePod(protocolVersion);
    writer.writePod(type);
    writer.writePod(sessionId);
}

bool PacketHeader::deserialize(PacketReader& reader, PacketHeader& header)
{
    return reader.readPod(header.protocolVersion) &&
           reader.readPod(header.type) &&
           reader.readPod(header.sessionId);
}

void RomValidationData::serialize(PacketWriter& writer) const
{
    writer.writePod(romCrc32);
    writer.writePod(mapperId);
    writer.writePod(subMapperId);
    writer.writePod(prgRomSize);
    writer.writePod(chrRomSize);
    writer.writePod(chrRamSize);
    writer.writePod(fileSize);
    writer.writeBytes(std::span<const uint8_t>(contentHash.data(), contentHash.size()));
}

bool RomValidationData::deserialize(PacketReader& reader, RomValidationData& data)
{
    std::vector<uint8_t> hashBytes;
    if(!reader.readPod(data.romCrc32) ||
       !reader.readPod(data.mapperId) ||
       !reader.readPod(data.subMapperId) ||
       !reader.readPod(data.prgRomSize) ||
       !reader.readPod(data.chrRomSize) ||
       !reader.readPod(data.chrRamSize) ||
       !reader.readPod(data.fileSize) ||
       !reader.readBytes(hashBytes, data.contentHash.size())) {
        return false;
    }
    std::copy(hashBytes.begin(), hashBytes.end(), data.contentHash.begin());
    return true;
}

void JoinRoomData::serialize(PacketWriter& writer) const
{
    writer.writePod(reconnectToken);
    writer.writePod(romLoaded);
    romValidation.serialize(writer);
}

bool JoinRoomData::deserialize(PacketReader& reader, JoinRoomData& data)
{
    return reader.readPod(data.reconnectToken) &&
           reader.readPod(data.romLoaded) &&
           RomValidationData::deserialize(reader, data.romValidation);
}

void JoinRejectedData::serialize(PacketWriter& writer) const
{
    writer.writePod(reason);
    romValidation.serialize(writer);
}

bool JoinRejectedData::deserialize(PacketReader& reader, JoinRejectedData& data)
{
    return reader.readPod(data.reason) &&
           RomValidationData::deserialize(reader, data.romValidation);
}

void RomValidationResultData::serialize(PacketWriter& writer) const
{
    writer.writePod(participantId);
    writer.writePod(romLoaded);
    writer.writePod(romCompatible);
    romValidation.serialize(writer);
}

bool RomValidationResultData::deserialize(PacketReader& reader, RomValidationResultData& data)
{
    return reader.readPod(data.participantId) &&
           reader.readPod(data.romLoaded) &&
           reader.readPod(data.romCompatible) &&
           RomValidationData::deserialize(reader, data.romValidation);
}

void InputTopologyData::Slot::serialize(PacketWriter& writer) const
{
    writer.writePod(slot);
    writer.writePod(assignable);
    writer.writePod(groupId);
    writer.writePod(deviceId);
    writer.writeString(groupLabel);
    writer.writeString(inputLabel);
}

bool InputTopologyData::Slot::deserialize(PacketReader& reader, Slot& slotData)
{
    return reader.readPod(slotData.slot) &&
           reader.readPod(slotData.assignable) &&
           reader.readPod(slotData.groupId) &&
           reader.readPod(slotData.deviceId) &&
           reader.readString(slotData.groupLabel) &&
           reader.readString(slotData.inputLabel);
}

size_t InputTopologyData::Slot::serializedSize() const
{
    return sizeof(PlayerSlot) +
           sizeof(uint8_t) +
           sizeof(InputGroupId) +
           sizeof(InputDeviceId) +
           sizeof(uint16_t) + groupLabel.size() +
           sizeof(uint16_t) + inputLabel.size();
}

void InputTopologyData::serialize(PacketWriter& writer) const
{
    const uint8_t slotCount = static_cast<uint8_t>(std::min<size_t>(slots.size(), std::numeric_limits<uint8_t>::max()));
    writer.writePod(slotCount);
    for(uint8_t index = 0; index < slotCount; ++index) {
        slots[index].serialize(writer);
    }
}

bool InputTopologyData::deserialize(PacketReader& reader, InputTopologyData& data)
{
    uint8_t slotCount = 0;
    if(!reader.readPod(slotCount)) return false;
    data.slots.clear();
    data.slots.reserve(slotCount);
    for(uint8_t index = 0; index < slotCount; ++index) {
        Slot slot;
        if(!Slot::deserialize(reader, slot)) return false;
        data.slots.push_back(std::move(slot));
    }
    return true;
}

size_t InputTopologyData::serializedSize() const
{
    size_t size = sizeof(uint8_t);
    for(const Slot& slot : slots) {
        size += slot.serializedSize();
    }
    return size;
}

void InputFrameData::serialize(PacketWriter& writer) const
{
    writer.writePod(timelineEpoch);
    writer.writePod(frame);
    writer.writePod(authoritativeFrameStartClockMicros);
    writer.writePod(participantId);
    writer.writePod(playerSlot);
    writer.writePod(buttonMaskLo);
    writer.writePod(buttonMaskHi);
    writer.writePod(sequence);
    writer.writePod(payloadSize);
}

bool InputFrameData::deserialize(PacketReader& reader, InputFrameData& data)
{
    return reader.readPod(data.timelineEpoch) &&
           reader.readPod(data.frame) &&
           reader.readPod(data.authoritativeFrameStartClockMicros) &&
           reader.readPod(data.participantId) &&
           reader.readPod(data.playerSlot) &&
           reader.readPod(data.buttonMaskLo) &&
           reader.readPod(data.buttonMaskHi) &&
           reader.readPod(data.sequence) &&
           reader.readPod(data.payloadSize);
}

void ConfirmedInputFramesData::serialize(PacketWriter& writer) const
{
    writer.writePod(timelineEpoch);
    writer.writePod(startFrame);
    writer.writePod(frameCount);
}

bool ConfirmedInputFramesData::deserialize(PacketReader& reader, ConfirmedInputFramesData& data)
{
    return reader.readPod(data.timelineEpoch) &&
           reader.readPod(data.startFrame) &&
           reader.readPod(data.frameCount);
}

void ConfirmedInputFrameEntry::SlotMask::serialize(PacketWriter& writer) const
{
    writer.writePod(slot);
    writer.writePod(buttonMaskLo);
    writer.writePod(buttonMaskHi);
}

bool ConfirmedInputFrameEntry::SlotMask::deserialize(PacketReader& reader, SlotMask& slotMask)
{
    return reader.readPod(slotMask.slot) &&
           reader.readPod(slotMask.buttonMaskLo) &&
           reader.readPod(slotMask.buttonMaskHi);
}

void ConfirmedInputFrameEntry::serialize(PacketWriter& writer) const
{
    writer.writePod(authoritativeFrameStartClockMicros);
    const uint8_t slotMaskCount = static_cast<uint8_t>(std::min<size_t>(slotMasks.size(), std::numeric_limits<uint8_t>::max()));
    writer.writePod(slotMaskCount);
    for(uint8_t index = 0; index < slotMaskCount; ++index) {
        slotMasks[index].serialize(writer);
    }
    writer.writePod(payloadSize);
}

bool ConfirmedInputFrameEntry::deserialize(PacketReader& reader, ConfirmedInputFrameEntry& entry)
{
    if(!reader.readPod(entry.authoritativeFrameStartClockMicros)) return false;
    uint8_t slotMaskCount = 0;
    if(!reader.readPod(slotMaskCount)) return false;
    entry.slotMasks.clear();
    entry.slotMasks.reserve(slotMaskCount);
    for(uint8_t index = 0; index < slotMaskCount; ++index) {
        SlotMask slotMask;
        if(!SlotMask::deserialize(reader, slotMask)) return false;
        entry.slotMasks.push_back(slotMask);
    }
    return reader.readPod(entry.payloadSize);
}

size_t ConfirmedInputFrameEntry::serializedSize() const
{
    return sizeof(authoritativeFrameStartClockMicros) +
           sizeof(uint8_t) +
           (sizeof(PlayerSlot) + sizeof(uint64_t) + sizeof(uint64_t)) * slotMasks.size() +
           sizeof(payloadSize);
}

void InputAckData::serialize(PacketWriter& writer) const
{
    writer.writePod(timelineEpoch);
    writer.writePod(participantId);
    writer.writePod(playerSlot);
    writer.writePod(contiguousFrame);
    writer.writePod(sequence);
}

bool InputAckData::deserialize(PacketReader& reader, InputAckData& data)
{
    return reader.readPod(data.timelineEpoch) &&
           reader.readPod(data.participantId) &&
           reader.readPod(data.playerSlot) &&
           reader.readPod(data.contiguousFrame) &&
           reader.readPod(data.sequence);
}

void InputResendRequestData::serialize(PacketWriter& writer) const
{
    writer.writePod(timelineEpoch);
    writer.writePod(participantId);
    writer.writePod(playerSlot);
    writer.writePod(startFrame);
    writer.writePod(frameCount);
}

bool InputResendRequestData::deserialize(PacketReader& reader, InputResendRequestData& data)
{
    return reader.readPod(data.timelineEpoch) &&
           reader.readPod(data.participantId) &&
           reader.readPod(data.playerSlot) &&
           reader.readPod(data.startFrame) &&
           reader.readPod(data.frameCount);
}

void FrameStatusData::serialize(PacketWriter& writer) const
{
    writer.writePod(timelineEpoch);
    writer.writePod(currentFrame);
    writer.writePod(lastConfirmedFrame);
    writer.writePod(inputDelayFrames);
    writer.writePod(predictFrames);
    topology.serialize(writer);
}

bool FrameStatusData::deserialize(PacketReader& reader, FrameStatusData& data)
{
    return reader.readPod(data.timelineEpoch) &&
           reader.readPod(data.currentFrame) &&
           reader.readPod(data.lastConfirmedFrame) &&
           reader.readPod(data.inputDelayFrames) &&
           reader.readPod(data.predictFrames) &&
           InputTopologyData::deserialize(reader, data.topology);
}

size_t FrameStatusData::serializedSize() const
{
    return sizeof(timelineEpoch) +
           sizeof(currentFrame) +
           sizeof(lastConfirmedFrame) +
           sizeof(inputDelayFrames) +
           sizeof(predictFrames) +
           topology.serializedSize();
}

void AssignControllerData::serialize(PacketWriter& writer) const
{
    writer.writePod(participantId);
    const uint8_t count = static_cast<uint8_t>(std::min<size_t>(controllerAssignments.size(), std::numeric_limits<uint8_t>::max()));
    writer.writePod(count);
    for(uint8_t index = 0; index < count; ++index) {
        writer.writePod(controllerAssignments[index]);
    }
}

bool AssignControllerData::deserialize(PacketReader& reader, AssignControllerData& data)
{
    if(!reader.readPod(data.participantId)) return false;
    uint8_t count = 0;
    if(!reader.readPod(count)) return false;
    data.controllerAssignments.clear();
    data.controllerAssignments.reserve(count);
    for(uint8_t index = 0; index < count; ++index) {
        PlayerSlot slot = kObserverPlayerSlot;
        if(!reader.readPod(slot)) return false;
        data.controllerAssignments.push_back(slot);
    }
    return true;
}

size_t AssignControllerData::serializedSize() const
{
    return sizeof(ParticipantId) + sizeof(uint8_t) + sizeof(PlayerSlot) * controllerAssignments.size();
}

void ParticipantLeftData::serialize(PacketWriter& writer) const
{
    writer.writePod(participantId);
    writer.writePod(disconnectReason);
}

bool ParticipantLeftData::deserialize(PacketReader& reader, ParticipantLeftData& data)
{
    return reader.readPod(data.participantId) &&
           reader.readPod(data.disconnectReason);
}

void LeaveRoomData::serialize(PacketWriter& writer) const
{
    writer.writePod(participantId);
}

bool LeaveRoomData::deserialize(PacketReader& reader, LeaveRoomData& data)
{
    return reader.readPod(data.participantId);
}

void StartSessionData::serialize(PacketWriter& writer) const
{
    writer.writePod(state);
    writer.writePod(inputDelayFrames);
    writer.writePod(predictFrames);
    writer.writePod(postResyncTimeAlignFrame);
    writer.writePod(postResyncTimeAlignClockMicros);
    topology.serialize(writer);
}

bool StartSessionData::deserialize(PacketReader& reader, StartSessionData& data)
{
    return reader.readPod(data.state) &&
           reader.readPod(data.inputDelayFrames) &&
           reader.readPod(data.predictFrames) &&
           reader.readPod(data.postResyncTimeAlignFrame) &&
           reader.readPod(data.postResyncTimeAlignClockMicros) &&
           InputTopologyData::deserialize(reader, data.topology);
}

size_t StartSessionData::serializedSize() const
{
    return sizeof(state) +
           sizeof(inputDelayFrames) +
           sizeof(predictFrames) +
           sizeof(postResyncTimeAlignFrame) +
           sizeof(postResyncTimeAlignClockMicros) +
           topology.serializedSize();
}

void PeerHealthData::serialize(PacketWriter& writer) const
{
    writer.writePod(participantId);
    writer.writePod(currentFrame);
    writer.writePod(lastConfirmedFrame);
    writer.writePod(lastProducedLocalInputFrame);
    writer.writePod(lastProducedLocalInputSequence);
    writer.writePod(localAssignmentCount);
    writer.writePod(lastLocalInputRejectReason);
    writer.writePod(lastLocalInputRejectFrame);
    writer.writePod(lastLocalInputRejectExpectedFrame);
    writer.writePod(pingMs);
    writer.writePod(jitterMs);
    writer.writePod(sharedClockMicros);
    writer.writePod(clockSyncRttMicros);
    writer.writePod(sharedClockSynchronized);
}

bool PeerHealthData::deserialize(PacketReader& reader, PeerHealthData& data)
{
    return reader.readPod(data.participantId) &&
           reader.readPod(data.currentFrame) &&
           reader.readPod(data.lastConfirmedFrame) &&
           reader.readPod(data.lastProducedLocalInputFrame) &&
           reader.readPod(data.lastProducedLocalInputSequence) &&
           reader.readPod(data.localAssignmentCount) &&
           reader.readPod(data.lastLocalInputRejectReason) &&
           reader.readPod(data.lastLocalInputRejectFrame) &&
           reader.readPod(data.lastLocalInputRejectExpectedFrame) &&
           reader.readPod(data.pingMs) &&
           reader.readPod(data.jitterMs) &&
           reader.readPod(data.sharedClockMicros) &&
           reader.readPod(data.clockSyncRttMicros) &&
           reader.readPod(data.sharedClockSynchronized);
}

void ClockSyncRequestData::serialize(PacketWriter& writer) const
{
    writer.writePod(sequence);
    writer.writePod(clientSendMicros);
}

bool ClockSyncRequestData::deserialize(PacketReader& reader, ClockSyncRequestData& data)
{
    return reader.readPod(data.sequence) &&
           reader.readPod(data.clientSendMicros);
}

void ClockSyncResponseData::serialize(PacketWriter& writer) const
{
    writer.writePod(sequence);
    writer.writePod(clientSendMicros);
    writer.writePod(hostReceiveMicros);
    writer.writePod(hostSendMicros);
}

bool ClockSyncResponseData::deserialize(PacketReader& reader, ClockSyncResponseData& data)
{
    return reader.readPod(data.sequence) &&
           reader.readPod(data.clientSendMicros) &&
           reader.readPod(data.hostReceiveMicros) &&
           reader.readPod(data.hostSendMicros);
}

void CrcReportData::serialize(PacketWriter& writer) const
{
    writer.writePod(timelineEpoch);
    writer.writePod(frame);
    writer.writePod(crc32);
    writer.writePod(severity);
    writer.writePod(submissionSource);
    writer.writePod(senderLocalSimulationFrame);
    writer.writePod(senderConfirmedFrame);
}

bool CrcReportData::deserialize(PacketReader& reader, CrcReportData& data)
{
    return reader.readPod(data.timelineEpoch) &&
           reader.readPod(data.frame) &&
           reader.readPod(data.crc32) &&
           reader.readPod(data.severity) &&
           reader.readPod(data.submissionSource) &&
           reader.readPod(data.senderLocalSimulationFrame) &&
           reader.readPod(data.senderConfirmedFrame);
}

void ResyncBeginData::serialize(PacketWriter& writer) const
{
    writer.writePod(resyncId);
    writer.writePod(timelineEpoch);
    writer.writePod(targetFrame);
    writer.writePod(confirmedFrame);
    writer.writePod(frameReadyFrame);
    writer.writePod(payloadSize);
    writer.writePod(payloadCrc32);
    writer.writePod(stateCrc32);
    writer.writePod(frameReadyCrc32);
    writer.writePod(inputSequenceBase);
    writer.writePod(reason);
}

bool ResyncBeginData::deserialize(PacketReader& reader, ResyncBeginData& data)
{
    return reader.readPod(data.resyncId) &&
           reader.readPod(data.timelineEpoch) &&
           reader.readPod(data.targetFrame) &&
           reader.readPod(data.confirmedFrame) &&
           reader.readPod(data.frameReadyFrame) &&
           reader.readPod(data.payloadSize) &&
           reader.readPod(data.payloadCrc32) &&
           reader.readPod(data.stateCrc32) &&
           reader.readPod(data.frameReadyCrc32) &&
           reader.readPod(data.inputSequenceBase) &&
           reader.readPod(data.reason);
}

void ResyncChunkData::serialize(PacketWriter& writer) const
{
    writer.writePod(resyncId);
    writer.writePod(offset);
    writer.writePod(size);
}

bool ResyncChunkData::deserialize(PacketReader& reader, ResyncChunkData& data)
{
    return reader.readPod(data.resyncId) &&
           reader.readPod(data.offset) &&
           reader.readPod(data.size);
}

void ResyncCompleteData::serialize(PacketWriter& writer) const
{
    writer.writePod(resyncId);
}

bool ResyncCompleteData::deserialize(PacketReader& reader, ResyncCompleteData& data)
{
    return reader.readPod(data.resyncId);
}

void ResyncAckData::serialize(PacketWriter& writer) const
{
    writer.writePod(resyncId);
    writer.writePod(participantId);
    writer.writePod(loadedFrame);
    writer.writePod(crc32);
    writer.writePod(success);
}

bool ResyncAckData::deserialize(PacketReader& reader, ResyncAckData& data)
{
    return reader.readPod(data.resyncId) &&
           reader.readPod(data.participantId) &&
           reader.readPod(data.loadedFrame) &&
           reader.readPod(data.crc32) &&
           reader.readPod(data.success);
}

void ResyncAbortData::serialize(PacketWriter& writer) const
{
    writer.writePod(resyncId);
    writer.writePod(participantId);
    writer.writePod(reason);
}

bool ResyncAbortData::deserialize(PacketReader& reader, ResyncAbortData& data)
{
    return reader.readPod(data.resyncId) &&
           reader.readPod(data.participantId) &&
           reader.readPod(data.reason);
}

void ResyncRequestData::serialize(PacketWriter& writer) const
{
    writer.writePod(participantId);
    writer.writePod(reason);
    writer.writePod(localFrame);
    writer.writePod(estimatedHostFrame);
    writer.writePod(confirmedThroughFrame);
    writer.writePod(lagFrames);
    writer.writePod(catchupBudgetFrames);
    writer.writePod(source);
    writer.writePod(flags);
}

bool ResyncRequestData::deserialize(PacketReader& reader, ResyncRequestData& data)
{
    return reader.readPod(data.participantId) &&
           reader.readPod(data.reason) &&
           reader.readPod(data.localFrame) &&
           reader.readPod(data.estimatedHostFrame) &&
           reader.readPod(data.confirmedThroughFrame) &&
           reader.readPod(data.lagFrames) &&
           reader.readPod(data.catchupBudgetFrames) &&
           reader.readPod(data.source) &&
           reader.readPod(data.flags);
}

} // namespace ConsoleNetplay
