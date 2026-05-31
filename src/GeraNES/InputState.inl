namespace
{

template<typename T>
bool readScalarAt(const std::vector<uint8_t>& data, size_t offset, T& value)
{
    if(offset + sizeof(T) > data.size()) {
        return false;
    }
    std::memcpy(&value, data.data() + offset, sizeof(T));
    return true;
}

template<typename T>
void writeScalarAt(std::vector<uint8_t>& data, size_t offset, const T& value)
{
    if(offset + sizeof(T) > data.size()) {
        return;
    }
    std::memcpy(data.data() + offset, &value, sizeof(T));
}

template<size_t N>
bool readBoolArrayAt(const std::vector<uint8_t>& data, size_t offset, std::array<bool, N>& value)
{
    if(offset + value.size() > data.size()) {
        return false;
    }
    std::memcpy(value.data(), data.data() + offset, value.size());
    return true;
}

template<size_t N>
void writeBoolArrayAt(std::vector<uint8_t>& data, size_t offset, const std::array<bool, N>& value)
{
    if(offset + value.size() > data.size()) {
        return;
    }
    std::memcpy(data.data() + offset, value.data(), value.size());
}

constexpr size_t kPad8SerializedSize = sizeof(bool) * 8;
constexpr size_t kPad12SerializedSize = sizeof(bool) * 12;
constexpr size_t kVirtualBoySerializedSize = sizeof(bool) * 14;
constexpr size_t kPointerSerializedSize = sizeof(int) * 2 + sizeof(bool);
constexpr size_t kArkanoidSerializedSize = sizeof(float) + sizeof(bool);
constexpr size_t kRelativePointerSerializedSize = sizeof(int) * 2 + sizeof(bool) * 2;
constexpr size_t kKonamiHyperShotSerializedSize = sizeof(bool) * 4;
constexpr size_t kPowerPadSerializedSize = 12;
constexpr size_t kSuborKeyboardSerializedSize = IExpansionDevice::SuborKeyboardKeys{}.size();
constexpr size_t kFamilyBasicKeyboardSerializedSize = IExpansionDevice::FamilyBasicKeyboardKeys{}.size();
constexpr size_t kBandaiExpansionSerializedSize = kPad8SerializedSize + kPointerSerializedSize;

enum class PortButtonsKind
{
    None,
    Pad8,
    Pad12,
    VirtualBoy
};

struct FieldLocation
{
    size_t offset = 0;
    bool valid = false;
};

struct PortButtonsLocation
{
    size_t offset = 0;
    PortButtonsKind kind = PortButtonsKind::None;
    bool valid = false;
};

bool isPad8Device(Settings::Device device)
{
    return device == Settings::Device::CONTROLLER ||
           device == Settings::Device::FAMICOM_CONTROLLER;
}

bool isPad12Device(Settings::Device device)
{
    return device == Settings::Device::SNES_CONTROLLER;
}

bool isVirtualBoyDevice(Settings::Device device)
{
    return device == Settings::Device::VIRTUAL_BOY_CONTROLLER;
}

size_t portPayloadSize(Settings::Device device)
{
    if(isPad8Device(device)) return kPad8SerializedSize;
    if(isPad12Device(device)) return kPad12SerializedSize;
    if(isVirtualBoyDevice(device)) return kVirtualBoySerializedSize;

    switch(device) {
        case Settings::Device::ZAPPER:
            return kPointerSerializedSize;
        case Settings::Device::ARKANOID_CONTROLLER:
            return kArkanoidSerializedSize;
        case Settings::Device::SNES_MOUSE:
        case Settings::Device::SUBOR_MOUSE:
            return kRelativePointerSerializedSize;
        case Settings::Device::POWER_PAD_SIDE_A:
        case Settings::Device::POWER_PAD_SIDE_B:
            return kPowerPadSerializedSize;
        case Settings::Device::BANDAI_HYPERSHOT:
        case Settings::Device::NONE:
            return 0;
    }

    return 0;
}

size_t expansionPayloadSize(Settings::ExpansionDevice device)
{
    switch(device) {
        case Settings::ExpansionDevice::BANDAI_HYPERSHOT:
            return kBandaiExpansionSerializedSize;
        case Settings::ExpansionDevice::KONAMI_HYPERSHOT:
            return kKonamiHyperShotSerializedSize;
        case Settings::ExpansionDevice::ARKANOID_CONTROLLER:
            return kArkanoidSerializedSize;
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A:
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B:
            return kPowerPadSerializedSize;
        case Settings::ExpansionDevice::SUBOR_KEYBOARD:
            return kSuborKeyboardSerializedSize;
        case Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD:
            return kFamilyBasicKeyboardSerializedSize;
        case Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM:
            return kPad8SerializedSize;
        case Settings::ExpansionDevice::NONE:
            return 0;
    }

    return 0;
}

size_t expectedSerializedInputDataSize(const InputState& state)
{
    if(state.multitapActive()) {
        return kPad8SerializedSize * 4;
    }

    return portPayloadSize(state.portDevice(1)) +
           portPayloadSize(state.portDevice(2)) +
           expansionPayloadSize(state.topology.expansionDevice);
}

void serializeDefaultPad8(SerializationBase& s)
{
    bool a = false, b = false, select = false, start = false;
    bool up = false, down = false, left = false, right = false;
    SERIALIZEDATA(s, a);
    SERIALIZEDATA(s, b);
    SERIALIZEDATA(s, select);
    SERIALIZEDATA(s, start);
    SERIALIZEDATA(s, up);
    SERIALIZEDATA(s, down);
    SERIALIZEDATA(s, left);
    SERIALIZEDATA(s, right);
}

void serializeDefaultPad12(SerializationBase& s)
{
    serializeDefaultPad8(s);
    bool x = false, y = false, l = false, r = false;
    SERIALIZEDATA(s, x);
    SERIALIZEDATA(s, y);
    SERIALIZEDATA(s, l);
    SERIALIZEDATA(s, r);
}

void serializeDefaultVirtualBoy(SerializationBase& s)
{
    bool a = false, b = false, select = false, start = false;
    bool up0 = false, down0 = false, left0 = false, right0 = false;
    bool up1 = false, down1 = false, left1 = false, right1 = false;
    bool l = false, r = false;
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

void serializeDefaultPointer(SerializationBase& s)
{
    int x = -1;
    int y = -1;
    bool trigger = false;
    SERIALIZEDATA(s, x);
    SERIALIZEDATA(s, y);
    SERIALIZEDATA(s, trigger);
}

void serializeDefaultArkanoid(SerializationBase& s)
{
    float position = 0.5f;
    bool button = false;
    SERIALIZEDATA(s, position);
    SERIALIZEDATA(s, button);
}

void serializeDefaultRelativePointer(SerializationBase& s)
{
    int deltaX = 0;
    int deltaY = 0;
    bool primary = false;
    bool secondary = false;
    SERIALIZEDATA(s, deltaX);
    SERIALIZEDATA(s, deltaY);
    SERIALIZEDATA(s, primary);
    SERIALIZEDATA(s, secondary);
}

void serializeDefaultPowerPad(SerializationBase& s)
{
    std::array<bool, 12> buttons = {};
    s.array(reinterpret_cast<uint8_t*>(buttons.data()), 1, buttons.size());
}

void serializeDefaultSuborKeyboard(SerializationBase& s)
{
    IExpansionDevice::SuborKeyboardKeys keys = {};
    s.array(reinterpret_cast<uint8_t*>(keys.data()), 1, keys.size());
}

void serializeDefaultFamilyBasicKeyboard(SerializationBase& s)
{
    IExpansionDevice::FamilyBasicKeyboardKeys keys = {};
    s.array(reinterpret_cast<uint8_t*>(keys.data()), 1, keys.size());
}

void serializeDefaultPortPayload(SerializationBase& s, Settings::Device device)
{
    if(isPad8Device(device)) {
        serializeDefaultPad8(s);
        return;
    }
    if(isPad12Device(device)) {
        serializeDefaultPad12(s);
        return;
    }
    if(isVirtualBoyDevice(device)) {
        serializeDefaultVirtualBoy(s);
        return;
    }

    switch(device) {
        case Settings::Device::ZAPPER:
            serializeDefaultPointer(s);
            break;
        case Settings::Device::ARKANOID_CONTROLLER:
            serializeDefaultArkanoid(s);
            break;
        case Settings::Device::SNES_MOUSE:
        case Settings::Device::SUBOR_MOUSE:
            serializeDefaultRelativePointer(s);
            break;
        case Settings::Device::POWER_PAD_SIDE_A:
        case Settings::Device::POWER_PAD_SIDE_B:
            serializeDefaultPowerPad(s);
            break;
        case Settings::Device::BANDAI_HYPERSHOT:
        case Settings::Device::NONE:
            break;
    }
}

void serializeDefaultExpansionPayload(SerializationBase& s, Settings::ExpansionDevice device)
{
    switch(device) {
        case Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM:
            serializeDefaultPad8(s);
            break;
        case Settings::ExpansionDevice::BANDAI_HYPERSHOT:
            serializeDefaultPad8(s);
            serializeDefaultPointer(s);
            break;
        case Settings::ExpansionDevice::KONAMI_HYPERSHOT:
        {
            bool p1Run = false, p1Jump = false, p2Run = false, p2Jump = false;
            SERIALIZEDATA(s, p1Run);
            SERIALIZEDATA(s, p1Jump);
            SERIALIZEDATA(s, p2Run);
            SERIALIZEDATA(s, p2Jump);
            break;
        }
        case Settings::ExpansionDevice::ARKANOID_CONTROLLER:
            serializeDefaultArkanoid(s);
            break;
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A:
        case Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B:
            serializeDefaultPowerPad(s);
            break;
        case Settings::ExpansionDevice::SUBOR_KEYBOARD:
            serializeDefaultSuborKeyboard(s);
            break;
        case Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD:
            serializeDefaultFamilyBasicKeyboard(s);
            break;
        case Settings::ExpansionDevice::NONE:
            break;
    }
}

std::vector<uint8_t> makeDefaultSerializedInputData(const InputState& state)
{
    Serialize s;
    s.reserve(expectedSerializedInputDataSize(state));
    if(state.multitapActive()) {
        serializeDefaultPad8(s);
        serializeDefaultPad8(s);
        serializeDefaultPad8(s);
        serializeDefaultPad8(s);
        return s.takeData();
    }

    serializeDefaultPortPayload(s, state.portDevice(1));
    serializeDefaultPortPayload(s, state.portDevice(2));
    serializeDefaultExpansionPayload(s, state.topology.expansionDevice);
    return s.takeData();
}

bool hasCanonicalSerializedInputData(const InputState& state)
{
    return state.serializedInputData.size() == expectedSerializedInputDataSize(state);
}

void ensureCanonicalSerializedInputData(InputState& state)
{
    if(hasCanonicalSerializedInputData(state)) {
        return;
    }
    state.serializedInputData = makeDefaultSerializedInputData(state);
}

size_t expansionPayloadOffset(const InputState& state)
{
    if(state.multitapActive()) {
        return kPad8SerializedSize * 4;
    }

    return portPayloadSize(state.portDevice(1)) + portPayloadSize(state.portDevice(2));
}

PortButtonsLocation locatePortButtons(const InputState& state, int port)
{
    if(state.multitapActive()) {
        if(port >= 1 && port <= 4) {
            return {static_cast<size_t>(port - 1) * kPad8SerializedSize, PortButtonsKind::Pad8, true};
        }
        return {};
    }

    if(port == 1 || port == 2) {
        const Settings::Device device = state.portDevice(port);
        const size_t offset = (port == 1) ? 0 : portPayloadSize(state.portDevice(1));
        if(isPad8Device(device)) return {offset, PortButtonsKind::Pad8, true};
        if(isPad12Device(device)) return {offset, PortButtonsKind::Pad12, true};
        if(isVirtualBoyDevice(device)) return {offset, PortButtonsKind::VirtualBoy, true};
        return {};
    }

    if(port == 3 && state.topology.expansionDevice == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM) {
        return {expansionPayloadOffset(state), PortButtonsKind::Pad8, true};
    }

    return {};
}

FieldLocation locateZapper(const InputState& state, int port)
{
    if(port == 1 && state.portDevice(1) == Settings::Device::ZAPPER) return {0, true};
    if(port == 2 && state.portDevice(2) == Settings::Device::ZAPPER) return {portPayloadSize(state.portDevice(1)), true};
    return {};
}

FieldLocation locateArkanoidController(const InputState& state, int port)
{
    if(port == 1 && state.portDevice(1) == Settings::Device::ARKANOID_CONTROLLER) return {0, true};
    if(port == 2 && state.portDevice(2) == Settings::Device::ARKANOID_CONTROLLER) return {portPayloadSize(state.portDevice(1)), true};
    return {};
}

FieldLocation locateRelativePointer(const InputState& state, int port, Settings::Device device)
{
    if(port == 1 && state.portDevice(1) == device) return {0, true};
    if(port == 2 && state.portDevice(2) == device) return {portPayloadSize(state.portDevice(1)), true};
    return {};
}

FieldLocation locatePowerPad(const InputState& state, int port)
{
    if(port == 1 &&
       (state.portDevice(1) == Settings::Device::POWER_PAD_SIDE_A ||
        state.portDevice(1) == Settings::Device::POWER_PAD_SIDE_B)) {
        return {0, true};
    }
    if(port == 2 &&
       (state.portDevice(2) == Settings::Device::POWER_PAD_SIDE_A ||
        state.portDevice(2) == Settings::Device::POWER_PAD_SIDE_B)) {
        return {portPayloadSize(state.portDevice(1)), true};
    }
    if(port == 1 &&
       (state.topology.expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_A ||
        state.topology.expansionDevice == Settings::ExpansionDevice::FAMILY_TRAINER_SIDE_B)) {
        return {expansionPayloadOffset(state), true};
    }
    return {};
}

FieldLocation locateBandaiButtons(const InputState& state)
{
    if(state.topology.expansionDevice != Settings::ExpansionDevice::BANDAI_HYPERSHOT) {
        return {};
    }
    return {expansionPayloadOffset(state), true};
}

FieldLocation locateBandaiPointer(const InputState& state)
{
    if(state.topology.expansionDevice != Settings::ExpansionDevice::BANDAI_HYPERSHOT) {
        return {};
    }
    return {expansionPayloadOffset(state) + kPad8SerializedSize, true};
}

FieldLocation locateArkanoidExpansion(const InputState& state)
{
    if(state.topology.expansionDevice != Settings::ExpansionDevice::ARKANOID_CONTROLLER) {
        return {};
    }
    return {expansionPayloadOffset(state), true};
}

FieldLocation locateKonamiHyperShot(const InputState& state)
{
    if(state.topology.expansionDevice != Settings::ExpansionDevice::KONAMI_HYPERSHOT) {
        return {};
    }
    return {expansionPayloadOffset(state), true};
}

FieldLocation locateSuborKeyboard(const InputState& state)
{
    if(state.topology.expansionDevice != Settings::ExpansionDevice::SUBOR_KEYBOARD) {
        return {};
    }
    return {expansionPayloadOffset(state), true};
}

FieldLocation locateFamilyBasicKeyboard(const InputState& state)
{
    if(state.topology.expansionDevice != Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD) {
        return {};
    }
    return {expansionPayloadOffset(state), true};
}

InputState::PadButtons readPadButtonsAt(const std::vector<uint8_t>& data, const PortButtonsLocation& location)
{
    InputState::PadButtons buttons;
    if(!location.valid) {
        return buttons;
    }

    size_t offset = location.offset;
    readScalarAt(data, offset, buttons.a); offset += sizeof(buttons.a);
    readScalarAt(data, offset, buttons.b); offset += sizeof(buttons.b);
    readScalarAt(data, offset, buttons.select); offset += sizeof(buttons.select);
    readScalarAt(data, offset, buttons.start); offset += sizeof(buttons.start);
    readScalarAt(data, offset, buttons.up); offset += sizeof(buttons.up);
    readScalarAt(data, offset, buttons.down); offset += sizeof(buttons.down);
    readScalarAt(data, offset, buttons.left); offset += sizeof(buttons.left);
    readScalarAt(data, offset, buttons.right); offset += sizeof(buttons.right);

    if(location.kind == PortButtonsKind::Pad12) {
        readScalarAt(data, offset, buttons.x); offset += sizeof(buttons.x);
        readScalarAt(data, offset, buttons.y); offset += sizeof(buttons.y);
        readScalarAt(data, offset, buttons.l); offset += sizeof(buttons.l);
        readScalarAt(data, offset, buttons.r);
    } else if(location.kind == PortButtonsKind::VirtualBoy) {
        readScalarAt(data, offset, buttons.up2); offset += sizeof(buttons.up2);
        readScalarAt(data, offset, buttons.down2); offset += sizeof(buttons.down2);
        readScalarAt(data, offset, buttons.left2); offset += sizeof(buttons.left2);
        readScalarAt(data, offset, buttons.right2); offset += sizeof(buttons.right2);
        readScalarAt(data, offset, buttons.l); offset += sizeof(buttons.l);
        readScalarAt(data, offset, buttons.r);
    }

    return buttons;
}

void writePadButtonsAt(std::vector<uint8_t>& data, const PortButtonsLocation& location, const InputState::PadButtons& buttons)
{
    if(!location.valid) {
        return;
    }

    size_t offset = location.offset;
    writeScalarAt(data, offset, buttons.a); offset += sizeof(buttons.a);
    writeScalarAt(data, offset, buttons.b); offset += sizeof(buttons.b);
    writeScalarAt(data, offset, buttons.select); offset += sizeof(buttons.select);
    writeScalarAt(data, offset, buttons.start); offset += sizeof(buttons.start);
    writeScalarAt(data, offset, buttons.up); offset += sizeof(buttons.up);
    writeScalarAt(data, offset, buttons.down); offset += sizeof(buttons.down);
    writeScalarAt(data, offset, buttons.left); offset += sizeof(buttons.left);
    writeScalarAt(data, offset, buttons.right); offset += sizeof(buttons.right);

    if(location.kind == PortButtonsKind::Pad12) {
        writeScalarAt(data, offset, buttons.x); offset += sizeof(buttons.x);
        writeScalarAt(data, offset, buttons.y); offset += sizeof(buttons.y);
        writeScalarAt(data, offset, buttons.l); offset += sizeof(buttons.l);
        writeScalarAt(data, offset, buttons.r);
    } else if(location.kind == PortButtonsKind::VirtualBoy) {
        writeScalarAt(data, offset, buttons.up2); offset += sizeof(buttons.up2);
        writeScalarAt(data, offset, buttons.down2); offset += sizeof(buttons.down2);
        writeScalarAt(data, offset, buttons.left2); offset += sizeof(buttons.left2);
        writeScalarAt(data, offset, buttons.right2); offset += sizeof(buttons.right2);
        writeScalarAt(data, offset, buttons.l); offset += sizeof(buttons.l);
        writeScalarAt(data, offset, buttons.r);
    }
}

}

inline InputState::PadButtons InputState::portButtons(int port) const
{
    const PortButtonsLocation location = locatePortButtons(*this, port);
    if(!location.valid || !hasCanonicalSerializedInputData(*this)) {
        return {};
    }
    return readPadButtonsAt(serializedInputData, location);
}

inline void InputState::setPortButtons(int port, const PadButtons& buttons)
{
    const PortButtonsLocation location = locatePortButtons(*this, port);
    if(!location.valid) {
        return;
    }
    ensureCanonicalSerializedInputData(*this);
    writePadButtonsAt(serializedInputData, location, buttons);
}

inline InputState::PadButtons InputState::bandaiButtons() const
{
    const FieldLocation location = locateBandaiButtons(*this);
    if(!location.valid || !hasCanonicalSerializedInputData(*this)) {
        return {};
    }
    return readPadButtonsAt(serializedInputData, {location.offset, PortButtonsKind::Pad8, true});
}

inline void InputState::setBandaiButtons(const PadButtons& buttons)
{
    const FieldLocation location = locateBandaiButtons(*this);
    if(!location.valid) {
        return;
    }
    ensureCanonicalSerializedInputData(*this);
    writePadButtonsAt(serializedInputData, {location.offset, PortButtonsKind::Pad8, true}, buttons);
}

inline InputState::PointerState InputState::zapper(int port) const
{
    const FieldLocation location = locateZapper(*this, port);
    if(!location.valid || !hasCanonicalSerializedInputData(*this)) {
        return {};
    }
    PointerState state;
    state.x = -1;
    state.y = -1;
    readScalarAt(serializedInputData, location.offset, state.x);
    readScalarAt(serializedInputData, location.offset + sizeof(state.x), state.y);
    readScalarAt(serializedInputData, location.offset + sizeof(state.x) + sizeof(state.y), state.trigger);
    return state;
}

inline void InputState::setZapper(int port, const PointerState& state)
{
    const FieldLocation location = locateZapper(*this, port);
    if(!location.valid) {
        return;
    }
    ensureCanonicalSerializedInputData(*this);
    writeScalarAt(serializedInputData, location.offset, state.x);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.x), state.y);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.x) + sizeof(state.y), state.trigger);
}

