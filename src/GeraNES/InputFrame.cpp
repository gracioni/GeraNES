#include "GeraNES/InputBuffer.h"

namespace
{
struct DecodedInputData {
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

    int zapperP1X = -1;
    int zapperP1Y = -1;
    bool zapperP1Trigger = false;
    int zapperP2X = -1;
    int zapperP2Y = -1;
    bool zapperP2Trigger = false;

    bool bandaiA = false;
    bool bandaiB = false;
    bool bandaiSelect = false;
    bool bandaiStart = false;
    bool bandaiUp = false;
    bool bandaiDown = false;
    bool bandaiLeft = false;
    bool bandaiRight = false;
    int bandaiX = -1;
    int bandaiY = -1;
    bool bandaiTrigger = false;

    float arkanoidP1Position = 0.5f;
    bool arkanoidP1Button = false;
    float arkanoidP2Position = 0.5f;
    bool arkanoidP2Button = false;
    float arkanoidFamicomPosition = 0.5f;
    bool arkanoidFamicomButton = false;

    bool konamiP1Run = false;
    bool konamiP1Jump = false;
    bool konamiP2Run = false;
    bool konamiP2Jump = false;

    int snesMouseP1DeltaX = 0;
    int snesMouseP1DeltaY = 0;
    bool snesMouseP1Left = false;
    bool snesMouseP1Right = false;
    int snesMouseP2DeltaX = 0;
    int snesMouseP2DeltaY = 0;
    bool snesMouseP2Left = false;
    bool snesMouseP2Right = false;

    bool vbP1A = false;
    bool vbP1B = false;
    bool vbP1Select = false;
    bool vbP1Start = false;
    bool vbP1Up0 = false;
    bool vbP1Down0 = false;
    bool vbP1Left0 = false;
    bool vbP1Right0 = false;
    bool vbP1Up1 = false;
    bool vbP1Down1 = false;
    bool vbP1Left1 = false;
    bool vbP1Right1 = false;
    bool vbP1L = false;
    bool vbP1R = false;

    bool vbP2A = false;
    bool vbP2B = false;
    bool vbP2Select = false;
    bool vbP2Start = false;
    bool vbP2Up0 = false;
    bool vbP2Down0 = false;
    bool vbP2Left0 = false;
    bool vbP2Right0 = false;
    bool vbP2Up1 = false;
    bool vbP2Down1 = false;
    bool vbP2Left1 = false;
    bool vbP2Right1 = false;
    bool vbP2L = false;
    bool vbP2R = false;

    int suborMouseP1DeltaX = 0;
    int suborMouseP1DeltaY = 0;
    bool suborMouseP1Left = false;
    bool suborMouseP1Right = false;
    int suborMouseP2DeltaX = 0;
    int suborMouseP2DeltaY = 0;
    bool suborMouseP2Left = false;
    bool suborMouseP2Right = false;

