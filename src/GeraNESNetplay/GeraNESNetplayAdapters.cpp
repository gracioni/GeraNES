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

std::vector<InputSlotDescriptor> makeGeraNESInputTopology(const InputTopology& inputTopology)
{
    return makeGeraNESInputTopology(
        inputTopology.port1Device,
        inputTopology.port2Device,
        inputTopology.expansionDevice,
        inputTopology.nesMultitapDevice,
        inputTopology.famicomMultitapDevice
    );
}

InputTopology roomInputTopology(const RoomState& room)
{
    InputTopology inputTopology;
    inputTopology.port1Device = geraNESPortDeviceFromTopology(room, kPort1PlayerSlot);
    inputTopology.port2Device = geraNESPortDeviceFromTopology(room, kPort2PlayerSlot);
    inputTopology.expansionDevice = geraNESExpansionDeviceFromTopology(room);
    inputTopology.nesMultitapDevice = geraNESNesMultitapDeviceFromTopology(room);
    inputTopology.famicomMultitapDevice = geraNESFamicomMultitapDeviceFromTopology(room);
    return inputTopology;
}

InputTopology frameInputTopology(const InputFrame& inputFrame)
{
    return inputFrame.state.topology;
}

void applyInputTopology(InputFrame& inputFrame, const InputTopology& inputTopology)
{
    inputFrame.state.topology = inputTopology;
}

} // namespace

RoomState roomWithGeraNESInputTopology(RoomState room,
                                       const InputTopology& inputTopology)
{
    room.inputTopology = makeGeraNESInputTopology(inputTopology);
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
                                 const InputTopology& inputTopology,
                                 std::optional<ParticipantId> preservedParticipantId,
                                 PlayerSlot preservedAssignment)
{
    const RoomState currentRoom = coordinator.session().roomState();
    std::vector<InputSlotDescriptor> candidateTopology = makeGeraNESInputTopology(inputTopology);
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
    InputTopology inputTopology = {};
};