inline InputState::PointerState InputState::bandaiPointer() const
{
    const FieldLocation location = locateBandaiPointer(*this);
    if(!location.valid || !hasCanonicalSerializedInputData(*this)) {
        return {};
    }
    PointerState state;
    state.x = -1;
    state.y = -1;
    readScalarAt(serializedInputData, location.offset, state.x);
    readScalarAt(serializedInputData, location.offset + sizeof(state.x), state.y);
    readScalarAt(serializedInputData, location.offset + sizeof(state.x) + sizeof(state.y), state.trigger);
    return state;
}

inline void InputState::setBandaiPointer(const PointerState& state)
{
    const FieldLocation location = locateBandaiPointer(*this);
    if(!location.valid) {
        return;
    }
    ensureCanonicalSerializedInputData(*this);
    writeScalarAt(serializedInputData, location.offset, state.x);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.x), state.y);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.x) + sizeof(state.y), state.trigger);
}

inline InputState::ArkanoidState InputState::arkanoidController(int port) const
{
    const FieldLocation location = locateArkanoidController(*this, port);
    if(!location.valid || !hasCanonicalSerializedInputData(*this)) {
        return {};
    }
    ArkanoidState state;
    readScalarAt(serializedInputData, location.offset, state.position);
    readScalarAt(serializedInputData, location.offset + sizeof(state.position), state.button);
    return state;
}

