#pragma once

#include <string>
#include <vector>

#include "GeraNES/InputBuffer.h"
#include "GeraNESApp/EmulationHost.h"
#include "GeraNESNetplay/NetSession.h"

namespace Netplay {

inline const char* portDeviceLabel(Settings::Device device)
{
    switch(device) {
        case Settings::Device::CONTROLLER: return "Standard Controller";
        case Settings::Device::FAMICOM_CONTROLLER: return "Famicom Controller";
        case Settings::Device::ZAPPER: return "Zapper";
        case Settings::Device::ARKANOID_CONTROLLER: return "Arkanoid Controller";
        case Settings::Device::BANDAI_HYPERSHOT: return "Bandai Hyper Shot";
        case Settings::Device::SNES_MOUSE: return "SNES Mouse";
        case Settings::Device::SNES_CONTROLLER: return "SNES Controller";
        case Settings::Device::POWER_PAD_SIDE_A: return "Power Pad (Side A)";
        case Settings::Device::POWER_PAD_SIDE_B: return "Power Pad (Side B)";
        case Settings::Device::SUBOR_MOUSE: return "Subor Mouse";
        case Settings::Device::VIRTUAL_BOY_CONTROLLER: return "Virtual Boy Controller";
        case Settings::Device::NONE:
        default:
            return "None";
    }
}

inline const char* expansionDeviceLabel(Settings::ExpansionDevice device)
{
    switch(device) {
        case Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM: return "Standard Controller (Famicom)";
        case Settings::ExpansionDevice::BANDAI_HYPERSHOT: return "Bandai Hyper Shot";
        case Settings::ExpansionDevice::KONAMI_HYPERSHOT: return "Konami Hyper Shot";
        case Settings::ExpansionDevice::ARKANOID_CONTROLLER: return "Arkanoid Controller (Famicom)";
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A: return "Family Trainer (Side A)";
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B: return "Family Trainer (Side B)";
        case Settings::ExpansionDevice::SUBOR_KEYBOARD: return "Subor Keyboard";
        case Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD: return "Family Basic Keyboard";
        case Settings::ExpansionDevice::NONE:
        default:
            return "None";
    }
}

inline std::string inputAssignmentGroupLabel(PlayerSlot slot, const RoomState& room)
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

inline std::string inputAssignmentLeafLabel(PlayerSlot slot, const RoomState& room)
{
    switch(slot) {
        case kPort1PlayerSlot:
            return portDeviceLabel(room.port1Device.value_or(Settings::Device::NONE));
        case kPort2PlayerSlot:
            return portDeviceLabel(room.port2Device.value_or(Settings::Device::NONE));
        case kExpansionPlayerSlot:
            return expansionDeviceLabel(room.expansionDevice);
        case kMultitapP1PlayerSlot:
        case kMultitapP2PlayerSlot:
        case kMultitapP3PlayerSlot:
        case kMultitapP4PlayerSlot: {
            const unsigned controllerIndex = static_cast<unsigned>(slot - kMultitapP1PlayerSlot) + 1u;
            if(room.nesMultitapDevice == Settings::NesMultitapDevice::FOUR_SCORE) {
                return "Four Score P" + std::to_string(controllerIndex);
            }
            if(room.famicomMultitapDevice == Settings::FamicomMultitapDevice::HORI_ADAPTER) {
                return "Hori Adapter P" + std::to_string(controllerIndex);
            }
            return "Multitap P" + std::to_string(controllerIndex);
        }
        case kObserverPlayerSlot:
        default:
            return "Observer";
    }
}

inline bool isMultitapActive(const RoomState& room)
{
    return room.nesMultitapDevice != Settings::NesMultitapDevice::NONE ||
           room.famicomMultitapDevice != Settings::FamicomMultitapDevice::NONE;
}

inline std::vector<PlayerSlot> availableInputAssignments(const RoomState& room)
{
    std::vector<PlayerSlot> assignments;
    if(isMultitapActive(room)) {
        assignments.push_back(kMultitapP1PlayerSlot);
        assignments.push_back(kMultitapP2PlayerSlot);
        assignments.push_back(kMultitapP3PlayerSlot);
        assignments.push_back(kMultitapP4PlayerSlot);
        return assignments;
    }

    if(room.port1Device.has_value() && room.port1Device != std::optional<Settings::Device>(Settings::Device::NONE)) {
        assignments.push_back(kPort1PlayerSlot);
    }
    if(room.port2Device.has_value() && room.port2Device != std::optional<Settings::Device>(Settings::Device::NONE)) {
        assignments.push_back(kPort2PlayerSlot);
    }
    if(room.expansionDevice != Settings::ExpansionDevice::NONE) {
        assignments.push_back(kExpansionPlayerSlot);
    }
    return assignments;
}

inline bool isAssignmentAvailable(PlayerSlot slot, const RoomState& room)
{
    if(slot == kObserverPlayerSlot) return true;
    const auto assignments = availableInputAssignments(room);
    return std::find(assignments.begin(), assignments.end(), slot) != assignments.end();
}

inline RoomState roomWithTopology(RoomState room,
                                  std::optional<Settings::Device> port1Device,
                                  std::optional<Settings::Device> port2Device,
                                  Settings::ExpansionDevice expansionDevice,
                                  Settings::NesMultitapDevice nesMultitapDevice,
                                  Settings::FamicomMultitapDevice famicomMultitapDevice)
{
    room.port1Device = port1Device;
    room.port2Device = port2Device;
    room.expansionDevice = expansionDevice;
    room.nesMultitapDevice = nesMultitapDevice;
    room.famicomMultitapDevice = famicomMultitapDevice;
    return room;
}

inline bool isInputAssignmentClaimedByOtherParticipant(const RoomState& room,
                                                       ParticipantId participantId,
                                                       PlayerSlot slot)
{
    if(slot == kObserverPlayerSlot) return false;

    for(const auto& participant : room.participants) {
        if(participant.id == participantId) continue;
        if(participant.controllerAssignment == slot) {
            return true;
        }
    }

    return false;
}

inline bool canAssignInputCandidate(const RoomState& room,
                                    ParticipantId participantId,
                                    std::optional<Settings::Device> port1Device,
                                    std::optional<Settings::Device> port2Device,
                                    Settings::ExpansionDevice expansionDevice,
                                    Settings::NesMultitapDevice nesMultitapDevice,
                                    Settings::FamicomMultitapDevice famicomMultitapDevice,
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
            switch(participant.controllerAssignment) {
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

    std::vector<PlayerSlot> claimedAssignments;
    claimedAssignments.reserve(candidateRoom.participants.size());

    for(const auto& participant : candidateRoom.participants) {
        const PlayerSlot effectiveAssignment =
            participant.id == participantId ? slot : participant.controllerAssignment;
        if(effectiveAssignment == kObserverPlayerSlot) {
            continue;
        }
        if(!isAssignmentAvailable(effectiveAssignment, candidateRoom)) {
            return false;
        }
        if(std::find(claimedAssignments.begin(), claimedAssignments.end(), effectiveAssignment) != claimedAssignments.end()) {
            return false;
        }
        claimedAssignments.push_back(effectiveAssignment);
    }

    return true;
}

inline std::string inputAssignmentLabel(PlayerSlot slot, const RoomState& room)
{
    if(slot == kObserverPlayerSlot) {
        return "Observer";
    }
    return inputAssignmentGroupLabel(slot, room) + " - " + inputAssignmentLeafLabel(slot, room);
}

inline InputFrame makeRoomTopologyBaseFrame(FrameNumber frame, const RoomState& room)
{
    InputFrame inputFrame{};
    inputFrame.frame = frame;
    inputFrame.port1Device = room.port1Device.value_or(Settings::Device::NONE);
    inputFrame.port2Device = room.port2Device.value_or(Settings::Device::NONE);
    inputFrame.expansionDevice = room.expansionDevice;
    inputFrame.nesMultitapDevice = room.nesMultitapDevice;
    inputFrame.famicomMultitapDevice = room.famicomMultitapDevice;
    return inputFrame;
}

inline InputFrame makeContributionBase(const InputFrame& baseFrame)
{
    InputFrame contribution{};
    contribution.frame = baseFrame.frame;
    contribution.port1Device = baseFrame.port1Device;
    contribution.port2Device = baseFrame.port2Device;
    contribution.expansionDevice = baseFrame.expansionDevice;
    contribution.nesMultitapDevice = baseFrame.nesMultitapDevice;
    contribution.famicomMultitapDevice = baseFrame.famicomMultitapDevice;
    return contribution;
}

inline InputFrame buildAssignedContribution(PlayerSlot slot,
                                            const EmulationHost::InputState& state,
                                            const InputFrame& baseFrame)
{
    InputFrame contribution = makeContributionBase(baseFrame);

    switch(slot) {
        case kPort1PlayerSlot:
            contribution.p1A = state.p1A; contribution.p1B = state.p1B; contribution.p1Select = state.p1Select; contribution.p1Start = state.p1Start;
            contribution.p1Up = state.p1Up; contribution.p1Down = state.p1Down; contribution.p1Left = state.p1Left; contribution.p1Right = state.p1Right;
            contribution.p1X = state.p1X; contribution.p1Y = state.p1Y; contribution.p1L = state.p1L; contribution.p1R = state.p1R;
            contribution.vbP1A = state.p1A; contribution.vbP1B = state.p1B; contribution.vbP1Select = state.p1Select; contribution.vbP1Start = state.p1Start;
            contribution.vbP1Up0 = state.p1Up; contribution.vbP1Down0 = state.p1Down; contribution.vbP1Left0 = state.p1Left; contribution.vbP1Right0 = state.p1Right;
            contribution.vbP1Up1 = state.p1Up2; contribution.vbP1Down1 = state.p1Down2; contribution.vbP1Left1 = state.p1Left2; contribution.vbP1Right1 = state.p1Right2;
            contribution.vbP1L = state.p1L; contribution.vbP1R = state.p1R;
            contribution.zapperP1X = state.zapperX; contribution.zapperP1Y = state.zapperY; contribution.zapperP1Trigger = state.zapperP1Trigger;
            contribution.arkanoidP1Position = state.arkanoidNesPosition; contribution.arkanoidP1Button = state.mousePrimaryButton;
            contribution.snesMouseP1DeltaX = state.mouseDeltaX; contribution.snesMouseP1DeltaY = state.mouseDeltaY;
            contribution.snesMouseP1Left = state.mousePrimaryButton; contribution.snesMouseP1Right = state.mouseSecondaryButton;
            contribution.suborMouseP1DeltaX = state.mouseDeltaX; contribution.suborMouseP1DeltaY = state.mouseDeltaY;
            contribution.suborMouseP1Left = state.mousePrimaryButton; contribution.suborMouseP1Right = state.mouseSecondaryButton;
            contribution.powerPadP1Buttons = state.p1PowerPadButtons;
            break;
        case kPort2PlayerSlot:
            contribution.p2A = state.p2A; contribution.p2B = state.p2B; contribution.p2Select = state.p2Select; contribution.p2Start = state.p2Start;
            contribution.p2Up = state.p2Up; contribution.p2Down = state.p2Down; contribution.p2Left = state.p2Left; contribution.p2Right = state.p2Right;
            contribution.p2X = state.p2X; contribution.p2Y = state.p2Y; contribution.p2L = state.p2L; contribution.p2R = state.p2R;
            contribution.vbP2A = state.p2A; contribution.vbP2B = state.p2B; contribution.vbP2Select = state.p2Select; contribution.vbP2Start = state.p2Start;
            contribution.vbP2Up0 = state.p2Up; contribution.vbP2Down0 = state.p2Down; contribution.vbP2Left0 = state.p2Left; contribution.vbP2Right0 = state.p2Right;
            contribution.vbP2Up1 = state.p2Up2; contribution.vbP2Down1 = state.p2Down2; contribution.vbP2Left1 = state.p2Left2; contribution.vbP2Right1 = state.p2Right2;
            contribution.vbP2L = state.p2L; contribution.vbP2R = state.p2R;
            contribution.zapperP2X = state.zapperX; contribution.zapperP2Y = state.zapperY; contribution.zapperP2Trigger = state.zapperP2Trigger;
            contribution.arkanoidP2Position = state.arkanoidNesPosition; contribution.arkanoidP2Button = state.mousePrimaryButton;
            contribution.snesMouseP2DeltaX = state.mouseDeltaX; contribution.snesMouseP2DeltaY = state.mouseDeltaY;
            contribution.snesMouseP2Left = state.mousePrimaryButton; contribution.snesMouseP2Right = state.mouseSecondaryButton;
            contribution.suborMouseP2DeltaX = state.mouseDeltaX; contribution.suborMouseP2DeltaY = state.mouseDeltaY;
            contribution.suborMouseP2Left = state.mousePrimaryButton; contribution.suborMouseP2Right = state.mouseSecondaryButton;
            contribution.powerPadP2Buttons = state.p2PowerPadButtons;
            contribution.bandaiA = state.p2A; contribution.bandaiB = state.p2B; contribution.bandaiSelect = state.p2Select; contribution.bandaiStart = state.p2Start;
            contribution.bandaiUp = state.p2Up; contribution.bandaiDown = state.p2Down; contribution.bandaiLeft = state.p2Left; contribution.bandaiRight = state.p2Right;
            break;
        case kExpansionPlayerSlot:
            contribution.p3A = state.p3A; contribution.p3B = state.p3B; contribution.p3Select = state.p3Select; contribution.p3Start = state.p3Start;
            contribution.p3Up = state.p3Up; contribution.p3Down = state.p3Down; contribution.p3Left = state.p3Left; contribution.p3Right = state.p3Right;
            contribution.bandaiX = state.zapperX; contribution.bandaiY = state.zapperY; contribution.bandaiTrigger = state.bandaiTrigger;
            contribution.arkanoidFamicomPosition = state.arkanoidFamicomPosition; contribution.arkanoidFamicomButton = state.mousePrimaryButton;
            contribution.konamiP1Run = state.konamiP1Run; contribution.konamiP1Jump = state.konamiP1Jump;
            contribution.konamiP2Run = state.konamiP2Run; contribution.konamiP2Jump = state.konamiP2Jump;
            contribution.suborKeyboardKeys = state.suborKeyboardKeys;
            contribution.familyBasicKeyboardKeys = state.familyBasicKeyboardKeys;
            contribution.powerPadP1Buttons = state.p1PowerPadButtons;
            contribution.powerPadP2Buttons = state.p2PowerPadButtons;
            break;
        case kMultitapP1PlayerSlot:
            contribution.p1A = state.p1A; contribution.p1B = state.p1B; contribution.p1Select = state.p1Select; contribution.p1Start = state.p1Start;
            contribution.p1Up = state.p1Up; contribution.p1Down = state.p1Down; contribution.p1Left = state.p1Left; contribution.p1Right = state.p1Right;
            break;
        case kMultitapP2PlayerSlot:
            contribution.p2A = state.p2A; contribution.p2B = state.p2B; contribution.p2Select = state.p2Select; contribution.p2Start = state.p2Start;
            contribution.p2Up = state.p2Up; contribution.p2Down = state.p2Down; contribution.p2Left = state.p2Left; contribution.p2Right = state.p2Right;
            break;
        case kMultitapP3PlayerSlot:
            contribution.p3A = state.p3A; contribution.p3B = state.p3B; contribution.p3Select = state.p3Select; contribution.p3Start = state.p3Start;
            contribution.p3Up = state.p3Up; contribution.p3Down = state.p3Down; contribution.p3Left = state.p3Left; contribution.p3Right = state.p3Right;
            break;
        case kMultitapP4PlayerSlot:
            contribution.p4A = state.p4A; contribution.p4B = state.p4B; contribution.p4Select = state.p4Select; contribution.p4Start = state.p4Start;
            contribution.p4Up = state.p4Up; contribution.p4Down = state.p4Down; contribution.p4Left = state.p4Left; contribution.p4Right = state.p4Right;
            break;
        default:
            break;
    }

    return contribution;
}

inline uint64_t assignedContributionPrimaryMask(PlayerSlot slot, const InputFrame& contribution)
{
    auto buildMask = [](bool a, bool b, bool select, bool start,
                        bool up, bool down, bool left, bool right,
                        bool x = false, bool y = false, bool l = false, bool r = false,
                        bool up2 = false, bool down2 = false, bool left2 = false, bool right2 = false) {
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
    };

    switch(slot) {
        case kPort1PlayerSlot:
        case kMultitapP1PlayerSlot:
            return buildMask(contribution.p1A, contribution.p1B, contribution.p1Select, contribution.p1Start,
                             contribution.p1Up, contribution.p1Down, contribution.p1Left, contribution.p1Right,
                             contribution.p1X, contribution.p1Y, contribution.p1L, contribution.p1R,
                             contribution.vbP1Up1, contribution.vbP1Down1, contribution.vbP1Left1, contribution.vbP1Right1);
        case kPort2PlayerSlot:
        case kMultitapP2PlayerSlot:
            return buildMask(contribution.p2A, contribution.p2B, contribution.p2Select, contribution.p2Start,
                             contribution.p2Up, contribution.p2Down, contribution.p2Left, contribution.p2Right,
                             contribution.p2X, contribution.p2Y, contribution.p2L, contribution.p2R,
                             contribution.vbP2Up1, contribution.vbP2Down1, contribution.vbP2Left1, contribution.vbP2Right1);
        case kExpansionPlayerSlot:
        case kMultitapP3PlayerSlot:
            return buildMask(contribution.p3A, contribution.p3B, contribution.p3Select, contribution.p3Start,
                             contribution.p3Up, contribution.p3Down, contribution.p3Left, contribution.p3Right);
        case kMultitapP4PlayerSlot:
            return buildMask(contribution.p4A, contribution.p4B, contribution.p4Select, contribution.p4Start,
                             contribution.p4Up, contribution.p4Down, contribution.p4Left, contribution.p4Right);
        default:
            return 0;
    }
}

inline void applyAssignedContribution(InputFrame& target, PlayerSlot slot, const InputFrame& contribution)
{
    switch(slot) {
        case kPort1PlayerSlot:
            target.p1A = contribution.p1A; target.p1B = contribution.p1B; target.p1Select = contribution.p1Select; target.p1Start = contribution.p1Start;
            target.p1Up = contribution.p1Up; target.p1Down = contribution.p1Down; target.p1Left = contribution.p1Left; target.p1Right = contribution.p1Right;
            target.p1X = contribution.p1X; target.p1Y = contribution.p1Y; target.p1L = contribution.p1L; target.p1R = contribution.p1R;
            target.vbP1A = contribution.vbP1A; target.vbP1B = contribution.vbP1B; target.vbP1Select = contribution.vbP1Select; target.vbP1Start = contribution.vbP1Start;
            target.vbP1Up0 = contribution.vbP1Up0; target.vbP1Down0 = contribution.vbP1Down0; target.vbP1Left0 = contribution.vbP1Left0; target.vbP1Right0 = contribution.vbP1Right0;
            target.vbP1Up1 = contribution.vbP1Up1; target.vbP1Down1 = contribution.vbP1Down1; target.vbP1Left1 = contribution.vbP1Left1; target.vbP1Right1 = contribution.vbP1Right1;
            target.vbP1L = contribution.vbP1L; target.vbP1R = contribution.vbP1R;
            target.zapperP1X = contribution.zapperP1X; target.zapperP1Y = contribution.zapperP1Y; target.zapperP1Trigger = contribution.zapperP1Trigger;
            target.arkanoidP1Position = contribution.arkanoidP1Position; target.arkanoidP1Button = contribution.arkanoidP1Button;
            target.snesMouseP1DeltaX = contribution.snesMouseP1DeltaX; target.snesMouseP1DeltaY = contribution.snesMouseP1DeltaY;
            target.snesMouseP1Left = contribution.snesMouseP1Left; target.snesMouseP1Right = contribution.snesMouseP1Right;
            target.suborMouseP1DeltaX = contribution.suborMouseP1DeltaX; target.suborMouseP1DeltaY = contribution.suborMouseP1DeltaY;
            target.suborMouseP1Left = contribution.suborMouseP1Left; target.suborMouseP1Right = contribution.suborMouseP1Right;
            target.powerPadP1Buttons = contribution.powerPadP1Buttons;
            break;
        case kPort2PlayerSlot:
            target.p2A = contribution.p2A; target.p2B = contribution.p2B; target.p2Select = contribution.p2Select; target.p2Start = contribution.p2Start;
            target.p2Up = contribution.p2Up; target.p2Down = contribution.p2Down; target.p2Left = contribution.p2Left; target.p2Right = contribution.p2Right;
            target.p2X = contribution.p2X; target.p2Y = contribution.p2Y; target.p2L = contribution.p2L; target.p2R = contribution.p2R;
            target.vbP2A = contribution.vbP2A; target.vbP2B = contribution.vbP2B; target.vbP2Select = contribution.vbP2Select; target.vbP2Start = contribution.vbP2Start;
            target.vbP2Up0 = contribution.vbP2Up0; target.vbP2Down0 = contribution.vbP2Down0; target.vbP2Left0 = contribution.vbP2Left0; target.vbP2Right0 = contribution.vbP2Right0;
            target.vbP2Up1 = contribution.vbP2Up1; target.vbP2Down1 = contribution.vbP2Down1; target.vbP2Left1 = contribution.vbP2Left1; target.vbP2Right1 = contribution.vbP2Right1;
            target.vbP2L = contribution.vbP2L; target.vbP2R = contribution.vbP2R;
            target.zapperP2X = contribution.zapperP2X; target.zapperP2Y = contribution.zapperP2Y; target.zapperP2Trigger = contribution.zapperP2Trigger;
            target.arkanoidP2Position = contribution.arkanoidP2Position; target.arkanoidP2Button = contribution.arkanoidP2Button;
            target.snesMouseP2DeltaX = contribution.snesMouseP2DeltaX; target.snesMouseP2DeltaY = contribution.snesMouseP2DeltaY;
            target.snesMouseP2Left = contribution.snesMouseP2Left; target.snesMouseP2Right = contribution.snesMouseP2Right;
            target.suborMouseP2DeltaX = contribution.suborMouseP2DeltaX; target.suborMouseP2DeltaY = contribution.suborMouseP2DeltaY;
            target.suborMouseP2Left = contribution.suborMouseP2Left; target.suborMouseP2Right = contribution.suborMouseP2Right;
            target.powerPadP2Buttons = contribution.powerPadP2Buttons;
            target.bandaiA = contribution.bandaiA; target.bandaiB = contribution.bandaiB; target.bandaiSelect = contribution.bandaiSelect; target.bandaiStart = contribution.bandaiStart;
            target.bandaiUp = contribution.bandaiUp; target.bandaiDown = contribution.bandaiDown; target.bandaiLeft = contribution.bandaiLeft; target.bandaiRight = contribution.bandaiRight;
            break;
        case kExpansionPlayerSlot:
            target.p3A = contribution.p3A; target.p3B = contribution.p3B; target.p3Select = contribution.p3Select; target.p3Start = contribution.p3Start;
            target.p3Up = contribution.p3Up; target.p3Down = contribution.p3Down; target.p3Left = contribution.p3Left; target.p3Right = contribution.p3Right;
            target.bandaiX = contribution.bandaiX; target.bandaiY = contribution.bandaiY; target.bandaiTrigger = contribution.bandaiTrigger;
            target.arkanoidFamicomPosition = contribution.arkanoidFamicomPosition; target.arkanoidFamicomButton = contribution.arkanoidFamicomButton;
            target.konamiP1Run = contribution.konamiP1Run; target.konamiP1Jump = contribution.konamiP1Jump;
            target.konamiP2Run = contribution.konamiP2Run; target.konamiP2Jump = contribution.konamiP2Jump;
            target.suborKeyboardKeys = contribution.suborKeyboardKeys;
            target.familyBasicKeyboardKeys = contribution.familyBasicKeyboardKeys;
            target.powerPadP1Buttons = contribution.powerPadP1Buttons;
            target.powerPadP2Buttons = contribution.powerPadP2Buttons;
            break;
        case kMultitapP1PlayerSlot:
            target.p1A = contribution.p1A; target.p1B = contribution.p1B; target.p1Select = contribution.p1Select; target.p1Start = contribution.p1Start;
            target.p1Up = contribution.p1Up; target.p1Down = contribution.p1Down; target.p1Left = contribution.p1Left; target.p1Right = contribution.p1Right;
            break;
        case kMultitapP2PlayerSlot:
            target.p2A = contribution.p2A; target.p2B = contribution.p2B; target.p2Select = contribution.p2Select; target.p2Start = contribution.p2Start;
            target.p2Up = contribution.p2Up; target.p2Down = contribution.p2Down; target.p2Left = contribution.p2Left; target.p2Right = contribution.p2Right;
            break;
        case kMultitapP3PlayerSlot:
            target.p3A = contribution.p3A; target.p3B = contribution.p3B; target.p3Select = contribution.p3Select; target.p3Start = contribution.p3Start;
            target.p3Up = contribution.p3Up; target.p3Down = contribution.p3Down; target.p3Left = contribution.p3Left; target.p3Right = contribution.p3Right;
            break;
        case kMultitapP4PlayerSlot:
            target.p4A = contribution.p4A; target.p4B = contribution.p4B; target.p4Select = contribution.p4Select; target.p4Start = contribution.p4Start;
            target.p4Up = contribution.p4Up; target.p4Down = contribution.p4Down; target.p4Left = contribution.p4Left; target.p4Right = contribution.p4Right;
            break;
        default:
            break;
    }
}

} // namespace Netplay