std::vector<uint8_t> makeAdapterFramePayload(const InputFrame& inputFrame)
{
    PacketWriter writer;
    writer.writePod(kAdapterFramePayloadVersion);
    const InputTopology inputTopology = frameInputTopology(inputFrame);
    writer.writePod(static_cast<uint8_t>(inputTopology.port1Device.value_or(Settings::Device::NONE)));
    writer.writePod(static_cast<uint8_t>(inputTopology.port2Device.value_or(Settings::Device::NONE)));
    writer.writePod(static_cast<uint8_t>(inputTopology.expansionDevice));
    writer.writePod(static_cast<uint8_t>(inputTopology.nesMultitapDevice));
    writer.writePod(static_cast<uint8_t>(inputTopology.famicomMultitapDevice));
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
    payload.inputTopology.port1Device = static_cast<Settings::Device>(port1Device);
    payload.inputTopology.port2Device = static_cast<Settings::Device>(port2Device);
    payload.inputTopology.expansionDevice = static_cast<Settings::ExpansionDevice>(expansionDevice);
    payload.inputTopology.nesMultitapDevice = static_cast<Settings::NesMultitapDevice>(nesMultitapDevice);
    payload.inputTopology.famicomMultitapDevice = static_cast<Settings::FamicomMultitapDevice>(famicomMultitapDevice);
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

InputState::PadButtons padButtonsFromMask(uint64_t mask)
{
    return {
        (mask & (1ull << 0)) != 0,
        (mask & (1ull << 1)) != 0,
        (mask & (1ull << 2)) != 0,
        (mask & (1ull << 3)) != 0,
        (mask & (1ull << 4)) != 0,
        (mask & (1ull << 5)) != 0,
        (mask & (1ull << 6)) != 0,
        (mask & (1ull << 7)) != 0,
        (mask & (1ull << 8)) != 0,
        (mask & (1ull << 9)) != 0,
        (mask & (1ull << 10)) != 0,
        (mask & (1ull << 11)) != 0,
        (mask & (1ull << 12)) != 0,
        (mask & (1ull << 13)) != 0,
        (mask & (1ull << 14)) != 0,
        (mask & (1ull << 15)) != 0
    };
}

void applyMask(InputFrame& frame, PlayerSlot slot, uint64_t mask)
{
    const InputState::PadButtons buttons = padButtonsFromMask(mask);

    switch(slot) {
        case kPort1PlayerSlot:
            frame.state.setPortButtons(1, buttons);
            break;
        case kPort2PlayerSlot:
            frame.state.setPortButtons(2, buttons);
            break;
        case kExpansionPlayerSlot:
            frame.state.setPortButtons(3, buttons);
            frame.state.setBandaiButtons(buttons);
            break;
        case kMultitapP1PlayerSlot:
            frame.state.setPortButtons(1, buttons);
            break;
        case kMultitapP2PlayerSlot:
            frame.state.setPortButtons(2, buttons);
            break;
        case kMultitapP3PlayerSlot:
            frame.state.setPortButtons(3, buttons);
            break;
        case kMultitapP4PlayerSlot:
            frame.state.setPortButtons(4, buttons);
            break;
        default:
            break;
    }
}

void applyBandaiPadMask(InputFrame& frame, uint64_t mask)
{
    frame.state.setBandaiButtons(padButtonsFromMask(mask));
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

    const InputTopology& topology = inputFrame.state.topology;
    const Settings::Device port1Device = topology.port1Device.value_or(Settings::Device::NONE);
    const Settings::Device port2Device = topology.port2Device.value_or(Settings::Device::NONE);
    const Settings::ExpansionDevice expansionDevice = topology.expansionDevice;
    switch(slot) {
        case kPort1PlayerSlot:
            if(port1Device == Settings::Device::ZAPPER) {
                const InputState::PointerState state = inputFrame.state.zapper(1);
                writeHeader(AdapterSlotPayloadKind::Zapper);
                writer.writePod(static_cast<int16_t>(state.x));
                writer.writePod(static_cast<int16_t>(state.y));
                writer.writePod(boolMask(state.trigger));
            } else if(port1Device == Settings::Device::ARKANOID_CONTROLLER) {
                const InputState::ArkanoidState state = inputFrame.state.arkanoidController(1);
                writeHeader(AdapterSlotPayloadKind::Arkanoid);
                writer.writePod(state.position);
                writer.writePod(boolMask(state.button));
            } else if(port1Device == Settings::Device::SNES_MOUSE || port1Device == Settings::Device::SUBOR_MOUSE) {
                const bool subor = port1Device == Settings::Device::SUBOR_MOUSE;
                const InputState::RelativePointerState state = subor ? inputFrame.state.suborMouse(1) : inputFrame.state.snesMouse(1);
                writeHeader(AdapterSlotPayloadKind::Mouse);
                writer.writePod(static_cast<int16_t>(state.deltaX));
                writer.writePod(static_cast<int16_t>(state.deltaY));
                writer.writePod(boolMask(state.primary, state.secondary));
            } else if(port1Device == Settings::Device::POWER_PAD_SIDE_A || port1Device == Settings::Device::POWER_PAD_SIDE_B) {
                writeHeader(AdapterSlotPayloadKind::PowerPad);
                writer.writePod(powerPadMask(inputFrame.state.powerPadButtons(1)));
            }
            break;
        case kPort2PlayerSlot:
            if(port2Device == Settings::Device::ZAPPER) {
                const InputState::PointerState state = inputFrame.state.zapper(2);
                writeHeader(AdapterSlotPayloadKind::Zapper);
                writer.writePod(static_cast<int16_t>(state.x));
                writer.writePod(static_cast<int16_t>(state.y));
                writer.writePod(boolMask(state.trigger));
            } else if(port2Device == Settings::Device::ARKANOID_CONTROLLER) {
                const InputState::ArkanoidState state = inputFrame.state.arkanoidController(2);
                writeHeader(AdapterSlotPayloadKind::Arkanoid);
                writer.writePod(state.position);
                writer.writePod(boolMask(state.button));
            } else if(port2Device == Settings::Device::SNES_MOUSE || port2Device == Settings::Device::SUBOR_MOUSE) {
                const bool subor = port2Device == Settings::Device::SUBOR_MOUSE;
                const InputState::RelativePointerState state = subor ? inputFrame.state.suborMouse(2) : inputFrame.state.snesMouse(2);
                writeHeader(AdapterSlotPayloadKind::Mouse);
                writer.writePod(static_cast<int16_t>(state.deltaX));
                writer.writePod(static_cast<int16_t>(state.deltaY));
                writer.writePod(boolMask(state.primary, state.secondary));
            } else if(port2Device == Settings::Device::POWER_PAD_SIDE_A || port2Device == Settings::Device::POWER_PAD_SIDE_B) {
                writeHeader(AdapterSlotPayloadKind::PowerPad);
                writer.writePod(powerPadMask(inputFrame.state.powerPadButtons(2)));
            }
            break;
        case kExpansionPlayerSlot:
            if(expansionDevice == Settings::ExpansionDevice::BANDAI_HYPERSHOT) {
                const InputState::PointerState state = inputFrame.state.bandaiPointer();
                writeHeader(AdapterSlotPayloadKind::BandaiPointer);
                writer.writePod(static_cast<int16_t>(state.x));
                writer.writePod(static_cast<int16_t>(state.y));
                writer.writePod(boolMask(state.trigger));
            } else if(expansionDevice == Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
                const InputState::ArkanoidState state = inputFrame.state.arkanoidExpansion();
                writeHeader(AdapterSlotPayloadKind::Arkanoid);
                writer.writePod(state.position);
                writer.writePod(boolMask(state.button));
            } else if(expansionDevice == Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
                const InputState::KonamiHyperShotState state = inputFrame.state.konamiHyperShot();
                writeHeader(AdapterSlotPayloadKind::KonamiHyperShot);
                writer.writePod(boolMask(state.p1Run, state.p1Jump, state.p2Run, state.p2Jump));
            } else if(expansionDevice == Settings::ExpansionDevice::SUBOR_KEYBOARD) {
                writeHeader(AdapterSlotPayloadKind::Keyboard);
                writer.writePod(static_cast<uint8_t>(0));
                writeKeyboardKeys(writer, inputFrame.state.suborKeyboardKeys());
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD) {
                writeHeader(AdapterSlotPayloadKind::Keyboard);
                writer.writePod(static_cast<uint8_t>(1));
                writeKeyboardKeys(writer, inputFrame.state.familyBasicKeyboardKeys());
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
                      expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B) {
                writeHeader(AdapterSlotPayloadKind::FamilyTrainer);
                writer.writePod(powerPadMask(inputFrame.state.powerPadButtons(1)));
                writer.writePod(powerPadMask(inputFrame.state.powerPadButtons(2)));
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
    const InputTopology& topology = inputFrame.state.topology;
    const Settings::Device port1Device = topology.port1Device.value_or(Settings::Device::NONE);
    const Settings::Device port2Device = topology.port2Device.value_or(Settings::Device::NONE);
    const Settings::ExpansionDevice expansionDevice = topology.expansionDevice;
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
                frame.state.setZapper(1, {x, y, (flags & 1u) != 0});
            } else if(slot == kPort2PlayerSlot) {
                frame.state.setZapper(2, {x, y, (flags & 1u) != 0});
            }
            break;
        case AdapterSlotPayloadKind::Arkanoid:
            if(!reader.readPod(position) || !reader.readPod(flags)) return false;
            if(slot == kPort1PlayerSlot) {
                frame.state.setArkanoidController(1, {position, (flags & 1u) != 0});
            } else if(slot == kPort2PlayerSlot) {
                frame.state.setArkanoidController(2, {position, (flags & 1u) != 0});
            } else if(slot == kExpansionPlayerSlot) {
                frame.state.setArkanoidExpansion({position, (flags & 1u) != 0});
            }
            break;
        case AdapterSlotPayloadKind::Mouse:
            if(!reader.readPod(x) || !reader.readPod(y) || !reader.readPod(flags)) return false;
            if(slot == kPort1PlayerSlot &&
               adapterPayload.inputTopology.port1Device == std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE)) {
                frame.state.setSuborMouse(1, {x, y, (flags & 1u) != 0, (flags & 2u) != 0});
            } else if(slot == kPort1PlayerSlot) {
                frame.state.setSnesMouse(1, {x, y, (flags & 1u) != 0, (flags & 2u) != 0});
            } else if(slot == kPort2PlayerSlot &&
                      adapterPayload.inputTopology.port2Device == std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE)) {
                frame.state.setSuborMouse(2, {x, y, (flags & 1u) != 0, (flags & 2u) != 0});
            } else if(slot == kPort2PlayerSlot) {
                frame.state.setSnesMouse(2, {x, y, (flags & 1u) != 0, (flags & 2u) != 0});
            }
            break;
        case AdapterSlotPayloadKind::PowerPad:
            if(!reader.readPod(mask)) return false;
            if(slot == kPort1PlayerSlot) {
                auto buttons = frame.state.powerPadButtons(1);
                applyPowerPadMask(buttons, mask);
                frame.state.setPowerPadButtons(1, buttons);
            }
            if(slot == kPort2PlayerSlot) {
                auto buttons = frame.state.powerPadButtons(2);
                applyPowerPadMask(buttons, mask);
                frame.state.setPowerPadButtons(2, buttons);
            }
            break;
        case AdapterSlotPayloadKind::BandaiPointer:
            if(!reader.readPod(x) || !reader.readPod(y) || !reader.readPod(flags)) return false;
            frame.state.setBandaiPointer({x, y, (flags & 1u) != 0});
            break;
        case AdapterSlotPayloadKind::KonamiHyperShot:
            if(!reader.readPod(flags)) return false;
            frame.state.setKonamiHyperShot({
                (flags & 1u) != 0,
                (flags & 2u) != 0,
                (flags & 4u) != 0,
                (flags & 8u) != 0
            });
            break;
        case AdapterSlotPayloadKind::Keyboard:
            if(!reader.readPod(flags)) return false;
            if(flags == 0) {
                auto keys = frame.state.suborKeyboardKeys();
                if(!readKeyboardKeys(reader, keys)) return false;
                frame.state.setSuborKeyboardKeys(keys);
            } else if(flags == 1) {
                auto keys = frame.state.familyBasicKeyboardKeys();
                if(!readKeyboardKeys(reader, keys)) return false;
                frame.state.setFamilyBasicKeyboardKeys(keys);
            } else {
                return false;
            }
            break;
        case AdapterSlotPayloadKind::FamilyTrainer:
            if(!reader.readPod(mask) || !reader.readPod(mask2)) return false;
            {
                auto buttons1 = frame.state.powerPadButtons(1);
                auto buttons2 = frame.state.powerPadButtons(2);
                applyPowerPadMask(buttons1, mask);
                applyPowerPadMask(buttons2, mask2);
                frame.state.setPowerPadButtons(1, buttons1);
                frame.state.setPowerPadButtons(2, buttons2);
            }
            break;
        default:
            return false;
    }
    if(reader.remaining() != 0) {
        return false;
    }
    return true;
}

} // namespace