inline void InputState::setArkanoidController(int port, const ArkanoidState& state)
{
    const FieldLocation location = locateArkanoidController(*this, port);
    if(!location.valid) {
        return;
    }
    ensureCanonicalSerializedInputData(*this);
    writeScalarAt(serializedInputData, location.offset, state.position);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.position), state.button);
}

inline InputState::ArkanoidState InputState::arkanoidExpansion() const
{
    const FieldLocation location = locateArkanoidExpansion(*this);
    if(!location.valid || !hasCanonicalSerializedInputData(*this)) {
        return {};
    }
    ArkanoidState state;
    readScalarAt(serializedInputData, location.offset, state.position);
    readScalarAt(serializedInputData, location.offset + sizeof(state.position), state.button);
    return state;
}

inline void InputState::setArkanoidExpansion(const ArkanoidState& state)
{
    const FieldLocation location = locateArkanoidExpansion(*this);
    if(!location.valid) {
        return;
    }
    ensureCanonicalSerializedInputData(*this);
    writeScalarAt(serializedInputData, location.offset, state.position);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.position), state.button);
}

inline InputState::RelativePointerState InputState::snesMouse(int port) const
{
    const FieldLocation location = locateRelativePointer(*this, port, Settings::Device::SNES_MOUSE);
    if(!location.valid || !hasCanonicalSerializedInputData(*this)) {
        return {};
    }
    RelativePointerState state;
    readScalarAt(serializedInputData, location.offset, state.deltaX);
    readScalarAt(serializedInputData, location.offset + sizeof(state.deltaX), state.deltaY);
    readScalarAt(serializedInputData, location.offset + sizeof(state.deltaX) + sizeof(state.deltaY), state.primary);
    readScalarAt(serializedInputData, location.offset + sizeof(state.deltaX) + sizeof(state.deltaY) + sizeof(state.primary), state.secondary);
    return state;
}

