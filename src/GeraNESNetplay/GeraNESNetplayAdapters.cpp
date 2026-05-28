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
    InputFrame::DecodedData decoded = frame.decodedData();
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
            decoded.p1A = a; decoded.p1B = b; decoded.p1Select = select; decoded.p1Start = start;
            decoded.p1Up = up; decoded.p1Down = down; decoded.p1Left = left; decoded.p1Right = right;
            decoded.p1X = x; decoded.p1Y = y; decoded.p1L = l; decoded.p1R = r;
            decoded.vbP1A = a; decoded.vbP1B = b; decoded.vbP1Select = select; decoded.vbP1Start = start;
            decoded.vbP1Up0 = up; decoded.vbP1Down0 = down; decoded.vbP1Left0 = left; decoded.vbP1Right0 = right;
            decoded.vbP1Up1 = up2; decoded.vbP1Down1 = down2; decoded.vbP1Left1 = left2; decoded.vbP1Right1 = right2;
            decoded.vbP1L = l; decoded.vbP1R = r;
            break;
        case kPort2PlayerSlot:
            decoded.p2A = a; decoded.p2B = b; decoded.p2Select = select; decoded.p2Start = start;
            decoded.p2Up = up; decoded.p2Down = down; decoded.p2Left = left; decoded.p2Right = right;
            decoded.p2X = x; decoded.p2Y = y; decoded.p2L = l; decoded.p2R = r;
            decoded.vbP2A = a; decoded.vbP2B = b; decoded.vbP2Select = select; decoded.vbP2Start = start;
            decoded.vbP2Up0 = up; decoded.vbP2Down0 = down; decoded.vbP2Left0 = left; decoded.vbP2Right0 = right;
            decoded.vbP2Up1 = up2; decoded.vbP2Down1 = down2; decoded.vbP2Left1 = left2; decoded.vbP2Right1 = right2;
            decoded.vbP2L = l; decoded.vbP2R = r;
            break;
        case kExpansionPlayerSlot:
            decoded.p3A = a; decoded.p3B = b; decoded.p3Select = select; decoded.p3Start = start;
            decoded.p3Up = up; decoded.p3Down = down; decoded.p3Left = left; decoded.p3Right = right;
            decoded.bandaiA = a; decoded.bandaiB = b; decoded.bandaiSelect = select; decoded.bandaiStart = start;
            decoded.bandaiUp = up; decoded.bandaiDown = down; decoded.bandaiLeft = left; decoded.bandaiRight = right;
            break;
        case kMultitapP1PlayerSlot:
            decoded.p1A = a; decoded.p1B = b; decoded.p1Select = select; decoded.p1Start = start;
            decoded.p1Up = up; decoded.p1Down = down; decoded.p1Left = left; decoded.p1Right = right;
            break;
        case kMultitapP2PlayerSlot:
            decoded.p2A = a; decoded.p2B = b; decoded.p2Select = select; decoded.p2Start = start;
            decoded.p2Up = up; decoded.p2Down = down; decoded.p2Left = left; decoded.p2Right = right;
            break;
        case kMultitapP3PlayerSlot:
            decoded.p3A = a; decoded.p3B = b; decoded.p3Select = select; decoded.p3Start = start;
            decoded.p3Up = up; decoded.p3Down = down; decoded.p3Left = left; decoded.p3Right = right;
            break;
        case kMultitapP4PlayerSlot:
            decoded.p4A = a; decoded.p4B = b; decoded.p4Select = select; decoded.p4Start = start;
            decoded.p4Up = up; decoded.p4Down = down; decoded.p4Left = left; decoded.p4Right = right;
            break;
        default:
            break;
    }
    (void)frame.setDecodedData(decoded);
}