NetplayInputFrame toNetplayInputFrame(const InputFrame& inputFrame)
{
    const InputState::PadButtons p1 = inputFrame.state.portButtons(1);
    const InputState::PadButtons p2 = inputFrame.state.portButtons(2);
    const InputState::PadButtons p3 = inputFrame.state.portButtons(3);
    const InputState::PadButtons p4 = inputFrame.state.portButtons(4);
    const InputState::PadButtons bandai = inputFrame.state.bandaiButtons();
    NetplayInputFrame frame;
    frame.frame = inputFrame.frame;
    frame.framePayload = makeAdapterFramePayload(inputFrame);
    frame.buttonMaskLo[kPort1PlayerSlot] =
        buildMask(p1.a, p1.b, p1.select, p1.start,
                  p1.up, p1.down, p1.left, p1.right,
                  p1.x, p1.y, p1.l, p1.r,
                  p1.up2, p1.down2, p1.left2, p1.right2);
    frame.buttonMaskLo[kPort2PlayerSlot] =
        buildMask(p2.a, p2.b, p2.select, p2.start,
                  p2.up, p2.down, p2.left, p2.right,
                  p2.x, p2.y, p2.l, p2.r,
                  p2.up2, p2.down2, p2.left2, p2.right2);
    frame.buttonMaskLo[kExpansionPlayerSlot] =
        buildMask(p3.a || bandai.a,
                  p3.b || bandai.b,
                  p3.select || bandai.select,
                  p3.start || bandai.start,
                  p3.up || bandai.up,
                  p3.down || bandai.down,
                  p3.left || bandai.left,
                  p3.right || bandai.right);
    frame.buttonMaskLo[kMultitapP1PlayerSlot] =
        buildMask(p1.a, p1.b, p1.select, p1.start,
                  p1.up, p1.down, p1.left, p1.right);
    frame.buttonMaskLo[kMultitapP2PlayerSlot] =
        buildMask(p2.a, p2.b, p2.select, p2.start,
                  p2.up, p2.down, p2.left, p2.right);
    frame.buttonMaskLo[kMultitapP3PlayerSlot] =
        buildMask(p3.a, p3.b, p3.select, p3.start,
                  p3.up, p3.down, p3.left, p3.right);
    frame.buttonMaskLo[kMultitapP4PlayerSlot] =
        buildMask(p4.a, p4.b, p4.select, p4.start,
                  p4.up, p4.down, p4.left, p4.right);
    attachAdapterPayloads(frame, inputFrame);
    return frame;
}