inline void InputState::setSnesMouse(int port, const RelativePointerState& state)
{
    const FieldLocation location = locateRelativePointer(*this, port, Settings::Device::SNES_MOUSE);
    if(!location.valid) {
        return;
    }
    ensureCanonicalSerializedInputData(*this);
    writeScalarAt(serializedInputData, location.offset, state.deltaX);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.deltaX), state.deltaY);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.deltaX) + sizeof(state.deltaY), state.primary);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.deltaX) + sizeof(state.deltaY) + sizeof(state.primary), state.secondary);
}

inline InputState::RelativePointerState InputState::suborMouse(int port) const
{
    const FieldLocation location = locateRelativePointer(*this, port, Settings::Device::SUBOR_MOUSE);
    if(!location.valid || !hasCanonicalSerializedInputData(*this)) {
        return {};
    }
    RelativePointerState state;
    readScalarAt(serializedInputData, location.offset, state.deltaX);
    readScalarAt(serializedInputData, location.offset + sizeof(state.deltaX), state.deltaY);
    readScalarAt(serializedInputData, location.offset + sizeof(state.deltaX) + sizeof(state.deltaY), state.primary);
    readScalarAt(serializedInputData, location.offset + sizeof(state.deltaX) + sizeof(state.deltaY) + sizeof(state.primary), state.secondary);
    return state;
}