    IExpansionDevice::SuborKeyboardKeys suborKeyboardKeys = {};
    IExpansionDevice::FamilyBasicKeyboardKeys familyBasicKeyboardKeys = {};
    std::array<bool, 12> powerPadP1Buttons = {};
    std::array<bool, 12> powerPadP2Buttons = {};
};

void serializePad8(SerializationBase& s,
                   bool& a,
                   bool& b,
                   bool& select,
                   bool& start,
                   bool& up,
                   bool& down,
                   bool& left,
                   bool& right)
{
    SERIALIZEDATA(s, a);
    SERIALIZEDATA(s, b);
    SERIALIZEDATA(s, select);
    SERIALIZEDATA(s, start);
    SERIALIZEDATA(s, up);
    SERIALIZEDATA(s, down);
    SERIALIZEDATA(s, left);
    SERIALIZEDATA(s, right);
}

void serializePad12(SerializationBase& s,
                    bool& a,
                    bool& b,
                    bool& select,
                    bool& start,
                    bool& up,
                    bool& down,
                    bool& left,
                    bool& right,
                    bool& x,
                    bool& y,
                    bool& l,
                    bool& r)
{
    serializePad8(s, a, b, select, start, up, down, left, right);
    SERIALIZEDATA(s, x);
    SERIALIZEDATA(s, y);
    SERIALIZEDATA(s, l);
    SERIALIZEDATA(s, r);
}

void serializeVirtualBoy(SerializationBase& s,
                         bool& a,
                         bool& b,
                         bool& select,
                         bool& start,
                         bool& up0,
                         bool& down0,
                         bool& left0,
                         bool& right0,
                         bool& up1,
                         bool& down1,
                         bool& left1,
                         bool& right1,
                         bool& l,
                         bool& r)
{
    SERIALIZEDATA(s, a);
    SERIALIZEDATA(s, b);
    SERIALIZEDATA(s, select);
    SERIALIZEDATA(s, start);
    SERIALIZEDATA(s, up0);
    SERIALIZEDATA(s, down0);
    SERIALIZEDATA(s, left0);
    SERIALIZEDATA(s, right0);
    SERIALIZEDATA(s, up1);
    SERIALIZEDATA(s, down1);
    SERIALIZEDATA(s, left1);
    SERIALIZEDATA(s, right1);
    SERIALIZEDATA(s, l);
    SERIALIZEDATA(s, r);
}

template<size_t N>
void serializeBoolArray(SerializationBase& s, std::array<bool, N>& values)
{
    s.array(reinterpret_cast<uint8_t*>(values.data()), 1, values.size());
}

void serializePortPayload(SerializationBase& s,
                          Settings::Device device,
                          int port,
                          DecodedInputData& decoded)
{
    switch(device) {
        case Settings::Device::CONTROLLER:
        case Settings::Device::FAMICOM_CONTROLLER:
            if(port == 1) serializePad8(s, decoded.p1A, decoded.p1B, decoded.p1Select, decoded.p1Start, decoded.p1Up, decoded.p1Down, decoded.p1Left, decoded.p1Right);
            else serializePad8(s, decoded.p2A, decoded.p2B, decoded.p2Select, decoded.p2Start, decoded.p2Up, decoded.p2Down, decoded.p2Left, decoded.p2Right);
            break;
        case Settings::Device::SNES_CONTROLLER:
            if(port == 1) serializePad12(s, decoded.p1A, decoded.p1B, decoded.p1Select, decoded.p1Start, decoded.p1Up, decoded.p1Down, decoded.p1Left, decoded.p1Right, decoded.p1X, decoded.p1Y, decoded.p1L, decoded.p1R);
            else serializePad12(s, decoded.p2A, decoded.p2B, decoded.p2Select, decoded.p2Start, decoded.p2Up, decoded.p2Down, decoded.p2Left, decoded.p2Right, decoded.p2X, decoded.p2Y, decoded.p2L, decoded.p2R);
            break;
        case Settings::Device::VIRTUAL_BOY_CONTROLLER:
            if(port == 1) serializeVirtualBoy(s, decoded.vbP1A, decoded.vbP1B, decoded.vbP1Select, decoded.vbP1Start, decoded.vbP1Up0, decoded.vbP1Down0, decoded.vbP1Left0, decoded.vbP1Right0, decoded.vbP1Up1, decoded.vbP1Down1, decoded.vbP1Left1, decoded.vbP1Right1, decoded.vbP1L, decoded.vbP1R);
            else serializeVirtualBoy(s, decoded.vbP2A, decoded.vbP2B, decoded.vbP2Select, decoded.vbP2Start, decoded.vbP2Up0, decoded.vbP2Down0, decoded.vbP2Left0, decoded.vbP2Right0, decoded.vbP2Up1, decoded.vbP2Down1, decoded.vbP2Left1, decoded.vbP2Right1, decoded.vbP2L, decoded.vbP2R);
            break;
        case Settings::Device::ZAPPER:
            if(port == 1) {
                SERIALIZEDATA(s, decoded.zapperP1X);
                SERIALIZEDATA(s, decoded.zapperP1Y);
                SERIALIZEDATA(s, decoded.zapperP1Trigger);
            } else {
                SERIALIZEDATA(s, decoded.zapperP2X);
                SERIALIZEDATA(s, decoded.zapperP2Y);
                SERIALIZEDATA(s, decoded.zapperP2Trigger);
            }
            break;
        case Settings::Device::ARKANOID_CONTROLLER:
            if(port == 1) {
                SERIALIZEDATA(s, decoded.arkanoidP1Position);
                SERIALIZEDATA(s, decoded.arkanoidP1Button);
            } else {
                SERIALIZEDATA(s, decoded.arkanoidP2Position);
                SERIALIZEDATA(s, decoded.arkanoidP2Button);
            }
            break;
        case Settings::Device::SNES_MOUSE:
            if(port == 1) {
                SERIALIZEDATA(s, decoded.snesMouseP1DeltaX);
                SERIALIZEDATA(s, decoded.snesMouseP1DeltaY);
                SERIALIZEDATA(s, decoded.snesMouseP1Left);
                SERIALIZEDATA(s, decoded.snesMouseP1Right);
            } else {
                SERIALIZEDATA(s, decoded.snesMouseP2DeltaX);
                SERIALIZEDATA(s, decoded.snesMouseP2DeltaY);
                SERIALIZEDATA(s, decoded.snesMouseP2Left);
                SERIALIZEDATA(s, decoded.snesMouseP2Right);
            }
            break;
        case Settings::Device::SUBOR_MOUSE:
            if(port == 1) {
                SERIALIZEDATA(s, decoded.suborMouseP1DeltaX);
                SERIALIZEDATA(s, decoded.suborMouseP1DeltaY);
                SERIALIZEDATA(s, decoded.suborMouseP1Left);
                SERIALIZEDATA(s, decoded.suborMouseP1Right);
            } else {
                SERIALIZEDATA(s, decoded.suborMouseP2DeltaX);
                SERIALIZEDATA(s, decoded.suborMouseP2DeltaY);
                SERIALIZEDATA(s, decoded.suborMouseP2Left);
                SERIALIZEDATA(s, decoded.suborMouseP2Right);
            }
            break;
        case Settings::Device::POWER_PAD_SIDE_A:
        case Settings::Device::POWER_PAD_SIDE_B:
            if(port == 1) {
                serializeBoolArray(s, decoded.powerPadP1Buttons);
            } else {
                serializeBoolArray(s, decoded.powerPadP2Buttons);
            }
            break;
        case Settings::Device::BANDAI_HYPERSHOT:
        case Settings::Device::NONE:
            break;
    }
}

void serializeExpansionPayload(SerializationBase& s,
                               Settings::ExpansionDevice device,
                               DecodedInputData& decoded)
{
    switch(device) {
        case Settings::ExpansionDevice::NONE:
        case Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM:
            break;
        case Settings::ExpansionDevice::BANDAI_HYPERSHOT:
            serializePad8(s, decoded.bandaiA, decoded.bandaiB, decoded.bandaiSelect, decoded.bandaiStart, decoded.bandaiUp, decoded.bandaiDown, decoded.bandaiLeft, decoded.bandaiRight);
            SERIALIZEDATA(s, decoded.bandaiX);
            SERIALIZEDATA(s, decoded.bandaiY);
            SERIALIZEDATA(s, decoded.bandaiTrigger);
            break;
        case Settings::ExpansionDevice::KONAMI_HYPERSHOT:
            SERIALIZEDATA(s, decoded.konamiP1Run);
            SERIALIZEDATA(s, decoded.konamiP1Jump);
            SERIALIZEDATA(s, decoded.konamiP2Run);
            SERIALIZEDATA(s, decoded.konamiP2Jump);
            break;
        case Settings::ExpansionDevice::ARKANOID_CONTROLLER:
            SERIALIZEDATA(s, decoded.arkanoidFamicomPosition);
            SERIALIZEDATA(s, decoded.arkanoidFamicomButton);
            break;
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A:
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B:
            serializeBoolArray(s, decoded.powerPadP1Buttons);
            break;
        case Settings::ExpansionDevice::SUBOR_KEYBOARD:
            serializeBoolArray(s, decoded.suborKeyboardKeys);
            break;
        case Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD:
            serializeBoolArray(s, decoded.familyBasicKeyboardKeys);
            break;
    }
}

void serializeDecodedDataForTopology(SerializationBase& s,
                                     const InputState& frame,
                                     DecodedInputData& decoded)
{
    if(frame.multitapActive()) {
        serializePad8(s, decoded.p1A, decoded.p1B, decoded.p1Select, decoded.p1Start, decoded.p1Up, decoded.p1Down, decoded.p1Left, decoded.p1Right);
        serializePad8(s, decoded.p2A, decoded.p2B, decoded.p2Select, decoded.p2Start, decoded.p2Up, decoded.p2Down, decoded.p2Left, decoded.p2Right);
        serializePad8(s, decoded.p3A, decoded.p3B, decoded.p3Select, decoded.p3Start, decoded.p3Up, decoded.p3Down, decoded.p3Left, decoded.p3Right);
        serializePad8(s, decoded.p4A, decoded.p4B, decoded.p4Select, decoded.p4Start, decoded.p4Up, decoded.p4Down, decoded.p4Left, decoded.p4Right);
    } else {
        serializePortPayload(s, frame.port1Device, 1, decoded);
        serializePortPayload(s, frame.port2Device, 2, decoded);
    }
    serializeExpansionPayload(s, frame.expansionDevice, decoded);
}

DecodedInputData decodeFrameData(const InputState& frame)
{
    DecodedInputData decoded;
    if(frame.serializedInputData.empty()) {
        return decoded;
    }

    Deserialize d;
    d.setData(frame.serializedInputData);
    serializeDecodedDataForTopology(d, frame, decoded);
    if(d.error()) {
        return {};
    }
    return decoded;
}

bool encodeFrameData(InputState& frame, const DecodedInputData& decoded)
{
    DecodedInputData mutableDecoded = decoded;
    Serialize s;
    serializeDecodedDataForTopology(s, frame, mutableDecoded);
    frame.serializedInputData = s.takeData();
    return true;
}

bool portUsesPadButtons(const InputState& frame, int port)
{
    if(frame.multitapActive()) {
        return port >= 1 && port <= 4;
    }
    if(port == 1) {
        return frame.port1Device == Settings::Device::CONTROLLER ||
               frame.port1Device == Settings::Device::FAMICOM_CONTROLLER ||
               frame.port1Device == Settings::Device::SNES_CONTROLLER ||
               frame.port1Device == Settings::Device::VIRTUAL_BOY_CONTROLLER;
    }
    if(port == 2) {
        return frame.port2Device == Settings::Device::CONTROLLER ||
               frame.port2Device == Settings::Device::FAMICOM_CONTROLLER ||
               frame.port2Device == Settings::Device::SNES_CONTROLLER ||
               frame.port2Device == Settings::Device::VIRTUAL_BOY_CONTROLLER;
    }
    if(port == 3) {
        return frame.expansionDevice == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM;
    }
    return false;
}
}

