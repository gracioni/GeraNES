#include "GeraNESNetplay/GeraNESNetplayAdapters.h"

#include <string>

#include "ConsoleNetplay/NetplayCoordinator.h"
#include "ConsoleNetplay/NetplayInputAssignment.h"
#include "ConsoleNetplay/NetSerialization.h"

namespace GeraNESNetplay {

using ConsoleNetplay::InputSlotDescriptor;
using ConsoleNetplay::InputDeviceId;
using ConsoleNetplay::applyAssignedContribution;
using ConsoleNetplay::kInvalidParticipantId;
using ConsoleNetplay::kObserverPlayerSlot;
using ConsoleNetplay::makeContributionBase;
using ConsoleNetplay::makeRoomTopologyBaseNetplayFrame;
using ConsoleNetplay::PacketReader;
using ConsoleNetplay::PacketWriter;
using ConsoleNetplay::ParticipantInfo;
using ConsoleNetplay::participantAssignments;

namespace {

InputDeviceId geraNESDeviceId(Settings::Device device)
{
    return static_cast<InputDeviceId>(0x400u + static_cast<uint8_t>(device));
}

InputDeviceId geraNESExpansionDeviceId(Settings::ExpansionDevice device)
{
    return static_cast<InputDeviceId>(0x100u + static_cast<uint8_t>(device));
}

InputDeviceId geraNESNesMultitapDeviceId(Settings::NesMultitapDevice device)
{
    return static_cast<InputDeviceId>(0x200u + static_cast<uint8_t>(device));
}

InputDeviceId geraNESFamicomMultitapDeviceId(Settings::FamicomMultitapDevice device)
{
    return static_cast<InputDeviceId>(0x300u + static_cast<uint8_t>(device));
}

const char* geraNESPortDeviceLabel(Settings::Device device)
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

const char* geraNESExpansionDeviceLabel(Settings::ExpansionDevice device)
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

bool geraNESMultitapActive(Settings::NesMultitapDevice nesMultitapDevice,
                           Settings::FamicomMultitapDevice famicomMultitapDevice)
{
    return nesMultitapDevice != Settings::NesMultitapDevice::NONE ||
           famicomMultitapDevice != Settings::FamicomMultitapDevice::NONE;
}

std::optional<PlayerSlot> mapAssignmentForGeraNESCandidateTopology(PlayerSlot assignment,
                                                                   bool currentUsesMultitap,
                                                                   bool candidateUsesMultitap)
{
    if(currentUsesMultitap == candidateUsesMultitap) {
        return assignment;
    }

    if(!currentUsesMultitap && candidateUsesMultitap) {
        switch(assignment) {
            case kPort1PlayerSlot: return kMultitapP1PlayerSlot;
            case kPort2PlayerSlot: return kMultitapP2PlayerSlot;
            default: return std::nullopt;
        }
    }

    switch(assignment) {
        case kMultitapP1PlayerSlot: return kPort1PlayerSlot;
        case kMultitapP2PlayerSlot: return kPort2PlayerSlot;
        default: return std::nullopt;
    }
}

std::optional<PlayerSlot> mapAssignmentForGeraNESTopologyChange(PlayerSlot assignment,
                                                                const RoomState& currentRoom,
                                                                const std::vector<InputSlotDescriptor>& candidateTopology)
{
    const bool currentUsesMultitap =
        geraNESNesMultitapDeviceFromTopology(currentRoom) != Settings::NesMultitapDevice::NONE ||
        geraNESFamicomMultitapDeviceFromTopology(currentRoom) != Settings::FamicomMultitapDevice::NONE;
    const bool candidateUsesMultitap =
        findInputSlot(candidateTopology, kMultitapP1PlayerSlot) != nullptr;
    return mapAssignmentForGeraNESCandidateTopology(assignment, currentUsesMultitap, candidateUsesMultitap);
}

std::vector<InputSlotDescriptor> makeGeraNESInputTopology(
    std::optional<Settings::Device> port1Device,
    std::optional<Settings::Device> port2Device,
    Settings::ExpansionDevice expansionDevice,
    Settings::NesMultitapDevice nesMultitapDevice,
    Settings::FamicomMultitapDevice famicomMultitapDevice)
{
    std::vector<InputSlotDescriptor> topology;
    if(geraNESMultitapActive(nesMultitapDevice, famicomMultitapDevice)) {
        const bool fourScore = nesMultitapDevice == Settings::NesMultitapDevice::FOUR_SCORE;
        const char* groupLabel = fourScore ? "Four Score" : "Hori Adapter";
        const InputDeviceId deviceId = fourScore
            ? geraNESNesMultitapDeviceId(nesMultitapDevice)
            : geraNESFamicomMultitapDeviceId(famicomMultitapDevice);
        for(size_t index = 0; index < kMultitapPlayerSlots.size(); ++index) {
            const PlayerSlot slot = kMultitapPlayerSlots[index];
            topology.push_back({slot, 4, deviceId, true, groupLabel, "P" + std::to_string(index + 1u)});
        }
        return topology;
    }

    if(port1Device.has_value() && *port1Device != Settings::Device::NONE) {
        topology.push_back({kPort1PlayerSlot, 1, geraNESDeviceId(*port1Device), true, "Port 1", geraNESPortDeviceLabel(*port1Device)});
    }
    if(port2Device.has_value() && *port2Device != Settings::Device::NONE) {
        topology.push_back({kPort2PlayerSlot, 2, geraNESDeviceId(*port2Device), true, "Port 2", geraNESPortDeviceLabel(*port2Device)});
    }
    if(expansionDevice != Settings::ExpansionDevice::NONE) {
        topology.push_back({kExpansionPlayerSlot, 3, geraNESExpansionDeviceId(expansionDevice), true, "Expansion Port", geraNESExpansionDeviceLabel(expansionDevice)});
    }
    return topology;
}

} // namespace

RoomState roomWithGeraNESInputTopology(RoomState room,
                                       std::optional<Settings::Device> port1Device,
                                       std::optional<Settings::Device> port2Device,
                                       Settings::ExpansionDevice expansionDevice,
                                       Settings::NesMultitapDevice nesMultitapDevice,
                                       Settings::FamicomMultitapDevice famicomMultitapDevice)
{
    room.inputTopology = makeGeraNESInputTopology(port1Device, port2Device, expansionDevice, nesMultitapDevice, famicomMultitapDevice);
    return room;
}

bool canAssignGeraNESInputCandidate(const RoomState& room,
                                    ParticipantId participantId,
                                    std::optional<Settings::Device> port1Device,
                                    std::optional<Settings::Device> port2Device,
                                    Settings::ExpansionDevice expansionDevice,
                                    Settings::NesMultitapDevice nesMultitapDevice,
                                    Settings::FamicomMultitapDevice famicomMultitapDevice,
                                    PlayerSlot slot)
{
    const Settings::NesMultitapDevice currentNesMultitap = geraNESNesMultitapDeviceFromTopology(room);
    const Settings::FamicomMultitapDevice currentFamicomMultitap = geraNESFamicomMultitapDeviceFromTopology(room);
    const bool currentUsesNesMultitap = currentNesMultitap != Settings::NesMultitapDevice::NONE;
    const bool currentUsesFamicomMultitap = currentFamicomMultitap != Settings::FamicomMultitapDevice::NONE;
    const bool candidateUsesNesMultitap = nesMultitapDevice != Settings::NesMultitapDevice::NONE;
    const bool candidateUsesFamicomMultitap = famicomMultitapDevice != Settings::FamicomMultitapDevice::NONE;
    const bool changesMultitapFamily =
        currentUsesNesMultitap != candidateUsesNesMultitap ||
        currentUsesFamicomMultitap != candidateUsesFamicomMultitap;
    if(changesMultitapFamily) {
        for(const ParticipantInfo& participant : room.participants) {
            if(participant.id == participantId) continue;
            for(PlayerSlot assignment : participantAssignments(participant)) {
                if(isGeraNESMultitapPlayerSlot(assignment)) {
                    return false;
                }
            }
        }
    }

    RoomState candidateRoom = room;
    if(currentUsesNesMultitap != candidateUsesNesMultitap ||
       currentUsesFamicomMultitap != candidateUsesFamicomMultitap) {
        for(ParticipantInfo& participant : candidateRoom.participants) {
            std::vector<PlayerSlot> mappedAssignments;
            for(PlayerSlot assignment : participantAssignments(participant)) {
                const std::optional<PlayerSlot> mapped =
                    mapAssignmentForGeraNESCandidateTopology(
                        assignment,
                        currentUsesNesMultitap || currentUsesFamicomMultitap,
                        candidateUsesNesMultitap || candidateUsesFamicomMultitap
                    );
                if(mapped.has_value()) {
                    mappedAssignments.push_back(*mapped);
                }
            }
            participant.controllerAssignments = std::move(mappedAssignments);
            participant.normalizeControllerAssignments();
        }
    }

    return canAssignInputCandidate(
        candidateRoom,
        participantId,
        makeGeraNESInputTopology(port1Device, port2Device, expansionDevice, nesMultitapDevice, famicomMultitapDevice),
        slot
    );
}

void setGeraNESRoomInputTopology(NetplayCoordinator& coordinator,
                                 std::optional<Settings::Device> port1Device,
                                 std::optional<Settings::Device> port2Device,
                                 Settings::ExpansionDevice expansionDevice,
                                 Settings::NesMultitapDevice nesMultitapDevice,
                                 Settings::FamicomMultitapDevice famicomMultitapDevice,
                                 std::optional<ParticipantId> preservedParticipantId,
                                 PlayerSlot preservedAssignment)
{
    const RoomState currentRoom = coordinator.session().roomState();
    std::vector<InputSlotDescriptor> candidateTopology =
        makeGeraNESInputTopology(
            port1Device,
            port2Device,
            expansionDevice,
            nesMultitapDevice,
            famicomMultitapDevice
        );
    const std::vector<InputSlotDescriptor> remapTopology = candidateTopology;
    coordinator.setRoomInputTopology(
        candidateTopology,
        preservedParticipantId,
        preservedAssignment,
        [currentRoom, remapTopology](PlayerSlot assignment) {
            return remapGeraNESAssignmentForTopologyChange(assignment, currentRoom, remapTopology);
        }
    );
}

std::optional<PlayerSlot> remapGeraNESAssignmentForTopologyChange(PlayerSlot assignment,
                                                                  const RoomState& currentRoom,
                                                                  const std::vector<InputSlotDescriptor>& candidateTopology)
{
    return mapAssignmentForGeraNESTopologyChange(assignment, currentRoom, candidateTopology);
}

Settings::Device geraNESPortDeviceFromTopology(const RoomState& room, PlayerSlot slot)
{
    const InputSlotDescriptor* descriptor = findInputSlot(room.inputTopology, slot);
    if(descriptor == nullptr || (descriptor->deviceId & 0xFF00u) != 0x0400u) {
        return Settings::Device::NONE;
    }
    return static_cast<Settings::Device>(static_cast<uint8_t>(descriptor->deviceId & 0x00FFu));
}

Settings::ExpansionDevice geraNESExpansionDeviceFromTopology(const RoomState& room)
{
    const InputSlotDescriptor* descriptor = findInputSlot(room.inputTopology, kExpansionPlayerSlot);
    if(descriptor == nullptr || (descriptor->deviceId & 0xFF00u) != 0x0100u) {
        return Settings::ExpansionDevice::NONE;
    }
    return static_cast<Settings::ExpansionDevice>(static_cast<uint8_t>(descriptor->deviceId & 0x00FFu));
}

Settings::NesMultitapDevice geraNESNesMultitapDeviceFromTopology(const RoomState& room)
{
    const InputSlotDescriptor* descriptor = findInputSlot(room.inputTopology, kMultitapP1PlayerSlot);
    if(descriptor == nullptr || (descriptor->deviceId & 0xFF00u) != 0x0200u) {
        return Settings::NesMultitapDevice::NONE;
    }
    return static_cast<Settings::NesMultitapDevice>(static_cast<uint8_t>(descriptor->deviceId & 0x00FFu));
}

Settings::FamicomMultitapDevice geraNESFamicomMultitapDeviceFromTopology(const RoomState& room)
{
    const InputSlotDescriptor* descriptor = findInputSlot(room.inputTopology, kMultitapP1PlayerSlot);
    if(descriptor == nullptr || (descriptor->deviceId & 0xFF00u) != 0x0300u) {
        return Settings::FamicomMultitapDevice::NONE;
    }
    return static_cast<Settings::FamicomMultitapDevice>(static_cast<uint8_t>(descriptor->deviceId & 0x00FFu));
}

namespace {

constexpr uint8_t kAdapterFramePayloadVersion = 1;
constexpr uint8_t kAdapterSlotPayloadVersion = 1;

enum class AdapterSlotPayloadKind : uint8_t
{
    None = 0,
    Zapper = 1,
    Arkanoid = 2,
    Mouse = 3,
    PowerPad = 4,
    BandaiPointer = 5,
    KonamiHyperShot = 6,
    Keyboard = 7,
    FamilyTrainer = 8
};

struct AdapterFramePayload
{
    uint8_t version = kAdapterFramePayloadVersion;
    Settings::Device port1Device = Settings::Device::NONE;
    Settings::Device port2Device = Settings::Device::NONE;
    Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
    Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
    Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;
};

std::vector<uint8_t> makeAdapterFramePayload(const InputFrame& inputFrame)
{
    PacketWriter writer;
    writer.writePod(kAdapterFramePayloadVersion);
    writer.writePod(static_cast<uint8_t>(inputFrame.port1Device));
    writer.writePod(static_cast<uint8_t>(inputFrame.port2Device));
    writer.writePod(static_cast<uint8_t>(inputFrame.expansionDevice));
    writer.writePod(static_cast<uint8_t>(inputFrame.nesMultitapDevice));
    writer.writePod(static_cast<uint8_t>(inputFrame.famicomMultitapDevice));
    return writer.data();
}

bool readAdapterFramePayload(const NetplayInputFrame& inputFrame, AdapterFramePayload& payload)
{
    if(inputFrame.framePayload.empty()) return false;
    PacketReader reader(inputFrame.framePayload.data(), inputFrame.framePayload.size());
    uint8_t port1Device = 0;
    uint8_t port2Device = 0;
    uint8_t expansionDevice = 0;
    uint8_t nesMultitapDevice = 0;
    uint8_t famicomMultitapDevice = 0;
    if(!reader.readPod(payload.version) ||
       !reader.readPod(port1Device) ||
       !reader.readPod(port2Device) ||
       !reader.readPod(expansionDevice) ||
       !reader.readPod(nesMultitapDevice) ||
       !reader.readPod(famicomMultitapDevice)) {
        return false;
    }
    if(payload.version != kAdapterFramePayloadVersion || reader.remaining() != 0) return false;
    payload.port1Device = static_cast<Settings::Device>(port1Device);
    payload.port2Device = static_cast<Settings::Device>(port2Device);
    payload.expansionDevice = static_cast<Settings::ExpansionDevice>(expansionDevice);
    payload.nesMultitapDevice = static_cast<Settings::NesMultitapDevice>(nesMultitapDevice);
    payload.famicomMultitapDevice = static_cast<Settings::FamicomMultitapDevice>(famicomMultitapDevice);
    return true;
}

uint64_t buildMask(bool a, bool b, bool select, bool start, bool up, bool down, bool left, bool right,
                   bool x = false, bool y = false, bool l = false, bool r = false,
                   bool up2 = false, bool down2 = false, bool left2 = false, bool right2 = false)
{
    uint64_t mask = 0;
    if(a) mask |= 1ull << 0;
    if(b) mask |= 1ull << 1;
    if(select) mask |= 1ull << 2;
    if(start) mask |= 1ull << 3;
    if(up) mask |= 1ull << 4;
    if(down) mask |= 1ull << 5;
    if(left) mask |= 1ull << 6;
    if(right) mask |= 1ull << 7;
    if(x) mask |= 1ull << 8;
    if(y) mask |= 1ull << 9;
    if(l) mask |= 1ull << 10;
    if(r) mask |= 1ull << 11;
    if(up2) mask |= 1ull << 12;
    if(down2) mask |= 1ull << 13;
    if(left2) mask |= 1ull << 14;
    if(right2) mask |= 1ull << 15;
    return mask;
}

void applyMask(InputFrame& frame, PlayerSlot slot, uint64_t mask)
{
    const bool a = (mask & (1ull << 0)) != 0;
    const bool b = (mask & (1ull << 1)) != 0;
    const bool select = (mask & (1ull << 2)) != 0;
    const bool start = (mask & (1ull << 3)) != 0;
    const bool up = (mask & (1ull << 4)) != 0;
    const bool down = (mask & (1ull << 5)) != 0;
    const bool left = (mask & (1ull << 6)) != 0;
    const bool right = (mask & (1ull << 7)) != 0;
    const bool x = (mask & (1ull << 8)) != 0;
    const bool y = (mask & (1ull << 9)) != 0;
    const bool l = (mask & (1ull << 10)) != 0;
    const bool r = (mask & (1ull << 11)) != 0;
    const bool up2 = (mask & (1ull << 12)) != 0;
    const bool down2 = (mask & (1ull << 13)) != 0;
    const bool left2 = (mask & (1ull << 14)) != 0;
    const bool right2 = (mask & (1ull << 15)) != 0;

    switch(slot) {
        case kPort1PlayerSlot:
            frame.p1A = a; frame.p1B = b; frame.p1Select = select; frame.p1Start = start;
            frame.p1Up = up; frame.p1Down = down; frame.p1Left = left; frame.p1Right = right;
            frame.p1X = x; frame.p1Y = y; frame.p1L = l; frame.p1R = r;
            frame.vbP1A = a; frame.vbP1B = b; frame.vbP1Select = select; frame.vbP1Start = start;
            frame.vbP1Up0 = up; frame.vbP1Down0 = down; frame.vbP1Left0 = left; frame.vbP1Right0 = right;
            frame.vbP1Up1 = up2; frame.vbP1Down1 = down2; frame.vbP1Left1 = left2; frame.vbP1Right1 = right2;
            frame.vbP1L = l; frame.vbP1R = r;
            break;
        case kPort2PlayerSlot:
            frame.p2A = a; frame.p2B = b; frame.p2Select = select; frame.p2Start = start;
            frame.p2Up = up; frame.p2Down = down; frame.p2Left = left; frame.p2Right = right;
            frame.p2X = x; frame.p2Y = y; frame.p2L = l; frame.p2R = r;
            frame.vbP2A = a; frame.vbP2B = b; frame.vbP2Select = select; frame.vbP2Start = start;
            frame.vbP2Up0 = up; frame.vbP2Down0 = down; frame.vbP2Left0 = left; frame.vbP2Right0 = right;
            frame.vbP2Up1 = up2; frame.vbP2Down1 = down2; frame.vbP2Left1 = left2; frame.vbP2Right1 = right2;
            frame.vbP2L = l; frame.vbP2R = r;
            break;
        case kExpansionPlayerSlot:
            frame.p3A = a; frame.p3B = b; frame.p3Select = select; frame.p3Start = start;
            frame.p3Up = up; frame.p3Down = down; frame.p3Left = left; frame.p3Right = right;
            frame.bandaiA = a; frame.bandaiB = b; frame.bandaiSelect = select; frame.bandaiStart = start;
            frame.bandaiUp = up; frame.bandaiDown = down; frame.bandaiLeft = left; frame.bandaiRight = right;
            break;
        case kMultitapP1PlayerSlot:
            frame.p1A = a; frame.p1B = b; frame.p1Select = select; frame.p1Start = start;
            frame.p1Up = up; frame.p1Down = down; frame.p1Left = left; frame.p1Right = right;
            break;
        case kMultitapP2PlayerSlot:
            frame.p2A = a; frame.p2B = b; frame.p2Select = select; frame.p2Start = start;
            frame.p2Up = up; frame.p2Down = down; frame.p2Left = left; frame.p2Right = right;
            break;
        case kMultitapP3PlayerSlot:
            frame.p3A = a; frame.p3B = b; frame.p3Select = select; frame.p3Start = start;
            frame.p3Up = up; frame.p3Down = down; frame.p3Left = left; frame.p3Right = right;
            break;
        case kMultitapP4PlayerSlot:
            frame.p4A = a; frame.p4B = b; frame.p4Select = select; frame.p4Start = start;
            frame.p4Up = up; frame.p4Down = down; frame.p4Left = left; frame.p4Right = right;
            break;
        default:
            break;
    }
}

void applyBandaiPadMask(InputFrame& frame, uint64_t mask)
{
    frame.bandaiA = (mask & (1ull << 0)) != 0;
    frame.bandaiB = (mask & (1ull << 1)) != 0;
    frame.bandaiSelect = (mask & (1ull << 2)) != 0;
    frame.bandaiStart = (mask & (1ull << 3)) != 0;
    frame.bandaiUp = (mask & (1ull << 4)) != 0;
    frame.bandaiDown = (mask & (1ull << 5)) != 0;
    frame.bandaiLeft = (mask & (1ull << 6)) != 0;
    frame.bandaiRight = (mask & (1ull << 7)) != 0;
}

uint8_t boolMask(bool bit0, bool bit1 = false, bool bit2 = false, bool bit3 = false)
{
    uint8_t mask = 0;
    if(bit0) mask |= 1u << 0;
    if(bit1) mask |= 1u << 1;
    if(bit2) mask |= 1u << 2;
    if(bit3) mask |= 1u << 3;
    return mask;
}

uint16_t powerPadMask(const std::array<bool, 12>& buttons)
{
    uint16_t mask = 0;
    for(size_t index = 0; index < buttons.size(); ++index) {
        if(buttons[index]) mask |= static_cast<uint16_t>(1u << index);
    }
    return mask;
}

void applyPowerPadMask(std::array<bool, 12>& buttons, uint16_t mask)
{
    for(size_t index = 0; index < buttons.size(); ++index) {
        buttons[index] = (mask & static_cast<uint16_t>(1u << index)) != 0;
    }
}

template<size_t Count>
void writeKeyboardKeys(PacketWriter& writer, const std::array<bool, Count>& keys)
{
    uint8_t activeCount = 0;
    for(bool key : keys) {
        if(key) ++activeCount;
    }
    writer.writePod(activeCount);
    for(size_t index = 0; index < keys.size(); ++index) {
        if(keys[index]) writer.writePod(static_cast<uint8_t>(index));
    }
}

template<size_t Count>
bool readKeyboardKeys(PacketReader& reader, std::array<bool, Count>& keys)
{
    uint8_t activeCount = 0;
    if(!reader.readPod(activeCount)) return false;
    keys = {};
    for(uint8_t index = 0; index < activeCount; ++index) {
        uint8_t key = 0;
        if(!reader.readPod(key) || key >= Count) return false;
        keys[key] = true;
    }
    return true;
}

std::vector<uint8_t> makeSlotPayload(const InputFrame& inputFrame, PlayerSlot slot)
{
    PacketWriter writer;
    const auto writeHeader = [&](AdapterSlotPayloadKind kind) {
        writer.writePod(kAdapterSlotPayloadVersion);
        writer.writePod(kind);
    };

    const Settings::Device port1Device = inputFrame.port1Device;
    const Settings::Device port2Device = inputFrame.port2Device;
    const Settings::ExpansionDevice expansionDevice = inputFrame.expansionDevice;
    switch(slot) {
        case kPort1PlayerSlot:
            if(port1Device == Settings::Device::ZAPPER) {
                writeHeader(AdapterSlotPayloadKind::Zapper);
                writer.writePod(static_cast<int16_t>(inputFrame.zapperP1X));
                writer.writePod(static_cast<int16_t>(inputFrame.zapperP1Y));
                writer.writePod(boolMask(inputFrame.zapperP1Trigger));
            } else if(port1Device == Settings::Device::ARKANOID_CONTROLLER) {
                writeHeader(AdapterSlotPayloadKind::Arkanoid);
                writer.writePod(inputFrame.arkanoidP1Position);
                writer.writePod(boolMask(inputFrame.arkanoidP1Button));
            } else if(port1Device == Settings::Device::SNES_MOUSE || port1Device == Settings::Device::SUBOR_MOUSE) {
                writeHeader(AdapterSlotPayloadKind::Mouse);
                const bool subor = port1Device == Settings::Device::SUBOR_MOUSE;
                writer.writePod(static_cast<int16_t>(subor ? inputFrame.suborMouseP1DeltaX : inputFrame.snesMouseP1DeltaX));
                writer.writePod(static_cast<int16_t>(subor ? inputFrame.suborMouseP1DeltaY : inputFrame.snesMouseP1DeltaY));
                writer.writePod(boolMask(subor ? inputFrame.suborMouseP1Left : inputFrame.snesMouseP1Left,
                                         subor ? inputFrame.suborMouseP1Right : inputFrame.snesMouseP1Right));
            } else if(port1Device == Settings::Device::POWER_PAD_SIDE_A || port1Device == Settings::Device::POWER_PAD_SIDE_B) {
                writeHeader(AdapterSlotPayloadKind::PowerPad);
                writer.writePod(powerPadMask(inputFrame.powerPadP1Buttons));
            }
            break;
        case kPort2PlayerSlot:
            if(port2Device == Settings::Device::ZAPPER) {
                writeHeader(AdapterSlotPayloadKind::Zapper);
                writer.writePod(static_cast<int16_t>(inputFrame.zapperP2X));
                writer.writePod(static_cast<int16_t>(inputFrame.zapperP2Y));
                writer.writePod(boolMask(inputFrame.zapperP2Trigger));
            } else if(port2Device == Settings::Device::ARKANOID_CONTROLLER) {
                writeHeader(AdapterSlotPayloadKind::Arkanoid);
                writer.writePod(inputFrame.arkanoidP2Position);
                writer.writePod(boolMask(inputFrame.arkanoidP2Button));
            } else if(port2Device == Settings::Device::SNES_MOUSE || port2Device == Settings::Device::SUBOR_MOUSE) {
                writeHeader(AdapterSlotPayloadKind::Mouse);
                const bool subor = port2Device == Settings::Device::SUBOR_MOUSE;
                writer.writePod(static_cast<int16_t>(subor ? inputFrame.suborMouseP2DeltaX : inputFrame.snesMouseP2DeltaX));
                writer.writePod(static_cast<int16_t>(subor ? inputFrame.suborMouseP2DeltaY : inputFrame.snesMouseP2DeltaY));
                writer.writePod(boolMask(subor ? inputFrame.suborMouseP2Left : inputFrame.snesMouseP2Left,
                                         subor ? inputFrame.suborMouseP2Right : inputFrame.snesMouseP2Right));
            } else if(port2Device == Settings::Device::POWER_PAD_SIDE_A || port2Device == Settings::Device::POWER_PAD_SIDE_B) {
                writeHeader(AdapterSlotPayloadKind::PowerPad);
                writer.writePod(powerPadMask(inputFrame.powerPadP2Buttons));
            }
            break;
        case kExpansionPlayerSlot:
            if(expansionDevice == Settings::ExpansionDevice::BANDAI_HYPERSHOT) {
                writeHeader(AdapterSlotPayloadKind::BandaiPointer);
                writer.writePod(static_cast<int16_t>(inputFrame.bandaiX));
                writer.writePod(static_cast<int16_t>(inputFrame.bandaiY));
                writer.writePod(boolMask(inputFrame.bandaiTrigger));
            } else if(expansionDevice == Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
                writeHeader(AdapterSlotPayloadKind::Arkanoid);
                writer.writePod(inputFrame.arkanoidFamicomPosition);
                writer.writePod(boolMask(inputFrame.arkanoidFamicomButton));
            } else if(expansionDevice == Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
                writeHeader(AdapterSlotPayloadKind::KonamiHyperShot);
                writer.writePod(boolMask(inputFrame.konamiP1Run, inputFrame.konamiP1Jump,
                                         inputFrame.konamiP2Run, inputFrame.konamiP2Jump));
            } else if(expansionDevice == Settings::ExpansionDevice::SUBOR_KEYBOARD) {
                writeHeader(AdapterSlotPayloadKind::Keyboard);
                writer.writePod(static_cast<uint8_t>(0));
                writeKeyboardKeys(writer, inputFrame.suborKeyboardKeys);
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD) {
                writeHeader(AdapterSlotPayloadKind::Keyboard);
                writer.writePod(static_cast<uint8_t>(1));
                writeKeyboardKeys(writer, inputFrame.familyBasicKeyboardKeys);
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
                      expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B) {
                writeHeader(AdapterSlotPayloadKind::FamilyTrainer);
                writer.writePod(powerPadMask(inputFrame.powerPadP1Buttons));
                writer.writePod(powerPadMask(inputFrame.powerPadP2Buttons));
            }
            break;
        default:
            break;
    }
    return writer.data();
}

bool isControllerLike(Settings::Device device)
{
    return device == Settings::Device::CONTROLLER ||
           device == Settings::Device::FAMICOM_CONTROLLER ||
           device == Settings::Device::SNES_CONTROLLER ||
           device == Settings::Device::VIRTUAL_BOY_CONTROLLER;
}

bool slotNeedsAdapterPayload(const InputFrame& inputFrame, PlayerSlot slot)
{
    const Settings::Device port1Device = inputFrame.port1Device;
    const Settings::Device port2Device = inputFrame.port2Device;
    const Settings::ExpansionDevice expansionDevice = inputFrame.expansionDevice;
    switch(slot) {
        case kPort1PlayerSlot:
            return port1Device != Settings::Device::NONE && !isControllerLike(port1Device);
        case kPort2PlayerSlot:
            return port2Device != Settings::Device::NONE &&
                   !isControllerLike(port2Device) &&
                   port2Device != Settings::Device::BANDAI_HYPERSHOT;
        case kExpansionPlayerSlot:
            return expansionDevice != Settings::ExpansionDevice::NONE &&
                   expansionDevice != Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM;
        default:
            return false;
    }
}

void attachAdapterPayloads(NetplayInputFrame& frame, const InputFrame& inputFrame)
{
    for(PlayerSlot slot : kAllGeraNESPlayerSlots) {
        if(slotNeedsAdapterPayload(inputFrame, slot)) {
            frame.slotPayloads[slot] = makeSlotPayload(inputFrame, slot);
        }
    }
}

bool applySlotPayload(InputFrame& frame,
                      const NetplayInputFrame& inputFrame,
                      const AdapterFramePayload& adapterPayload,
                      PlayerSlot slot)
{
    if(inputFrame.slotPayloads[slot].empty()) return false;
    PacketReader reader(inputFrame.slotPayloads[slot].data(), inputFrame.slotPayloads[slot].size());
    uint8_t version = 0;
    AdapterSlotPayloadKind kind = AdapterSlotPayloadKind::None;
    if(!reader.readPod(version) || version != kAdapterSlotPayloadVersion) return false;
    if(!reader.readPod(kind)) return false;

    int16_t x = 0;
    int16_t y = 0;
    uint8_t flags = 0;
    float position = 0.5f;
    uint16_t mask = 0;
    uint16_t mask2 = 0;
    switch(kind) {
        case AdapterSlotPayloadKind::Zapper:
            if(!reader.readPod(x) || !reader.readPod(y) || !reader.readPod(flags)) return false;
            if(slot == kPort1PlayerSlot) {
                frame.zapperP1X = x; frame.zapperP1Y = y; frame.zapperP1Trigger = (flags & 1u) != 0;
            } else if(slot == kPort2PlayerSlot) {
                frame.zapperP2X = x; frame.zapperP2Y = y; frame.zapperP2Trigger = (flags & 1u) != 0;
            }
            break;
        case AdapterSlotPayloadKind::Arkanoid:
            if(!reader.readPod(position) || !reader.readPod(flags)) return false;
            if(slot == kPort1PlayerSlot) {
                frame.arkanoidP1Position = position; frame.arkanoidP1Button = (flags & 1u) != 0;
            } else if(slot == kPort2PlayerSlot) {
                frame.arkanoidP2Position = position; frame.arkanoidP2Button = (flags & 1u) != 0;
            } else if(slot == kExpansionPlayerSlot) {
                frame.arkanoidFamicomPosition = position; frame.arkanoidFamicomButton = (flags & 1u) != 0;
            }
            break;
        case AdapterSlotPayloadKind::Mouse:
            if(!reader.readPod(x) || !reader.readPod(y) || !reader.readPod(flags)) return false;
            if(slot == kPort1PlayerSlot && adapterPayload.port1Device == Settings::Device::SUBOR_MOUSE) {
                frame.suborMouseP1DeltaX = x; frame.suborMouseP1DeltaY = y;
                frame.suborMouseP1Left = (flags & 1u) != 0; frame.suborMouseP1Right = (flags & 2u) != 0;
            } else if(slot == kPort1PlayerSlot) {
                frame.snesMouseP1DeltaX = x; frame.snesMouseP1DeltaY = y;
                frame.snesMouseP1Left = (flags & 1u) != 0; frame.snesMouseP1Right = (flags & 2u) != 0;
            } else if(slot == kPort2PlayerSlot && adapterPayload.port2Device == Settings::Device::SUBOR_MOUSE) {
                frame.suborMouseP2DeltaX = x; frame.suborMouseP2DeltaY = y;
                frame.suborMouseP2Left = (flags & 1u) != 0; frame.suborMouseP2Right = (flags & 2u) != 0;
            } else if(slot == kPort2PlayerSlot) {
                frame.snesMouseP2DeltaX = x; frame.snesMouseP2DeltaY = y;
                frame.snesMouseP2Left = (flags & 1u) != 0; frame.snesMouseP2Right = (flags & 2u) != 0;
            }
            break;
        case AdapterSlotPayloadKind::PowerPad:
            if(!reader.readPod(mask)) return false;
            if(slot == kPort1PlayerSlot) applyPowerPadMask(frame.powerPadP1Buttons, mask);
            if(slot == kPort2PlayerSlot) applyPowerPadMask(frame.powerPadP2Buttons, mask);
            break;
        case AdapterSlotPayloadKind::BandaiPointer:
            if(!reader.readPod(x) || !reader.readPod(y) || !reader.readPod(flags)) return false;
            frame.bandaiX = x; frame.bandaiY = y; frame.bandaiTrigger = (flags & 1u) != 0;
            break;
        case AdapterSlotPayloadKind::KonamiHyperShot:
            if(!reader.readPod(flags)) return false;
            frame.konamiP1Run = (flags & 1u) != 0; frame.konamiP1Jump = (flags & 2u) != 0;
            frame.konamiP2Run = (flags & 4u) != 0; frame.konamiP2Jump = (flags & 8u) != 0;
            break;
        case AdapterSlotPayloadKind::Keyboard:
            if(!reader.readPod(flags)) return false;
            if(flags == 0) {
                if(!readKeyboardKeys(reader, frame.suborKeyboardKeys)) return false;
            } else if(flags == 1) {
                if(!readKeyboardKeys(reader, frame.familyBasicKeyboardKeys)) return false;
            } else {
                return false;
            }
            break;
        case AdapterSlotPayloadKind::FamilyTrainer:
            if(!reader.readPod(mask) || !reader.readPod(mask2)) return false;
            applyPowerPadMask(frame.powerPadP1Buttons, mask);
            applyPowerPadMask(frame.powerPadP2Buttons, mask2);
            break;
        default:
            return false;
    }
    return reader.remaining() == 0;
}

} // namespace

NetplayInputFrame toNetplayInputFrame(const InputFrame& inputFrame)
{
    NetplayInputFrame frame;
    frame.frame = inputFrame.frame;
    frame.timelineEpoch = inputFrame.timelineEpoch;
    frame.speculative = inputFrame.speculative;
    frame.framePayload = makeAdapterFramePayload(inputFrame);
    frame.buttonMaskLo[kPort1PlayerSlot] =
        buildMask(inputFrame.p1A, inputFrame.p1B, inputFrame.p1Select, inputFrame.p1Start,
                  inputFrame.p1Up, inputFrame.p1Down, inputFrame.p1Left, inputFrame.p1Right,
                  inputFrame.p1X, inputFrame.p1Y, inputFrame.p1L, inputFrame.p1R,
                  inputFrame.vbP1Up1, inputFrame.vbP1Down1, inputFrame.vbP1Left1, inputFrame.vbP1Right1);
    frame.buttonMaskLo[kPort2PlayerSlot] =
        buildMask(inputFrame.p2A, inputFrame.p2B, inputFrame.p2Select, inputFrame.p2Start,
                  inputFrame.p2Up, inputFrame.p2Down, inputFrame.p2Left, inputFrame.p2Right,
                  inputFrame.p2X, inputFrame.p2Y, inputFrame.p2L, inputFrame.p2R,
                  inputFrame.vbP2Up1, inputFrame.vbP2Down1, inputFrame.vbP2Left1, inputFrame.vbP2Right1);
    frame.buttonMaskLo[kExpansionPlayerSlot] =
        buildMask(inputFrame.p3A || inputFrame.bandaiA,
                  inputFrame.p3B || inputFrame.bandaiB,
                  inputFrame.p3Select || inputFrame.bandaiSelect,
                  inputFrame.p3Start || inputFrame.bandaiStart,
                  inputFrame.p3Up || inputFrame.bandaiUp,
                  inputFrame.p3Down || inputFrame.bandaiDown,
                  inputFrame.p3Left || inputFrame.bandaiLeft,
                  inputFrame.p3Right || inputFrame.bandaiRight);
    frame.buttonMaskLo[kMultitapP1PlayerSlot] =
        buildMask(inputFrame.p1A, inputFrame.p1B, inputFrame.p1Select, inputFrame.p1Start,
                  inputFrame.p1Up, inputFrame.p1Down, inputFrame.p1Left, inputFrame.p1Right);
    frame.buttonMaskLo[kMultitapP2PlayerSlot] =
        buildMask(inputFrame.p2A, inputFrame.p2B, inputFrame.p2Select, inputFrame.p2Start,
                  inputFrame.p2Up, inputFrame.p2Down, inputFrame.p2Left, inputFrame.p2Right);
    frame.buttonMaskLo[kMultitapP3PlayerSlot] =
        buildMask(inputFrame.p3A, inputFrame.p3B, inputFrame.p3Select, inputFrame.p3Start,
                  inputFrame.p3Up, inputFrame.p3Down, inputFrame.p3Left, inputFrame.p3Right);
    frame.buttonMaskLo[kMultitapP4PlayerSlot] =
        buildMask(inputFrame.p4A, inputFrame.p4B, inputFrame.p4Select, inputFrame.p4Start,
                  inputFrame.p4Up, inputFrame.p4Down, inputFrame.p4Left, inputFrame.p4Right);
    attachAdapterPayloads(frame, inputFrame);
    return frame;
}

InputFrame toGeraNESInputFrame(const NetplayInputFrame& inputFrame)
{
    InputFrame fallbackTopologyFrame{};
    fallbackTopologyFrame.frame = inputFrame.frame;
    fallbackTopologyFrame.timelineEpoch = inputFrame.timelineEpoch;
    return toGeraNESInputFrame(inputFrame, fallbackTopologyFrame);
}

InputFrame toGeraNESInputFrame(const NetplayInputFrame& inputFrame, const InputFrame& fallbackTopologyFrame)
{
    InputFrame frame;
    frame.frame = inputFrame.frame;
    frame.timelineEpoch = inputFrame.timelineEpoch;
    frame.speculative = inputFrame.speculative;
    AdapterFramePayload adapterPayload;
    const bool hasAdapterPayload = readAdapterFramePayload(inputFrame, adapterPayload);
    const bool payloadHasTopology =
        adapterPayload.port1Device != Settings::Device::NONE ||
        adapterPayload.port2Device != Settings::Device::NONE ||
        adapterPayload.expansionDevice != Settings::ExpansionDevice::NONE ||
        adapterPayload.nesMultitapDevice != Settings::NesMultitapDevice::NONE ||
        adapterPayload.famicomMultitapDevice != Settings::FamicomMultitapDevice::NONE;
    const bool fallbackHasTopology =
        fallbackTopologyFrame.port1Device != Settings::Device::NONE ||
        fallbackTopologyFrame.port2Device != Settings::Device::NONE ||
        fallbackTopologyFrame.expansionDevice != Settings::ExpansionDevice::NONE ||
        fallbackTopologyFrame.nesMultitapDevice != Settings::NesMultitapDevice::NONE ||
        fallbackTopologyFrame.famicomMultitapDevice != Settings::FamicomMultitapDevice::NONE;

    if(hasAdapterPayload && (payloadHasTopology || !fallbackHasTopology)) {
        frame.port1Device = adapterPayload.port1Device;
        frame.port2Device = adapterPayload.port2Device;
        frame.expansionDevice = adapterPayload.expansionDevice;
        frame.nesMultitapDevice = adapterPayload.nesMultitapDevice;
        frame.famicomMultitapDevice = adapterPayload.famicomMultitapDevice;
    } else {
        frame.port1Device = fallbackTopologyFrame.port1Device;
        frame.port2Device = fallbackTopologyFrame.port2Device;
        frame.expansionDevice = fallbackTopologyFrame.expansionDevice;
        frame.nesMultitapDevice = fallbackTopologyFrame.nesMultitapDevice;
        frame.famicomMultitapDevice = fallbackTopologyFrame.famicomMultitapDevice;
        adapterPayload.port1Device = frame.port1Device;
        adapterPayload.port2Device = frame.port2Device;
        adapterPayload.expansionDevice = frame.expansionDevice;
        adapterPayload.nesMultitapDevice = frame.nesMultitapDevice;
        adapterPayload.famicomMultitapDevice = frame.famicomMultitapDevice;
    }
    const bool multitapActive =
        adapterPayload.nesMultitapDevice != Settings::NesMultitapDevice::NONE ||
        adapterPayload.famicomMultitapDevice != Settings::FamicomMultitapDevice::NONE;
    if(multitapActive) {
        applyMask(frame, kMultitapP1PlayerSlot, inputFrame.buttonMaskLo[kMultitapP1PlayerSlot]);
        applyMask(frame, kMultitapP2PlayerSlot, inputFrame.buttonMaskLo[kMultitapP2PlayerSlot]);
        applyMask(frame, kMultitapP3PlayerSlot, inputFrame.buttonMaskLo[kMultitapP3PlayerSlot]);
        applyMask(frame, kMultitapP4PlayerSlot, inputFrame.buttonMaskLo[kMultitapP4PlayerSlot]);
    } else {
        applyMask(frame, kPort1PlayerSlot, inputFrame.buttonMaskLo[kPort1PlayerSlot]);
        applyMask(frame, kPort2PlayerSlot, inputFrame.buttonMaskLo[kPort2PlayerSlot]);
        applyMask(frame, kExpansionPlayerSlot, inputFrame.buttonMaskLo[kExpansionPlayerSlot]);
    }
    if(adapterPayload.port2Device == Settings::Device::BANDAI_HYPERSHOT) {
        applyBandaiPadMask(frame, inputFrame.buttonMaskLo[kPort2PlayerSlot]);
    }
    for(PlayerSlot slot : kAllGeraNESPlayerSlots) {
        (void)applySlotPayload(frame, inputFrame, adapterPayload, slot);
    }
    return frame;
}

InputFrame makeRoomTopologyBaseFrame(FrameNumber frame, const RoomState& room)
{
    InputFrame inputFrame{};
    inputFrame.frame = frame;
    inputFrame.timelineEpoch = room.timelineEpoch;
    inputFrame.port1Device = geraNESPortDeviceFromTopology(room, kPort1PlayerSlot);
    inputFrame.port2Device = geraNESPortDeviceFromTopology(room, kPort2PlayerSlot);
    inputFrame.expansionDevice = geraNESExpansionDeviceFromTopology(room);
    inputFrame.nesMultitapDevice = geraNESNesMultitapDeviceFromTopology(room);
    inputFrame.famicomMultitapDevice = geraNESFamicomMultitapDeviceFromTopology(room);
    return inputFrame;
}

namespace {

InputFrame makeContributionBase(const InputFrame& baseFrame)
{
    InputFrame contribution{};
    contribution.frame = baseFrame.frame;
    contribution.timelineEpoch = baseFrame.timelineEpoch;
    contribution.speculative = baseFrame.speculative;
    contribution.port1Device = baseFrame.port1Device;
    contribution.port2Device = baseFrame.port2Device;
    contribution.expansionDevice = baseFrame.expansionDevice;
    contribution.nesMultitapDevice = baseFrame.nesMultitapDevice;
    contribution.famicomMultitapDevice = baseFrame.famicomMultitapDevice;
    return contribution;
}

} // namespace

InputFrame buildAssignedContribution(PlayerSlot slot,
                                            const GeraNESInputState& state,
                                            const InputFrame& baseFrame)
{
    InputFrame contribution = makeContributionBase(baseFrame);

    switch(slot) {
        case kPort1PlayerSlot: {
            const Settings::Device device = baseFrame.port1Device;
            if(device == Settings::Device::CONTROLLER ||
               device == Settings::Device::FAMICOM_CONTROLLER ||
               device == Settings::Device::SNES_CONTROLLER ||
               device == Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                contribution.p1A = state.p1A; contribution.p1B = state.p1B; contribution.p1Select = state.p1Select; contribution.p1Start = state.p1Start;
                contribution.p1Up = state.p1Up; contribution.p1Down = state.p1Down; contribution.p1Left = state.p1Left; contribution.p1Right = state.p1Right;
                contribution.p1X = state.p1X; contribution.p1Y = state.p1Y; contribution.p1L = state.p1L; contribution.p1R = state.p1R;
                contribution.vbP1A = state.p1A; contribution.vbP1B = state.p1B; contribution.vbP1Select = state.p1Select; contribution.vbP1Start = state.p1Start;
                contribution.vbP1Up0 = state.p1Up; contribution.vbP1Down0 = state.p1Down; contribution.vbP1Left0 = state.p1Left; contribution.vbP1Right0 = state.p1Right;
                contribution.vbP1Up1 = state.p1Up2; contribution.vbP1Down1 = state.p1Down2; contribution.vbP1Left1 = state.p1Left2; contribution.vbP1Right1 = state.p1Right2;
                contribution.vbP1L = state.p1L; contribution.vbP1R = state.p1R;
            } else if(device == Settings::Device::ZAPPER) {
                contribution.zapperP1X = state.zapperX; contribution.zapperP1Y = state.zapperY; contribution.zapperP1Trigger = state.zapperP1Trigger;
            } else if(device == Settings::Device::ARKANOID_CONTROLLER) {
                contribution.arkanoidP1Position = state.arkanoidNesPosition; contribution.arkanoidP1Button = state.mousePrimaryButton;
            } else if(device == Settings::Device::SNES_MOUSE) {
                contribution.snesMouseP1DeltaX = state.mouseDeltaX; contribution.snesMouseP1DeltaY = state.mouseDeltaY;
                contribution.snesMouseP1Left = state.mousePrimaryButton; contribution.snesMouseP1Right = state.mouseSecondaryButton;
            } else if(device == Settings::Device::SUBOR_MOUSE) {
                contribution.suborMouseP1DeltaX = state.mouseDeltaX; contribution.suborMouseP1DeltaY = state.mouseDeltaY;
                contribution.suborMouseP1Left = state.mousePrimaryButton; contribution.suborMouseP1Right = state.mouseSecondaryButton;
            } else if(device == Settings::Device::POWER_PAD_SIDE_A ||
                      device == Settings::Device::POWER_PAD_SIDE_B) {
                contribution.powerPadP1Buttons = state.p1PowerPadButtons;
            }
            break;
        }
        case kPort2PlayerSlot: {
            const Settings::Device device = baseFrame.port2Device;
            if(device == Settings::Device::CONTROLLER ||
               device == Settings::Device::FAMICOM_CONTROLLER ||
               device == Settings::Device::SNES_CONTROLLER ||
               device == Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                contribution.p2A = state.p2A; contribution.p2B = state.p2B; contribution.p2Select = state.p2Select; contribution.p2Start = state.p2Start;
                contribution.p2Up = state.p2Up; contribution.p2Down = state.p2Down; contribution.p2Left = state.p2Left; contribution.p2Right = state.p2Right;
                contribution.p2X = state.p2X; contribution.p2Y = state.p2Y; contribution.p2L = state.p2L; contribution.p2R = state.p2R;
                contribution.vbP2A = state.p2A; contribution.vbP2B = state.p2B; contribution.vbP2Select = state.p2Select; contribution.vbP2Start = state.p2Start;
                contribution.vbP2Up0 = state.p2Up; contribution.vbP2Down0 = state.p2Down; contribution.vbP2Left0 = state.p2Left; contribution.vbP2Right0 = state.p2Right;
                contribution.vbP2Up1 = state.p2Up2; contribution.vbP2Down1 = state.p2Down2; contribution.vbP2Left1 = state.p2Left2; contribution.vbP2Right1 = state.p2Right2;
                contribution.vbP2L = state.p2L; contribution.vbP2R = state.p2R;
            } else if(device == Settings::Device::ZAPPER) {
                contribution.zapperP2X = state.zapperX; contribution.zapperP2Y = state.zapperY; contribution.zapperP2Trigger = state.zapperP2Trigger;
            } else if(device == Settings::Device::ARKANOID_CONTROLLER) {
                contribution.arkanoidP2Position = state.arkanoidNesPosition; contribution.arkanoidP2Button = state.mousePrimaryButton;
            } else if(device == Settings::Device::SNES_MOUSE) {
                contribution.snesMouseP2DeltaX = state.mouseDeltaX; contribution.snesMouseP2DeltaY = state.mouseDeltaY;
                contribution.snesMouseP2Left = state.mousePrimaryButton; contribution.snesMouseP2Right = state.mouseSecondaryButton;
            } else if(device == Settings::Device::SUBOR_MOUSE) {
                contribution.suborMouseP2DeltaX = state.mouseDeltaX; contribution.suborMouseP2DeltaY = state.mouseDeltaY;
                contribution.suborMouseP2Left = state.mousePrimaryButton; contribution.suborMouseP2Right = state.mouseSecondaryButton;
            } else if(device == Settings::Device::POWER_PAD_SIDE_A ||
                      device == Settings::Device::POWER_PAD_SIDE_B) {
                contribution.powerPadP2Buttons = state.p2PowerPadButtons;
            } else if(device == Settings::Device::BANDAI_HYPERSHOT) {
                contribution.bandaiA = state.p2A; contribution.bandaiB = state.p2B; contribution.bandaiSelect = state.p2Select; contribution.bandaiStart = state.p2Start;
                contribution.bandaiUp = state.p2Up; contribution.bandaiDown = state.p2Down; contribution.bandaiLeft = state.p2Left; contribution.bandaiRight = state.p2Right;
            }
            break;
        }
        case kExpansionPlayerSlot: {
            const Settings::ExpansionDevice expansionDevice = baseFrame.expansionDevice;
            if(expansionDevice == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM) {
                contribution.p3A = state.p3A; contribution.p3B = state.p3B; contribution.p3Select = state.p3Select; contribution.p3Start = state.p3Start;
                contribution.p3Up = state.p3Up; contribution.p3Down = state.p3Down; contribution.p3Left = state.p3Left; contribution.p3Right = state.p3Right;
            } else if(expansionDevice == Settings::ExpansionDevice::BANDAI_HYPERSHOT) {
                contribution.bandaiX = state.zapperX; contribution.bandaiY = state.zapperY; contribution.bandaiTrigger = state.bandaiTrigger;
            } else if(expansionDevice == Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
                contribution.arkanoidFamicomPosition = state.arkanoidFamicomPosition; contribution.arkanoidFamicomButton = state.mousePrimaryButton;
            } else if(expansionDevice == Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
                contribution.konamiP1Run = state.konamiP1Run; contribution.konamiP1Jump = state.konamiP1Jump;
                contribution.konamiP2Run = state.konamiP2Run; contribution.konamiP2Jump = state.konamiP2Jump;
            } else if(expansionDevice == Settings::ExpansionDevice::SUBOR_KEYBOARD) {
                contribution.suborKeyboardKeys = state.suborKeyboardKeys;
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD) {
                contribution.familyBasicKeyboardKeys = state.familyBasicKeyboardKeys;
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
                      expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B) {
                contribution.powerPadP1Buttons = state.p1PowerPadButtons;
                contribution.powerPadP2Buttons = state.p2PowerPadButtons;
            }
            break;
        }
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

void applyAssignedContribution(InputFrame& target, PlayerSlot slot, const InputFrame& contribution)
{
    switch(slot) {
        case kPort1PlayerSlot: {
            const Settings::Device device = target.port1Device;
            if(device == Settings::Device::CONTROLLER ||
               device == Settings::Device::FAMICOM_CONTROLLER ||
               device == Settings::Device::SNES_CONTROLLER ||
               device == Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                target.p1A = contribution.p1A; target.p1B = contribution.p1B; target.p1Select = contribution.p1Select; target.p1Start = contribution.p1Start;
                target.p1Up = contribution.p1Up; target.p1Down = contribution.p1Down; target.p1Left = contribution.p1Left; target.p1Right = contribution.p1Right;
                target.p1X = contribution.p1X; target.p1Y = contribution.p1Y; target.p1L = contribution.p1L; target.p1R = contribution.p1R;
                target.vbP1A = contribution.vbP1A; target.vbP1B = contribution.vbP1B; target.vbP1Select = contribution.vbP1Select; target.vbP1Start = contribution.vbP1Start;
                target.vbP1Up0 = contribution.vbP1Up0; target.vbP1Down0 = contribution.vbP1Down0; target.vbP1Left0 = contribution.vbP1Left0; target.vbP1Right0 = contribution.vbP1Right0;
                target.vbP1Up1 = contribution.vbP1Up1; target.vbP1Down1 = contribution.vbP1Down1; target.vbP1Left1 = contribution.vbP1Left1; target.vbP1Right1 = contribution.vbP1Right1;
                target.vbP1L = contribution.vbP1L; target.vbP1R = contribution.vbP1R;
            } else if(device == Settings::Device::ZAPPER) {
                target.zapperP1X = contribution.zapperP1X; target.zapperP1Y = contribution.zapperP1Y; target.zapperP1Trigger = contribution.zapperP1Trigger;
            } else if(device == Settings::Device::ARKANOID_CONTROLLER) {
                target.arkanoidP1Position = contribution.arkanoidP1Position; target.arkanoidP1Button = contribution.arkanoidP1Button;
            } else if(device == Settings::Device::SNES_MOUSE) {
                target.snesMouseP1DeltaX = contribution.snesMouseP1DeltaX; target.snesMouseP1DeltaY = contribution.snesMouseP1DeltaY;
                target.snesMouseP1Left = contribution.snesMouseP1Left; target.snesMouseP1Right = contribution.snesMouseP1Right;
            } else if(device == Settings::Device::SUBOR_MOUSE) {
                target.suborMouseP1DeltaX = contribution.suborMouseP1DeltaX; target.suborMouseP1DeltaY = contribution.suborMouseP1DeltaY;
                target.suborMouseP1Left = contribution.suborMouseP1Left; target.suborMouseP1Right = contribution.suborMouseP1Right;
            } else if(device == Settings::Device::POWER_PAD_SIDE_A ||
                      device == Settings::Device::POWER_PAD_SIDE_B) {
                target.powerPadP1Buttons = contribution.powerPadP1Buttons;
            }
            break;
        }
        case kPort2PlayerSlot: {
            const Settings::Device device = target.port2Device;
            if(device == Settings::Device::CONTROLLER ||
               device == Settings::Device::FAMICOM_CONTROLLER ||
               device == Settings::Device::SNES_CONTROLLER ||
               device == Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                target.p2A = contribution.p2A; target.p2B = contribution.p2B; target.p2Select = contribution.p2Select; target.p2Start = contribution.p2Start;
                target.p2Up = contribution.p2Up; target.p2Down = contribution.p2Down; target.p2Left = contribution.p2Left; target.p2Right = contribution.p2Right;
                target.p2X = contribution.p2X; target.p2Y = contribution.p2Y; target.p2L = contribution.p2L; target.p2R = contribution.p2R;
                target.vbP2A = contribution.vbP2A; target.vbP2B = contribution.vbP2B; target.vbP2Select = contribution.vbP2Select; target.vbP2Start = contribution.vbP2Start;
                target.vbP2Up0 = contribution.vbP2Up0; target.vbP2Down0 = contribution.vbP2Down0; target.vbP2Left0 = contribution.vbP2Left0; target.vbP2Right0 = contribution.vbP2Right0;
                target.vbP2Up1 = contribution.vbP2Up1; target.vbP2Down1 = contribution.vbP2Down1; target.vbP2Left1 = contribution.vbP2Left1; target.vbP2Right1 = contribution.vbP2Right1;
                target.vbP2L = contribution.vbP2L; target.vbP2R = contribution.vbP2R;
            } else if(device == Settings::Device::ZAPPER) {
                target.zapperP2X = contribution.zapperP2X; target.zapperP2Y = contribution.zapperP2Y; target.zapperP2Trigger = contribution.zapperP2Trigger;
            } else if(device == Settings::Device::ARKANOID_CONTROLLER) {
                target.arkanoidP2Position = contribution.arkanoidP2Position; target.arkanoidP2Button = contribution.arkanoidP2Button;
            } else if(device == Settings::Device::SNES_MOUSE) {
                target.snesMouseP2DeltaX = contribution.snesMouseP2DeltaX; target.snesMouseP2DeltaY = contribution.snesMouseP2DeltaY;
                target.snesMouseP2Left = contribution.snesMouseP2Left; target.snesMouseP2Right = contribution.snesMouseP2Right;
            } else if(device == Settings::Device::SUBOR_MOUSE) {
                target.suborMouseP2DeltaX = contribution.suborMouseP2DeltaX; target.suborMouseP2DeltaY = contribution.suborMouseP2DeltaY;
                target.suborMouseP2Left = contribution.suborMouseP2Left; target.suborMouseP2Right = contribution.suborMouseP2Right;
            } else if(device == Settings::Device::POWER_PAD_SIDE_A ||
                      device == Settings::Device::POWER_PAD_SIDE_B) {
                target.powerPadP2Buttons = contribution.powerPadP2Buttons;
            } else if(device == Settings::Device::BANDAI_HYPERSHOT) {
                target.bandaiA = contribution.bandaiA; target.bandaiB = contribution.bandaiB; target.bandaiSelect = contribution.bandaiSelect; target.bandaiStart = contribution.bandaiStart;
                target.bandaiUp = contribution.bandaiUp; target.bandaiDown = contribution.bandaiDown; target.bandaiLeft = contribution.bandaiLeft; target.bandaiRight = contribution.bandaiRight;
            }
            break;
        }
        case kExpansionPlayerSlot: {
            const Settings::ExpansionDevice expansionDevice = target.expansionDevice;
            if(expansionDevice == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM) {
                target.p3A = contribution.p3A; target.p3B = contribution.p3B; target.p3Select = contribution.p3Select; target.p3Start = contribution.p3Start;
                target.p3Up = contribution.p3Up; target.p3Down = contribution.p3Down; target.p3Left = contribution.p3Left; target.p3Right = contribution.p3Right;
            } else if(expansionDevice == Settings::ExpansionDevice::BANDAI_HYPERSHOT) {
                target.bandaiX = contribution.bandaiX; target.bandaiY = contribution.bandaiY; target.bandaiTrigger = contribution.bandaiTrigger;
            } else if(expansionDevice == Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
                target.arkanoidFamicomPosition = contribution.arkanoidFamicomPosition; target.arkanoidFamicomButton = contribution.arkanoidFamicomButton;
            } else if(expansionDevice == Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
                target.konamiP1Run = contribution.konamiP1Run; target.konamiP1Jump = contribution.konamiP1Jump;
                target.konamiP2Run = contribution.konamiP2Run; target.konamiP2Jump = contribution.konamiP2Jump;
            } else if(expansionDevice == Settings::ExpansionDevice::SUBOR_KEYBOARD) {
                target.suborKeyboardKeys = contribution.suborKeyboardKeys;
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD) {
                target.familyBasicKeyboardKeys = contribution.familyBasicKeyboardKeys;
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
                      expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B) {
                target.powerPadP1Buttons = contribution.powerPadP1Buttons;
                target.powerPadP2Buttons = contribution.powerPadP2Buttons;
            }
            break;
        }
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

bool injectInputFrameForTests(NetplayCoordinator& coordinator,
                              const InputFrameData& input,
                              const InputFrame& contribution)
{
    return coordinator.injectInputFrameForTests(input, toNetplayInputFrame(contribution));
}

} // namespace GeraNESNetplay
