#include "GeraNESNetplay/GeraNESInputFrameAdapter.h"

#include "GeraNESNetplay/GeraNESNetplayAdapters.h"

namespace GeraNESNetplay
{

namespace {

struct PadButtons
{
    bool a = false;
    bool b = false;
    bool select = false;
    bool start = false;
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool x = false;
    bool y = false;
    bool l = false;
    bool r = false;
    bool up2 = false;
    bool down2 = false;
    bool left2 = false;
    bool right2 = false;
};

PadButtons decodePadMask(uint64_t mask)
{
    PadButtons buttons;
    buttons.a = (mask & (1ull << 0)) != 0;
    buttons.b = (mask & (1ull << 1)) != 0;
    buttons.select = (mask & (1ull << 2)) != 0;
    buttons.start = (mask & (1ull << 3)) != 0;
    buttons.up = (mask & (1ull << 4)) != 0;
    buttons.down = (mask & (1ull << 5)) != 0;
    buttons.left = (mask & (1ull << 6)) != 0;
    buttons.right = (mask & (1ull << 7)) != 0;
    buttons.x = (mask & (1ull << 8)) != 0;
    buttons.y = (mask & (1ull << 9)) != 0;
    buttons.l = (mask & (1ull << 10)) != 0;
    buttons.r = (mask & (1ull << 11)) != 0;
    buttons.up2 = (mask & (1ull << 12)) != 0;
    buttons.down2 = (mask & (1ull << 13)) != 0;
    buttons.left2 = (mask & (1ull << 14)) != 0;
    buttons.right2 = (mask & (1ull << 15)) != 0;
    return buttons;
}

GeraNESInputState toGeraNESInputState(const EmulationHost::InputState& state)
{
    const EmulationHost::InputState::PadButtons p1 = state.portButtons(1);
    const EmulationHost::InputState::PadButtons p2 = state.portButtons(2);
    const EmulationHost::InputState::PadButtons p3 = state.portButtons(3);
    const EmulationHost::InputState::PadButtons p4 = state.portButtons(4);
    const EmulationHost::InputState::PointerState zapper1 = state.zapper(1);
    const EmulationHost::InputState::PointerState zapper2 = state.zapper(2);
    const EmulationHost::InputState::PointerState bandai = state.bandaiPointer();
    const EmulationHost::InputState::RelativePointerState snes1 = state.snesMouse(1);
    const EmulationHost::InputState::RelativePointerState snes2 = state.snesMouse(2);
    const EmulationHost::InputState::RelativePointerState subor1 = state.suborMouse(1);
    const EmulationHost::InputState::RelativePointerState subor2 = state.suborMouse(2);
    const EmulationHost::InputState::KonamiHyperShotState konami = state.konamiHyperShot();
    const EmulationHost::InputState::ArkanoidState arkanoid1 = state.arkanoidController(1);
    const EmulationHost::InputState::ArkanoidState arkanoidExpansion = state.arkanoidExpansion();
    GeraNESInputState netplayState;
    netplayState.p1A = p1.a; netplayState.p1B = p1.b; netplayState.p1Select = p1.select; netplayState.p1Start = p1.start;
    netplayState.p1Up = p1.up; netplayState.p1Down = p1.down; netplayState.p1Left = p1.left; netplayState.p1Right = p1.right;
    netplayState.p1X = p1.x; netplayState.p1Y = p1.y; netplayState.p1L = p1.l; netplayState.p1R = p1.r;
    netplayState.p1Up2 = p1.up2; netplayState.p1Down2 = p1.down2; netplayState.p1Left2 = p1.left2; netplayState.p1Right2 = p1.right2;

    netplayState.p2A = p2.a; netplayState.p2B = p2.b; netplayState.p2Select = p2.select; netplayState.p2Start = p2.start;
    netplayState.p2Up = p2.up; netplayState.p2Down = p2.down; netplayState.p2Left = p2.left; netplayState.p2Right = p2.right;
    netplayState.p2X = p2.x; netplayState.p2Y = p2.y; netplayState.p2L = p2.l; netplayState.p2R = p2.r;
    netplayState.p2Up2 = p2.up2; netplayState.p2Down2 = p2.down2; netplayState.p2Left2 = p2.left2; netplayState.p2Right2 = p2.right2;

    netplayState.p3A = p3.a; netplayState.p3B = p3.b; netplayState.p3Select = p3.select; netplayState.p3Start = p3.start;
    netplayState.p3Up = p3.up; netplayState.p3Down = p3.down; netplayState.p3Left = p3.left; netplayState.p3Right = p3.right;
    netplayState.p4A = p4.a; netplayState.p4B = p4.b; netplayState.p4Select = p4.select; netplayState.p4Start = p4.start;
    netplayState.p4Up = p4.up; netplayState.p4Down = p4.down; netplayState.p4Left = p4.left; netplayState.p4Right = p4.right;

    netplayState.p1PowerPadButtons = state.powerPadButtons(1);
    netplayState.p2PowerPadButtons = state.powerPadButtons(2);
    netplayState.suborKeyboardKeys = state.suborKeyboardKeys();
    netplayState.familyBasicKeyboardKeys = state.familyBasicKeyboardKeys();
    netplayState.zapperX = zapper1.x != -1 || zapper1.y != -1 || zapper1.trigger ? zapper1.x : (zapper2.x != -1 || zapper2.y != -1 || zapper2.trigger ? zapper2.x : bandai.x);
    netplayState.zapperY = zapper1.x != -1 || zapper1.y != -1 || zapper1.trigger ? zapper1.y : (zapper2.x != -1 || zapper2.y != -1 || zapper2.trigger ? zapper2.y : bandai.y);
    netplayState.mouseDeltaX = snes1.deltaX != 0 || snes1.deltaY != 0 || snes1.primary || snes1.secondary ? snes1.deltaX :
                               (snes2.deltaX != 0 || snes2.deltaY != 0 || snes2.primary || snes2.secondary ? snes2.deltaX :
                               (subor1.deltaX != 0 || subor1.deltaY != 0 || subor1.primary || subor1.secondary ? subor1.deltaX : subor2.deltaX));
    netplayState.mouseDeltaY = snes1.deltaX != 0 || snes1.deltaY != 0 || snes1.primary || snes1.secondary ? snes1.deltaY :
                               (snes2.deltaX != 0 || snes2.deltaY != 0 || snes2.primary || snes2.secondary ? snes2.deltaY :
                               (subor1.deltaX != 0 || subor1.deltaY != 0 || subor1.primary || subor1.secondary ? subor1.deltaY : subor2.deltaY));
    netplayState.arkanoidNesPosition = arkanoid1.position;
    netplayState.arkanoidFamicomPosition = arkanoidExpansion.position;
    netplayState.zapperP1Trigger = zapper1.trigger;
    netplayState.zapperP2Trigger = zapper2.trigger;
    netplayState.bandaiTrigger = bandai.trigger;
    netplayState.konamiP1Run = konami.p1Run;
    netplayState.konamiP1Jump = konami.p1Jump;
    netplayState.konamiP2Run = konami.p2Run;
    netplayState.konamiP2Jump = konami.p2Jump;
    netplayState.mousePrimaryButton = snes1.primary || snes2.primary || subor1.primary || subor2.primary || arkanoid1.button || arkanoidExpansion.button;
    netplayState.mouseSecondaryButton = snes1.secondary || snes2.secondary || subor1.secondary || subor2.secondary;
    return netplayState;
}

} // namespace

void applyPadMaskToInputState(EmulationHost::InputState& state, PlayerSlot slot, uint64_t mask)
{
    const PadButtons buttons = decodePadMask(mask);
    switch(slot) {
        case kPort1PlayerSlot:
        case kMultitapP1PlayerSlot:
            state.setPortButtons(1, {buttons.a, buttons.b, buttons.select, buttons.start,
                                     buttons.up, buttons.down, buttons.left, buttons.right,
                                     buttons.x, buttons.y, buttons.l, buttons.r,
                                     buttons.up2, buttons.down2, buttons.left2, buttons.right2});
            break;
        case kPort2PlayerSlot:
        case kMultitapP2PlayerSlot:
            state.setPortButtons(2, {buttons.a, buttons.b, buttons.select, buttons.start,
                                     buttons.up, buttons.down, buttons.left, buttons.right,
                                     buttons.x, buttons.y, buttons.l, buttons.r,
                                     buttons.up2, buttons.down2, buttons.left2, buttons.right2});
            break;
        case kExpansionPlayerSlot:
        case kMultitapP3PlayerSlot:
            state.setPortButtons(3, {buttons.a, buttons.b, buttons.select, buttons.start,
                                     buttons.up, buttons.down, buttons.left, buttons.right});
            break;
        case kMultitapP4PlayerSlot:
            state.setPortButtons(4, {buttons.a, buttons.b, buttons.select, buttons.start,
                                     buttons.up, buttons.down, buttons.left, buttons.right});
            break;
        default:
            break;
    }
}

void applyInputFrameToInputState(EmulationHost::InputState& state, const InputFrame& inputFrame)
{
    state = inputFrame.state;
}

NetplayInputFrame buildGeraNESLocalInputContribution(PlayerSlot slot,
                                                     FrameNumber frame,
                                                     const EmulationHost::InputState& localInputState,
                                                     const RoomState& room)
{
    EmulationHost::InputState stampedState = localInputState;
    const InputFrame baseFrame = makeRoomTopologyBaseFrame(frame, room);
    stampedState.topology = baseFrame.state.topology;
    const GeraNESInputState inputState = toGeraNESInputState(stampedState);
    return toNetplayInputFrame(buildAssignedContribution(slot, inputState, makeRoomTopologyBaseFrame(frame, room)));
}

} // namespace GeraNESNetplay