void applyBandaiPadMask(InputFrame& frame, uint64_t mask)
{
    InputFrame::DecodedData decoded = frame.decodedData();
    decoded.bandaiA = (mask & (1ull << 0)) != 0;
    decoded.bandaiB = (mask & (1ull << 1)) != 0;
    decoded.bandaiSelect = (mask & (1ull << 2)) != 0;
    decoded.bandaiStart = (mask & (1ull << 3)) != 0;
    decoded.bandaiUp = (mask & (1ull << 4)) != 0;
    decoded.bandaiDown = (mask & (1ull << 5)) != 0;
    decoded.bandaiLeft = (mask & (1ull << 6)) != 0;
    decoded.bandaiRight = (mask & (1ull << 7)) != 0;
    (void)frame.setDecodedData(decoded);
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
    const InputFrame::DecodedData decoded = inputFrame.decodedData();
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
                writer.writePod(static_cast<int16_t>(decoded.zapperP1X));
                writer.writePod(static_cast<int16_t>(decoded.zapperP1Y));
                writer.writePod(boolMask(decoded.zapperP1Trigger));
            } else if(port1Device == Settings::Device::ARKANOID_CONTROLLER) {
                writeHeader(AdapterSlotPayloadKind::Arkanoid);
                writer.writePod(decoded.arkanoidP1Position);
                writer.writePod(boolMask(decoded.arkanoidP1Button));
            } else if(port1Device == Settings::Device::SNES_MOUSE || port1Device == Settings::Device::SUBOR_MOUSE) {
                writeHeader(AdapterSlotPayloadKind::Mouse);
                const bool subor = port1Device == Settings::Device::SUBOR_MOUSE;
                writer.writePod(static_cast<int16_t>(subor ? decoded.suborMouseP1DeltaX : decoded.snesMouseP1DeltaX));
                writer.writePod(static_cast<int16_t>(subor ? decoded.suborMouseP1DeltaY : decoded.snesMouseP1DeltaY));
                writer.writePod(boolMask(subor ? decoded.suborMouseP1Left : decoded.snesMouseP1Left,
                                         subor ? decoded.suborMouseP1Right : decoded.snesMouseP1Right));
            } else if(port1Device == Settings::Device::POWER_PAD_SIDE_A || port1Device == Settings::Device::POWER_PAD_SIDE_B) {
                writeHeader(AdapterSlotPayloadKind::PowerPad);
                writer.writePod(powerPadMask(decoded.powerPadP1Buttons));
            }
            break;
        case kPort2PlayerSlot:
            if(port2Device == Settings::Device::ZAPPER) {
                writeHeader(AdapterSlotPayloadKind::Zapper);
                writer.writePod(static_cast<int16_t>(decoded.zapperP2X));
                writer.writePod(static_cast<int16_t>(decoded.zapperP2Y));
                writer.writePod(boolMask(decoded.zapperP2Trigger));
            } else if(port2Device == Settings::Device::ARKANOID_CONTROLLER) {
                writeHeader(AdapterSlotPayloadKind::Arkanoid);
                writer.writePod(decoded.arkanoidP2Position);
                writer.writePod(boolMask(decoded.arkanoidP2Button));
            } else if(port2Device == Settings::Device::SNES_MOUSE || port2Device == Settings::Device::SUBOR_MOUSE) {
                writeHeader(AdapterSlotPayloadKind::Mouse);
                const bool subor = port2Device == Settings::Device::SUBOR_MOUSE;
                writer.writePod(static_cast<int16_t>(subor ? decoded.suborMouseP2DeltaX : decoded.snesMouseP2DeltaX));
                writer.writePod(static_cast<int16_t>(subor ? decoded.suborMouseP2DeltaY : decoded.snesMouseP2DeltaY));
                writer.writePod(boolMask(subor ? decoded.suborMouseP2Left : decoded.snesMouseP2Left,
                                         subor ? decoded.suborMouseP2Right : decoded.snesMouseP2Right));
            } else if(port2Device == Settings::Device::POWER_PAD_SIDE_A || port2Device == Settings::Device::POWER_PAD_SIDE_B) {
                writeHeader(AdapterSlotPayloadKind::PowerPad);
                writer.writePod(powerPadMask(decoded.powerPadP2Buttons));
            }
            break;
        case kExpansionPlayerSlot:
            if(expansionDevice == Settings::ExpansionDevice::BANDAI_HYPERSHOT) {
                writeHeader(AdapterSlotPayloadKind::BandaiPointer);
                writer.writePod(static_cast<int16_t>(decoded.bandaiX));
                writer.writePod(static_cast<int16_t>(decoded.bandaiY));
                writer.writePod(boolMask(decoded.bandaiTrigger));
            } else if(expansionDevice == Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
                writeHeader(AdapterSlotPayloadKind::Arkanoid);
                writer.writePod(decoded.arkanoidFamicomPosition);
                writer.writePod(boolMask(decoded.arkanoidFamicomButton));
            } else if(expansionDevice == Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
                writeHeader(AdapterSlotPayloadKind::KonamiHyperShot);
                writer.writePod(boolMask(decoded.konamiP1Run, decoded.konamiP1Jump,
                                         decoded.konamiP2Run, decoded.konamiP2Jump));
            } else if(expansionDevice == Settings::ExpansionDevice::SUBOR_KEYBOARD) {
                writeHeader(AdapterSlotPayloadKind::Keyboard);
                writer.writePod(static_cast<uint8_t>(0));
                writeKeyboardKeys(writer, decoded.suborKeyboardKeys);
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD) {
                writeHeader(AdapterSlotPayloadKind::Keyboard);
                writer.writePod(static_cast<uint8_t>(1));
                writeKeyboardKeys(writer, decoded.familyBasicKeyboardKeys);
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
                      expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B) {
                writeHeader(AdapterSlotPayloadKind::FamilyTrainer);
                writer.writePod(powerPadMask(decoded.powerPadP1Buttons));
                writer.writePod(powerPadMask(decoded.powerPadP2Buttons));
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
    InputFrame::DecodedData decoded = frame.decodedData();
    switch(kind) {
        case AdapterSlotPayloadKind::Zapper:
            if(!reader.readPod(x) || !reader.readPod(y) || !reader.readPod(flags)) return false;
            if(slot == kPort1PlayerSlot) {
                decoded.zapperP1X = x; decoded.zapperP1Y = y; decoded.zapperP1Trigger = (flags & 1u) != 0;
            } else if(slot == kPort2PlayerSlot) {
                decoded.zapperP2X = x; decoded.zapperP2Y = y; decoded.zapperP2Trigger = (flags & 1u) != 0;
            }
            break;
        case AdapterSlotPayloadKind::Arkanoid:
            if(!reader.readPod(position) || !reader.readPod(flags)) return false;
            if(slot == kPort1PlayerSlot) {
                decoded.arkanoidP1Position = position; decoded.arkanoidP1Button = (flags & 1u) != 0;
            } else if(slot == kPort2PlayerSlot) {
                decoded.arkanoidP2Position = position; decoded.arkanoidP2Button = (flags & 1u) != 0;
            } else if(slot == kExpansionPlayerSlot) {
                decoded.arkanoidFamicomPosition = position; decoded.arkanoidFamicomButton = (flags & 1u) != 0;
            }
            break;
        case AdapterSlotPayloadKind::Mouse:
            if(!reader.readPod(x) || !reader.readPod(y) || !reader.readPod(flags)) return false;
            if(slot == kPort1PlayerSlot && adapterPayload.port1Device == Settings::Device::SUBOR_MOUSE) {
                decoded.suborMouseP1DeltaX = x; decoded.suborMouseP1DeltaY = y;
                decoded.suborMouseP1Left = (flags & 1u) != 0; decoded.suborMouseP1Right = (flags & 2u) != 0;
            } else if(slot == kPort1PlayerSlot) {
                decoded.snesMouseP1DeltaX = x; decoded.snesMouseP1DeltaY = y;
                decoded.snesMouseP1Left = (flags & 1u) != 0; decoded.snesMouseP1Right = (flags & 2u) != 0;
            } else if(slot == kPort2PlayerSlot && adapterPayload.port2Device == Settings::Device::SUBOR_MOUSE) {
                decoded.suborMouseP2DeltaX = x; decoded.suborMouseP2DeltaY = y;
                decoded.suborMouseP2Left = (flags & 1u) != 0; decoded.suborMouseP2Right = (flags & 2u) != 0;
            } else if(slot == kPort2PlayerSlot) {
                decoded.snesMouseP2DeltaX = x; decoded.snesMouseP2DeltaY = y;
                decoded.snesMouseP2Left = (flags & 1u) != 0; decoded.snesMouseP2Right = (flags & 2u) != 0;
            }
            break;
        case AdapterSlotPayloadKind::PowerPad:
            if(!reader.readPod(mask)) return false;
            if(slot == kPort1PlayerSlot) applyPowerPadMask(decoded.powerPadP1Buttons, mask);
            if(slot == kPort2PlayerSlot) applyPowerPadMask(decoded.powerPadP2Buttons, mask);
            break;
        case AdapterSlotPayloadKind::BandaiPointer:
            if(!reader.readPod(x) || !reader.readPod(y) || !reader.readPod(flags)) return false;
            decoded.bandaiX = x; decoded.bandaiY = y; decoded.bandaiTrigger = (flags & 1u) != 0;
            break;
        case AdapterSlotPayloadKind::KonamiHyperShot:
            if(!reader.readPod(flags)) return false;
            decoded.konamiP1Run = (flags & 1u) != 0; decoded.konamiP1Jump = (flags & 2u) != 0;
            decoded.konamiP2Run = (flags & 4u) != 0; decoded.konamiP2Jump = (flags & 8u) != 0;
            break;
        case AdapterSlotPayloadKind::Keyboard:
            if(!reader.readPod(flags)) return false;
            if(flags == 0) {
                if(!readKeyboardKeys(reader, decoded.suborKeyboardKeys)) return false;
            } else if(flags == 1) {
                if(!readKeyboardKeys(reader, decoded.familyBasicKeyboardKeys)) return false;
            } else {
                return false;
            }
            break;
        case AdapterSlotPayloadKind::FamilyTrainer:
            if(!reader.readPod(mask) || !reader.readPod(mask2)) return false;
            applyPowerPadMask(decoded.powerPadP1Buttons, mask);
            applyPowerPadMask(decoded.powerPadP2Buttons, mask2);
            break;
        default:
            return false;
    }
    if(reader.remaining() != 0) {
        return false;
    }
    return frame.setDecodedData(decoded);
}

} // namespace

