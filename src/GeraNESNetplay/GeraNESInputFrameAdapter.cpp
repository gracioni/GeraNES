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
    GeraNESInputState netplayState;
    netplayState.p1A = state.p1A; netplayState.p1B = state.p1B; netplayState.p1Select = state.p1Select; netplayState.p1Start = state.p1Start;
    netplayState.p1Up = state.p1Up; netplayState.p1Down = state.p1Down; netplayState.p1Left = state.p1Left; netplayState.p1Right = state.p1Right;
    netplayState.p1X = state.p1X; netplayState.p1Y = state.p1Y; netplayState.p1L = state.p1L; netplayState.p1R = state.p1R;
    netplayState.p1Up2 = state.p1Up2; netplayState.p1Down2 = state.p1Down2; netplayState.p1Left2 = state.p1Left2; netplayState.p1Right2 = state.p1Right2;

    netplayState.p2A = state.p2A; netplayState.p2B = state.p2B; netplayState.p2Select = state.p2Select; netplayState.p2Start = state.p2Start;
    netplayState.p2Up = state.p2Up; netplayState.p2Down = state.p2Down; netplayState.p2Left = state.p2Left; netplayState.p2Right = state.p2Right;
    netplayState.p2X = state.p2X; netplayState.p2Y = state.p2Y; netplayState.p2L = state.p2L; netplayState.p2R = state.p2R;
    netplayState.p2Up2 = state.p2Up2; netplayState.p2Down2 = state.p2Down2; netplayState.p2Left2 = state.p2Left2; netplayState.p2Right2 = state.p2Right2;

    netplayState.p3A = state.p3A; netplayState.p3B = state.p3B; netplayState.p3Select = state.p3Select; netplayState.p3Start = state.p3Start;
    netplayState.p3Up = state.p3Up; netplayState.p3Down = state.p3Down; netplayState.p3Left = state.p3Left; netplayState.p3Right = state.p3Right;
    netplayState.p4A = state.p4A; netplayState.p4B = state.p4B; netplayState.p4Select = state.p4Select; netplayState.p4Start = state.p4Start;
    netplayState.p4Up = state.p4Up; netplayState.p4Down = state.p4Down; netplayState.p4Left = state.p4Left; netplayState.p4Right = state.p4Right;

    netplayState.p1PowerPadButtons = state.p1PowerPadButtons;
    netplayState.p2PowerPadButtons = state.p2PowerPadButtons;
    netplayState.suborKeyboardKeys = state.suborKeyboardKeys;
    netplayState.familyBasicKeyboardKeys = state.familyBasicKeyboardKeys;
    netplayState.zapperX = state.zapperX;
    netplayState.zapperY = state.zapperY;
    netplayState.mouseDeltaX = state.mouseDeltaX;
    netplayState.mouseDeltaY = state.mouseDeltaY;
    netplayState.arkanoidNesPosition = state.arkanoidNesPosition;
    netplayState.arkanoidFamicomPosition = state.arkanoidFamicomPosition;
    netplayState.zapperP1Trigger = state.zapperP1Trigger;
    netplayState.zapperP2Trigger = state.zapperP2Trigger;
    netplayState.bandaiTrigger = state.bandaiTrigger;
    netplayState.konamiP1Run = state.konamiP1Run;
    netplayState.konamiP1Jump = state.konamiP1Jump;
    netplayState.konamiP2Run = state.konamiP2Run;
    netplayState.konamiP2Jump = state.konamiP2Jump;
    netplayState.mousePrimaryButton = state.mousePrimaryButton;
    netplayState.mouseSecondaryButton = state.mouseSecondaryButton;
    return netplayState;
}

} // namespace