inline void InputState::setSuborMouse(int port, const RelativePointerState& state)
{
    const FieldLocation location = locateRelativePointer(*this, port, Settings::Device::SUBOR_MOUSE);
    if(!location.valid) {
        return;
    }
    ensureCanonicalSerializedInputData(*this);
    writeScalarAt(serializedInputData, location.offset, state.deltaX);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.deltaX), state.deltaY);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.deltaX) + sizeof(state.deltaY), state.primary);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.deltaX) + sizeof(state.deltaY) + sizeof(state.primary), state.secondary);
}

inline InputState::KonamiHyperShotState InputState::konamiHyperShot() const
{
    const FieldLocation location = locateKonamiHyperShot(*this);
    if(!location.valid || !hasCanonicalSerializedInputData(*this)) {
        return {};
    }
    KonamiHyperShotState state;
    readScalarAt(serializedInputData, location.offset, state.p1Run);
    readScalarAt(serializedInputData, location.offset + sizeof(state.p1Run), state.p1Jump);
    readScalarAt(serializedInputData, location.offset + sizeof(state.p1Run) + sizeof(state.p1Jump), state.p2Run);
    readScalarAt(serializedInputData, location.offset + sizeof(state.p1Run) + sizeof(state.p1Jump) + sizeof(state.p2Run), state.p2Jump);
    return state;
}

