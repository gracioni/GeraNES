#include "GeraNESNetplay/GeraNESInputFrameAdapter.h"

#include "GeraNESNetplay/GeraNESNetplayAdapters.h"

namespace GeraNESNetplay
{

using ConsoleNetplay::kExpansionPlayerSlot;
using ConsoleNetplay::kMultitapP1PlayerSlot;
using ConsoleNetplay::kMultitapP2PlayerSlot;
using ConsoleNetplay::kMultitapP3PlayerSlot;
using ConsoleNetplay::kMultitapP4PlayerSlot;
using ConsoleNetplay::kPort1PlayerSlot;
using ConsoleNetplay::kPort2PlayerSlot;

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
    state.p1A = inputFrame.p1A; state.p1B = inputFrame.p1B; state.p1Select = inputFrame.p1Select; state.p1Start = inputFrame.p1Start;
    state.p1Up = inputFrame.p1Up; state.p1Down = inputFrame.p1Down; state.p1Left = inputFrame.p1Left; state.p1Right = inputFrame.p1Right;
    state.p1X = inputFrame.p1X; state.p1Y = inputFrame.p1Y; state.p1L = inputFrame.p1L; state.p1R = inputFrame.p1R;
    state.p1Up2 = inputFrame.vbP1Up1; state.p1Down2 = inputFrame.vbP1Down1; state.p1Left2 = inputFrame.vbP1Left1; state.p1Right2 = inputFrame.vbP1Right1;

    state.p2A = inputFrame.p2A; state.p2B = inputFrame.p2B; state.p2Select = inputFrame.p2Select; state.p2Start = inputFrame.p2Start;
    state.p2Up = inputFrame.p2Up; state.p2Down = inputFrame.p2Down; state.p2Left = inputFrame.p2Left; state.p2Right = inputFrame.p2Right;
    state.p2X = inputFrame.p2X; state.p2Y = inputFrame.p2Y; state.p2L = inputFrame.p2L; state.p2R = inputFrame.p2R;
    state.p2Up2 = inputFrame.vbP2Up1; state.p2Down2 = inputFrame.vbP2Down1; state.p2Left2 = inputFrame.vbP2Left1; state.p2Right2 = inputFrame.vbP2Right1;

    state.p3A = inputFrame.p3A; state.p3B = inputFrame.p3B; state.p3Select = inputFrame.p3Select; state.p3Start = inputFrame.p3Start;
    state.p3Up = inputFrame.p3Up; state.p3Down = inputFrame.p3Down; state.p3Left = inputFrame.p3Left; state.p3Right = inputFrame.p3Right;
    state.p4A = inputFrame.p4A; state.p4B = inputFrame.p4B; state.p4Select = inputFrame.p4Select; state.p4Start = inputFrame.p4Start;
    state.p4Up = inputFrame.p4Up; state.p4Down = inputFrame.p4Down; state.p4Left = inputFrame.p4Left; state.p4Right = inputFrame.p4Right;

    state.p1PowerPadButtons = inputFrame.powerPadP1Buttons;
    state.p2PowerPadButtons = inputFrame.powerPadP2Buttons;
    state.suborKeyboardKeys = inputFrame.suborKeyboardKeys;
    state.familyBasicKeyboardKeys = inputFrame.familyBasicKeyboardKeys;

    if(inputFrame.zapperP1X != -1 || inputFrame.zapperP1Y != -1 || inputFrame.zapperP1Trigger) {
        state.zapperX = inputFrame.zapperP1X;
        state.zapperY = inputFrame.zapperP1Y;
    } else if(inputFrame.zapperP2X != -1 || inputFrame.zapperP2Y != -1 || inputFrame.zapperP2Trigger) {
        state.zapperX = inputFrame.zapperP2X;
        state.zapperY = inputFrame.zapperP2Y;
    } else if(inputFrame.bandaiX != -1 || inputFrame.bandaiY != -1 || inputFrame.bandaiTrigger) {
        state.zapperX = inputFrame.bandaiX;
        state.zapperY = inputFrame.bandaiY;
    } else {
        state.zapperX = -1;
        state.zapperY = -1;
    }

    if(inputFrame.snesMouseP1DeltaX != 0 || inputFrame.snesMouseP1DeltaY != 0 ||
       inputFrame.snesMouseP1Left || inputFrame.snesMouseP1Right) {
        state.mouseDeltaX = inputFrame.snesMouseP1DeltaX;
        state.mouseDeltaY = inputFrame.snesMouseP1DeltaY;
        state.mousePrimaryButton = inputFrame.snesMouseP1Left;
        state.mouseSecondaryButton = inputFrame.snesMouseP1Right;
    } else if(inputFrame.snesMouseP2DeltaX != 0 || inputFrame.snesMouseP2DeltaY != 0 ||
              inputFrame.snesMouseP2Left || inputFrame.snesMouseP2Right) {
        state.mouseDeltaX = inputFrame.snesMouseP2DeltaX;
        state.mouseDeltaY = inputFrame.snesMouseP2DeltaY;
        state.mousePrimaryButton = inputFrame.snesMouseP2Left;
        state.mouseSecondaryButton = inputFrame.snesMouseP2Right;
    } else if(inputFrame.suborMouseP1DeltaX != 0 || inputFrame.suborMouseP1DeltaY != 0 ||
              inputFrame.suborMouseP1Left || inputFrame.suborMouseP1Right) {
        state.mouseDeltaX = inputFrame.suborMouseP1DeltaX;
        state.mouseDeltaY = inputFrame.suborMouseP1DeltaY;
        state.mousePrimaryButton = inputFrame.suborMouseP1Left;
        state.mouseSecondaryButton = inputFrame.suborMouseP1Right;
    } else if(inputFrame.suborMouseP2DeltaX != 0 || inputFrame.suborMouseP2DeltaY != 0 ||
              inputFrame.suborMouseP2Left || inputFrame.suborMouseP2Right) {
        state.mouseDeltaX = inputFrame.suborMouseP2DeltaX;
        state.mouseDeltaY = inputFrame.suborMouseP2DeltaY;
        state.mousePrimaryButton = inputFrame.suborMouseP2Left;
        state.mouseSecondaryButton = inputFrame.suborMouseP2Right;
    } else {
        state.mouseDeltaX = 0;
        state.mouseDeltaY = 0;
        state.mousePrimaryButton = false;
        state.mouseSecondaryButton = false;
    }

    state.zapperP1Trigger = inputFrame.zapperP1Trigger;
    state.zapperP2Trigger = inputFrame.zapperP2Trigger;
    state.bandaiTrigger = inputFrame.bandaiTrigger;
    state.konamiP1Run = inputFrame.konamiP1Run;
    state.konamiP1Jump = inputFrame.konamiP1Jump;
    state.konamiP2Run = inputFrame.konamiP2Run;
    state.konamiP2Jump = inputFrame.konamiP2Jump;
    state.arkanoidNesPosition = inputFrame.arkanoidP1Button || inputFrame.arkanoidP1Position != 0.5f
        ? inputFrame.arkanoidP1Position
        : inputFrame.arkanoidP2Position;
    state.arkanoidFamicomPosition = inputFrame.arkanoidFamicomPosition;

    if(inputFrame.arkanoidP1Button || inputFrame.arkanoidP2Button || inputFrame.arkanoidFamicomButton) {
        state.mousePrimaryButton = state.mousePrimaryButton ||
                                   inputFrame.arkanoidP1Button ||
                                   inputFrame.arkanoidP2Button ||
                                   inputFrame.arkanoidFamicomButton;
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
