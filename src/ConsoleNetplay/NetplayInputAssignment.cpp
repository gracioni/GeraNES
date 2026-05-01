#include "ConsoleNetplay/NetplayInputAssignment.h"

#include <algorithm>
#include <utility>


namespace ConsoleNetplay {

std::string inputAssignmentGroupLabel(PlayerSlot slot, const RoomState& room)
{
    if(const InputSlotDescriptor* descriptor = findInputSlot(room.inputTopology, slot)) {
        if(!descriptor->groupLabel.empty()) return descriptor->groupLabel;
    }
    return slot == kObserverPlayerSlot ? "Observer" : "Input " + std::to_string(static_cast<unsigned>(slot) + 1u);
}

std::string inputAssignmentLeafLabel(PlayerSlot slot, const RoomState& room)
{
    if(const InputSlotDescriptor* descriptor = findInputSlot(room.inputTopology, slot)) {
        if(!descriptor->inputLabel.empty()) return descriptor->inputLabel;
    }
    return slot == kObserverPlayerSlot ? "Observer" : "Device";
}

std::vector<PlayerSlot> participantAssignments(const ParticipantInfo& participant)
{
    if(!participant.controllerAssignments.empty()) {
        return participant.controllerAssignments;
    }
    if(participant.controllerAssignment != kObserverPlayerSlot) {
        return {participant.controllerAssignment};
    }
    return {};
}

bool participantHasAssignment(const ParticipantInfo& participant, PlayerSlot slot)
{
    if(participant.hasControllerAssignment(slot)) {
        return true;
    }
    return participant.controllerAssignments.empty() && participant.controllerAssignment == slot;
}

bool participantIsObserver(const ParticipantInfo& participant)
{
    return participant.controllerAssignments.empty() && participant.controllerAssignment == kObserverPlayerSlot;
}

void syncParticipantRoleWithAssignments(ParticipantInfo& participant, bool keepHostRole)
{
    participant.normalizeControllerAssignments();
    if(keepHostRole) {
        participant.role = ParticipantRole::SessionOwner;
    } else {
        participant.role = participantIsObserver(participant) ? ParticipantRole::Observer : ParticipantRole::SessionParticipant;
    }
}

std::string participantAssignmentsLabel(const ParticipantInfo& participant, const RoomState& room)
{
    if(participantIsObserver(participant)) {
        return "Observer";
    }

    std::string label;
    bool first = true;
    for(PlayerSlot slot : participantAssignments(participant)) {
        if(!first) label += ", ";
        label += inputAssignmentLabel(slot, room);
        first = false;
    }
    return label;
}

std::vector<PlayerSlot> availableInputAssignments(const RoomState& room)
{
    std::vector<PlayerSlot> assignments;
    for(const InputSlotDescriptor& descriptor : room.inputTopology) {
        if(descriptor.assignable && descriptor.slot != kObserverPlayerSlot) {
            assignments.push_back(descriptor.slot);
        }
    }
    return assignments;
}

bool isAssignmentAvailable(PlayerSlot slot, const RoomState& room)
{
    if(slot == kObserverPlayerSlot) return true;
    const auto assignments = availableInputAssignments(room);
    return std::find(assignments.begin(), assignments.end(), slot) != assignments.end();
}

RoomState roomWithInputTopology(RoomState room, std::vector<InputSlotDescriptor> inputTopology)
{
    room.inputTopology = std::move(inputTopology);
    for(auto& participant : room.participants) {
        participant.normalizeControllerAssignments(&room.inputTopology);
    }
    return room;
}

bool isInputAssignmentClaimedByOtherParticipant(const RoomState& room,
                                                       ParticipantId participantId,
                                                       PlayerSlot slot)
{
    if(slot == kObserverPlayerSlot) return false;

    for(const auto& participant : room.participants) {
        if(participant.id == participantId) continue;
        if(participantHasAssignment(participant, slot)) {
            return true;
        }
    }

    return false;
}

bool canAssignInputCandidate(const RoomState& room,
                                    ParticipantId participantId,
                                    const std::vector<InputSlotDescriptor>& inputTopology,
                                    PlayerSlot slot)
{
    RoomState candidateRoom = roomWithInputTopology(room, inputTopology);

    std::vector<PlayerSlot> claimedAssignments;
    claimedAssignments.reserve(candidateRoom.participants.size() * 2u);

    for(const auto& participant : candidateRoom.participants) {
        std::vector<PlayerSlot> effectiveAssignments = participantAssignments(participant);
        if(participant.id == participantId && slot != kObserverPlayerSlot &&
           std::find(effectiveAssignments.begin(), effectiveAssignments.end(), slot) == effectiveAssignments.end()) {
            effectiveAssignments.push_back(slot);
        }

        std::sort(effectiveAssignments.begin(), effectiveAssignments.end());
        effectiveAssignments.erase(std::unique(effectiveAssignments.begin(), effectiveAssignments.end()),
                                   effectiveAssignments.end());

        for(PlayerSlot effectiveAssignment : effectiveAssignments) {
            if(!isAssignmentAvailable(effectiveAssignment, candidateRoom)) {
                return false;
            }
            if(std::find(claimedAssignments.begin(), claimedAssignments.end(), effectiveAssignment) != claimedAssignments.end()) {
                return false;
            }
            claimedAssignments.push_back(effectiveAssignment);
        }
    }

    return true;
}

std::string inputAssignmentLabel(PlayerSlot slot, const RoomState& room)
{
    if(slot == kObserverPlayerSlot) {
        return "Observer";
    }
    return inputAssignmentGroupLabel(slot, room) + " - " + inputAssignmentLeafLabel(slot, room);
}

NetplayInputFrame makeRoomTopologyBaseNetplayFrame(FrameNumber frame, const RoomState& room)
{
    NetplayInputFrame inputFrame{};
    inputFrame.frame = frame;
    inputFrame.timelineEpoch = room.timelineEpoch;
    return inputFrame;
}

NetplayInputFrame makeContributionBase(const NetplayInputFrame& baseFrame)
{
    NetplayInputFrame contribution{};
    contribution.frame = baseFrame.frame;
    contribution.timelineEpoch = baseFrame.timelineEpoch;
    contribution.framePayload = baseFrame.framePayload;
    return contribution;
}

uint64_t assignedContributionPrimaryMask(PlayerSlot slot, const NetplayInputFrame& contribution)
{
    return contribution.buttonMaskLo[slot];
}

void applyAssignedContribution(NetplayInputFrame& target, PlayerSlot slot, const NetplayInputFrame& contribution)
{
    if(target.framePayload.empty() && !contribution.framePayload.empty()) {
        target.framePayload = contribution.framePayload;
    }
    target.buttonMaskLo[slot] = contribution.buttonMaskLo[slot];
    target.buttonMaskHi[slot] = contribution.buttonMaskHi[slot];
    target.slotPayloads[slot] = contribution.slotPayloads[slot];

}

} // namespace ConsoleNetplay