void applyPadMaskToInputState(EmulationHost::InputState& state, PlayerSlot slot, uint64_t mask)
{
    const PadButtons buttons = decodePadMask(mask);
    switch(slot) {
        case kPort1PlayerSlot:
        case kMultitapP1PlayerSlot:
            state.p1A = buttons.a; state.p1B = buttons.b; state.p1Select = buttons.select; state.p1Start = buttons.start;
            state.p1Up = buttons.up; state.p1Down = buttons.down; state.p1Left = buttons.left; state.p1Right = buttons.right;
            state.p1X = buttons.x; state.p1Y = buttons.y; state.p1L = buttons.l; state.p1R = buttons.r;
            state.p1Up2 = buttons.up2; state.p1Down2 = buttons.down2; state.p1Left2 = buttons.left2; state.p1Right2 = buttons.right2;
            break;
        case kPort2PlayerSlot:
        case kMultitapP2PlayerSlot:
            state.p2A = buttons.a; state.p2B = buttons.b; state.p2Select = buttons.select; state.p2Start = buttons.start;
            state.p2Up = buttons.up; state.p2Down = buttons.down; state.p2Left = buttons.left; state.p2Right = buttons.right;
            state.p2X = buttons.x; state.p2Y = buttons.y; state.p2L = buttons.l; state.p2R = buttons.r;
            state.p2Up2 = buttons.up2; state.p2Down2 = buttons.down2; state.p2Left2 = buttons.left2; state.p2Right2 = buttons.right2;
            break;
        case kExpansionPlayerSlot:
        case kMultitapP3PlayerSlot:
            state.p3A = buttons.a; state.p3B = buttons.b; state.p3Select = buttons.select; state.p3Start = buttons.start;
            state.p3Up = buttons.up; state.p3Down = buttons.down; state.p3Left = buttons.left; state.p3Right = buttons.right;
            break;
        case kMultitapP4PlayerSlot:
            state.p4A = buttons.a; state.p4B = buttons.b; state.p4Select = buttons.select; state.p4Start = buttons.start;
            state.p4Up = buttons.up; state.p4Down = buttons.down; state.p4Left = buttons.left; state.p4Right = buttons.right;
            break;
        default:
            break;
    }
}

