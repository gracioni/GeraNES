#include "GeraNESNetplay/GeraNESNetplayAdapters.h"

#include "GeraNESNetplay/NetplayCoordinator.h"
#include "GeraNESNetplay/NetSerialization.h"

namespace Netplay {

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
    PortDevice port1Device = PortDevice::NONE;
    PortDevice port2Device = PortDevice::NONE;
    ExpansionDevice expansionDevice = ExpansionDevice::NONE;
    NesMultitapDevice nesMultitapDevice = NesMultitapDevice::NONE;
    FamicomMultitapDevice famicomMultitapDevice = FamicomMultitapDevice::NONE;
};

std::vector<uint8_t> makeAdapterFramePayload(const InputFrame& inputFrame)
{
    PacketWriter writer;
    writer.writePod(kAdapterFramePayloadVersion);
    writer.writePod(static_cast<uint8_t>(toNetplayPortDevice(inputFrame.port1Device)));
    writer.writePod(static_cast<uint8_t>(toNetplayPortDevice(inputFrame.port2Device)));
    writer.writePod(static_cast<uint8_t>(toNetplayExpansionDevice(inputFrame.expansionDevice)));
    writer.writePod(static_cast<uint8_t>(toNetplayNesMultitapDevice(inputFrame.nesMultitapDevice)));
    writer.writePod(static_cast<uint8_t>(toNetplayFamicomMultitapDevice(inputFrame.famicomMultitapDevice)));
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
    payload.port1Device = static_cast<PortDevice>(port1Device);
    payload.port2Device = static_cast<PortDevice>(port2Device);
    payload.expansionDevice = static_cast<ExpansionDevice>(expansionDevice);
    payload.nesMultitapDevice = static_cast<NesMultitapDevice>(nesMultitapDevice);
    payload.famicomMultitapDevice = static_cast<FamicomMultitapDevice>(famicomMultitapDevice);
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

    const PortDevice port1Device = toNetplayPortDevice(inputFrame.port1Device);
    const PortDevice port2Device = toNetplayPortDevice(inputFrame.port2Device);
    const ExpansionDevice expansionDevice = toNetplayExpansionDevice(inputFrame.expansionDevice);
    switch(slot) {
        case kPort1PlayerSlot:
            if(port1Device == PortDevice::ZAPPER) {
                writeHeader(AdapterSlotPayloadKind::Zapper);
                writer.writePod(static_cast<int16_t>(inputFrame.zapperP1X));
                writer.writePod(static_cast<int16_t>(inputFrame.zapperP1Y));
                writer.writePod(boolMask(inputFrame.zapperP1Trigger));
            } else if(port1Device == PortDevice::ARKANOID_CONTROLLER) {
                writeHeader(AdapterSlotPayloadKind::Arkanoid);
                writer.writePod(inputFrame.arkanoidP1Position);
                writer.writePod(boolMask(inputFrame.arkanoidP1Button));
            } else if(port1Device == PortDevice::SNES_MOUSE || port1Device == PortDevice::SUBOR_MOUSE) {
                writeHeader(AdapterSlotPayloadKind::Mouse);
                const bool subor = port1Device == PortDevice::SUBOR_MOUSE;
                writer.writePod(static_cast<int16_t>(subor ? inputFrame.suborMouseP1DeltaX : inputFrame.snesMouseP1DeltaX));
                writer.writePod(static_cast<int16_t>(subor ? inputFrame.suborMouseP1DeltaY : inputFrame.snesMouseP1DeltaY));
                writer.writePod(boolMask(subor ? inputFrame.suborMouseP1Left : inputFrame.snesMouseP1Left,
                                         subor ? inputFrame.suborMouseP1Right : inputFrame.snesMouseP1Right));
            } else if(port1Device == PortDevice::POWER_PAD_SIDE_A || port1Device == PortDevice::POWER_PAD_SIDE_B) {
                writeHeader(AdapterSlotPayloadKind::PowerPad);
                writer.writePod(powerPadMask(inputFrame.powerPadP1Buttons));
            }
            break;
        case kPort2PlayerSlot:
            if(port2Device == PortDevice::ZAPPER) {
                writeHeader(AdapterSlotPayloadKind::Zapper);
                writer.writePod(static_cast<int16_t>(inputFrame.zapperP2X));
                writer.writePod(static_cast<int16_t>(inputFrame.zapperP2Y));
                writer.writePod(boolMask(inputFrame.zapperP2Trigger));
            } else if(port2Device == PortDevice::ARKANOID_CONTROLLER) {
                writeHeader(AdapterSlotPayloadKind::Arkanoid);
                writer.writePod(inputFrame.arkanoidP2Position);
                writer.writePod(boolMask(inputFrame.arkanoidP2Button));
            } else if(port2Device == PortDevice::SNES_MOUSE || port2Device == PortDevice::SUBOR_MOUSE) {
                writeHeader(AdapterSlotPayloadKind::Mouse);
                const bool subor = port2Device == PortDevice::SUBOR_MOUSE;
                writer.writePod(static_cast<int16_t>(subor ? inputFrame.suborMouseP2DeltaX : inputFrame.snesMouseP2DeltaX));
                writer.writePod(static_cast<int16_t>(subor ? inputFrame.suborMouseP2DeltaY : inputFrame.snesMouseP2DeltaY));
                writer.writePod(boolMask(subor ? inputFrame.suborMouseP2Left : inputFrame.snesMouseP2Left,
                                         subor ? inputFrame.suborMouseP2Right : inputFrame.snesMouseP2Right));
            } else if(port2Device == PortDevice::POWER_PAD_SIDE_A || port2Device == PortDevice::POWER_PAD_SIDE_B) {
                writeHeader(AdapterSlotPayloadKind::PowerPad);
                writer.writePod(powerPadMask(inputFrame.powerPadP2Buttons));
            }
            break;
        case kExpansionPlayerSlot:
            if(expansionDevice == ExpansionDevice::BANDAI_HYPERSHOT) {
                writeHeader(AdapterSlotPayloadKind::BandaiPointer);
                writer.writePod(static_cast<int16_t>(inputFrame.bandaiX));
                writer.writePod(static_cast<int16_t>(inputFrame.bandaiY));
                writer.writePod(boolMask(inputFrame.bandaiTrigger));
            } else if(expansionDevice == ExpansionDevice::ARKANOID_CONTROLLER) {
                writeHeader(AdapterSlotPayloadKind::Arkanoid);
                writer.writePod(inputFrame.arkanoidFamicomPosition);
                writer.writePod(boolMask(inputFrame.arkanoidFamicomButton));
            } else if(expansionDevice == ExpansionDevice::KONAMI_HYPERSHOT) {
                writeHeader(AdapterSlotPayloadKind::KonamiHyperShot);
                writer.writePod(boolMask(inputFrame.konamiP1Run, inputFrame.konamiP1Jump,
                                         inputFrame.konamiP2Run, inputFrame.konamiP2Jump));
            } else if(expansionDevice == ExpansionDevice::SUBOR_KEYBOARD) {
                writeHeader(AdapterSlotPayloadKind::Keyboard);
                writer.writePod(static_cast<uint8_t>(0));
                writeKeyboardKeys(writer, inputFrame.suborKeyboardKeys);
            } else if(expansionDevice == ExpansionDevice::FAMILY_BASIC_KEYBOARD) {
                writeHeader(AdapterSlotPayloadKind::Keyboard);
                writer.writePod(static_cast<uint8_t>(1));
                writeKeyboardKeys(writer, inputFrame.familyBasicKeyboardKeys);
            } else if(expansionDevice == ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
                      expansionDevice == ExpansionDevice::FAMILY_TRAINER_SIDE_B) {
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

bool isControllerLike(PortDevice device)
{
    return device == PortDevice::CONTROLLER ||
           device == PortDevice::FAMICOM_CONTROLLER ||
           device == PortDevice::SNES_CONTROLLER ||
           device == PortDevice::VIRTUAL_BOY_CONTROLLER;
}

bool slotNeedsAdapterPayload(const InputFrame& inputFrame, PlayerSlot slot)
{
    const PortDevice port1Device = toNetplayPortDevice(inputFrame.port1Device);
    const PortDevice port2Device = toNetplayPortDevice(inputFrame.port2Device);
    const ExpansionDevice expansionDevice = toNetplayExpansionDevice(inputFrame.expansionDevice);
    switch(slot) {
        case kPort1PlayerSlot:
            return port1Device != PortDevice::NONE && !isControllerLike(port1Device);
        case kPort2PlayerSlot:
            return port2Device != PortDevice::NONE &&
                   !isControllerLike(port2Device) &&
                   port2Device != PortDevice::BANDAI_HYPERSHOT;
        case kExpansionPlayerSlot:
            return expansionDevice != ExpansionDevice::NONE &&
                   expansionDevice != ExpansionDevice::STANDARD_CONTROLLER_FAMICOM;
        default:
            return false;
    }
}

void attachAdapterPayloads(NetplayInputFrame& frame, const InputFrame& inputFrame)
{
    for(PlayerSlot slot = 0; slot <= kMaxAssignedPlayerSlot; ++slot) {
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
    if(slot > kMaxAssignedPlayerSlot || inputFrame.slotPayloads[slot].empty()) return false;
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
            if(slot == kPort1PlayerSlot && adapterPayload.port1Device == PortDevice::SUBOR_MOUSE) {
                frame.suborMouseP1DeltaX = x; frame.suborMouseP1DeltaY = y;
                frame.suborMouseP1Left = (flags & 1u) != 0; frame.suborMouseP1Right = (flags & 2u) != 0;
            } else if(slot == kPort1PlayerSlot) {
                frame.snesMouseP1DeltaX = x; frame.snesMouseP1DeltaY = y;
                frame.snesMouseP1Left = (flags & 1u) != 0; frame.snesMouseP1Right = (flags & 2u) != 0;
            } else if(slot == kPort2PlayerSlot && adapterPayload.port2Device == PortDevice::SUBOR_MOUSE) {
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
    InputFrame frame;
    frame.frame = inputFrame.frame;
    frame.timelineEpoch = inputFrame.timelineEpoch;
    frame.speculative = inputFrame.speculative;
    AdapterFramePayload adapterPayload;
    if(readAdapterFramePayload(inputFrame, adapterPayload)) {
        frame.port1Device = toSettingsDevice(adapterPayload.port1Device);
        frame.port2Device = toSettingsDevice(adapterPayload.port2Device);
        frame.expansionDevice = toSettingsExpansionDevice(adapterPayload.expansionDevice);
        frame.nesMultitapDevice = toSettingsNesMultitapDevice(adapterPayload.nesMultitapDevice);
        frame.famicomMultitapDevice = toSettingsFamicomMultitapDevice(adapterPayload.famicomMultitapDevice);
    }
    const bool multitapActive =
        adapterPayload.nesMultitapDevice != NesMultitapDevice::NONE ||
        adapterPayload.famicomMultitapDevice != FamicomMultitapDevice::NONE;
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
    if(adapterPayload.port2Device == PortDevice::BANDAI_HYPERSHOT) {
        applyBandaiPadMask(frame, inputFrame.buttonMaskLo[kPort2PlayerSlot]);
    }
    for(PlayerSlot slot = 0; slot <= kMaxAssignedPlayerSlot; ++slot) {
        (void)applySlotPayload(frame, inputFrame, adapterPayload, slot);
    }
    return frame;
}

bool injectInputFrameForTests(NetplayCoordinator& coordinator,
                              const InputFrameData& input,
                              const InputFrame& contribution)
{
    return coordinator.injectInputFrameForTests(input, toNetplayInputFrame(contribution));
}

void recordLocalInputFrame(NetplayCoordinator& coordinator,
                           FrameNumber frame,
                           PlayerSlot slot,
                           const InputFrame& contribution)
{
    coordinator.recordLocalInputFrame(frame, slot, toNetplayInputFrame(contribution));
}

} // namespace Netplay