InputState::PadButtons InputState::portButtons(int port) const
{
    if(!portUsesPadButtons(*this, port)) {
        return {};
    }
    const DecodedInputData decoded = decodeFrameData(*this);
    PadButtons buttons;
    switch(port) {
        case 1:
            buttons = {decoded.p1A, decoded.p1B, decoded.p1Select, decoded.p1Start, decoded.p1Up, decoded.p1Down, decoded.p1Left, decoded.p1Right,
                       decoded.p1X, decoded.p1Y, decoded.p1L, decoded.p1R, decoded.vbP1Up1, decoded.vbP1Down1, decoded.vbP1Left1, decoded.vbP1Right1};
            break;
        case 2:
            buttons = {decoded.p2A, decoded.p2B, decoded.p2Select, decoded.p2Start, decoded.p2Up, decoded.p2Down, decoded.p2Left, decoded.p2Right,
                       decoded.p2X, decoded.p2Y, decoded.p2L, decoded.p2R, decoded.vbP2Up1, decoded.vbP2Down1, decoded.vbP2Left1, decoded.vbP2Right1};
            break;
        case 3:
            buttons = {decoded.p3A, decoded.p3B, decoded.p3Select, decoded.p3Start, decoded.p3Up, decoded.p3Down, decoded.p3Left, decoded.p3Right};
            break;
        case 4:
            buttons = {decoded.p4A, decoded.p4B, decoded.p4Select, decoded.p4Start, decoded.p4Up, decoded.p4Down, decoded.p4Left, decoded.p4Right};
            break;
        default:
            break;
    }
    return buttons;
}