void applyInputFrameToInputState(EmulationHost::InputState& state, const InputFrame& inputFrame)
{
    const InputFrame::PadButtons p1 = inputFrame.portButtons(1);
    const InputFrame::PadButtons p2 = inputFrame.portButtons(2);
    const InputFrame::PadButtons p3 = inputFrame.portButtons(3);
    const InputFrame::PadButtons p4 = inputFrame.portButtons(4);
    const InputFrame::PointerState zapper1 = inputFrame.zapper(1);
    const InputFrame::PointerState zapper2 = inputFrame.zapper(2);
    const InputFrame::PointerState bandai = inputFrame.bandaiPointer();
    const InputFrame::RelativePointerState snes1 = inputFrame.snesMouse(1);
    const InputFrame::RelativePointerState snes2 = inputFrame.snesMouse(2);
    const InputFrame::RelativePointerState subor1 = inputFrame.suborMouse(1);
    const InputFrame::RelativePointerState subor2 = inputFrame.suborMouse(2);
    const InputFrame::KonamiHyperShotState konami = inputFrame.konamiHyperShot();
    const InputFrame::ArkanoidState arkanoid1 = inputFrame.arkanoidController(1);
    const InputFrame::ArkanoidState arkanoid2 = inputFrame.arkanoidController(2);
    const InputFrame::ArkanoidState arkanoidExpansion = inputFrame.arkanoidExpansion();

    state.p1A = p1.a; state.p1B = p1.b; state.p1Select = p1.select; state.p1Start = p1.start;
    state.p1Up = p1.up; state.p1Down = p1.down; state.p1Left = p1.left; state.p1Right = p1.right;
    state.p1X = p1.x; state.p1Y = p1.y; state.p1L = p1.l; state.p1R = p1.r;
    state.p1Up2 = p1.up2; state.p1Down2 = p1.down2; state.p1Left2 = p1.left2; state.p1Right2 = p1.right2;

    state.p2A = p2.a; state.p2B = p2.b; state.p2Select = p2.select; state.p2Start = p2.start;
    state.p2Up = p2.up; state.p2Down = p2.down; state.p2Left = p2.left; state.p2Right = p2.right;
    state.p2X = p2.x; state.p2Y = p2.y; state.p2L = p2.l; state.p2R = p2.r;
    state.p2Up2 = p2.up2; state.p2Down2 = p2.down2; state.p2Left2 = p2.left2; state.p2Right2 = p2.right2;

    state.p3A = p3.a; state.p3B = p3.b; state.p3Select = p3.select; state.p3Start = p3.start;
    state.p3Up = p3.up; state.p3Down = p3.down; state.p3Left = p3.left; state.p3Right = p3.right;
    state.p4A = p4.a; state.p4B = p4.b; state.p4Select = p4.select; state.p4Start = p4.start;
    state.p4Up = p4.up; state.p4Down = p4.down; state.p4Left = p4.left; state.p4Right = p4.right;

    state.p1PowerPadButtons = inputFrame.powerPadButtons(1);
    state.p2PowerPadButtons = inputFrame.powerPadButtons(2);
    state.suborKeyboardKeys = inputFrame.suborKeyboardKeys();
    state.familyBasicKeyboardKeys = inputFrame.familyBasicKeyboardKeys();

    if(zapper1.x != -1 || zapper1.y != -1 || zapper1.trigger) {
        state.zapperX = zapper1.x;
        state.zapperY = zapper1.y;
    } else if(zapper2.x != -1 || zapper2.y != -1 || zapper2.trigger) {
        state.zapperX = zapper2.x;
        state.zapperY = zapper2.y;
    } else if(bandai.x != -1 || bandai.y != -1 || bandai.trigger) {
        state.zapperX = bandai.x;
        state.zapperY = bandai.y;
    } else {
        state.zapperX = -1;
        state.zapperY = -1;
    }

    if(snes1.deltaX != 0 || snes1.deltaY != 0 || snes1.primary || snes1.secondary) {
        state.mouseDeltaX = snes1.deltaX;
        state.mouseDeltaY = snes1.deltaY;
        state.mousePrimaryButton = snes1.primary;
        state.mouseSecondaryButton = snes1.secondary;
    } else if(snes2.deltaX != 0 || snes2.deltaY != 0 || snes2.primary || snes2.secondary) {
        state.mouseDeltaX = snes2.deltaX;
        state.mouseDeltaY = snes2.deltaY;
        state.mousePrimaryButton = snes2.primary;
        state.mouseSecondaryButton = snes2.secondary;
    } else if(subor1.deltaX != 0 || subor1.deltaY != 0 || subor1.primary || subor1.secondary) {
        state.mouseDeltaX = subor1.deltaX;
        state.mouseDeltaY = subor1.deltaY;
        state.mousePrimaryButton = subor1.primary;
        state.mouseSecondaryButton = subor1.secondary;
    } else if(subor2.deltaX != 0 || subor2.deltaY != 0 || subor2.primary || subor2.secondary) {
        state.mouseDeltaX = subor2.deltaX;
        state.mouseDeltaY = subor2.deltaY;
        state.mousePrimaryButton = subor2.primary;
        state.mouseSecondaryButton = subor2.secondary;
    } else {
        state.mouseDeltaX = 0;
        state.mouseDeltaY = 0;
        state.mousePrimaryButton = false;
        state.mouseSecondaryButton = false;
    }

    state.zapperP1Trigger = zapper1.trigger;
    state.zapperP2Trigger = zapper2.trigger;
    state.bandaiTrigger = bandai.trigger;
    state.konamiP1Run = konami.p1Run;
    state.konamiP1Jump = konami.p1Jump;
    state.konamiP2Run = konami.p2Run;
    state.konamiP2Jump = konami.p2Jump;
    state.arkanoidNesPosition = arkanoid1.button || arkanoid1.position != 0.5f
        ? arkanoid1.position
        : arkanoid2.position;
    state.arkanoidFamicomPosition = arkanoidExpansion.position;

    if(arkanoid1.button || arkanoid2.button || arkanoidExpansion.button) {
        state.mousePrimaryButton = state.mousePrimaryButton ||
                                   arkanoid1.button ||
                                   arkanoid2.button ||
                                   arkanoidExpansion.button;
    }
}

NetplayInputFrame buildGeraNESLocalInputContribution(PlayerSlot slot,
                                                     FrameNumber frame,
                                                     const EmulationHost::InputState& localInputState,
                                                     const RoomState& room)
{
    const GeraNESInputState inputState = toGeraNESInputState(localInputState);
    return toNetplayInputFrame(buildAssignedContribution(slot, inputState, makeRoomTopologyBaseFrame(frame, room)));
}

} // namespace GeraNESNetplay