NetplayInputFrame toNetplayInputFrame(const InputFrame& inputFrame)
{
    const InputFrame::DecodedData decoded = inputFrame.decodedData();
    NetplayInputFrame frame;
    frame.frame = inputFrame.frame;
    frame.timelineEpoch = inputFrame.timelineEpoch;
    frame.framePayload = makeAdapterFramePayload(inputFrame);
    frame.buttonMaskLo[kPort1PlayerSlot] =
        buildMask(decoded.p1A, decoded.p1B, decoded.p1Select, decoded.p1Start,
                  decoded.p1Up, decoded.p1Down, decoded.p1Left, decoded.p1Right,
                  decoded.p1X, decoded.p1Y, decoded.p1L, decoded.p1R,
                  decoded.vbP1Up1, decoded.vbP1Down1, decoded.vbP1Left1, decoded.vbP1Right1);
    frame.buttonMaskLo[kPort2PlayerSlot] =
        buildMask(decoded.p2A, decoded.p2B, decoded.p2Select, decoded.p2Start,
                  decoded.p2Up, decoded.p2Down, decoded.p2Left, decoded.p2Right,
                  decoded.p2X, decoded.p2Y, decoded.p2L, decoded.p2R,
                  decoded.vbP2Up1, decoded.vbP2Down1, decoded.vbP2Left1, decoded.vbP2Right1);
    frame.buttonMaskLo[kExpansionPlayerSlot] =
        buildMask(decoded.p3A || decoded.bandaiA,
                  decoded.p3B || decoded.bandaiB,
                  decoded.p3Select || decoded.bandaiSelect,
                  decoded.p3Start || decoded.bandaiStart,
                  decoded.p3Up || decoded.bandaiUp,
                  decoded.p3Down || decoded.bandaiDown,
                  decoded.p3Left || decoded.bandaiLeft,
                  decoded.p3Right || decoded.bandaiRight);
    frame.buttonMaskLo[kMultitapP1PlayerSlot] =
        buildMask(decoded.p1A, decoded.p1B, decoded.p1Select, decoded.p1Start,
                  decoded.p1Up, decoded.p1Down, decoded.p1Left, decoded.p1Right);
    frame.buttonMaskLo[kMultitapP2PlayerSlot] =
        buildMask(decoded.p2A, decoded.p2B, decoded.p2Select, decoded.p2Start,
                  decoded.p2Up, decoded.p2Down, decoded.p2Left, decoded.p2Right);
    frame.buttonMaskLo[kMultitapP3PlayerSlot] =
        buildMask(decoded.p3A, decoded.p3B, decoded.p3Select, decoded.p3Start,
                  decoded.p3Up, decoded.p3Down, decoded.p3Left, decoded.p3Right);
    frame.buttonMaskLo[kMultitapP4PlayerSlot] =
        buildMask(decoded.p4A, decoded.p4B, decoded.p4Select, decoded.p4Start,
                  decoded.p4Up, decoded.p4Down, decoded.p4Left, decoded.p4Right);
    attachAdapterPayloads(frame, inputFrame);
    return frame;
}