void InputState::setPortButtons(int port, const PadButtons& buttons)
{
    DecodedInputData decoded = decodeFrameData(*this);
    switch(port) {
        case 1:
            decoded.p1A = buttons.a; decoded.p1B = buttons.b; decoded.p1Select = buttons.select; decoded.p1Start = buttons.start;
            decoded.p1Up = buttons.up; decoded.p1Down = buttons.down; decoded.p1Left = buttons.left; decoded.p1Right = buttons.right;
            decoded.p1X = buttons.x; decoded.p1Y = buttons.y; decoded.p1L = buttons.l; decoded.p1R = buttons.r;
            decoded.vbP1A = buttons.a; decoded.vbP1B = buttons.b; decoded.vbP1Select = buttons.select; decoded.vbP1Start = buttons.start;
            decoded.vbP1Up0 = buttons.up; decoded.vbP1Down0 = buttons.down; decoded.vbP1Left0 = buttons.left; decoded.vbP1Right0 = buttons.right;
            decoded.vbP1Up1 = buttons.up2; decoded.vbP1Down1 = buttons.down2; decoded.vbP1Left1 = buttons.left2; decoded.vbP1Right1 = buttons.right2;
            decoded.vbP1L = buttons.l; decoded.vbP1R = buttons.r;
            break;
        case 2:
            decoded.p2A = buttons.a; decoded.p2B = buttons.b; decoded.p2Select = buttons.select; decoded.p2Start = buttons.start;
            decoded.p2Up = buttons.up; decoded.p2Down = buttons.down; decoded.p2Left = buttons.left; decoded.p2Right = buttons.right;
            decoded.p2X = buttons.x; decoded.p2Y = buttons.y; decoded.p2L = buttons.l; decoded.p2R = buttons.r;
            decoded.vbP2A = buttons.a; decoded.vbP2B = buttons.b; decoded.vbP2Select = buttons.select; decoded.vbP2Start = buttons.start;
            decoded.vbP2Up0 = buttons.up; decoded.vbP2Down0 = buttons.down; decoded.vbP2Left0 = buttons.left; decoded.vbP2Right0 = buttons.right;
            decoded.vbP2Up1 = buttons.up2; decoded.vbP2Down1 = buttons.down2; decoded.vbP2Left1 = buttons.left2; decoded.vbP2Right1 = buttons.right2;
            decoded.vbP2L = buttons.l; decoded.vbP2R = buttons.r;
            break;
        case 3:
            decoded.p3A = buttons.a; decoded.p3B = buttons.b; decoded.p3Select = buttons.select; decoded.p3Start = buttons.start;
            decoded.p3Up = buttons.up; decoded.p3Down = buttons.down; decoded.p3Left = buttons.left; decoded.p3Right = buttons.right;
            break;
        case 4:
            decoded.p4A = buttons.a; decoded.p4B = buttons.b; decoded.p4Select = buttons.select; decoded.p4Start = buttons.start;
            decoded.p4Up = buttons.up; decoded.p4Down = buttons.down; decoded.p4Left = buttons.left; decoded.p4Right = buttons.right;
            break;
        default:
            return;
    }
    (void)encodeFrameData(*this, decoded);
}

