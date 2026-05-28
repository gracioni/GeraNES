#include "GeraNES/InputBuffer.h"

namespace
{
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
                          InputFrame::DecodedData& decoded)
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
                               InputFrame::DecodedData& decoded)
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
                                     const InputFrame& frame,
                                     InputFrame::DecodedData& decoded)
{
    const bool multitapActive =
        frame.nesMultitapDevice == Settings::NesMultitapDevice::FOUR_SCORE ||
        frame.famicomMultitapDevice == Settings::FamicomMultitapDevice::HORI_ADAPTER;
    if(multitapActive) {
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
}

InputFrame::DecodedData InputFrame::decodedData() const
{
    DecodedData decoded;
    if(serializedInputData.empty()) {
        return decoded;
    }

    Deserialize d;
    d.setData(serializedInputData);
    serializeDecodedDataForTopology(d, *this, decoded);
    if(d.error()) {
        return DecodedData{};
    }
    return decoded;
}

bool InputFrame::setDecodedData(const DecodedData& decoded)
{
    DecodedData mutableDecoded = decoded;
    Serialize s;
    serializeDecodedDataForTopology(s, *this, mutableDecoded);
    serializedInputData = s.takeData();
    return true;
}