InputFrame toGeraNESInputFrame(const NetplayInputFrame& inputFrame)
{
    InputFrame frame;
    frame.frame = inputFrame.frame;
    frame.timelineEpoch = inputFrame.timelineEpoch;
    AdapterFramePayload adapterPayload;
    if(readAdapterFramePayload(inputFrame, adapterPayload)) {
        frame.port1Device = adapterPayload.port1Device;
        frame.port2Device = adapterPayload.port2Device;
        frame.expansionDevice = adapterPayload.expansionDevice;
        frame.nesMultitapDevice = adapterPayload.nesMultitapDevice;
        frame.famicomMultitapDevice = adapterPayload.famicomMultitapDevice;
    }
    (void)frame.setDecodedData({});
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
    InputFrame::DecodedData decoded;

    switch(slot) {
        case kPort1PlayerSlot: {
            const Settings::Device device = baseFrame.port1Device;
            if(device == Settings::Device::CONTROLLER ||
               device == Settings::Device::FAMICOM_CONTROLLER ||
               device == Settings::Device::SNES_CONTROLLER ||
               device == Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                decoded.p1A = state.p1A; decoded.p1B = state.p1B; decoded.p1Select = state.p1Select; decoded.p1Start = state.p1Start;
                decoded.p1Up = state.p1Up; decoded.p1Down = state.p1Down; decoded.p1Left = state.p1Left; decoded.p1Right = state.p1Right;
                decoded.p1X = state.p1X; decoded.p1Y = state.p1Y; decoded.p1L = state.p1L; decoded.p1R = state.p1R;
                decoded.vbP1A = state.p1A; decoded.vbP1B = state.p1B; decoded.vbP1Select = state.p1Select; decoded.vbP1Start = state.p1Start;
                decoded.vbP1Up0 = state.p1Up; decoded.vbP1Down0 = state.p1Down; decoded.vbP1Left0 = state.p1Left; decoded.vbP1Right0 = state.p1Right;
                decoded.vbP1Up1 = state.p1Up2; decoded.vbP1Down1 = state.p1Down2; decoded.vbP1Left1 = state.p1Left2; decoded.vbP1Right1 = state.p1Right2;
                decoded.vbP1L = state.p1L; decoded.vbP1R = state.p1R;
            } else if(device == Settings::Device::ZAPPER) {
                decoded.zapperP1X = state.zapperX; decoded.zapperP1Y = state.zapperY; decoded.zapperP1Trigger = state.zapperP1Trigger;
            } else if(device == Settings::Device::ARKANOID_CONTROLLER) {
                decoded.arkanoidP1Position = state.arkanoidNesPosition; decoded.arkanoidP1Button = state.mousePrimaryButton;
            } else if(device == Settings::Device::SNES_MOUSE) {
                decoded.snesMouseP1DeltaX = state.mouseDeltaX; decoded.snesMouseP1DeltaY = state.mouseDeltaY;
                decoded.snesMouseP1Left = state.mousePrimaryButton; decoded.snesMouseP1Right = state.mouseSecondaryButton;
            } else if(device == Settings::Device::SUBOR_MOUSE) {
                decoded.suborMouseP1DeltaX = state.mouseDeltaX; decoded.suborMouseP1DeltaY = state.mouseDeltaY;
                decoded.suborMouseP1Left = state.mousePrimaryButton; decoded.suborMouseP1Right = state.mouseSecondaryButton;
            } else if(device == Settings::Device::POWER_PAD_SIDE_A ||
                      device == Settings::Device::POWER_PAD_SIDE_B) {
                decoded.powerPadP1Buttons = state.p1PowerPadButtons;
            }
            break;
        }
        case kPort2PlayerSlot: {
            const Settings::Device device = baseFrame.port2Device;
            if(device == Settings::Device::CONTROLLER ||
               device == Settings::Device::FAMICOM_CONTROLLER ||
               device == Settings::Device::SNES_CONTROLLER ||
               device == Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                decoded.p2A = state.p2A; decoded.p2B = state.p2B; decoded.p2Select = state.p2Select; decoded.p2Start = state.p2Start;
                decoded.p2Up = state.p2Up; decoded.p2Down = state.p2Down; decoded.p2Left = state.p2Left; decoded.p2Right = state.p2Right;
                decoded.p2X = state.p2X; decoded.p2Y = state.p2Y; decoded.p2L = state.p2L; decoded.p2R = state.p2R;
                decoded.vbP2A = state.p2A; decoded.vbP2B = state.p2B; decoded.vbP2Select = state.p2Select; decoded.vbP2Start = state.p2Start;
                decoded.vbP2Up0 = state.p2Up; decoded.vbP2Down0 = state.p2Down; decoded.vbP2Left0 = state.p2Left; decoded.vbP2Right0 = state.p2Right;
                decoded.vbP2Up1 = state.p2Up2; decoded.vbP2Down1 = state.p2Down2; decoded.vbP2Left1 = state.p2Left2; decoded.vbP2Right1 = state.p2Right2;
                decoded.vbP2L = state.p2L; decoded.vbP2R = state.p2R;
            } else if(device == Settings::Device::ZAPPER) {
                decoded.zapperP2X = state.zapperX; decoded.zapperP2Y = state.zapperY; decoded.zapperP2Trigger = state.zapperP2Trigger;
            } else if(device == Settings::Device::ARKANOID_CONTROLLER) {
                decoded.arkanoidP2Position = state.arkanoidNesPosition; decoded.arkanoidP2Button = state.mousePrimaryButton;
            } else if(device == Settings::Device::SNES_MOUSE) {
                decoded.snesMouseP2DeltaX = state.mouseDeltaX; decoded.snesMouseP2DeltaY = state.mouseDeltaY;
                decoded.snesMouseP2Left = state.mousePrimaryButton; decoded.snesMouseP2Right = state.mouseSecondaryButton;
            } else if(device == Settings::Device::SUBOR_MOUSE) {
                decoded.suborMouseP2DeltaX = state.mouseDeltaX; decoded.suborMouseP2DeltaY = state.mouseDeltaY;
                decoded.suborMouseP2Left = state.mousePrimaryButton; decoded.suborMouseP2Right = state.mouseSecondaryButton;
            } else if(device == Settings::Device::POWER_PAD_SIDE_A ||
                      device == Settings::Device::POWER_PAD_SIDE_B) {
                decoded.powerPadP2Buttons = state.p2PowerPadButtons;
            } else if(device == Settings::Device::BANDAI_HYPERSHOT) {
                decoded.bandaiA = state.p2A; decoded.bandaiB = state.p2B; decoded.bandaiSelect = state.p2Select; decoded.bandaiStart = state.p2Start;
                decoded.bandaiUp = state.p2Up; decoded.bandaiDown = state.p2Down; decoded.bandaiLeft = state.p2Left; decoded.bandaiRight = state.p2Right;
            }
            break;
        }
        case kExpansionPlayerSlot: {
            const Settings::ExpansionDevice expansionDevice = baseFrame.expansionDevice;
            if(expansionDevice == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM) {
                decoded.p3A = state.p3A; decoded.p3B = state.p3B; decoded.p3Select = state.p3Select; decoded.p3Start = state.p3Start;
                decoded.p3Up = state.p3Up; decoded.p3Down = state.p3Down; decoded.p3Left = state.p3Left; decoded.p3Right = state.p3Right;
            } else if(expansionDevice == Settings::ExpansionDevice::BANDAI_HYPERSHOT) {
                decoded.bandaiX = state.zapperX; decoded.bandaiY = state.zapperY; decoded.bandaiTrigger = state.bandaiTrigger;
            } else if(expansionDevice == Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
                decoded.arkanoidFamicomPosition = state.arkanoidFamicomPosition; decoded.arkanoidFamicomButton = state.mousePrimaryButton;
            } else if(expansionDevice == Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
                decoded.konamiP1Run = state.konamiP1Run; decoded.konamiP1Jump = state.konamiP1Jump;
                decoded.konamiP2Run = state.konamiP2Run; decoded.konamiP2Jump = state.konamiP2Jump;
            } else if(expansionDevice == Settings::ExpansionDevice::SUBOR_KEYBOARD) {
                decoded.suborKeyboardKeys = state.suborKeyboardKeys;
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD) {
                decoded.familyBasicKeyboardKeys = state.familyBasicKeyboardKeys;
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
                      expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B) {
                decoded.powerPadP1Buttons = state.p1PowerPadButtons;
                decoded.powerPadP2Buttons = state.p2PowerPadButtons;
            }
            break;
        }
        case kMultitapP1PlayerSlot:
            decoded.p1A = state.p1A; decoded.p1B = state.p1B; decoded.p1Select = state.p1Select; decoded.p1Start = state.p1Start;
            decoded.p1Up = state.p1Up; decoded.p1Down = state.p1Down; decoded.p1Left = state.p1Left; decoded.p1Right = state.p1Right;
            break;
        case kMultitapP2PlayerSlot:
            decoded.p2A = state.p2A; decoded.p2B = state.p2B; decoded.p2Select = state.p2Select; decoded.p2Start = state.p2Start;
            decoded.p2Up = state.p2Up; decoded.p2Down = state.p2Down; decoded.p2Left = state.p2Left; decoded.p2Right = state.p2Right;
            break;
        case kMultitapP3PlayerSlot:
            decoded.p3A = state.p3A; decoded.p3B = state.p3B; decoded.p3Select = state.p3Select; decoded.p3Start = state.p3Start;
            decoded.p3Up = state.p3Up; decoded.p3Down = state.p3Down; decoded.p3Left = state.p3Left; decoded.p3Right = state.p3Right;
            break;
        case kMultitapP4PlayerSlot:
            decoded.p4A = state.p4A; decoded.p4B = state.p4B; decoded.p4Select = state.p4Select; decoded.p4Start = state.p4Start;
            decoded.p4Up = state.p4Up; decoded.p4Down = state.p4Down; decoded.p4Left = state.p4Left; decoded.p4Right = state.p4Right;
            break;
        default:
            break;
    }
    (void)contribution.setDecodedData(decoded);
    return contribution;
}