InputState::PadButtons InputState::bandaiButtons() const
{
    if(expansionDevice != Settings::ExpansionDevice::BANDAI_HYPERSHOT &&
       port2Device != Settings::Device::BANDAI_HYPERSHOT) {
        return {};
    }
    const DecodedInputData decoded = decodeFrameData(*this);
    return {decoded.bandaiA, decoded.bandaiB, decoded.bandaiSelect, decoded.bandaiStart,
            decoded.bandaiUp, decoded.bandaiDown, decoded.bandaiLeft, decoded.bandaiRight};
}

void InputState::setBandaiButtons(const PadButtons& buttons)
{
    DecodedInputData decoded = decodeFrameData(*this);
    decoded.bandaiA = buttons.a; decoded.bandaiB = buttons.b; decoded.bandaiSelect = buttons.select; decoded.bandaiStart = buttons.start;
    decoded.bandaiUp = buttons.up; decoded.bandaiDown = buttons.down; decoded.bandaiLeft = buttons.left; decoded.bandaiRight = buttons.right;
    (void)encodeFrameData(*this, decoded);
}

InputState::PointerState InputState::zapper(int port) const
{
    if((port == 1 && port1Device != Settings::Device::ZAPPER) ||
       (port == 2 && port2Device != Settings::Device::ZAPPER)) {
        return {};
    }
    const DecodedInputData decoded = decodeFrameData(*this);
    return port == 1 ? PointerState{decoded.zapperP1X, decoded.zapperP1Y, decoded.zapperP1Trigger}
                     : PointerState{decoded.zapperP2X, decoded.zapperP2Y, decoded.zapperP2Trigger};
}

