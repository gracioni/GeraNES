#include "GeraNESNetplay/NetplayInputAssignment.h"

#include <algorithm>


namespace Netplay {

const char* portDeviceLabel(PortDevice device)
{
    switch(device) {
        case PortDevice::CONTROLLER: return "Standard Controller";
        case PortDevice::FAMICOM_CONTROLLER: return "Famicom Controller";
        case PortDevice::ZAPPER: return "Zapper";
        case PortDevice::ARKANOID_CONTROLLER: return "Arkanoid Controller";
        case PortDevice::BANDAI_HYPERSHOT: return "Bandai Hyper Shot";
        case PortDevice::SNES_MOUSE: return "SNES Mouse";
        case PortDevice::SNES_CONTROLLER: return "SNES Controller";
        case PortDevice::POWER_PAD_SIDE_A: return "Power Pad (Side A)";
        case PortDevice::POWER_PAD_SIDE_B: return "Power Pad (Side B)";
        case PortDevice::SUBOR_MOUSE: return "Subor Mouse";
        case PortDevice::VIRTUAL_BOY_CONTROLLER: return "Virtual Boy Controller";
        case PortDevice::NONE:
        default:
            return "None";
    }
}

const char* expansionDeviceLabel(ExpansionDevice device)
{
    switch(device) {
        case ExpansionDevice::STANDARD_CONTROLLER_FAMICOM: return "Standard Controller (Famicom)";
        case ExpansionDevice::BANDAI_HYPERSHOT: return "Bandai Hyper Shot";
        case ExpansionDevice::KONAMI_HYPERSHOT: return "Konami Hyper Shot";
        case ExpansionDevice::ARKANOID_CONTROLLER: return "Arkanoid Controller (Famicom)";
        case ExpansionDevice::FAMILY_TRAINER_SIDE_A: return "Family Trainer (Side A)";
        case ExpansionDevice::FAMILY_TRAINER_SIDE_B: return "Family Trainer (Side B)";
        case ExpansionDevice::SUBOR_KEYBOARD: return "Subor Keyboard";
        case ExpansionDevice::FAMILY_BASIC_KEYBOARD: return "Family Basic Keyboard";
        case ExpansionDevice::NONE:
        default:
            return "None";
    }
}

std::string inputAssignmentGroupLabel(PlayerSlot slot, const RoomState& room)
{
    (void)room;
    switch(slot) {
        case kPort1PlayerSlot: return "Port 1";
        case kPort2PlayerSlot: return "Port 2";
        case kExpansionPlayerSlot: return "Expansion Port";
        case kMultitapP1PlayerSlot:
        case kMultitapP2PlayerSlot:
        case kMultitapP3PlayerSlot:
        case kMultitapP4PlayerSlot:
            return "Multitap";
        case kObserverPlayerSlot:
        default:
            return "Observer";
    }
}

std::string inputAssignmentLeafLabel(PlayerSlot slot, const RoomState& room)
{
    switch(slot) {
        case kPort1PlayerSlot:
            return portDeviceLabel(room.port1Device.value_or(PortDevice::NONE));
        case kPort2PlayerSlot:
            return portDeviceLabel(room.port2Device.value_or(PortDevice::NONE));
        case kExpansionPlayerSlot:
            return expansionDeviceLabel(room.expansionDevice);
        case kMultitapP1PlayerSlot:
        case kMultitapP2PlayerSlot:
        case kMultitapP3PlayerSlot:
        case kMultitapP4PlayerSlot: {
            const unsigned controllerIndex = static_cast<unsigned>(slot - kMultitapP1PlayerSlot) + 1u;
            if(room.nesMultitapDevice == NesMultitapDevice::FOUR_SCORE) {
                return "Four Score P" + std::to_string(controllerIndex);
            }
            if(room.famicomMultitapDevice == FamicomMultitapDevice::HORI_ADAPTER) {
                return "Hori Adapter P" + std::to_string(controllerIndex);
            }
            return "Multitap P" + std::to_string(controllerIndex);
        }
        case kObserverPlayerSlot:
        default:
            return "Observer";
    }
}

bool isMultitapActive(const RoomState& room)
{
    return room.nesMultitapDevice != NesMultitapDevice::NONE ||
           room.famicomMultitapDevice != FamicomMultitapDevice::NONE;
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
    if(isMultitapActive(room)) {
        assignments.push_back(kMultitapP1PlayerSlot);
        assignments.push_back(kMultitapP2PlayerSlot);
        assignments.push_back(kMultitapP3PlayerSlot);
        assignments.push_back(kMultitapP4PlayerSlot);
        return assignments;
    }

    if(room.port1Device.has_value() && room.port1Device != std::optional<PortDevice>(PortDevice::NONE)) {
        assignments.push_back(kPort1PlayerSlot);
    }
    if(room.port2Device.has_value() && room.port2Device != std::optional<PortDevice>(PortDevice::NONE)) {
        assignments.push_back(kPort2PlayerSlot);
    }
    if(room.expansionDevice != ExpansionDevice::NONE) {
        assignments.push_back(kExpansionPlayerSlot);
    }
    return assignments;
}

bool isAssignmentAvailable(PlayerSlot slot, const RoomState& room)
{
    if(slot == kObserverPlayerSlot) return true;
    const auto assignments = availableInputAssignments(room);
    return std::find(assignments.begin(), assignments.end(), slot) != assignments.end();
}

RoomState roomWithTopology(RoomState room,
                                  std::optional<PortDevice> port1Device,
                                  std::optional<PortDevice> port2Device,
                                  ExpansionDevice expansionDevice,
                                  NesMultitapDevice nesMultitapDevice,
                                  FamicomMultitapDevice famicomMultitapDevice)
{
    room.port1Device = port1Device;
    room.port2Device = port2Device;
    room.expansionDevice = expansionDevice;
    room.nesMultitapDevice = nesMultitapDevice;
    room.famicomMultitapDevice = famicomMultitapDevice;
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
                                    std::optional<PortDevice> port1Device,
                                    std::optional<PortDevice> port2Device,
                                    ExpansionDevice expansionDevice,
                                    NesMultitapDevice nesMultitapDevice,
                                    FamicomMultitapDevice famicomMultitapDevice,
                                    PlayerSlot slot)
{
    RoomState candidateRoom = roomWithTopology(
        room,
        port1Device,
        port2Device,
        expansionDevice,
        nesMultitapDevice,
        famicomMultitapDevice
    );

    const bool currentMultitapActive = isMultitapActive(room);
    const bool candidateMultitapActive = isMultitapActive(candidateRoom);
    const bool multitapFamilyChanged =
        room.nesMultitapDevice != candidateRoom.nesMultitapDevice ||
        room.famicomMultitapDevice != candidateRoom.famicomMultitapDevice;
    if(currentMultitapActive && candidateMultitapActive && multitapFamilyChanged) {
        for(const auto& participant : room.participants) {
            if(participant.id == participantId) continue;
            for(PlayerSlot assignedSlot : participantAssignments(participant)) {
                switch(assignedSlot) {
                    case kMultitapP1PlayerSlot:
                    case kMultitapP2PlayerSlot:
                    case kMultitapP3PlayerSlot:
                    case kMultitapP4PlayerSlot:
                        return false;
                    default:
                        break;
                }
            }
        }
    }

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

namespace {

uint64_t buildButtonMask(bool a, bool b, bool select, bool start,
                         bool up, bool down, bool left, bool right,
                         bool x = false, bool y = false, bool l = false, bool r = false,
                         bool up2 = false, bool down2 = false, bool left2 = false, bool right2 = false)
{
    uint64_t mask = 0;
    if(a) mask |= (1ull << 0);
    if(b) mask |= (1ull << 1);
    if(select) mask |= (1ull << 2);
    if(start) mask |= (1ull << 3);
    if(up) mask |= (1ull << 4);
    if(down) mask |= (1ull << 5);
    if(left) mask |= (1ull << 6);
    if(right) mask |= (1ull << 7);
    if(x) mask |= (1ull << 8);
    if(y) mask |= (1ull << 9);
    if(l) mask |= (1ull << 10);
    if(r) mask |= (1ull << 11);
    if(up2) mask |= (1ull << 12);
    if(down2) mask |= (1ull << 13);
    if(left2) mask |= (1ull << 14);
    if(right2) mask |= (1ull << 15);
    return mask;
}

bool isControllerLike(PortDevice device)
{
    return device == PortDevice::CONTROLLER ||
           device == PortDevice::FAMICOM_CONTROLLER ||
           device == PortDevice::SNES_CONTROLLER ||
           device == PortDevice::VIRTUAL_BOY_CONTROLLER;
}

} // namespace

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

NetplayInputFrame buildAssignedContribution(PlayerSlot slot,
                                            const NetplayInputState& state,
                                            const NetplayInputFrame& baseFrame)
{
    NetplayInputFrame contribution = makeContributionBase(baseFrame);

    switch(slot) {
        case kPort1PlayerSlot:
            contribution.buttonMaskLo[slot] = buildButtonMask(
                state.p1A, state.p1B, state.p1Select, state.p1Start,
                state.p1Up, state.p1Down, state.p1Left, state.p1Right,
                state.p1X, state.p1Y, state.p1L, state.p1R,
                state.p1Up2, state.p1Down2, state.p1Left2, state.p1Right2);
            break;
        case kPort2PlayerSlot:
            contribution.buttonMaskLo[slot] = buildButtonMask(
                state.p2A, state.p2B, state.p2Select, state.p2Start,
                state.p2Up, state.p2Down, state.p2Left, state.p2Right,
                state.p2X, state.p2Y, state.p2L, state.p2R,
                state.p2Up2, state.p2Down2, state.p2Left2, state.p2Right2);
            break;
        case kExpansionPlayerSlot:
            contribution.buttonMaskLo[slot] = buildButtonMask(
                state.p3A, state.p3B, state.p3Select, state.p3Start,
                state.p3Up, state.p3Down, state.p3Left, state.p3Right);
            break;
        case kMultitapP1PlayerSlot:
            contribution.buttonMaskLo[slot] = buildButtonMask(state.p1A, state.p1B, state.p1Select, state.p1Start, state.p1Up, state.p1Down, state.p1Left, state.p1Right);
            break;
        case kMultitapP2PlayerSlot:
            contribution.buttonMaskLo[slot] = buildButtonMask(state.p2A, state.p2B, state.p2Select, state.p2Start, state.p2Up, state.p2Down, state.p2Left, state.p2Right);
            break;
        case kMultitapP3PlayerSlot:
            contribution.buttonMaskLo[slot] = buildButtonMask(state.p3A, state.p3B, state.p3Select, state.p3Start, state.p3Up, state.p3Down, state.p3Left, state.p3Right);
            break;
        case kMultitapP4PlayerSlot:
            contribution.buttonMaskLo[slot] = buildButtonMask(state.p4A, state.p4B, state.p4Select, state.p4Start, state.p4Up, state.p4Down, state.p4Left, state.p4Right);
            break;
        default:
            break;
    }

    return contribution;
}

uint64_t assignedContributionPrimaryMask(PlayerSlot slot, const NetplayInputFrame& contribution)
{
    return slot <= kMaxAssignedPlayerSlot ? contribution.buttonMaskLo[slot] : 0u;
}

void applyAssignedContribution(NetplayInputFrame& target, PlayerSlot slot, const NetplayInputFrame& contribution)
{
    if(target.framePayload.empty() && !contribution.framePayload.empty()) {
        target.framePayload = contribution.framePayload;
    }
    if(slot <= kMaxAssignedPlayerSlot) {
        target.buttonMaskLo[slot] = contribution.buttonMaskLo[slot];
        target.buttonMaskHi[slot] = contribution.buttonMaskHi[slot];
        target.slotPayloads[slot] = contribution.slotPayloads[slot];
    }

}

} // namespace Netplay