void applyAssignedContribution(InputFrame& target, PlayerSlot slot, const InputFrame& contribution)
{
    InputFrame::DecodedData targetDecoded = target.decodedData();
    const InputFrame::DecodedData contributionDecoded = contribution.decodedData();
    switch(slot) {
        case kPort1PlayerSlot: {
            const Settings::Device device = target.port1Device;
            if(device == Settings::Device::CONTROLLER ||
               device == Settings::Device::FAMICOM_CONTROLLER ||
               device == Settings::Device::SNES_CONTROLLER ||
               device == Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                targetDecoded.p1A = contributionDecoded.p1A; targetDecoded.p1B = contributionDecoded.p1B; targetDecoded.p1Select = contributionDecoded.p1Select; targetDecoded.p1Start = contributionDecoded.p1Start;
                targetDecoded.p1Up = contributionDecoded.p1Up; targetDecoded.p1Down = contributionDecoded.p1Down; targetDecoded.p1Left = contributionDecoded.p1Left; targetDecoded.p1Right = contributionDecoded.p1Right;
                targetDecoded.p1X = contributionDecoded.p1X; targetDecoded.p1Y = contributionDecoded.p1Y; targetDecoded.p1L = contributionDecoded.p1L; targetDecoded.p1R = contributionDecoded.p1R;
                targetDecoded.vbP1A = contributionDecoded.vbP1A; targetDecoded.vbP1B = contributionDecoded.vbP1B; targetDecoded.vbP1Select = contributionDecoded.vbP1Select; targetDecoded.vbP1Start = contributionDecoded.vbP1Start;
                targetDecoded.vbP1Up0 = contributionDecoded.vbP1Up0; targetDecoded.vbP1Down0 = contributionDecoded.vbP1Down0; targetDecoded.vbP1Left0 = contributionDecoded.vbP1Left0; targetDecoded.vbP1Right0 = contributionDecoded.vbP1Right0;
                targetDecoded.vbP1Up1 = contributionDecoded.vbP1Up1; targetDecoded.vbP1Down1 = contributionDecoded.vbP1Down1; targetDecoded.vbP1Left1 = contributionDecoded.vbP1Left1; targetDecoded.vbP1Right1 = contributionDecoded.vbP1Right1;
                targetDecoded.vbP1L = contributionDecoded.vbP1L; targetDecoded.vbP1R = contributionDecoded.vbP1R;
            } else if(device == Settings::Device::ZAPPER) {
                targetDecoded.zapperP1X = contributionDecoded.zapperP1X; targetDecoded.zapperP1Y = contributionDecoded.zapperP1Y; targetDecoded.zapperP1Trigger = contributionDecoded.zapperP1Trigger;
            } else if(device == Settings::Device::ARKANOID_CONTROLLER) {
                targetDecoded.arkanoidP1Position = contributionDecoded.arkanoidP1Position; targetDecoded.arkanoidP1Button = contributionDecoded.arkanoidP1Button;
            } else if(device == Settings::Device::SNES_MOUSE) {
                targetDecoded.snesMouseP1DeltaX = contributionDecoded.snesMouseP1DeltaX; targetDecoded.snesMouseP1DeltaY = contributionDecoded.snesMouseP1DeltaY;
                targetDecoded.snesMouseP1Left = contributionDecoded.snesMouseP1Left; targetDecoded.snesMouseP1Right = contributionDecoded.snesMouseP1Right;
            } else if(device == Settings::Device::SUBOR_MOUSE) {
                targetDecoded.suborMouseP1DeltaX = contributionDecoded.suborMouseP1DeltaX; targetDecoded.suborMouseP1DeltaY = contributionDecoded.suborMouseP1DeltaY;
                targetDecoded.suborMouseP1Left = contributionDecoded.suborMouseP1Left; targetDecoded.suborMouseP1Right = contributionDecoded.suborMouseP1Right;
            } else if(device == Settings::Device::POWER_PAD_SIDE_A ||
                      device == Settings::Device::POWER_PAD_SIDE_B) {
                targetDecoded.powerPadP1Buttons = contributionDecoded.powerPadP1Buttons;
            }
            break;
        }
        case kPort2PlayerSlot: {
            const Settings::Device device = target.port2Device;
            if(device == Settings::Device::CONTROLLER ||
               device == Settings::Device::FAMICOM_CONTROLLER ||
               device == Settings::Device::SNES_CONTROLLER ||
               device == Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                targetDecoded.p2A = contributionDecoded.p2A; targetDecoded.p2B = contributionDecoded.p2B; targetDecoded.p2Select = contributionDecoded.p2Select; targetDecoded.p2Start = contributionDecoded.p2Start;
                targetDecoded.p2Up = contributionDecoded.p2Up; targetDecoded.p2Down = contributionDecoded.p2Down; targetDecoded.p2Left = contributionDecoded.p2Left; targetDecoded.p2Right = contributionDecoded.p2Right;
                targetDecoded.p2X = contributionDecoded.p2X; targetDecoded.p2Y = contributionDecoded.p2Y; targetDecoded.p2L = contributionDecoded.p2L; targetDecoded.p2R = contributionDecoded.p2R;
                targetDecoded.vbP2A = contributionDecoded.vbP2A; targetDecoded.vbP2B = contributionDecoded.vbP2B; targetDecoded.vbP2Select = contributionDecoded.vbP2Select; targetDecoded.vbP2Start = contributionDecoded.vbP2Start;
                targetDecoded.vbP2Up0 = contributionDecoded.vbP2Up0; targetDecoded.vbP2Down0 = contributionDecoded.vbP2Down0; targetDecoded.vbP2Left0 = contributionDecoded.vbP2Left0; targetDecoded.vbP2Right0 = contributionDecoded.vbP2Right0;
                targetDecoded.vbP2Up1 = contributionDecoded.vbP2Up1; targetDecoded.vbP2Down1 = contributionDecoded.vbP2Down1; targetDecoded.vbP2Left1 = contributionDecoded.vbP2Left1; targetDecoded.vbP2Right1 = contributionDecoded.vbP2Right1;
                targetDecoded.vbP2L = contributionDecoded.vbP2L; targetDecoded.vbP2R = contributionDecoded.vbP2R;
            } else if(device == Settings::Device::ZAPPER) {
                targetDecoded.zapperP2X = contributionDecoded.zapperP2X; targetDecoded.zapperP2Y = contributionDecoded.zapperP2Y; targetDecoded.zapperP2Trigger = contributionDecoded.zapperP2Trigger;
            } else if(device == Settings::Device::ARKANOID_CONTROLLER) {
                targetDecoded.arkanoidP2Position = contributionDecoded.arkanoidP2Position; targetDecoded.arkanoidP2Button = contributionDecoded.arkanoidP2Button;
            } else if(device == Settings::Device::SNES_MOUSE) {
                targetDecoded.snesMouseP2DeltaX = contributionDecoded.snesMouseP2DeltaX; targetDecoded.snesMouseP2DeltaY = contributionDecoded.snesMouseP2DeltaY;
                targetDecoded.snesMouseP2Left = contributionDecoded.snesMouseP2Left; targetDecoded.snesMouseP2Right = contributionDecoded.snesMouseP2Right;
            } else if(device == Settings::Device::SUBOR_MOUSE) {
                targetDecoded.suborMouseP2DeltaX = contributionDecoded.suborMouseP2DeltaX; targetDecoded.suborMouseP2DeltaY = contributionDecoded.suborMouseP2DeltaY;
                targetDecoded.suborMouseP2Left = contributionDecoded.suborMouseP2Left; targetDecoded.suborMouseP2Right = contributionDecoded.suborMouseP2Right;
            } else if(device == Settings::Device::POWER_PAD_SIDE_A ||
                      device == Settings::Device::POWER_PAD_SIDE_B) {
                targetDecoded.powerPadP2Buttons = contributionDecoded.powerPadP2Buttons;
            } else if(device == Settings::Device::BANDAI_HYPERSHOT) {
                targetDecoded.bandaiA = contributionDecoded.bandaiA; targetDecoded.bandaiB = contributionDecoded.bandaiB; targetDecoded.bandaiSelect = contributionDecoded.bandaiSelect; targetDecoded.bandaiStart = contributionDecoded.bandaiStart;
                targetDecoded.bandaiUp = contributionDecoded.bandaiUp; targetDecoded.bandaiDown = contributionDecoded.bandaiDown; targetDecoded.bandaiLeft = contributionDecoded.bandaiLeft; targetDecoded.bandaiRight = contributionDecoded.bandaiRight;
            }
            break;
        }
        case kExpansionPlayerSlot: {
            const Settings::ExpansionDevice expansionDevice = target.expansionDevice;
            if(expansionDevice == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM) {
                targetDecoded.p3A = contributionDecoded.p3A; targetDecoded.p3B = contributionDecoded.p3B; targetDecoded.p3Select = contributionDecoded.p3Select; targetDecoded.p3Start = contributionDecoded.p3Start;
                targetDecoded.p3Up = contributionDecoded.p3Up; targetDecoded.p3Down = contributionDecoded.p3Down; targetDecoded.p3Left = contributionDecoded.p3Left; targetDecoded.p3Right = contributionDecoded.p3Right;
            } else if(expansionDevice == Settings::ExpansionDevice::BANDAI_HYPERSHOT) {
                targetDecoded.bandaiX = contributionDecoded.bandaiX; targetDecoded.bandaiY = contributionDecoded.bandaiY; targetDecoded.bandaiTrigger = contributionDecoded.bandaiTrigger;
            } else if(expansionDevice == Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
                targetDecoded.arkanoidFamicomPosition = contributionDecoded.arkanoidFamicomPosition; targetDecoded.arkanoidFamicomButton = contributionDecoded.arkanoidFamicomButton;
            } else if(expansionDevice == Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
                targetDecoded.konamiP1Run = contributionDecoded.konamiP1Run; targetDecoded.konamiP1Jump = contributionDecoded.konamiP1Jump;
                targetDecoded.konamiP2Run = contributionDecoded.konamiP2Run; targetDecoded.konamiP2Jump = contributionDecoded.konamiP2Jump;
            } else if(expansionDevice == Settings::ExpansionDevice::SUBOR_KEYBOARD) {
                targetDecoded.suborKeyboardKeys = contributionDecoded.suborKeyboardKeys;
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD) {
                targetDecoded.familyBasicKeyboardKeys = contributionDecoded.familyBasicKeyboardKeys;
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
                      expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B) {
                targetDecoded.powerPadP1Buttons = contributionDecoded.powerPadP1Buttons;
                targetDecoded.powerPadP2Buttons = contributionDecoded.powerPadP2Buttons;
            }
            break;
        }
        case kMultitapP1PlayerSlot:
            targetDecoded.p1A = contributionDecoded.p1A; targetDecoded.p1B = contributionDecoded.p1B; targetDecoded.p1Select = contributionDecoded.p1Select; targetDecoded.p1Start = contributionDecoded.p1Start;
            targetDecoded.p1Up = contributionDecoded.p1Up; targetDecoded.p1Down = contributionDecoded.p1Down; targetDecoded.p1Left = contributionDecoded.p1Left; targetDecoded.p1Right = contributionDecoded.p1Right;
            break;
        case kMultitapP2PlayerSlot:
            targetDecoded.p2A = contributionDecoded.p2A; targetDecoded.p2B = contributionDecoded.p2B; targetDecoded.p2Select = contributionDecoded.p2Select; targetDecoded.p2Start = contributionDecoded.p2Start;
            targetDecoded.p2Up = contributionDecoded.p2Up; targetDecoded.p2Down = contributionDecoded.p2Down; targetDecoded.p2Left = contributionDecoded.p2Left; targetDecoded.p2Right = contributionDecoded.p2Right;
            break;
        case kMultitapP3PlayerSlot:
            targetDecoded.p3A = contributionDecoded.p3A; targetDecoded.p3B = contributionDecoded.p3B; targetDecoded.p3Select = contributionDecoded.p3Select; targetDecoded.p3Start = contributionDecoded.p3Start;
            targetDecoded.p3Up = contributionDecoded.p3Up; targetDecoded.p3Down = contributionDecoded.p3Down; targetDecoded.p3Left = contributionDecoded.p3Left; targetDecoded.p3Right = contributionDecoded.p3Right;
            break;
        case kMultitapP4PlayerSlot:
            targetDecoded.p4A = contributionDecoded.p4A; targetDecoded.p4B = contributionDecoded.p4B; targetDecoded.p4Select = contributionDecoded.p4Select; targetDecoded.p4Start = contributionDecoded.p4Start;
            targetDecoded.p4Up = contributionDecoded.p4Up; targetDecoded.p4Down = contributionDecoded.p4Down; targetDecoded.p4Left = contributionDecoded.p4Left; targetDecoded.p4Right = contributionDecoded.p4Right;
            break;
        default:
            break;
    }
    (void)target.setDecodedData(targetDecoded);
}

bool injectInputFrameForTests(NetplayCoordinator& coordinator,
                              const InputFrameData& input,
                              const InputFrame& contribution)
{
    return coordinator.injectInputFrameForTests(input, toNetplayInputFrame(contribution));
}

} // namespace GeraNESNetplay
