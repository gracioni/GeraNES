#pragma once

#include <array>

namespace Netplay {

struct NetplayInputState
{
    bool p1A = false;
    bool p1B = false;
    bool p1Select = false;
    bool p1Start = false;
    bool p1Up = false;
    bool p1Down = false;
    bool p1Left = false;
    bool p1Right = false;
    bool p1X = false;
    bool p1Y = false;
    bool p1L = false;
    bool p1R = false;
    bool p1Up2 = false;
    bool p1Down2 = false;
    bool p1Left2 = false;
    bool p1Right2 = false;

    bool p2A = false;
    bool p2B = false;
    bool p2Select = false;
    bool p2Start = false;
    bool p2Up = false;
    bool p2Down = false;
    bool p2Left = false;
    bool p2Right = false;
    bool p2X = false;
    bool p2Y = false;
    bool p2L = false;
    bool p2R = false;
    bool p2Up2 = false;
    bool p2Down2 = false;
    bool p2Left2 = false;
    bool p2Right2 = false;

    bool p3A = false;
    bool p3B = false;
    bool p3Select = false;
    bool p3Start = false;
    bool p3Up = false;
    bool p3Down = false;
    bool p3Left = false;
    bool p3Right = false;

    bool p4A = false;
    bool p4B = false;
    bool p4Select = false;
    bool p4Start = false;
    bool p4Up = false;
    bool p4Down = false;
    bool p4Left = false;
    bool p4Right = false;

    std::array<bool, 12> p1PowerPadButtons = {};
    std::array<bool, 12> p2PowerPadButtons = {};
    std::array<bool, 99> suborKeyboardKeys = {};
    std::array<bool, 72> familyBasicKeyboardKeys = {};

    int zapperX = -1;
    int zapperY = -1;
    int mouseDeltaX = 0;
    int mouseDeltaY = 0;
    float arkanoidNesPosition = 0.5f;
    float arkanoidFamicomPosition = 0.5f;
    bool zapperP1Trigger = false;
    bool zapperP2Trigger = false;
    bool bandaiTrigger = false;
    bool konamiP1Run = false;
    bool konamiP1Jump = false;
    bool konamiP2Run = false;
    bool konamiP2Jump = false;
    bool mousePrimaryButton = false;
    bool mouseSecondaryButton = false;
};

} // namespace Netplay