void InputState::setZapper(int port, const PointerState& state)
{
    DecodedInputData decoded = decodeFrameData(*this);
    if(port == 1) {
        decoded.zapperP1X = state.x; decoded.zapperP1Y = state.y; decoded.zapperP1Trigger = state.trigger;
    } else if(port == 2) {
        decoded.zapperP2X = state.x; decoded.zapperP2Y = state.y; decoded.zapperP2Trigger = state.trigger;
    } else {
        return;
    }
    (void)encodeFrameData(*this, decoded);
}

InputState::PointerState InputState::bandaiPointer() const
{
    if(expansionDevice != Settings::ExpansionDevice::BANDAI_HYPERSHOT) {
        return {};
    }
    const DecodedInputData decoded = decodeFrameData(*this);
    return {decoded.bandaiX, decoded.bandaiY, decoded.bandaiTrigger};
}

void InputState::setBandaiPointer(const PointerState& state)
{
    DecodedInputData decoded = decodeFrameData(*this);
    decoded.bandaiX = state.x; decoded.bandaiY = state.y; decoded.bandaiTrigger = state.trigger;
    (void)encodeFrameData(*this, decoded);
}

InputState::ArkanoidState InputState::arkanoidController(int port) const
{
    if((port == 1 && port1Device != Settings::Device::ARKANOID_CONTROLLER) ||
       (port == 2 && port2Device != Settings::Device::ARKANOID_CONTROLLER)) {
        return {};
    }
    const DecodedInputData decoded = decodeFrameData(*this);
    return port == 1 ? ArkanoidState{decoded.arkanoidP1Position, decoded.arkanoidP1Button}
                     : ArkanoidState{decoded.arkanoidP2Position, decoded.arkanoidP2Button};
}

void InputState::setArkanoidController(int port, const ArkanoidState& state)
{
    DecodedInputData decoded = decodeFrameData(*this);
    if(port == 1) {
        decoded.arkanoidP1Position = state.position; decoded.arkanoidP1Button = state.button;
    } else if(port == 2) {
        decoded.arkanoidP2Position = state.position; decoded.arkanoidP2Button = state.button;
    } else {
        return;
    }
    (void)encodeFrameData(*this, decoded);
}

InputState::ArkanoidState InputState::arkanoidExpansion() const
{
    if(expansionDevice != Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
        return {};
    }
    const DecodedInputData decoded = decodeFrameData(*this);
    return {decoded.arkanoidFamicomPosition, decoded.arkanoidFamicomButton};
}

void InputState::setArkanoidExpansion(const ArkanoidState& state)
{
    DecodedInputData decoded = decodeFrameData(*this);
    decoded.arkanoidFamicomPosition = state.position; decoded.arkanoidFamicomButton = state.button;
    (void)encodeFrameData(*this, decoded);
}

InputState::RelativePointerState InputState::snesMouse(int port) const
{
    if((port == 1 && port1Device != Settings::Device::SNES_MOUSE) ||
       (port == 2 && port2Device != Settings::Device::SNES_MOUSE)) {
        return {};
    }
    const DecodedInputData decoded = decodeFrameData(*this);
    return port == 1 ? RelativePointerState{decoded.snesMouseP1DeltaX, decoded.snesMouseP1DeltaY, decoded.snesMouseP1Left, decoded.snesMouseP1Right}
                     : RelativePointerState{decoded.snesMouseP2DeltaX, decoded.snesMouseP2DeltaY, decoded.snesMouseP2Left, decoded.snesMouseP2Right};
}

void InputState::setSnesMouse(int port, const RelativePointerState& state)
{
    DecodedInputData decoded = decodeFrameData(*this);
    if(port == 1) {
        decoded.snesMouseP1DeltaX = state.deltaX; decoded.snesMouseP1DeltaY = state.deltaY; decoded.snesMouseP1Left = state.primary; decoded.snesMouseP1Right = state.secondary;
    } else if(port == 2) {
        decoded.snesMouseP2DeltaX = state.deltaX; decoded.snesMouseP2DeltaY = state.deltaY; decoded.snesMouseP2Left = state.primary; decoded.snesMouseP2Right = state.secondary;
    } else {
        return;
    }
    (void)encodeFrameData(*this, decoded);
}