inline void InputState::setKonamiHyperShot(const KonamiHyperShotState& state)
{
    const FieldLocation location = locateKonamiHyperShot(*this);
    if(!location.valid) {
        return;
    }
    ensureCanonicalSerializedInputData(*this);
    writeScalarAt(serializedInputData, location.offset, state.p1Run);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.p1Run), state.p1Jump);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.p1Run) + sizeof(state.p1Jump), state.p2Run);
    writeScalarAt(serializedInputData, location.offset + sizeof(state.p1Run) + sizeof(state.p1Jump) + sizeof(state.p2Run), state.p2Jump);
}

inline std::array<bool, 12> InputState::powerPadButtons(int port) const
{
    const FieldLocation location = locatePowerPad(*this, port);
    std::array<bool, 12> buttons = {};
    if(!location.valid || !hasCanonicalSerializedInputData(*this)) {
        return buttons;
    }
    (void)readBoolArrayAt(serializedInputData, location.offset, buttons);
    return buttons;
}

inline void InputState::setPowerPadButtons(int port, const std::array<bool, 12>& buttons)
{
    const FieldLocation location = locatePowerPad(*this, port);
    if(!location.valid) {
        return;
    }
    ensureCanonicalSerializedInputData(*this);
    writeBoolArrayAt(serializedInputData, location.offset, buttons);
}