InputFrame toGeraNESInputFrame(const NetplayInputFrame& inputFrame)
{
    InputFrame frame;
    frame.frame = inputFrame.frame;
    AdapterFramePayload adapterPayload;
    if(readAdapterFramePayload(inputFrame, adapterPayload)) {
        applyInputTopology(frame, adapterPayload.inputTopology);
    }
    const bool multitapActive =
        adapterPayload.inputTopology.nesMultitapDevice != Settings::NesMultitapDevice::NONE ||
        adapterPayload.inputTopology.famicomMultitapDevice != Settings::FamicomMultitapDevice::NONE;
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
    if(adapterPayload.inputTopology.port2Device == std::optional<Settings::Device>(Settings::Device::BANDAI_HYPERSHOT)) {
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
    applyInputTopology(inputFrame, roomInputTopology(room));
    return inputFrame;
}

namespace {

InputFrame makeContributionBase(const InputFrame& baseFrame)
{
    InputFrame contribution{};
    contribution.frame = baseFrame.frame;
    contribution.state.topology = baseFrame.state.topology;
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
            const Settings::Device device = baseFrame.state.topology.port1Device.value_or(Settings::Device::NONE);
            if(device == Settings::Device::CONTROLLER ||
               device == Settings::Device::FAMICOM_CONTROLLER ||
               device == Settings::Device::SNES_CONTROLLER ||
               device == Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                contribution.state.setPortButtons(1, {
                    state.p1A, state.p1B, state.p1Select, state.p1Start,
                    state.p1Up, state.p1Down, state.p1Left, state.p1Right,
                    state.p1X, state.p1Y, state.p1L, state.p1R,
                    state.p1Up2, state.p1Down2, state.p1Left2, state.p1Right2
                });
            } else if(device == Settings::Device::ZAPPER) {
                contribution.state.setZapper(1, {state.zapperX, state.zapperY, state.zapperP1Trigger});
            } else if(device == Settings::Device::ARKANOID_CONTROLLER) {
                contribution.state.setArkanoidController(1, {state.arkanoidNesPosition, state.mousePrimaryButton});
            } else if(device == Settings::Device::SNES_MOUSE) {
                contribution.state.setSnesMouse(1, {state.mouseDeltaX, state.mouseDeltaY, state.mousePrimaryButton, state.mouseSecondaryButton});
            } else if(device == Settings::Device::SUBOR_MOUSE) {
                contribution.state.setSuborMouse(1, {state.mouseDeltaX, state.mouseDeltaY, state.mousePrimaryButton, state.mouseSecondaryButton});
            } else if(device == Settings::Device::POWER_PAD_SIDE_A ||
                      device == Settings::Device::POWER_PAD_SIDE_B) {
                contribution.state.setPowerPadButtons(1, state.p1PowerPadButtons);
            }
            break;
        }
        case kPort2PlayerSlot: {
            const Settings::Device device = baseFrame.state.topology.port2Device.value_or(Settings::Device::NONE);
            if(device == Settings::Device::CONTROLLER ||
               device == Settings::Device::FAMICOM_CONTROLLER ||
               device == Settings::Device::SNES_CONTROLLER ||
               device == Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                contribution.state.setPortButtons(2, {
                    state.p2A, state.p2B, state.p2Select, state.p2Start,
                    state.p2Up, state.p2Down, state.p2Left, state.p2Right,
                    state.p2X, state.p2Y, state.p2L, state.p2R,
                    state.p2Up2, state.p2Down2, state.p2Left2, state.p2Right2
                });
            } else if(device == Settings::Device::ZAPPER) {
                contribution.state.setZapper(2, {state.zapperX, state.zapperY, state.zapperP2Trigger});
            } else if(device == Settings::Device::ARKANOID_CONTROLLER) {
                contribution.state.setArkanoidController(2, {state.arkanoidNesPosition, state.mousePrimaryButton});
            } else if(device == Settings::Device::SNES_MOUSE) {
                contribution.state.setSnesMouse(2, {state.mouseDeltaX, state.mouseDeltaY, state.mousePrimaryButton, state.mouseSecondaryButton});
            } else if(device == Settings::Device::SUBOR_MOUSE) {
                contribution.state.setSuborMouse(2, {state.mouseDeltaX, state.mouseDeltaY, state.mousePrimaryButton, state.mouseSecondaryButton});
            } else if(device == Settings::Device::POWER_PAD_SIDE_A ||
                      device == Settings::Device::POWER_PAD_SIDE_B) {
                contribution.state.setPowerPadButtons(2, state.p2PowerPadButtons);
            } else if(device == Settings::Device::BANDAI_HYPERSHOT) {
                contribution.state.setBandaiButtons({
                    state.p2A, state.p2B, state.p2Select, state.p2Start,
                    state.p2Up, state.p2Down, state.p2Left, state.p2Right
                });
            }
            break;
        }
        case kExpansionPlayerSlot: {
            const Settings::ExpansionDevice expansionDevice = baseFrame.state.topology.expansionDevice;
            if(expansionDevice == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM) {
                contribution.state.setPortButtons(3, {
                    state.p3A, state.p3B, state.p3Select, state.p3Start,
                    state.p3Up, state.p3Down, state.p3Left, state.p3Right
                });
            } else if(expansionDevice == Settings::ExpansionDevice::BANDAI_HYPERSHOT) {
                contribution.state.setBandaiPointer({state.zapperX, state.zapperY, state.bandaiTrigger});
            } else if(expansionDevice == Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
                contribution.state.setArkanoidExpansion({state.arkanoidFamicomPosition, state.mousePrimaryButton});
            } else if(expansionDevice == Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
                contribution.state.setKonamiHyperShot({state.konamiP1Run, state.konamiP1Jump, state.konamiP2Run, state.konamiP2Jump});
            } else if(expansionDevice == Settings::ExpansionDevice::SUBOR_KEYBOARD) {
                contribution.state.setSuborKeyboardKeys(state.suborKeyboardKeys);
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD) {
                contribution.state.setFamilyBasicKeyboardKeys(state.familyBasicKeyboardKeys);
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
                      expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B) {
                contribution.state.setPowerPadButtons(1, state.p1PowerPadButtons);
                contribution.state.setPowerPadButtons(2, state.p2PowerPadButtons);
            }
            break;
        }
        case kMultitapP1PlayerSlot:
            contribution.state.setPortButtons(1, {state.p1A, state.p1B, state.p1Select, state.p1Start, state.p1Up, state.p1Down, state.p1Left, state.p1Right});
            break;
        case kMultitapP2PlayerSlot:
            contribution.state.setPortButtons(2, {state.p2A, state.p2B, state.p2Select, state.p2Start, state.p2Up, state.p2Down, state.p2Left, state.p2Right});
            break;
        case kMultitapP3PlayerSlot:
            contribution.state.setPortButtons(3, {state.p3A, state.p3B, state.p3Select, state.p3Start, state.p3Up, state.p3Down, state.p3Left, state.p3Right});
            break;
        case kMultitapP4PlayerSlot:
            contribution.state.setPortButtons(4, {state.p4A, state.p4B, state.p4Select, state.p4Start, state.p4Up, state.p4Down, state.p4Left, state.p4Right});
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
            const Settings::Device device = target.state.topology.port1Device.value_or(Settings::Device::NONE);
            if(device == Settings::Device::CONTROLLER ||
               device == Settings::Device::FAMICOM_CONTROLLER ||
               device == Settings::Device::SNES_CONTROLLER ||
               device == Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                target.state.setPortButtons(1, contribution.state.portButtons(1));
            } else if(device == Settings::Device::ZAPPER) {
                target.state.setZapper(1, contribution.state.zapper(1));
            } else if(device == Settings::Device::ARKANOID_CONTROLLER) {
                target.state.setArkanoidController(1, contribution.state.arkanoidController(1));
            } else if(device == Settings::Device::SNES_MOUSE) {
                target.state.setSnesMouse(1, contribution.state.snesMouse(1));
            } else if(device == Settings::Device::SUBOR_MOUSE) {
                target.state.setSuborMouse(1, contribution.state.suborMouse(1));
            } else if(device == Settings::Device::POWER_PAD_SIDE_A ||
                      device == Settings::Device::POWER_PAD_SIDE_B) {
                target.state.setPowerPadButtons(1, contribution.state.powerPadButtons(1));
            }
            break;
        }
        case kPort2PlayerSlot: {
            const Settings::Device device = target.state.topology.port2Device.value_or(Settings::Device::NONE);
            if(device == Settings::Device::CONTROLLER ||
               device == Settings::Device::FAMICOM_CONTROLLER ||
               device == Settings::Device::SNES_CONTROLLER ||
               device == Settings::Device::VIRTUAL_BOY_CONTROLLER) {
                target.state.setPortButtons(2, contribution.state.portButtons(2));
            } else if(device == Settings::Device::ZAPPER) {
                target.state.setZapper(2, contribution.state.zapper(2));
            } else if(device == Settings::Device::ARKANOID_CONTROLLER) {
                target.state.setArkanoidController(2, contribution.state.arkanoidController(2));
            } else if(device == Settings::Device::SNES_MOUSE) {
                target.state.setSnesMouse(2, contribution.state.snesMouse(2));
            } else if(device == Settings::Device::SUBOR_MOUSE) {
                target.state.setSuborMouse(2, contribution.state.suborMouse(2));
            } else if(device == Settings::Device::POWER_PAD_SIDE_A ||
                      device == Settings::Device::POWER_PAD_SIDE_B) {
                target.state.setPowerPadButtons(2, contribution.state.powerPadButtons(2));
            } else if(device == Settings::Device::BANDAI_HYPERSHOT) {
                target.state.setBandaiButtons(contribution.state.bandaiButtons());
            }
            break;
        }
        case kExpansionPlayerSlot: {
            const Settings::ExpansionDevice expansionDevice = target.state.topology.expansionDevice;
            if(expansionDevice == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM) {
                target.state.setPortButtons(3, contribution.state.portButtons(3));
            } else if(expansionDevice == Settings::ExpansionDevice::BANDAI_HYPERSHOT) {
                target.state.setBandaiPointer(contribution.state.bandaiPointer());
            } else if(expansionDevice == Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
                target.state.setArkanoidExpansion(contribution.state.arkanoidExpansion());
            } else if(expansionDevice == Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
                target.state.setKonamiHyperShot(contribution.state.konamiHyperShot());
            } else if(expansionDevice == Settings::ExpansionDevice::SUBOR_KEYBOARD) {
                target.state.setSuborKeyboardKeys(contribution.state.suborKeyboardKeys());
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD) {
                target.state.setFamilyBasicKeyboardKeys(contribution.state.familyBasicKeyboardKeys());
            } else if(expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
                      expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B) {
                target.state.setPowerPadButtons(1, contribution.state.powerPadButtons(1));
                target.state.setPowerPadButtons(2, contribution.state.powerPadButtons(2));
            }
            break;
        }
        case kMultitapP1PlayerSlot:
            target.state.setPortButtons(1, contribution.state.portButtons(1));
            break;
        case kMultitapP2PlayerSlot:
            target.state.setPortButtons(2, contribution.state.portButtons(2));
            break;
        case kMultitapP3PlayerSlot:
            target.state.setPortButtons(3, contribution.state.portButtons(3));
            break;
        case kMultitapP4PlayerSlot:
            target.state.setPortButtons(4, contribution.state.portButtons(4));
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
