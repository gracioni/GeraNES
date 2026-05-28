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
    const InputFrame::DecodedData decoded = inputFrame.decodedData();
    state.p1A = decoded.p1A; state.p1B = decoded.p1B; state.p1Select = decoded.p1Select; state.p1Start = decoded.p1Start;
    state.p1Up = decoded.p1Up; state.p1Down = decoded.p1Down; state.p1Left = decoded.p1Left; state.p1Right = decoded.p1Right;
    state.p1X = decoded.p1X; state.p1Y = decoded.p1Y; state.p1L = decoded.p1L; state.p1R = decoded.p1R;
    state.p1Up2 = decoded.vbP1Up1; state.p1Down2 = decoded.vbP1Down1; state.p1Left2 = decoded.vbP1Left1; state.p1Right2 = decoded.vbP1Right1;

    state.p2A = decoded.p2A; state.p2B = decoded.p2B; state.p2Select = decoded.p2Select; state.p2Start = decoded.p2Start;
    state.p2Up = decoded.p2Up; state.p2Down = decoded.p2Down; state.p2Left = decoded.p2Left; state.p2Right = decoded.p2Right;
    state.p2X = decoded.p2X; state.p2Y = decoded.p2Y; state.p2L = decoded.p2L; state.p2R = decoded.p2R;
    state.p2Up2 = decoded.vbP2Up1; state.p2Down2 = decoded.vbP2Down1; state.p2Left2 = decoded.vbP2Left1; state.p2Right2 = decoded.vbP2Right1;

    state.p3A = decoded.p3A; state.p3B = decoded.p3B; state.p3Select = decoded.p3Select; state.p3Start = decoded.p3Start;
    state.p3Up = decoded.p3Up; state.p3Down = decoded.p3Down; state.p3Left = decoded.p3Left; state.p3Right = decoded.p3Right;
    state.p4A = decoded.p4A; state.p4B = decoded.p4B; state.p4Select = decoded.p4Select; state.p4Start = decoded.p4Start;
    state.p4Up = decoded.p4Up; state.p4Down = decoded.p4Down; state.p4Left = decoded.p4Left; state.p4Right = decoded.p4Right;

    state.p1PowerPadButtons = decoded.powerPadP1Buttons;
    state.p2PowerPadButtons = decoded.powerPadP2Buttons;
    state.suborKeyboardKeys = decoded.suborKeyboardKeys;
    state.familyBasicKeyboardKeys = decoded.familyBasicKeyboardKeys;

    if(decoded.zapperP1X != -1 || decoded.zapperP1Y != -1 || decoded.zapperP1Trigger) {
        state.zapperX = decoded.zapperP1X;
        state.zapperY = decoded.zapperP1Y;
    } else if(decoded.zapperP2X != -1 || decoded.zapperP2Y != -1 || decoded.zapperP2Trigger) {
        state.zapperX = decoded.zapperP2X;
        state.zapperY = decoded.zapperP2Y;
    } else if(decoded.bandaiX != -1 || decoded.bandaiY != -1 || decoded.bandaiTrigger) {
        state.zapperX = decoded.bandaiX;
        state.zapperY = decoded.bandaiY;
    } else {
        state.zapperX = -1;
        state.zapperY = -1;
    }

    if(decoded.snesMouseP1DeltaX != 0 || decoded.snesMouseP1DeltaY != 0 ||
       decoded.snesMouseP1Left || decoded.snesMouseP1Right) {
        state.mouseDeltaX = decoded.snesMouseP1DeltaX;
        state.mouseDeltaY = decoded.snesMouseP1DeltaY;
        state.mousePrimaryButton = decoded.snesMouseP1Left;
        state.mouseSecondaryButton = decoded.snesMouseP1Right;
    } else if(decoded.snesMouseP2DeltaX != 0 || decoded.snesMouseP2DeltaY != 0 ||
              decoded.snesMouseP2Left || decoded.snesMouseP2Right) {
        state.mouseDeltaX = decoded.snesMouseP2DeltaX;
        state.mouseDeltaY = decoded.snesMouseP2DeltaY;
        state.mousePrimaryButton = decoded.snesMouseP2Left;
        state.mouseSecondaryButton = decoded.snesMouseP2Right;
    } else if(decoded.suborMouseP1DeltaX != 0 || decoded.suborMouseP1DeltaY != 0 ||
              decoded.suborMouseP1Left || decoded.suborMouseP1Right) {
        state.mouseDeltaX = decoded.suborMouseP1DeltaX;
        state.mouseDeltaY = decoded.suborMouseP1DeltaY;
        state.mousePrimaryButton = decoded.suborMouseP1Left;
        state.mouseSecondaryButton = decoded.suborMouseP1Right;
    } else if(decoded.suborMouseP2DeltaX != 0 || decoded.suborMouseP2DeltaY != 0 ||
              decoded.suborMouseP2Left || decoded.suborMouseP2Right) {
        state.mouseDeltaX = decoded.suborMouseP2DeltaX;
        state.mouseDeltaY = decoded.suborMouseP2DeltaY;
        state.mousePrimaryButton = decoded.suborMouseP2Left;
        state.mouseSecondaryButton = decoded.suborMouseP2Right;
    } else {
        state.mouseDeltaX = 0;
        state.mouseDeltaY = 0;
        state.mousePrimaryButton = false;
        state.mouseSecondaryButton = false;
    }

    state.zapperP1Trigger = decoded.zapperP1Trigger;
    state.zapperP2Trigger = decoded.zapperP2Trigger;
    state.bandaiTrigger = decoded.bandaiTrigger;
    state.konamiP1Run = decoded.konamiP1Run;
    state.konamiP1Jump = decoded.konamiP1Jump;
    state.konamiP2Run = decoded.konamiP2Run;
    state.konamiP2Jump = decoded.konamiP2Jump;
    state.arkanoidNesPosition = decoded.arkanoidP1Button || decoded.arkanoidP1Position != 0.5f
        ? decoded.arkanoidP1Position
        : decoded.arkanoidP2Position;
    state.arkanoidFamicomPosition = decoded.arkanoidFamicomPosition;

    if(decoded.arkanoidP1Button || decoded.arkanoidP2Button || decoded.arkanoidFamicomButton) {
        state.mousePrimaryButton = state.mousePrimaryButton ||
                                   decoded.arkanoidP1Button ||
                                   decoded.arkanoidP2Button ||
                                   decoded.arkanoidFamicomButton;
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