InputState::RelativePointerState InputState::suborMouse(int port) const
{
    if((port == 1 && port1Device != Settings::Device::SUBOR_MOUSE) ||
       (port == 2 && port2Device != Settings::Device::SUBOR_MOUSE)) {
        return {};
    }
    const DecodedInputData decoded = decodeFrameData(*this);
    return port == 1 ? RelativePointerState{decoded.suborMouseP1DeltaX, decoded.suborMouseP1DeltaY, decoded.suborMouseP1Left, decoded.suborMouseP1Right}
                     : RelativePointerState{decoded.suborMouseP2DeltaX, decoded.suborMouseP2DeltaY, decoded.suborMouseP2Left, decoded.suborMouseP2Right};
}

void InputState::setSuborMouse(int port, const RelativePointerState& state)
{
    DecodedInputData decoded = decodeFrameData(*this);
    if(port == 1) {
        decoded.suborMouseP1DeltaX = state.deltaX; decoded.suborMouseP1DeltaY = state.deltaY; decoded.suborMouseP1Left = state.primary; decoded.suborMouseP1Right = state.secondary;
    } else if(port == 2) {
        decoded.suborMouseP2DeltaX = state.deltaX; decoded.suborMouseP2DeltaY = state.deltaY; decoded.suborMouseP2Left = state.primary; decoded.suborMouseP2Right = state.secondary;
    } else {
        return;
    }
    (void)encodeFrameData(*this, decoded);
}

InputState::KonamiHyperShotState InputState::konamiHyperShot() const
{
    if(expansionDevice != Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
        return {};
    }
    const DecodedInputData decoded = decodeFrameData(*this);
    return {decoded.konamiP1Run, decoded.konamiP1Jump, decoded.konamiP2Run, decoded.konamiP2Jump};
}

void InputState::setKonamiHyperShot(const KonamiHyperShotState& state)
{
    DecodedInputData decoded = decodeFrameData(*this);
    decoded.konamiP1Run = state.p1Run; decoded.konamiP1Jump = state.p1Jump; decoded.konamiP2Run = state.p2Run; decoded.konamiP2Jump = state.p2Jump;
    (void)encodeFrameData(*this, decoded);
}

std::array<bool, 12> InputState::powerPadButtons(int port) const
{
    const bool familyTrainer =
        expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
        expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B;
    if((port == 1 && port1Device != Settings::Device::POWER_PAD_SIDE_A && port1Device != Settings::Device::POWER_PAD_SIDE_B && !familyTrainer) ||
       (port == 2 && port2Device != Settings::Device::POWER_PAD_SIDE_A && port2Device != Settings::Device::POWER_PAD_SIDE_B && !familyTrainer)) {
        return {};
    }
    const DecodedInputData decoded = decodeFrameData(*this);
    return port == 1 ? decoded.powerPadP1Buttons : decoded.powerPadP2Buttons;
}

void InputState::setPowerPadButtons(int port, const std::array<bool, 12>& buttons)
{
    DecodedInputData decoded = decodeFrameData(*this);
    if(port == 1) {
        decoded.powerPadP1Buttons = buttons;
    } else if(port == 2) {
        decoded.powerPadP2Buttons = buttons;
    } else {
        return;
    }
    (void)encodeFrameData(*this, decoded);
}

IExpansionDevice::SuborKeyboardKeys InputState::suborKeyboardKeys() const
{
    if(expansionDevice != Settings::ExpansionDevice::SUBOR_KEYBOARD) {
        return {};
    }
    return decodeFrameData(*this).suborKeyboardKeys;
}

void InputState::setSuborKeyboardKeys(const IExpansionDevice::SuborKeyboardKeys& keys)
{
    DecodedInputData decoded = decodeFrameData(*this);
    decoded.suborKeyboardKeys = keys;
    (void)encodeFrameData(*this, decoded);
}

IExpansionDevice::FamilyBasicKeyboardKeys InputState::familyBasicKeyboardKeys() const
{
    if(expansionDevice != Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD) {
        return {};
    }
    return decodeFrameData(*this).familyBasicKeyboardKeys;
}

void InputState::setFamilyBasicKeyboardKeys(const IExpansionDevice::FamilyBasicKeyboardKeys& keys)
{
    DecodedInputData decoded = decodeFrameData(*this);
    decoded.familyBasicKeyboardKeys = keys;
    (void)encodeFrameData(*this, decoded);
}