inline IExpansionDevice::SuborKeyboardKeys InputState::suborKeyboardKeys() const
{
    const FieldLocation location = locateSuborKeyboard(*this);
    IExpansionDevice::SuborKeyboardKeys keys = {};
    if(!location.valid || !hasCanonicalSerializedInputData(*this)) {
        return keys;
    }
    (void)readBoolArrayAt(serializedInputData, location.offset, keys);
    return keys;
}

inline void InputState::setSuborKeyboardKeys(const IExpansionDevice::SuborKeyboardKeys& keys)
{
    const FieldLocation location = locateSuborKeyboard(*this);
    if(!location.valid) {
        return;
    }
    ensureCanonicalSerializedInputData(*this);
    writeBoolArrayAt(serializedInputData, location.offset, keys);
}

inline IExpansionDevice::FamilyBasicKeyboardKeys InputState::familyBasicKeyboardKeys() const
{
    const FieldLocation location = locateFamilyBasicKeyboard(*this);
    IExpansionDevice::FamilyBasicKeyboardKeys keys = {};
    if(!location.valid || !hasCanonicalSerializedInputData(*this)) {
        return keys;
    }
    (void)readBoolArrayAt(serializedInputData, location.offset, keys);
    return keys;
}

inline void InputState::setFamilyBasicKeyboardKeys(const IExpansionDevice::FamilyBasicKeyboardKeys& keys)
{
    const FieldLocation location = locateFamilyBasicKeyboard(*this);
    if(!location.valid) {
        return;
    }
    ensureCanonicalSerializedInputData(*this);
    writeBoolArrayAt(serializedInputData, location.offset, keys);
}
