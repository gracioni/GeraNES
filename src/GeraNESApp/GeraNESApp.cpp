#include "GeraNESApp/GeraNESApp.h"

#include "GeraNESNetplay/NetplayWindowUI.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cctype>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <sstream>

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#endif

namespace {
#ifdef __EMSCRIPTEN__
constexpr Uint64 kWebMainLoopCounterFrequency = 1000000u;

Uint64 currentMainLoopCounter()
{
    return static_cast<Uint64>(emscripten_get_now() * 1000.0);
}

Uint64 currentMainLoopCounterFrequency()
{
    return kWebMainLoopCounterFrequency;
}
#else
Uint64 currentMainLoopCounter()
{
    return SDL_GetPerformanceCounter();
}

Uint64 currentMainLoopCounterFrequency()
{
    return SDL_GetPerformanceFrequency();
}
#endif
}

void GeraNESApp::updateMVP()
{
    glm::mat4 proj = glm::ortho(0.0f, static_cast<float>(this->width()), static_cast<float>(this->height()), 0.0f, -1.0f, 1.0f);
    m_mvp = proj * glm::mat4(1.0f);
}

bool GeraNESApp::useCustomWindowChrome()
{
#ifdef __EMSCRIPTEN__
    return false;
#else
    return m_customWindowChromeEnabled && !isFullScreen();
#endif
}

float GeraNESApp::customTitleBarHeight()
{
    return useCustomWindowChrome() ? 44.0f : 0.0f;
}

float GeraNESApp::customSideFrameWidth()
{
    return 0.0f;
}

float GeraNESApp::customBottomFrameHeight()
{
    return 0.0f;
}

float GeraNESApp::customContentTopPadding()
{
    return 0.0f;
}

SDL_Rect GeraNESApp::emulatorClientArea()
{
    const int side = static_cast<int>(std::round(customSideFrameWidth()));
    const int top = static_cast<int>(std::round(customTitleBarHeight() + static_cast<float>(m_menuBarHeight) + customContentTopPadding()));
    const int bottom = static_cast<int>(std::round(customBottomFrameHeight()));
    return SDL_Rect{
        side,
        top,
        std::max(0, width() - side * 2),
        std::max(0, height() - top - bottom)
    };
}

void GeraNESApp::onLog(const std::string& msg, Logger::Type type)
{
    const std::string safeMsg = msg.empty() ? "Unknown error." : msg;

    if(type == Logger::Type::USER) {
        m_userToast.show(safeMsg);
        return;
    }

    std::ofstream file(LOG_FILE, std::ios_base::app);
    file << safeMsg << std::endl;
    std::cout << safeMsg << std::endl;

    if(type == Logger::Type::ERROR) {
        m_errorMessage = safeMsg;
        m_showErrorWindow = true;

        if(!m_emu.valid()) {
            if(m_defaultCursor.has_value()) m_defaultCursor->setAsCurrent();
        }
    }

    std::string msgType = "";
    switch(type) {
        case Logger::Type::INFO: msgType = "[Info] "; break;
        case Logger::Type::WARNING: msgType = "[Warning] "; break;
        case Logger::Type::ERROR: msgType = "[Error] "; break;
        case Logger::Type::DEBUG: msgType = "[Debug] "; break;
        case Logger::Type::USER: break;
    }

    m_log += msgType + safeMsg + "\n";

    const size_t needed = m_log.size() + 1;
    if(m_logBuf.capacity() < needed) {
        m_logBuf.reserve(std::max(needed, m_logBuf.capacity() * 2));
    }

    m_logBuf.resize(needed);
    memcpy(m_logBuf.data(), m_log.c_str(), needed);
}

bool GeraNESApp::isNetplayClientRestricted() const
{
    const auto snapshot = GeraNESNetplay::menuSnapshot(m_netplayRuntime);
    return snapshot.inputManaged && !snapshot.hosting;
}

bool GeraNESApp::isNetplayRomChangeRestricted() const
{
    const auto snapshot = m_netplayRuntime.uiSnapshot();
    return snapshot.hosting || snapshot.connected || snapshot.reconnecting;
}

void GeraNESApp::notifyNetplayClientRestrictedAction(const char* action)
{
    if(!isNetplayClientRestricted()) return;

    const std::string message = std::string(action) + " is disabled while connected as a netplay client";
    m_userToast.show(message);
    m_netplayRuntime.appendNetplayLog(message);
}

void GeraNESApp::notifyNetplayRomChangeRestrictedAction(const char* action)
{
    if(!isNetplayRomChangeRestricted()) return;

    const std::string message = std::string(action) + " is disabled while netplay is active";
    m_userToast.show(message);
    m_netplayRuntime.appendNetplayLog(message);
}

bool GeraNESApp::canUseNetplaySessionPause() const
{
    const auto snapshot = m_netplayRuntime.uiSnapshot();
    return snapshot.active &&
           snapshot.hosting &&
           (snapshot.room.state == ConsoleNetplay::SessionState::Running ||
            snapshot.room.state == ConsoleNetplay::SessionState::Paused);
}

bool GeraNESApp::isNetplayPauseRestricted() const
{
    const auto snapshot = m_netplayRuntime.uiSnapshot();
    if(!(snapshot.active || snapshot.connected || snapshot.reconnecting || snapshot.hosting)) {
        return false;
    }
    return !canUseNetplaySessionPause();
}

void GeraNESApp::notifyNetplayPauseRestrictedAction()
{
    if(!isNetplayPauseRestricted()) return;

    const auto snapshot = m_netplayRuntime.uiSnapshot();
    const std::string message =
        snapshot.active && !snapshot.hosting
            ? "Only the owner can pause the netplay session"
            : "Pause is unavailable in the current netplay state";
    m_userToast.show(message);
    m_netplayRuntime.appendNetplayLog(message);
}

void GeraNESApp::togglePauseAction()
{
    if(!m_emu.valid()) return;
    if(AppSettings::instance().data.debug.cpuDebuggerEnabled) {
        return;
    }
    if(canUseNetplaySessionPause()) {
        m_netplayRuntime.toggleHostedSessionPause();
        return;
    }
    if(isNetplayPauseRestricted()) {
        notifyNetplayPauseRestrictedAction();
        return;
    }
    m_emu.togglePaused();
}

void GeraNESApp::resetAction()
{
    if(!m_emu.valid()) return;
    if(isNetplayClientRestricted()) {
        notifyNetplayClientRestrictedAction("Reset");
        return;
    }
    m_emu.reset();
}

void GeraNESApp::closeRomAction()
{
    if(!m_emu.valid()) return;
    if(isNetplayRomChangeRestricted()) {
        notifyNetplayRomChangeRestrictedAction("Close ROM");
        return;
    }

    m_emu.closeRom();
    setTitle("GeraNES");
    m_framebufferUploadCopy.assign(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT, 0u);
    m_textureUploadBuffer.assign(256u * 256u, 0u);

    if(m_texture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, m_textureUploadBuffer.data());
    }
}

bool GeraNESApp::shouldSuppressRewindForNetplay() const
{
    const auto snapshot = m_netplayRuntime.uiSnapshot();
    return snapshot.active || snapshot.hosting || snapshot.connected || snapshot.reconnecting;
}

void GeraNESApp::applyEffectiveRewindSettings()
{
    static int lastAppliedEffectiveMaxRewindTime = -1;
    const int effectiveMaxRewindTime = shouldSuppressRewindForNetplay()
        ? 0
        : std::max(0, AppSettings::instance().data.improvements.maxRewindTime);
    if(effectiveMaxRewindTime == lastAppliedEffectiveMaxRewindTime) {
        return;
    }

    lastAppliedEffectiveMaxRewindTime = effectiveMaxRewindTime;
    m_emu.setupRewindSystem(effectiveMaxRewindTime > 0, effectiveMaxRewindTime);
}

bool GeraNESApp::isTouchCompatibleControllerDevice(const std::optional<Settings::Device>& device)
{
    return device == std::optional<Settings::Device>(Settings::Device::CONTROLLER) ||
           device == std::optional<Settings::Device>(Settings::Device::FAMICOM_CONTROLLER) ||
           device == std::optional<Settings::Device>(Settings::Device::SNES_CONTROLLER);
}

const char* GeraNESApp::touchDeviceLabel(Settings::Device device)
{
    switch(device) {
        case Settings::Device::CONTROLLER: return "Standard Controller";
        case Settings::Device::FAMICOM_CONTROLLER: return "Famicom Controller";
        case Settings::Device::SNES_CONTROLLER: return "SNES Controller";
        default: return "Unsupported";
    }
}

bool GeraNESApp::isTouchCompatibleExpansionDevice(Settings::ExpansionDevice device)
{
    return device == Settings::ExpansionDevice::STANDARD_CONTROLLER_FAMICOM;
}

std::string GeraNESApp::touchTargetMenuLabel(AppSettings::TouchControlsTarget target)
{
    switch(target) {
        case AppSettings::TouchControlsTarget::Port1Controller:
            return "Port 1";
        case AppSettings::TouchControlsTarget::Port2Controller:
            return "Port 2";
        case AppSettings::TouchControlsTarget::Expansion:
            return "Expansion";
        case AppSettings::TouchControlsTarget::MultitapP1:
            return "P1";
        case AppSettings::TouchControlsTarget::MultitapP2:
            return "P2";
        case AppSettings::TouchControlsTarget::MultitapP3:
            return "P3";
        case AppSettings::TouchControlsTarget::MultitapP4:
            return "P4";
    }

    return "Unavailable";
}

AppSettings::TouchControlsTarget GeraNESApp::preferredTouchTargetForTopology(
    const IEmulationHost::InputTopologySnapshot& topology)
{
    const bool multitapActive =
        topology.nesMultitapDevice == Settings::NesMultitapDevice::FOUR_SCORE ||
        topology.famicomMultitapDevice == Settings::FamicomMultitapDevice::HORI_ADAPTER;
    if(multitapActive) {
        return AppSettings::TouchControlsTarget::MultitapP1;
    }
    if(isTouchCompatibleExpansionDevice(topology.expansionDevice)) {
        return AppSettings::TouchControlsTarget::Expansion;
    }
    if(isTouchCompatibleControllerDevice(topology.port1Device)) {
        return AppSettings::TouchControlsTarget::Port1Controller;
    }
    if(isTouchCompatibleControllerDevice(topology.port2Device)) {
        return AppSettings::TouchControlsTarget::Port2Controller;
    }
    return AppSettings::TouchControlsTarget::Port1Controller;
}

namespace {

std::optional<AppSettings::TouchControlsTarget> touchTargetForNetplaySlot(ConsoleNetplay::PlayerSlot slot);
bool touchTargetMatchesNetplayAssignment(AppSettings::TouchControlsTarget target,
                                         ConsoleNetplay::PlayerSlot slot);

} // namespace

void GeraNESApp::normalizeTouchControlsTargetForCurrentTopology()
{
    AppSettings::instance().data.input.touchControls.target =
        preferredTouchTargetForTopology(m_emu.getInputTopologySnapshot());
}

AppSettings::TouchControlsTarget GeraNESApp::effectiveTouchControlsTarget() const
{
    AppSettings::TouchControlsTarget touchTarget =
        AppSettings::instance().data.input.touchControls.target;
    const auto netplaySnapshot = m_netplayRuntime.uiSnapshot();
    if(!netplaySnapshot.active) {
        return touchTarget;
    }

    for(const auto& participant : netplaySnapshot.room.participants) {
        if(participant.id != netplaySnapshot.localParticipantId ||
           participant.controllerAssignments.empty()) {
            continue;
        }
        const bool targetMatchesLocalAssignment =
            std::any_of(
                participant.controllerAssignments.begin(),
                participant.controllerAssignments.end(),
                [touchTarget](ConsoleNetplay::PlayerSlot slot) {
                    return touchTargetMatchesNetplayAssignment(touchTarget, slot);
                }
            );
        if(targetMatchesLocalAssignment) {
            break;
        }
        const auto preferredTarget =
            touchTargetForNetplaySlot(participant.controllerAssignments.front());
        if(preferredTarget.has_value()) {
            touchTarget = *preferredTarget;
        }
        break;
    }

    return touchTarget;
}

namespace {

std::optional<AppSettings::TouchControlsTarget> touchTargetForNetplaySlot(ConsoleNetplay::PlayerSlot slot)
{
    switch(slot) {
        case GeraNESNetplay::kPort1PlayerSlot: return AppSettings::TouchControlsTarget::Port1Controller;
        case GeraNESNetplay::kPort2PlayerSlot: return AppSettings::TouchControlsTarget::Port2Controller;
        case GeraNESNetplay::kExpansionPlayerSlot: return AppSettings::TouchControlsTarget::Expansion;
        case GeraNESNetplay::kMultitapP1PlayerSlot: return AppSettings::TouchControlsTarget::MultitapP1;
        case GeraNESNetplay::kMultitapP2PlayerSlot: return AppSettings::TouchControlsTarget::MultitapP2;
        case GeraNESNetplay::kMultitapP3PlayerSlot: return AppSettings::TouchControlsTarget::MultitapP3;
        case GeraNESNetplay::kMultitapP4PlayerSlot: return AppSettings::TouchControlsTarget::MultitapP4;
        default: return std::nullopt;
    }
}

bool touchTargetMatchesNetplayAssignment(AppSettings::TouchControlsTarget target,
                                         ConsoleNetplay::PlayerSlot slot)
{
    const auto expected = touchTargetForNetplaySlot(slot);
    return expected.has_value() && *expected == target;
}

} // namespace

void GeraNESApp::setIfNegative(std::string& dst, int value)
{
    dst = value >= 0 ? std::to_string(value) : "";
}

void GeraNESApp::setIfNegativeKb(std::string& dst, int bytesValue)
{
    dst = bytesValue >= 0 ? std::to_string(bytesValue / 1024) : "";
}

void GeraNESApp::loadRomDatabaseEditorFromCurrentRom()
{
    m_romDbEditor = RomDatabaseEditorData();
    m_romDbSaved = RomDatabaseEditorData();

    if(!m_emu.valid()) {
        m_romDbEditor.loaded = false;
        m_romDbEditor.statusMessage = "No ROM loaded";
        return;
    }

    m_emu.withExclusiveAccess([&](auto& emu) {
        Cartridge& cart = emu.getConsole().cartridge();
        m_romDbEditor.loaded = true;
        m_romDbEditor.PrgChrCrc32 = cart.prgChrCrc32String();

        GameDatabase::Item* item = GameDatabase::instance().findByCrc(m_romDbEditor.PrgChrCrc32);
        if(item != nullptr) {
            m_romDbEditor.foundInDatabase = true;
            m_romDbEditor.statusMessage = "Current ROM is in the database";
            GameDatabase::RawItem raw = GameDatabase::toRawItem(*item);
            m_romDbEditor.PrgChrCrc32 = raw.PrgChrCrc32;
            m_romDbEditor.System = raw.System;
            m_romDbEditor.Board = raw.Board;
            m_romDbEditor.PCB = raw.PCB;
            m_romDbEditor.Chip = raw.Chip;
            m_romDbEditor.Mapper = raw.Mapper;
            m_romDbEditor.PrgRomSize = raw.PrgRomSize;
            m_romDbEditor.ChrRomSize = raw.ChrRomSize;
            m_romDbEditor.ChrRamSize = raw.ChrRamSize;
            m_romDbEditor.WorkRamSize = raw.WorkRamSize;
            m_romDbEditor.SaveRamSize = raw.SaveRamSize;
            m_romDbEditor.HasBattery = raw.HasBattery;
            m_romDbEditor.Mirroring = raw.Mirroring;
            m_romDbEditor.InputType = raw.InputType;
            m_romDbEditor.BusConflicts = raw.BusConflicts;
            m_romDbEditor.SubMapperId = raw.SubMapperId;
            m_romDbEditor.VsSystemType = raw.VsSystemType;
            m_romDbEditor.VsPpuModel = raw.VsPpuModel;
            m_romDbSaved = m_romDbEditor;
            return;
        }

        m_romDbEditor.foundInDatabase = false;
        m_romDbEditor.statusMessage = "Current ROM is NOT in the database";
        switch(cart.system()) {
            case GameDatabase::System::NesNtsc: m_romDbEditor.System = "NesNtsc"; break;
            case GameDatabase::System::NesPal: m_romDbEditor.System = "NesPal"; break;
            case GameDatabase::System::Famicom: m_romDbEditor.System = "Famicom"; break;
            case GameDatabase::System::Dendy: m_romDbEditor.System = "Dendy"; break;
            case GameDatabase::System::VsSystem: m_romDbEditor.System = "VsSystem"; break;
            case GameDatabase::System::Playchoice: m_romDbEditor.System = "Playchoice"; break;
            case GameDatabase::System::FDS: m_romDbEditor.System = "FDS"; break;
            default: m_romDbEditor.System = ""; break;
        }

        m_romDbEditor.Board = "";
        m_romDbEditor.PCB = "";
        m_romDbEditor.Chip = cart.chip();
        setIfNegative(m_romDbEditor.Mapper, cart.mapperId());
        setIfNegativeKb(m_romDbEditor.PrgRomSize, cart.prgSize());
        setIfNegativeKb(m_romDbEditor.ChrRomSize, cart.chrSize());
        setIfNegativeKb(m_romDbEditor.ChrRamSize, cart.chrRamSize());
        setIfNegativeKb(m_romDbEditor.WorkRamSize, cart.ramSize());
        setIfNegativeKb(m_romDbEditor.SaveRamSize, cart.dbSaveRamSize());
        m_romDbEditor.HasBattery = cart.hasBattery() ? "1" : "0";
        m_romDbEditor.Mirroring = "";
        m_romDbEditor.InputType = std::to_string(static_cast<int>(cart.inputType()));
        m_romDbEditor.BusConflicts = "";
        setIfNegative(m_romDbEditor.SubMapperId, cart.subMapperId());
        m_romDbEditor.VsSystemType = "0";
        m_romDbEditor.VsPpuModel = std::to_string(static_cast<int>(cart.vsPpuModel()));
    });
}

void GeraNESApp::saveRomDatabaseEditor()
{
    if(!m_romDbEditor.loaded) return;

    GameDatabase::RawItem raw;
    raw.PrgChrCrc32 = m_romDbEditor.PrgChrCrc32;
    raw.System = m_romDbEditor.System;
    raw.Board = m_romDbEditor.Board;
    raw.PCB = m_romDbEditor.PCB;
    raw.Chip = m_romDbEditor.Chip;
    raw.Mapper = m_romDbEditor.Mapper;
    raw.PrgRomSize = m_romDbEditor.PrgRomSize;
    raw.ChrRomSize = m_romDbEditor.ChrRomSize;
    raw.ChrRamSize = m_romDbEditor.ChrRamSize;
    raw.WorkRamSize = m_romDbEditor.WorkRamSize;
    raw.SaveRamSize = m_romDbEditor.SaveRamSize;
    raw.HasBattery = m_romDbEditor.HasBattery;
    raw.Mirroring = m_romDbEditor.Mirroring;
    raw.InputType = m_romDbEditor.InputType;
    raw.BusConflicts = m_romDbEditor.BusConflicts;
    raw.SubMapperId = m_romDbEditor.SubMapperId;
    raw.VsSystemType = m_romDbEditor.VsSystemType;
    raw.VsPpuModel = m_romDbEditor.VsPpuModel;

    std::string error;
    if(GameDatabase::instance().upsertRawItem(raw, &error)) {
        Logger::instance().log("ROM database entry saved", Logger::Type::USER);
        loadRomDatabaseEditorFromCurrentRom();
    } else {
        if(error.empty()) error = "Failed to save ROM database entry";
        Logger::instance().log(error, Logger::Type::ERROR);
    }
}

void GeraNESApp::removeRomDatabaseEditor()
{
    if(!m_romDbEditor.loaded || !m_romDbEditor.foundInDatabase) return;

    std::string error;
    if(GameDatabase::instance().removeByCrc(m_romDbEditor.PrgChrCrc32, &error)) {
        Logger::instance().log("ROM database entry removed", Logger::Type::USER);
        loadRomDatabaseEditorFromCurrentRom();
    } else {
        if(error.empty()) error = "Failed to remove ROM database entry";
        Logger::instance().log(error, Logger::Type::ERROR);
    }
}

std::tuple<int, int> GeraNESApp::getNesCursor(int screenX, int screenY)
{
    int nesX = ((screenX - m_nesScreenRect.min.x) * PPU::SCREEN_WIDTH) / m_nesScreenRect.getWidth();
    int clipTop = m_clipHeightValue;
    int visibleNES = PPU::SCREEN_HEIGHT - 2 * m_clipHeightValue;
    int nesY = clipTop + ((screenY - m_nesScreenRect.min.y) * visibleNES) / m_nesScreenRect.getHeight();
    return std::make_tuple(nesX, nesY);
}

std::tuple<int, int> GeraNESApp::getClampedNesCursor(int screenX, int screenY)
{
    const int minX = static_cast<int>(m_nesScreenRect.min.x);
    const int minY = static_cast<int>(m_nesScreenRect.min.y);
    const int maxX = static_cast<int>(m_nesScreenRect.max.x) - 1;
    const int maxY = static_cast<int>(m_nesScreenRect.max.y) - 1;
    const int clampedX = std::clamp(screenX, minX, maxX);
    const int clampedY = std::clamp(screenY, minY, maxY);
    auto [nesX, nesY] = getNesCursor(clampedX, clampedY);
    nesX = std::clamp(nesX, 0, PPU::SCREEN_WIDTH - 1);
    nesY = std::clamp(nesY, m_clipHeightValue, PPU::SCREEN_HEIGHT - m_clipHeightValue - 1);
    return std::make_tuple(nesX, nesY);
}

bool GeraNESApp::isSnesMouseActive() const
{
    return m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::SNES_MOUSE) ||
           m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::SNES_MOUSE);
}

bool GeraNESApp::isSuborMouseActive() const
{
    return m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE) ||
           m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE);
}

bool GeraNESApp::isSuborKeyboardActive() const
{
    return m_emu.getExpansionDevice() == Settings::ExpansionDevice::SUBOR_KEYBOARD;
}

bool GeraNESApp::isFamilyBasicKeyboardActive() const
{
    return m_emu.getExpansionDevice() == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD;
}

IExpansionDevice::SuborKeyboardKeys GeraNESApp::captureSuborKeyboardState()
{
    IExpansionDevice::SuborKeyboardKeys keys = {};
    const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);

    auto pressed = [&](SDL_Scancode scancode) { return keyboardState[scancode] != 0; };
    auto pressedAny = [&](std::initializer_list<SDL_Scancode> scancodes) {
        for(SDL_Scancode scancode : scancodes) {
            if(keyboardState[scancode] != 0) return true;
        }
        return false;
    };
    auto set = [&](SuborKeyboard::Button button, bool value) { keys[static_cast<size_t>(button)] = value; };

    set(SuborKeyboard::Button::A, pressed(SDL_SCANCODE_A));
    set(SuborKeyboard::Button::B, pressed(SDL_SCANCODE_B));
    set(SuborKeyboard::Button::C, pressed(SDL_SCANCODE_C));
    set(SuborKeyboard::Button::D, pressed(SDL_SCANCODE_D));
    set(SuborKeyboard::Button::E, pressed(SDL_SCANCODE_E));
    set(SuborKeyboard::Button::F, pressed(SDL_SCANCODE_F));
    set(SuborKeyboard::Button::G, pressed(SDL_SCANCODE_G));
    set(SuborKeyboard::Button::H, pressed(SDL_SCANCODE_H));
    set(SuborKeyboard::Button::I, pressed(SDL_SCANCODE_I));
    set(SuborKeyboard::Button::J, pressed(SDL_SCANCODE_J));
    set(SuborKeyboard::Button::K, pressed(SDL_SCANCODE_K));
    set(SuborKeyboard::Button::L, pressed(SDL_SCANCODE_L));
    set(SuborKeyboard::Button::M, pressed(SDL_SCANCODE_M));
    set(SuborKeyboard::Button::N, pressed(SDL_SCANCODE_N));
    set(SuborKeyboard::Button::O, pressed(SDL_SCANCODE_O));
    set(SuborKeyboard::Button::P, pressed(SDL_SCANCODE_P));
    set(SuborKeyboard::Button::Q, pressed(SDL_SCANCODE_Q));
    set(SuborKeyboard::Button::R, pressed(SDL_SCANCODE_R));
    set(SuborKeyboard::Button::S, pressed(SDL_SCANCODE_S));
    set(SuborKeyboard::Button::T, pressed(SDL_SCANCODE_T));
    set(SuborKeyboard::Button::U, pressed(SDL_SCANCODE_U));
    set(SuborKeyboard::Button::V, pressed(SDL_SCANCODE_V));
    set(SuborKeyboard::Button::W, pressed(SDL_SCANCODE_W));
    set(SuborKeyboard::Button::X, pressed(SDL_SCANCODE_X));
    set(SuborKeyboard::Button::Y, pressed(SDL_SCANCODE_Y));
    set(SuborKeyboard::Button::Z, pressed(SDL_SCANCODE_Z));
    set(SuborKeyboard::Button::Num0, pressed(SDL_SCANCODE_0));
    set(SuborKeyboard::Button::Num1, pressed(SDL_SCANCODE_1));
    set(SuborKeyboard::Button::Num2, pressed(SDL_SCANCODE_2));
    set(SuborKeyboard::Button::Num3, pressed(SDL_SCANCODE_3));
    set(SuborKeyboard::Button::Num4, pressed(SDL_SCANCODE_4));
    set(SuborKeyboard::Button::Num5, pressed(SDL_SCANCODE_5));
    set(SuborKeyboard::Button::Num6, pressed(SDL_SCANCODE_6));
    set(SuborKeyboard::Button::Num7, pressed(SDL_SCANCODE_7));
    set(SuborKeyboard::Button::Num8, pressed(SDL_SCANCODE_8));
    set(SuborKeyboard::Button::Num9, pressed(SDL_SCANCODE_9));
    set(SuborKeyboard::Button::F1, pressed(SDL_SCANCODE_F1));
    set(SuborKeyboard::Button::F2, pressed(SDL_SCANCODE_F2));
    set(SuborKeyboard::Button::F3, pressed(SDL_SCANCODE_F3));
    set(SuborKeyboard::Button::F4, pressed(SDL_SCANCODE_F4));
    set(SuborKeyboard::Button::F5, pressed(SDL_SCANCODE_F5));
    set(SuborKeyboard::Button::F6, pressed(SDL_SCANCODE_F6));
    set(SuborKeyboard::Button::F7, pressed(SDL_SCANCODE_F7));
    set(SuborKeyboard::Button::F8, pressed(SDL_SCANCODE_F8));
    set(SuborKeyboard::Button::F9, pressed(SDL_SCANCODE_F9));
    set(SuborKeyboard::Button::F10, pressed(SDL_SCANCODE_F10));
    set(SuborKeyboard::Button::F11, pressed(SDL_SCANCODE_F11));
    set(SuborKeyboard::Button::F12, pressed(SDL_SCANCODE_F12));
    set(SuborKeyboard::Button::Numpad0, pressed(SDL_SCANCODE_KP_0));
    set(SuborKeyboard::Button::Numpad1, pressed(SDL_SCANCODE_KP_1));
    set(SuborKeyboard::Button::Numpad2, pressed(SDL_SCANCODE_KP_2));
    set(SuborKeyboard::Button::Numpad3, pressed(SDL_SCANCODE_KP_3));
    set(SuborKeyboard::Button::Numpad4, pressed(SDL_SCANCODE_KP_4));
    set(SuborKeyboard::Button::Numpad5, pressed(SDL_SCANCODE_KP_5));
    set(SuborKeyboard::Button::Numpad6, pressed(SDL_SCANCODE_KP_6));
    set(SuborKeyboard::Button::Numpad7, pressed(SDL_SCANCODE_KP_7));
    set(SuborKeyboard::Button::Numpad8, pressed(SDL_SCANCODE_KP_8));
    set(SuborKeyboard::Button::Numpad9, pressed(SDL_SCANCODE_KP_9));
    set(SuborKeyboard::Button::NumpadEnter, pressed(SDL_SCANCODE_KP_ENTER));
    set(SuborKeyboard::Button::NumpadDot, pressed(SDL_SCANCODE_KP_PERIOD));
    set(SuborKeyboard::Button::NumpadPlus, pressed(SDL_SCANCODE_KP_PLUS));
    set(SuborKeyboard::Button::NumpadMultiply, pressed(SDL_SCANCODE_KP_MULTIPLY));
    set(SuborKeyboard::Button::NumpadDivide, pressed(SDL_SCANCODE_KP_DIVIDE));
    set(SuborKeyboard::Button::NumpadMinus, pressed(SDL_SCANCODE_KP_MINUS));
    set(SuborKeyboard::Button::NumLock, pressed(SDL_SCANCODE_NUMLOCKCLEAR));
    set(SuborKeyboard::Button::Comma, pressed(SDL_SCANCODE_COMMA));
    set(SuborKeyboard::Button::Dot, pressed(SDL_SCANCODE_PERIOD));
    set(SuborKeyboard::Button::SemiColon, pressed(SDL_SCANCODE_SEMICOLON));
    set(SuborKeyboard::Button::Apostrophe, pressed(SDL_SCANCODE_APOSTROPHE));
    set(SuborKeyboard::Button::Slash, pressed(SDL_SCANCODE_SLASH));
    set(SuborKeyboard::Button::Backslash, pressed(SDL_SCANCODE_BACKSLASH));
    set(SuborKeyboard::Button::Equal, pressed(SDL_SCANCODE_EQUALS));
    set(SuborKeyboard::Button::Minus, pressed(SDL_SCANCODE_MINUS));
    set(SuborKeyboard::Button::Grave, pressed(SDL_SCANCODE_GRAVE));
    set(SuborKeyboard::Button::LeftBracket, pressed(SDL_SCANCODE_LEFTBRACKET));
    set(SuborKeyboard::Button::RightBracket, pressed(SDL_SCANCODE_RIGHTBRACKET));
    set(SuborKeyboard::Button::CapsLock, pressed(SDL_SCANCODE_CAPSLOCK));
    set(SuborKeyboard::Button::Pause, pressed(SDL_SCANCODE_PAUSE));
    set(SuborKeyboard::Button::Ctrl, pressedAny({SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL}));
    set(SuborKeyboard::Button::Shift, pressedAny({SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT}));
    set(SuborKeyboard::Button::Alt, pressedAny({SDL_SCANCODE_LALT, SDL_SCANCODE_RALT}));
    set(SuborKeyboard::Button::Space, pressed(SDL_SCANCODE_SPACE));
    set(SuborKeyboard::Button::Backspace, pressed(SDL_SCANCODE_BACKSPACE));
    set(SuborKeyboard::Button::Tab, pressed(SDL_SCANCODE_TAB));
    set(SuborKeyboard::Button::Esc, pressed(SDL_SCANCODE_ESCAPE));
    set(SuborKeyboard::Button::Enter, pressedAny({SDL_SCANCODE_RETURN, SDL_SCANCODE_RETURN2}));
    set(SuborKeyboard::Button::End, pressed(SDL_SCANCODE_END));
    set(SuborKeyboard::Button::Home, pressed(SDL_SCANCODE_HOME));
    set(SuborKeyboard::Button::Ins, pressed(SDL_SCANCODE_INSERT));
    set(SuborKeyboard::Button::Delete, pressed(SDL_SCANCODE_DELETE));
    set(SuborKeyboard::Button::PageUp, pressed(SDL_SCANCODE_PAGEUP));
    set(SuborKeyboard::Button::PageDown, pressed(SDL_SCANCODE_PAGEDOWN));
    set(SuborKeyboard::Button::Up, pressed(SDL_SCANCODE_UP));
    set(SuborKeyboard::Button::Down, pressed(SDL_SCANCODE_DOWN));
    set(SuborKeyboard::Button::Left, pressed(SDL_SCANCODE_LEFT));
    set(SuborKeyboard::Button::Right, pressed(SDL_SCANCODE_RIGHT));
    return keys;
}

IExpansionDevice::FamilyBasicKeyboardKeys GeraNESApp::captureFamilyBasicKeyboardState()
{
    IExpansionDevice::FamilyBasicKeyboardKeys keys = {};
    const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);

    auto pressed = [&](SDL_Scancode scancode) { return keyboardState[scancode] != 0; };
    auto pressedAny = [&](std::initializer_list<SDL_Scancode> scancodes) {
        for(SDL_Scancode scancode : scancodes) {
            if(keyboardState[scancode] != 0) return true;
        }
        return false;
    };
    auto set = [&](FamilyBasicKeyboard::Button button, bool value) { keys[static_cast<size_t>(button)] = value; };

    set(FamilyBasicKeyboard::Button::A, pressed(SDL_SCANCODE_A));
    set(FamilyBasicKeyboard::Button::B, pressed(SDL_SCANCODE_B));
    set(FamilyBasicKeyboard::Button::C, pressed(SDL_SCANCODE_C));
    set(FamilyBasicKeyboard::Button::D, pressed(SDL_SCANCODE_D));
    set(FamilyBasicKeyboard::Button::E, pressed(SDL_SCANCODE_E));
    set(FamilyBasicKeyboard::Button::F, pressed(SDL_SCANCODE_F));
    set(FamilyBasicKeyboard::Button::G, pressed(SDL_SCANCODE_G));
    set(FamilyBasicKeyboard::Button::H, pressed(SDL_SCANCODE_H));
    set(FamilyBasicKeyboard::Button::I, pressed(SDL_SCANCODE_I));
    set(FamilyBasicKeyboard::Button::J, pressed(SDL_SCANCODE_J));
    set(FamilyBasicKeyboard::Button::K, pressed(SDL_SCANCODE_K));
    set(FamilyBasicKeyboard::Button::L, pressed(SDL_SCANCODE_L));
    set(FamilyBasicKeyboard::Button::M, pressed(SDL_SCANCODE_M));
    set(FamilyBasicKeyboard::Button::N, pressed(SDL_SCANCODE_N));
    set(FamilyBasicKeyboard::Button::O, pressed(SDL_SCANCODE_O));
    set(FamilyBasicKeyboard::Button::P, pressed(SDL_SCANCODE_P));
    set(FamilyBasicKeyboard::Button::Q, pressed(SDL_SCANCODE_Q));
    set(FamilyBasicKeyboard::Button::R, pressed(SDL_SCANCODE_R));
    set(FamilyBasicKeyboard::Button::S, pressed(SDL_SCANCODE_S));
    set(FamilyBasicKeyboard::Button::T, pressed(SDL_SCANCODE_T));
    set(FamilyBasicKeyboard::Button::U, pressed(SDL_SCANCODE_U));
    set(FamilyBasicKeyboard::Button::V, pressed(SDL_SCANCODE_V));
    set(FamilyBasicKeyboard::Button::W, pressed(SDL_SCANCODE_W));
    set(FamilyBasicKeyboard::Button::X, pressed(SDL_SCANCODE_X));
    set(FamilyBasicKeyboard::Button::Y, pressed(SDL_SCANCODE_Y));
    set(FamilyBasicKeyboard::Button::Z, pressed(SDL_SCANCODE_Z));
    set(FamilyBasicKeyboard::Button::Num0, pressed(SDL_SCANCODE_0));
    set(FamilyBasicKeyboard::Button::Num1, pressed(SDL_SCANCODE_1));
    set(FamilyBasicKeyboard::Button::Num2, pressed(SDL_SCANCODE_2));
    set(FamilyBasicKeyboard::Button::Num3, pressed(SDL_SCANCODE_3));
    set(FamilyBasicKeyboard::Button::Num4, pressed(SDL_SCANCODE_4));
    set(FamilyBasicKeyboard::Button::Num5, pressed(SDL_SCANCODE_5));
    set(FamilyBasicKeyboard::Button::Num6, pressed(SDL_SCANCODE_6));
    set(FamilyBasicKeyboard::Button::Num7, pressed(SDL_SCANCODE_7));
    set(FamilyBasicKeyboard::Button::Num8, pressed(SDL_SCANCODE_8));
    set(FamilyBasicKeyboard::Button::Num9, pressed(SDL_SCANCODE_9));
    set(FamilyBasicKeyboard::Button::Return, pressedAny({SDL_SCANCODE_RETURN, SDL_SCANCODE_RETURN2}));
    set(FamilyBasicKeyboard::Button::Space, pressed(SDL_SCANCODE_SPACE));
    set(FamilyBasicKeyboard::Button::Del, pressed(SDL_SCANCODE_BACKSPACE));
    set(FamilyBasicKeyboard::Button::Ins, pressed(SDL_SCANCODE_INSERT));
    set(FamilyBasicKeyboard::Button::Esc, pressed(SDL_SCANCODE_ESCAPE));
    set(FamilyBasicKeyboard::Button::Ctrl, pressedAny({SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL}));
    set(FamilyBasicKeyboard::Button::RightShift, pressed(SDL_SCANCODE_RSHIFT));
    set(FamilyBasicKeyboard::Button::LeftShift, pressed(SDL_SCANCODE_LSHIFT));
    set(FamilyBasicKeyboard::Button::RightBracket, pressed(SDL_SCANCODE_RIGHTBRACKET));
    set(FamilyBasicKeyboard::Button::LeftBracket, pressed(SDL_SCANCODE_LEFTBRACKET));
    set(FamilyBasicKeyboard::Button::Up, pressed(SDL_SCANCODE_UP));
    set(FamilyBasicKeyboard::Button::Down, pressed(SDL_SCANCODE_DOWN));
    set(FamilyBasicKeyboard::Button::Left, pressed(SDL_SCANCODE_LEFT));
    set(FamilyBasicKeyboard::Button::Right, pressed(SDL_SCANCODE_RIGHT));
    set(FamilyBasicKeyboard::Button::Dot, pressed(SDL_SCANCODE_PERIOD));
    set(FamilyBasicKeyboard::Button::Comma, pressed(SDL_SCANCODE_COMMA));
    set(FamilyBasicKeyboard::Button::Colon, pressed(SDL_SCANCODE_SEMICOLON) && (pressed(SDL_SCANCODE_LSHIFT) || pressed(SDL_SCANCODE_RSHIFT)));
    set(FamilyBasicKeyboard::Button::SemiColon, pressed(SDL_SCANCODE_SEMICOLON));
    set(FamilyBasicKeyboard::Button::Underscore, pressed(SDL_SCANCODE_MINUS) && (pressed(SDL_SCANCODE_LSHIFT) || pressed(SDL_SCANCODE_RSHIFT)));
    set(FamilyBasicKeyboard::Button::Slash, pressed(SDL_SCANCODE_SLASH));
    set(FamilyBasicKeyboard::Button::Minus, pressed(SDL_SCANCODE_MINUS));
    set(FamilyBasicKeyboard::Button::Caret, pressed(SDL_SCANCODE_GRAVE));
    set(FamilyBasicKeyboard::Button::F1, pressed(SDL_SCANCODE_F1));
    set(FamilyBasicKeyboard::Button::F2, pressed(SDL_SCANCODE_F2));
    set(FamilyBasicKeyboard::Button::F3, pressed(SDL_SCANCODE_F3));
    set(FamilyBasicKeyboard::Button::F4, pressed(SDL_SCANCODE_F4));
    set(FamilyBasicKeyboard::Button::F5, pressed(SDL_SCANCODE_F5));
    set(FamilyBasicKeyboard::Button::F6, pressed(SDL_SCANCODE_F6));
    set(FamilyBasicKeyboard::Button::F7, pressed(SDL_SCANCODE_F7));
    set(FamilyBasicKeyboard::Button::F8, pressed(SDL_SCANCODE_F8));
    set(FamilyBasicKeyboard::Button::Yen, pressed(SDL_SCANCODE_BACKSLASH));
    set(FamilyBasicKeyboard::Button::Stop, pressed(SDL_SCANCODE_PAUSE));
    set(FamilyBasicKeyboard::Button::AtSign, pressed(SDL_SCANCODE_APOSTROPHE) && (pressed(SDL_SCANCODE_LSHIFT) || pressed(SDL_SCANCODE_RSHIFT)));
    set(FamilyBasicKeyboard::Button::Grph, pressedAny({SDL_SCANCODE_LALT, SDL_SCANCODE_RALT}));
    set(FamilyBasicKeyboard::Button::ClrHome, pressed(SDL_SCANCODE_HOME));
    set(FamilyBasicKeyboard::Button::Kana, pressed(SDL_SCANCODE_CAPSLOCK));
    return keys;
}

bool GeraNESApp::isArkanoidActive() const
{
    return m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) ||
           m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) ||
           m_emu.getExpansionDevice() == Settings::ExpansionDevice::ARKANOID_CONTROLLER;
}

void GeraNESApp::setArkanoidGrab(bool active)
{
    if(m_arkanoidGrabActive == active) return;
    m_arkanoidGrabActive = active;
#ifndef __EMSCRIPTEN__
    SDL_SetWindowGrab(this->sdlWindow(), active ? SDL_TRUE : SDL_FALSE);
#endif
    SDL_SetRelativeMouseMode(active ? SDL_TRUE : SDL_FALSE);
    if(active) {
        m_arkanoidSuppressClickUntilRelease = true;
        Logger::instance().log("Mouse grabbed. Press Escape to release the mouse.", Logger::Type::USER);
    } else {
        m_arkanoidSuppressClickUntilRelease = false;
        m_forceImGuiMouseResync = true;
        Logger::instance().log("Mouse released.", Logger::Type::USER);
    }
    m_hasLastMousePosition = false;
}

void GeraNESApp::setSnesMouseGrab(bool active)
{
    if(m_snesMouseGrabActive == active) return;
    m_snesMouseGrabActive = active;
#ifndef __EMSCRIPTEN__
    SDL_SetWindowGrab(this->sdlWindow(), active ? SDL_TRUE : SDL_FALSE);
#endif
    SDL_SetRelativeMouseMode(active ? SDL_TRUE : SDL_FALSE);
    if(active) {
        m_snesMouseSuppressClickUntilRelease = true;
        Logger::instance().log("Mouse grabbed. Press Escape to release the mouse.", Logger::Type::USER);
    } else {
        m_snesMouseSuppressClickUntilRelease = false;
        m_forceImGuiMouseResync = true;
        Logger::instance().log("Mouse released.", Logger::Type::USER);
    }
    m_hasLastMousePosition = false;
}

void GeraNESApp::openFile(const char* path)
{
    if(isNetplayRomChangeRestricted()) {
        notifyNetplayRomChangeRestrictedAction("Open ROM");
        return;
    }

    AppSettings::instance().data.addRecentFile(path);
    AppSettings::instance().data.setLastFolder(path);
    if(m_emu.open(path)) {
        normalizeTouchControlsTargetForCurrentTopology();
        const std::string filename = fs::path(path).filename().string();
        this->setTitle(std::string("GeraNES - ") + filename);
        Logger::instance().log("Rom loaded", Logger::Type::USER);
        m_netplayRuntime.refreshLocalRomSelectionImmediate();
    } else {
        Logger::instance().log("Failed to load ROM", Logger::Type::USER);
    }
}

void GeraNESApp::syncSettings()
{
    AppSettings::instance().load();

    auto cfg = AppSettings::instance().data;

    m_emu.configAudioDevice(cfg.audio.audioDevice, cfg.audio.sampleRate, cfg.audio.sampleSize);
    m_emu.setAudioVolume(cfg.audio.volume);
    m_audioDevices = m_emu.getAudioList();
    cfg.audio.audioDevice = m_emu.currentAudioDeviceName();

    cfg.input.getControllerInfo(0, m_controller1);
    cfg.input.getControllerInfo(1, m_controller2);
    cfg.input.getControllerInfo(2, m_controller3);
    cfg.input.getControllerInfo(3, m_controller4);
    m_powerPadInfo = cfg.input.powerPad;
    cfg.input.getSnesControllerInfo(0, m_snesController1);
    cfg.input.getSnesControllerInfo(1, m_snesController2);
    cfg.input.getVirtualBoyControllerInfo(0, m_virtualBoyController1);
    cfg.input.getVirtualBoyControllerInfo(1, m_virtualBoyController2);
    m_konamiHyperShot = cfg.input.konamiHyperShot;
    m_systemInput = cfg.input.system;

    cfg.improvements.maxRewindTime = std::max(0, cfg.improvements.maxRewindTime);
    const int effectiveMaxRewindTime = shouldSuppressRewindForNetplay() ? 0 : cfg.improvements.maxRewindTime;
    m_emu.setupRewindSystem(effectiveMaxRewindTime > 0, effectiveMaxRewindTime);
    m_emu.disableSpriteLimit(cfg.improvements.disableSpritesLimit);
    m_emu.enableOverclock(cfg.improvements.overclock);
    m_emu.configureNetplaySnapshots(std::max(0, cfg.netplay.rollbackWindowFrames));
    m_netplayRuntime.setLocalReconnectToken(0);
    const auto availableBackends = ConsoleNetplay::availableNetTransportBackends();
    ConsoleNetplay::NetTransportBackend configuredBackend = static_cast<ConsoleNetplay::NetTransportBackend>(std::clamp(cfg.netplay.transportBackend, 0, 1));
    if(std::find(availableBackends.begin(), availableBackends.end(), configuredBackend) == availableBackends.end()) {
        configuredBackend = ConsoleNetplay::defaultNetTransportBackend();
        cfg.netplay.transportBackend = static_cast<int>(configuredBackend);
    }
    ConsoleNetplay::NetTransportOptions transportOptions;
#ifdef __EMSCRIPTEN__
    cfg.netplay.useEmbeddedSignalingServer = false;
#endif
    transportOptions.useEmbeddedWebRtcSignalingServer = cfg.netplay.useEmbeddedSignalingServer;
    transportOptions.embeddedWebRtcSignalingPort =
        static_cast<uint16_t>(std::clamp(cfg.netplay.embeddedSignalingPort, 1, 65535));
    transportOptions.webRtcSignaling = ConsoleNetplay::WebRtcSignalingConfig{
        cfg.netplay.signalingUrl,
        cfg.netplay.signalingRoomId,
        cfg.netplay.signalingPassword
    };
    m_netplayRuntime.setTransportOptions(transportOptions);
    m_netplayRuntime.setTransportBackend(configuredBackend);

    m_vsyncMode = static_cast<VSyncMode>(cfg.video.vsyncMode);
    m_filterMode = static_cast<FilterMode>(cfg.video.filterMode);
    m_videoScaleMode = static_cast<VideoScaleMode>(
        std::clamp(cfg.video.scaleMode, static_cast<int>(ASPECT_FIT), static_cast<int>(PIXEL_PERFECT_BEST_FIT))
    );
    m_pixelPerfectScale = std::clamp(cfg.video.pixelPerfectScale, 1, 16);
    m_fullScreen = cfg.video.fullScreen;
    m_fullScreenMode = std::clamp(cfg.video.fullScreenMode, 0, 1);
    AppSettings::instance().data.debug.cpuDebuggerEnabled = false;
    m_showCpuDebuggerWindow = false;
    m_showCpuBreakpointsWindow = false;
    AppSettings::instance().data.debug.showCpuDebugger = false;
    AppSettings::instance().data.debug.showCpuBreakpoints = false;
}

void GeraNESApp::syncCpuDebugRuntimeState()
{
    auto& debugSettings = AppSettings::instance().data.debug;
    if(m_pendingEnableCpuDebuggerAfterNetplayDisconnect && !isNetplayBlockingCpuDebug()) {
        m_pendingEnableCpuDebuggerAfterNetplayDisconnect = false;
        debugSettings.cpuDebuggerEnabled = true;
        if(m_emu.valid() && !m_emu.paused()) {
            m_cpuDebuggerAutoPaused = true;
            m_emu.withExclusiveAccess([](auto& emu) {
                emu.setPaused(true);
            });
        }
    }

    if(!debugSettings.cpuDebuggerEnabled) {
        return;
    }

    if(isNetplayBlockingCpuDebug()) {
        disableCpuDebugging();
        return;
    }

    if(!m_emu.valid()) {
        return;
    }

    uint64_t breakpointSequence = 0;
    bool breakpointValid = false;
    m_emu.withExclusiveAccess([&](auto& emu) {
        emu.setDebugBreakpointsArmed(true);
        const GeraNESEmu::DebugBreakpointHit& hit = emu.debugBreakpointHit();
        breakpointSequence = hit.sequence;
        breakpointValid = hit.valid;
    });

    if(breakpointValid && breakpointSequence != 0 && breakpointSequence != m_lastSeenCpuBreakpointSequence) {
        m_lastSeenCpuBreakpointSequence = breakpointSequence;
        m_showCpuDebuggerWindow = true;
        AppSettings::instance().data.debug.showCpuDebugger = true;
    }
}

void GeraNESApp::disableCpuDebugging()
{
    AppSettings::instance().data.debug.cpuDebuggerEnabled = false;
    AppSettings::instance().data.debug.showCpuDebugger = false;
    AppSettings::instance().data.debug.showCpuBreakpoints = false;
    m_showCpuDebuggerWindow = false;
    m_showCpuBreakpointsWindow = false;
    m_pendingEnableCpuDebuggerAfterNetplayDisconnect = false;
    m_lastSeenCpuBreakpointSequence = 0;
    const bool shouldResumeSimulation = m_emu.valid() && m_emu.paused();
    m_cpuDebuggerAutoPaused = false;

    m_emu.withExclusiveAccess([](auto& emu) {
        emu.setDebugBreakpointsArmed(false);
        emu.clearDebugBreakpointHit();
    });

    if(shouldResumeSimulation) {
        m_emu.withExclusiveAccess([](auto& emu) {
            emu.setPaused(false);
        });
    }
}

void GeraNESApp::requestEnableCpuDebugger()
{
    if(isNetplayBlockingCpuDebug()) {
        disableCpuDebugging();
        m_pendingEnableCpuDebuggerAfterNetplayDisconnect = true;
        m_netplayRuntime.disconnect();
        m_userToast.show("Netplay disconnected so CPU debugger can be enabled");
        return;
    }

    AppSettings::instance().data.debug.cpuDebuggerEnabled = true;
    m_pendingEnableCpuDebuggerAfterNetplayDisconnect = false;
    m_cpuDebuggerAutoPaused = false;
    if(m_emu.valid() && !m_emu.paused()) {
        m_cpuDebuggerAutoPaused = true;
        m_emu.withExclusiveAccess([](auto& emu) {
            emu.setPaused(true);
        });
    }
}

bool GeraNESApp::isNetplayBlockingCpuDebug() const
{
    const auto snapshot = m_netplayRuntime.uiSnapshot();
    return snapshot.active ||
           snapshot.hosting ||
           snapshot.connected ||
           snapshot.reconnecting;
}

void GeraNESApp::createShortcuts()
{
    m_shortcuts.add(ShortcutManager::Data{"fullscreen", "Fullscreen", "Alt+F", [this]() {
        m_fullScreen = !this->isFullScreen();
        this->setFullScreen(m_fullScreen, m_fullScreenMode == 1);
        updateVSyncConfig();
        m_mainLoopLastCounter = currentMainLoopCounter();
        m_mainLoopCounterFrequency = currentMainLoopCounterFrequency();
        m_mainLoopCounterRemainder = 0;
        m_presenterFrameAccumScaled = 0;
        m_presenterStepRemainder = 0;
        AppSettings::instance().data.video.fullScreen = m_fullScreen;
    }});

    m_shortcuts.add(ShortcutManager::Data{"openRom", "Open ROM", "Alt+O", [this]() {
        if(isNetplayRomChangeRestricted()) {
            notifyNetplayRomChangeRestrictedAction("Open ROM");
            return;
        }
        openRom();
    }});

#ifndef __EMSCRIPTEN__
    m_shortcuts.add(ShortcutManager::Data{"quit", "Quit", "Alt+Q", [this]() {
        quit();
    }});
#endif

    m_shortcuts.add(ShortcutManager::Data{"horizontalStretch", "Stretch to Fill", "Alt+H", [this]() {
        m_videoScaleMode = m_videoScaleMode == STRETCH_TO_FILL ? ASPECT_FIT : STRETCH_TO_FILL;
        AppSettings::instance().data.video.scaleMode = static_cast<int>(m_videoScaleMode);
        AppSettings::instance().data.video.horizontalStretch = m_videoScaleMode == STRETCH_TO_FILL;
        m_updateObjectsFlag = true;
    }});

    m_shortcuts.add(ShortcutManager::Data{"saveState", "Save State", "Alt+S", [this]() {
        if(!m_emu.valid()) return;
        if(isNetplayClientRestricted()) {
            notifyNetplayClientRestrictedAction("Save state");
            return;
        }
        m_emu.saveState(static_cast<uint8_t>(AppSettings::instance().data.saveStateSlot));
    }});

    m_shortcuts.add(ShortcutManager::Data{"loadState", "Load State", "Alt+L", [this]() {
        if(!m_emu.valid()) return;
        if(isNetplayClientRestricted()) {
            notifyNetplayClientRestrictedAction("Load state");
            return;
        }
        m_emu.loadState(static_cast<uint8_t>(AppSettings::instance().data.saveStateSlot));
    }});

    m_shortcuts.add(ShortcutManager::Data{"pause", "Pause", "Alt+P", [this]() {
        togglePauseAction();
    }});

    m_shortcuts.add(ShortcutManager::Data{"cpuDebugger", "CPU Debugger", "Alt+D", [this]() {
        m_showCpuDebuggerWindow = !m_showCpuDebuggerWindow;
        AppSettings::instance().data.debug.showCpuDebugger = m_showCpuDebuggerWindow;
        if(m_showCpuDebuggerWindow) {
            requestEnableCpuDebugger();
        } else {
            disableCpuDebugging();
        }
    }});

    m_shortcuts.add(ShortcutManager::Data{"cpuBreakpoints", "CPU Breakpoints", "Alt+B", [this]() {
        m_showCpuBreakpointsWindow = !m_showCpuBreakpointsWindow;
        AppSettings::instance().data.debug.showCpuBreakpoints = m_showCpuBreakpointsWindow;
    }});
}

void GeraNESApp::updateCursor()
{
    if(!isArkanoidActive() && m_arkanoidGrabActive) {
        setArkanoidGrab(false);
    }
    if(!isSnesMouseActive() && !isSuborMouseActive() && m_snesMouseGrabActive) {
        setSnesMouseGrab(false);
    }

    if(m_snesMouseGrabActive || m_arkanoidGrabActive) {
        if(m_defaultCursor.has_value() && !m_defaultCursor->isCurrent()) {
            m_defaultCursor->setAsCurrent();
        }
        if(m_cursorVisible) {
            SDL_ShowCursor(SDL_DISABLE);
            m_cursorVisible = false;
        }
        return;
    }

    int mx;
    int my;
    SDL_GetMouseState(&mx, &my);

    const bool inside = pointInRect(glm::vec2(mx, my), m_nesScreenRect);
    const bool useArkanoidCursor =
        m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) ||
        m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) ||
        m_emu.getExpansionDevice() == Settings::ExpansionDevice::ARKANOID_CONTROLLER;

    const bool usePointerDevice =
        m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ZAPPER) ||
        m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ZAPPER) ||
        m_emu.getExpansionDevice() == Settings::ExpansionDevice::BANDAI_HYPERSHOT ||
        useArkanoidCursor;

    auto setCursorVisibility = [this](bool visible) {
        if(m_cursorVisible == visible) return;
        SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
        m_cursorVisible = visible;
    };

    if(!m_imGuiWantsMouse && useArkanoidCursor && inside) {
        setCursorVisibility(false);
    } else if(!m_imGuiWantsMouse && inside && usePointerDevice) {
        setCursorVisibility(true);
        if(m_crossCursor.has_value() && !m_crossCursor->isCurrent()) {
            m_crossCursor->setAsCurrent();
        }
    } else {
        setCursorVisibility(true);
        if(m_defaultCursor.has_value() && !m_defaultCursor->isCurrent()) {
            m_defaultCursor->setAsCurrent();
        }
    }
}

GeraNESApp::GeraNESApp()
    : m_emu(m_audioOutput)
{
    GeraNESNetplay::attachRuntimeWakeToHost(m_netplayRuntime, m_emu);
    GeraNESNetplay::installProcessGlobalFrontendNetplayLogCallbackOnce();
    m_emu.setPreAdvanceHook([this](GeraNESEmu& emu) {
        IEmulationHost::InputState latestInputState{};
        {
            std::scoped_lock stateLock(m_netplayInputStateMutex);
            latestInputState = m_netplayLatestInputState;
        }
        auto& cfg = AppSettings::instance().data.netplay;
        const ConsoleNetplay::RuntimeExecutionSettings runtimeSettings =
            GeraNESNetplay::buildGeraNESRuntimeExecutionSettings(
                m_emu,
                cfg.autoGameplayTuning,
                cfg.showNetplayDebugLog,
                cfg.gameplayReceiveDelayMs,
                cfg.inputDelayFrames,
                cfg.predictFrames
            );
        const ConsoleNetplay::NetplayAppRuntime::UpdateResult updateResult =
            GeraNESNetplay::executeRuntimeFrame(
            m_netplayRuntime,
            m_emu,
            emu,
            latestInputState,
            runtimeSettings
        );
        cfg.inputDelayFrames = static_cast<int>(updateResult.inputDelayFrames);
        cfg.predictFrames = static_cast<int>(updateResult.predictFrames);
    });

    std::ofstream file(LOG_FILE);
    file.close();

    Logger::instance().signalLog.bind_auto(&GeraNESApp::onLog, this);

    m_inputBindingConfigWindow.signalShow.bind(&GeraNESApp::onCaptureBegin, this);
    m_inputBindingConfigWindow.signalClose.bind(&GeraNESApp::onInputBindingCaptureEnd, this);
    m_powerPadConfigWindow.signalShow.bind(&GeraNESApp::onCaptureBegin, this);
    m_powerPadConfigWindow.signalClose.bind(&GeraNESApp::onInputBindingCaptureEnd, this);

    m_audioDevices = m_emu.getAudioList();

    syncSettings();
    createShortcuts();
    loadShaderList();
    loadPaletteList();
    const std::string configuredPalette = AppSettings::instance().data.video.paletteName;
    auto paletteIt = std::find_if(m_paletteList.begin(), m_paletteList.end(), [&configuredPalette](const PaletteItem& item) {
        return item.name == configuredPalette || (configuredPalette.empty() && item.builtIn);
    });
    if(paletteIt == m_paletteList.end()) paletteIt = m_paletteList.begin();
    if(paletteIt != m_paletteList.end()) {
        applyPalette(paletteIt->colors, paletteIt->name);
    }

#ifdef __EMSCRIPTEN__
    emcriptenRegisterVisibilityHandler(reinterpret_cast<intptr_t>(this));
    emcriptenRegisterUnloadHandler(reinterpret_cast<intptr_t>(this));
#endif
}

void GeraNESApp::loadShaderList()
{
    const char* SHADER_DIR = "shaders/";
    shaderList.clear();

    const std::string dir = fs::path(SHADER_DIR).parent_path().string();
    if(!fs::exists(dir)) fs::create_directory(dir);

    for(const auto& entry : fs::directory_iterator(dir)) {
        if(fs::is_regular_file(entry.path()) && entry.path().extension() == ".glsl") {
            ShaderItem item = {entry.path().filename().string(), entry.path().string()};
            shaderList.push_back(item);
        }
    }

    std::sort(shaderList.begin(), shaderList.end(), [](const ShaderItem& a, const ShaderItem& b) {
        return a.label < b.label;
    });
}

namespace
{
    fs::path palettesDirectory()
    {
        return AppSettings::storageDirectory() / "palettes";
    }

    std::string paletteColorToHex(uint32_t color)
    {
        std::ostringstream out;
        out << "#"
            << std::uppercase << std::hex << std::setfill('0')
            << std::setw(2) << (color & 0xFF)
            << std::setw(2) << ((color >> 8) & 0xFF)
            << std::setw(2) << ((color >> 16) & 0xFF);
        return out.str();
    }

    bool paletteColorFromHex(const std::string& text, uint32_t& outColor)
    {
        std::string hex = text;
        if(!hex.empty() && hex[0] == '#') hex.erase(hex.begin());
        if(hex.size() != 6) return false;

        char* end = nullptr;
        const unsigned long value = std::strtoul(hex.c_str(), &end, 16);
        if(end == hex.c_str() || (end != nullptr && *end != '\0')) return false;

        const uint32_t r = value >> 16;
        const uint32_t g = (value >> 8) & 0xFF;
        const uint32_t b = value & 0xFF;
        outColor = 0xFF000000u | r | (g << 8) | (b << 16);
        return true;
    }

    fs::path palettePathForName(const std::string& name)
    {
        std::string safeName;
        for(char c : name) {
            if(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') {
                safeName += c;
            } else if(c == ' ') {
                safeName += '_';
            }
        }
        if(safeName.empty()) safeName = "palette";
        return palettesDirectory() / (safeName + ".json");
    }
}

void GeraNESApp::loadPaletteList()
{
    m_paletteList.clear();

    PaletteItem defaultPalette;
    defaultPalette.name = "Default";
    defaultPalette.builtIn = true;
    std::copy(std::begin(NES_PALETTE), std::end(NES_PALETTE), defaultPalette.colors.begin());
    m_paletteList.push_back(defaultPalette);

    std::error_code ec;
    fs::create_directories(palettesDirectory(), ec);

    if(fs::exists(palettesDirectory())) {
        for(const auto& entry : fs::directory_iterator(palettesDirectory())) {
            if(!entry.is_regular_file() || entry.path().extension() != ".json") continue;

            try {
                std::ifstream file(entry.path());
                const nlohmann::json j = nlohmann::json::parse(file);
                if(!j.contains("colors") || !j["colors"].is_array() || j["colors"].size() != 64) continue;

                PaletteItem item;
                item.name = j.value("name", entry.path().stem().string());
                item.path = entry.path();
                item.builtIn = false;
                for(size_t i = 0; i < item.colors.size(); ++i) {
                    uint32_t color = defaultPalette.colors[i];
                    paletteColorFromHex(j["colors"][i].get<std::string>(), color);
                    item.colors[i] = color;
                }
                m_paletteList.push_back(std::move(item));
            }
            catch(...) {
                Logger::instance().log("Failed to load palette: " + entry.path().string(), Logger::Type::WARNING);
            }
        }
    }

    std::sort(m_paletteList.begin() + 1, m_paletteList.end(), [](const PaletteItem& a, const PaletteItem& b) {
        return a.name < b.name;
    });
}

const GeraNESApp::ShaderItem* GeraNESApp::findShaderByLabel(const std::string& label) const
{
    auto it = std::find_if(shaderList.begin(), shaderList.end(), [&label](const ShaderItem& item) {
        return item.label == label;
    });
    return it != shaderList.end() ? &(*it) : nullptr;
}

void GeraNESApp::applyPalette(const std::array<uint32_t, 64>& colors, const std::string& name)
{
    m_editPalette = colors;
    m_selectedPaletteName = name;
    m_paletteNameInput = name;
    AppSettings::instance().data.video.paletteName = name == "Default" ? "" : name;
    m_emu.setColorPalette(colors);
}

void GeraNESApp::saveCurrentPalette()
{
    std::string name = m_paletteNameInput.empty() ? "Palette" : m_paletteNameInput;
    if(name == "Default") name = "Custom Default";

    fs::create_directories(palettesDirectory());
    const fs::path path = palettePathForName(name);

    nlohmann::json colors = nlohmann::json::array();
    for(uint32_t color : m_editPalette) {
        colors.push_back(paletteColorToHex(color));
    }

    nlohmann::json j{
        {"name", name},
        {"colors", colors}
    };

    std::ofstream file(path);
    file << std::setw(4) << j;
    file.close();

    loadPaletteList();
    auto saved = std::find_if(m_paletteList.begin(), m_paletteList.end(), [&name](const PaletteItem& item) {
        return item.name == name;
    });
    if(saved != m_paletteList.end()) {
        applyPalette(saved->colors, saved->name);
    } else {
        applyPalette(m_editPalette, name);
    }
    Logger::instance().log("Palette saved: " + name, Logger::Type::USER);
}

void GeraNESApp::createNewPalette()
{
    m_paletteNameInput = "New Palette";
    std::copy(std::begin(NES_PALETTE), std::end(NES_PALETTE), m_editPalette.begin());
    applyPalette(m_editPalette, m_paletteNameInput);
}

void GeraNESApp::deleteCurrentPalette()
{
    if(m_selectedPaletteName.empty() || m_selectedPaletteName == "Default") return;

    auto it = std::find_if(m_paletteList.begin(), m_paletteList.end(), [this](const PaletteItem& item) {
        return item.name == m_selectedPaletteName;
    });
    if(it == m_paletteList.end() || it->builtIn || it->path.empty()) return;

    std::error_code ec;
    fs::remove(it->path, ec);
    if(ec) {
        Logger::instance().log("Failed to delete palette: " + it->path.string(), Logger::Type::WARNING);
        return;
    }

    loadPaletteList();
    if(!m_paletteList.empty()) {
        applyPalette(m_paletteList.front().colors, m_paletteList.front().name);
    }
    Logger::instance().log("Palette deleted", Logger::Type::USER);
}

#ifdef __EMSCRIPTEN__
void GeraNESApp::processUploadedFile(const char* fileName, size_t fileSize, const uint8_t* fileContent)
{
    Logger::instance().log(
        std::string("Processing uploaded file: ") + (fileName ? fileName : "<null>") +
        " (" + std::to_string(fileSize) + " bytes)",
        Logger::Type::INFO
    );

    if(fileName == nullptr || fileContent == nullptr || fileSize == 0) {
        Logger::instance().log("Uploaded file payload is invalid", Logger::Type::ERROR);
        return;
    }

    const fs::path uploadDir = "/uploads";
    std::error_code ec;
    fs::create_directories(uploadDir, ec);
    if(ec) {
        Logger::instance().log(
            std::string("Failed to prepare upload directory: ") + ec.message(),
            Logger::Type::ERROR
        );
        return;
    }

    const fs::path targetPath = uploadDir / fs::path(fileName).filename();
    FILE* file = fopen(targetPath.string().c_str(), "wb");

    if(file) {
        const size_t written = fwrite(fileContent, sizeof(uint8_t), fileSize, file);
        fclose(file);

        if(written != fileSize) {
            Logger::instance().log("Failed writing file in processUploadedFile call", Logger::Type::ERROR);
            return;
        }

        Logger::instance().log(
            std::string("Uploaded file stored at: ") + targetPath.string(),
            Logger::Type::INFO
        );
        openFile(targetPath.string().c_str());
    } else {
        Logger::instance().log("Failed to open file for writing in processUploadedFile call", Logger::Type::ERROR);
    }
}

void GeraNESApp::restartAudioModule()
{
    m_emu.restartAudio();
}

void GeraNESApp::onWebVisibilityChanged(bool visible)
{
    m_mainLoopLastCounter = currentMainLoopCounter();
    m_mainLoopCounterFrequency = currentMainLoopCounterFrequency();
    m_mainLoopCounterRemainder = 0;
    m_lastMainLoopDtMs = 0;
    m_hasLastMousePosition = false;
    m_forceImGuiMouseResync = true;
    m_emu.withExclusiveAccess([this, visible](auto& emu) {
        m_netplayRuntime.notifyWebVisibilityChanged(visible);
    });

    if(!visible) {
        m_webVisibilitySuspended = true;
        m_emu.setSimulationSuspended(true);
        m_emu.discardQueuedAudio();
        return;
    }

    const bool wasSuspended = m_webVisibilitySuspended;
    m_webVisibilitySuspended = false;
    const auto netplayMenu = GeraNESNetplay::menuSnapshot(m_netplayRuntime);
    const bool deferObserverResume =
        netplayMenu.inputManaged &&
        !netplayMenu.hosting &&
        netplayMenu.localAssignments.empty();
    if(!deferObserverResume) {
        m_emu.setSimulationSuspended(false);
    }
    m_emu.discardQueuedAudio();
    if(wasSuspended) {
        m_emu.restartAudio();
    }
}

void GeraNESApp::onWebAppUnload()
{
    m_netplayRuntime.shutdownForUnload();
}

void GeraNESApp::onSessionImportComplete()
{
    syncSettings();
}
#endif

void GeraNESApp::onCaptureBegin()
{
    m_emuInputEnabled = false;
}

void GeraNESApp::onInputBindingCaptureEnd()
{
    m_emuInputEnabled = true;
    persistSettingsForShutdown();
}

void GeraNESApp::persistSettingsForShutdown()
{
    auto& settings = AppSettings::instance();
    settings.data.input.setControllerInfo(0, m_controller1);
    settings.data.input.setControllerInfo(1, m_controller2);
    settings.data.input.setControllerInfo(2, m_controller3);
    settings.data.input.setControllerInfo(3, m_controller4);
    settings.data.input.powerPad = m_powerPadInfo;
    settings.data.input.setSnesControllerInfo(0, m_snesController1);
    settings.data.input.setSnesControllerInfo(1, m_snesController2);
    settings.data.input.setVirtualBoyControllerInfo(0, m_virtualBoyController1);
    settings.data.input.setVirtualBoyControllerInfo(1, m_virtualBoyController2);
    settings.data.input.konamiHyperShot = m_konamiHyperShot;
    settings.data.input.system = m_systemInput;
    settings.save();
}

void GeraNESApp::onQuitRequested()
{
    persistSettingsForShutdown();
}

GeraNESApp::~GeraNESApp()
{
    persistSettingsForShutdown();
    m_emu.shutdown();
    m_netplayRuntime.shutdownForUnload();
    destroyPostProcessTargets();
    if(m_texture != 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    if(m_ppuNametableTexture != 0) {
        glDeleteTextures(1, &m_ppuNametableTexture);
        m_ppuNametableTexture = 0;
    }
    if(m_ppuChrTexture != 0) {
        glDeleteTextures(1, &m_ppuChrTexture);
        m_ppuChrTexture = 0;
    }
    if(m_ppuEventTexture != 0) {
        glDeleteTextures(1, &m_ppuEventTexture);
        m_ppuEventTexture = 0;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void GeraNESApp::openRom()
{
    if(isNetplayRomChangeRestricted()) {
        notifyNetplayRomChangeRestrictedAction("Open ROM");
        return;
    }

#ifndef __EMSCRIPTEN__
    const bool resumeAfterDialog = m_emu.withExclusiveAccess([](auto& emu) {
        if(!emu.valid()) return false;

        const bool shouldResume = !emu.paused();
        if(shouldResume) {
            emu.togglePaused();
        }
        return shouldResume;
    });
    setWindowsNativePumpEnabled(false);

    const bool restoreAfterDialog = this->isFullScreen();
#ifndef _WIN32
    if(restoreAfterDialog) minimizeWindow();
#endif

    NFD_Init();

    nfdu8char_t* outPath = nullptr;
#ifdef ENABLE_NSF_PLAYER
    nfdu8filteritem_t filterItem[] = {
        { "Supported Files", "nes,nsf,fds,zip,ips,ups,bps" },
        { "NES", "nes" },
        { "NSF", "nsf" },
        { "FDS", "fds" },
        { "ZIP", "zip" },
        { "Patch", "ips,ups,bps" }
    };
#else
    nfdu8filteritem_t filterItem[] = {
        { "Supported Files", "nes,fds,zip,ips,ups,bps" },
        { "NES", "nes" },
        { "FDS", "fds" },
        { "ZIP", "zip" },
        { "Patch", "ips,ups,bps" }
    };
#endif
    nfdopendialogu8args_t args = {};
    args.filterList = filterItem;
    args.filterCount = sizeof(filterItem) / sizeof(nfdu8filteritem_t);
    args.defaultPath = AppSettings::instance().data.getLastFolder().c_str();
#ifdef _WIN32
    args.parentWindow.type = NFD_WINDOW_HANDLE_TYPE_WINDOWS;
    args.parentWindow.handle = nativeWindowHandle();
#endif

    const nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);

    if(result == NFD_OKAY) {
        openFile(outPath);
        NFD_FreePathU8(outPath);
    } else if(result != NFD_CANCEL) {
        Logger::instance().log(NFD_GetError(), Logger::Type::ERROR);
    }

    NFD_Quit();
    setWindowsNativePumpEnabled(true);

    m_emu.withExclusiveAccess([resumeAfterDialog](auto& emu) {
        if(resumeAfterDialog && emu.paused()) {
            emu.togglePaused();
        }
    });
#else
    emcriptenFileDialog(reinterpret_cast<intptr_t>(this));
#endif

#ifndef __EMSCRIPTEN__
    if(restoreAfterDialog) this->restoreWindow();
#endif
}

void GeraNESApp::updateVSyncConfig()
{
    switch(m_vsyncMode) {
        case OFF: this->setVSync(0); break;
        case SYNCRONIZED: this->setVSync(1); break;
        case ADAPTATIVE: this->setVSync(-1); break;
    }
}

void GeraNESApp::updateFilterConfig()
{
    switch(m_filterMode) {
        case NEAREST:
            glBindTexture(GL_TEXTURE_2D, m_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case BILINEAR:
            glBindTexture(GL_TEXTURE_2D, m_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
    }
}

void GeraNESApp::updateShaderConfig()
{
    auto& video = AppSettings::instance().data.video;

    std::vector<ShaderPass> loadedPasses;
    loadedPasses.reserve(video.shaderStack.size());

    for(auto& configuredPass : video.shaderStack) {
        const std::string& shaderName = configuredPass.label;
        if(shaderName.empty() || !configuredPass.enabled) {
            continue;
        }

        const ShaderItem* item = findShaderByLabel(shaderName);
        if(item == nullptr) {
            Logger::instance().log("Shader not found: " + shaderName + ". Skipping pass.", Logger::Type::WARNING);
            continue;
        }

        ShaderPass pass;
        pass.label = item->label;
        pass.path = item->path;
        pass.enabled = configuredPass.enabled;
        if(compileShaderProgram(pass.program, pass.path, &configuredPass.parameters, &pass.parameters)) {
            loadedPasses.push_back(std::move(pass));
        } else {
            Logger::instance().log("Failed to load shader " + shaderName + ". Skipping pass.", Logger::Type::WARNING);
        }
    }

    if(loadedPasses.empty()) {
        ShaderPass pass;
        pass.label = "default";
        if(!compileShaderProgram(pass.program, "", nullptr, &pass.parameters)) return;
        loadedPasses.push_back(std::move(pass));
        if(video.shaderStack.empty()) video.shaderName.clear();
    } else {
        video.shaderName = loadedPasses.front().label;
    }

    m_shaderPasses = std::move(loadedPasses);
}

void GeraNESApp::pollAndPrepareInput()
{
    if(m_emuInputEnabled) {
        InputManager& im = InputManager::instance();
        im.updateInputs();

        const auto inputTopology = m_emu.getInputTopologySnapshot();
        const bool touchInputActive = !m_imGuiWantsMouse;
        const auto touchTarget = effectiveTouchControlsTarget();
        const bool multitapActive =
            inputTopology.nesMultitapDevice == Settings::NesMultitapDevice::FOUR_SCORE ||
            inputTopology.famicomMultitapDevice == Settings::FamicomMultitapDevice::HORI_ADAPTER;
        const bool touchTargetsPort1 =
            touchInputActive &&
            touchTarget == AppSettings::TouchControlsTarget::Port1Controller &&
            !multitapActive &&
            isTouchCompatibleControllerDevice(inputTopology.port1Device);
        const bool touchTargetsPort2 =
            touchInputActive &&
            touchTarget == AppSettings::TouchControlsTarget::Port2Controller &&
            !multitapActive &&
            isTouchCompatibleControllerDevice(inputTopology.port2Device);
        const bool touchTargetsExpansion =
            touchInputActive &&
            touchTarget == AppSettings::TouchControlsTarget::Expansion &&
            !multitapActive &&
            isTouchCompatibleExpansionDevice(inputTopology.expansionDevice);
        const bool touchTargetsMultitapP1 =
            touchInputActive &&
            multitapActive &&
            touchTarget == AppSettings::TouchControlsTarget::MultitapP1;
        const bool touchTargetsMultitapP2 =
            touchInputActive &&
            multitapActive &&
            touchTarget == AppSettings::TouchControlsTarget::MultitapP2;
        const bool touchTargetsMultitapP3 =
            touchInputActive &&
            multitapActive &&
            touchTarget == AppSettings::TouchControlsTarget::MultitapP3;
        const bool touchTargetsMultitapP4 =
            touchInputActive &&
            multitapActive &&
            touchTarget == AppSettings::TouchControlsTarget::MultitapP4;

        const bool p1A = im.isPressed(m_controller1.a) || ((touchTargetsPort1 || touchTargetsMultitapP1) && m_touch->buttons().a);
        const bool p1B = im.isPressed(m_controller1.b) || ((touchTargetsPort1 || touchTargetsMultitapP1) && m_touch->buttons().b);
        const bool p1Select = im.isPressed(m_controller1.select) || ((touchTargetsPort1 || touchTargetsMultitapP1) && m_touch->buttons().select);
        const bool p1Start = im.isPressed(m_controller1.start) || ((touchTargetsPort1 || touchTargetsMultitapP1) && m_touch->buttons().start);
        const bool p1Up = im.isPressed(m_controller1.up) || ((touchTargetsPort1 || touchTargetsMultitapP1) && m_touch->buttons().up);
        const bool p1Down = im.isPressed(m_controller1.down) || ((touchTargetsPort1 || touchTargetsMultitapP1) && m_touch->buttons().down);
        const bool p1Left = im.isPressed(m_controller1.left) || ((touchTargetsPort1 || touchTargetsMultitapP1) && m_touch->buttons().left);
        const bool p1Right = im.isPressed(m_controller1.right) || ((touchTargetsPort1 || touchTargetsMultitapP1) && m_touch->buttons().right);
        const bool p1X = im.isPressed(m_snesController1.x);
        const bool p1Y = im.isPressed(m_snesController1.y);
        const bool p1L = im.isPressed(m_snesController1.l);
        const bool p1R = im.isPressed(m_snesController1.r);

        const bool p2A = im.isPressed(m_controller2.a) || ((touchTargetsPort2 || touchTargetsMultitapP2) && m_touch->buttons().a);
        const bool p2B = im.isPressed(m_controller2.b) || ((touchTargetsPort2 || touchTargetsMultitapP2) && m_touch->buttons().b);
        const bool p2Select = im.isPressed(m_controller2.select) || ((touchTargetsPort2 || touchTargetsMultitapP2) && m_touch->buttons().select);
        const bool p2Start = im.isPressed(m_controller2.start) || ((touchTargetsPort2 || touchTargetsMultitapP2) && m_touch->buttons().start);
        const bool p2Up = im.isPressed(m_controller2.up) || ((touchTargetsPort2 || touchTargetsMultitapP2) && m_touch->buttons().up);
        const bool p2Down = im.isPressed(m_controller2.down) || ((touchTargetsPort2 || touchTargetsMultitapP2) && m_touch->buttons().down);
        const bool p2Left = im.isPressed(m_controller2.left) || ((touchTargetsPort2 || touchTargetsMultitapP2) && m_touch->buttons().left);
        const bool p2Right = im.isPressed(m_controller2.right) || ((touchTargetsPort2 || touchTargetsMultitapP2) && m_touch->buttons().right);
        const bool p2X = im.isPressed(m_snesController2.x);
        const bool p2Y = im.isPressed(m_snesController2.y);
        const bool p2L = im.isPressed(m_snesController2.l);
        const bool p2R = im.isPressed(m_snesController2.r);
        const bool p3A = im.isPressed(m_controller3.a) || ((touchTargetsExpansion || touchTargetsMultitapP3) && m_touch->buttons().a);
        const bool p3B = im.isPressed(m_controller3.b) || ((touchTargetsExpansion || touchTargetsMultitapP3) && m_touch->buttons().b);
        const bool p3Select = im.isPressed(m_controller3.select) || ((touchTargetsExpansion || touchTargetsMultitapP3) && m_touch->buttons().select);
        const bool p3Start = im.isPressed(m_controller3.start) || ((touchTargetsExpansion || touchTargetsMultitapP3) && m_touch->buttons().start);
        const bool p3Up = im.isPressed(m_controller3.up) || ((touchTargetsExpansion || touchTargetsMultitapP3) && m_touch->buttons().up);
        const bool p3Down = im.isPressed(m_controller3.down) || ((touchTargetsExpansion || touchTargetsMultitapP3) && m_touch->buttons().down);
        const bool p3Left = im.isPressed(m_controller3.left) || ((touchTargetsExpansion || touchTargetsMultitapP3) && m_touch->buttons().left);
        const bool p3Right = im.isPressed(m_controller3.right) || ((touchTargetsExpansion || touchTargetsMultitapP3) && m_touch->buttons().right);
        const bool p4A = im.isPressed(m_controller4.a) || (touchTargetsMultitapP4 && m_touch->buttons().a);
        const bool p4B = im.isPressed(m_controller4.b) || (touchTargetsMultitapP4 && m_touch->buttons().b);
        const bool p4Select = im.isPressed(m_controller4.select) || (touchTargetsMultitapP4 && m_touch->buttons().select);
        const bool p4Start = im.isPressed(m_controller4.start) || (touchTargetsMultitapP4 && m_touch->buttons().start);
        const bool p4Up = im.isPressed(m_controller4.up) || (touchTargetsMultitapP4 && m_touch->buttons().up);
        const bool p4Down = im.isPressed(m_controller4.down) || (touchTargetsMultitapP4 && m_touch->buttons().down);
        const bool p4Left = im.isPressed(m_controller4.left) || (touchTargetsMultitapP4 && m_touch->buttons().left);
        const bool p4Right = im.isPressed(m_controller4.right) || (touchTargetsMultitapP4 && m_touch->buttons().right);
        const bool konamiP1Run = im.isPressed(m_konamiHyperShot.p1Run);
        const bool konamiP1Jump = im.isPressed(m_konamiHyperShot.p1Jump);
        const bool konamiP2Run = im.isPressed(m_konamiHyperShot.p2Run);
        const bool konamiP2Jump = im.isPressed(m_konamiHyperShot.p2Jump);
        const bool p1UsesVirtualBoyController = m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER);
        const bool p2UsesVirtualBoyController = m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::VIRTUAL_BOY_CONTROLLER);
        const bool p1PrimaryA = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.a) : p1A;
        const bool p1PrimaryB = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.b) : p1B;
        const bool p1PrimarySelect = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.select) : p1Select;
        const bool p1PrimaryStart = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.start) : p1Start;
        const bool p1PrimaryUp = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.up) : p1Up;
        const bool p1PrimaryDown = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.down) : p1Down;
        const bool p1PrimaryLeft = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.left) : p1Left;
        const bool p1PrimaryRight = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.right) : p1Right;
        const bool p1PrimaryL = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.l) : p1L;
        const bool p1PrimaryR = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.r) : p1R;
        const bool p1Up2 = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.up2) : false;
        const bool p1Down2 = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.down2) : false;
        const bool p1Left2 = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.left2) : false;
        const bool p1Right2 = p1UsesVirtualBoyController ? im.isPressed(m_virtualBoyController1.right2) : false;
        const bool p2PrimaryA = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.a) : p2A;
        const bool p2PrimaryB = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.b) : p2B;
        const bool p2PrimarySelect = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.select) : p2Select;
        const bool p2PrimaryStart = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.start) : p2Start;
        const bool p2PrimaryUp = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.up) : p2Up;
        const bool p2PrimaryDown = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.down) : p2Down;
        const bool p2PrimaryLeft = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.left) : p2Left;
        const bool p2PrimaryRight = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.right) : p2Right;
        const bool p2PrimaryL = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.l) : p2L;
        const bool p2PrimaryR = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.r) : p2R;
        const bool p2Up2 = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.up2) : false;
        const bool p2Down2 = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.down2) : false;
        const bool p2Left2 = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.left2) : false;
        const bool p2Right2 = p2UsesVirtualBoyController ? im.isPressed(m_virtualBoyController2.right2) : false;
        const bool keyboardExpansionActive = isSuborKeyboardActive() || isFamilyBasicKeyboardActive();

        if(!keyboardExpansionActive) {
            if(!isNetplayClientRestricted()) {
                if(im.isJustPressed(m_systemInput.saveState)) {
                    m_emu.saveState(static_cast<uint8_t>(AppSettings::instance().data.saveStateSlot));
                }
                if(im.isJustPressed(m_systemInput.loadState)) {
                    m_emu.loadState(static_cast<uint8_t>(AppSettings::instance().data.saveStateSlot));
                }
            }
        }
        std::array<bool, 12> p1PowerPadButtons = {};
        std::array<bool, 12> p2PowerPadButtons = {};
        for(size_t i = 0; i < m_powerPadInfo.bindings.size(); ++i) {
            p1PowerPadButtons[i] = im.isPressed(m_powerPadInfo.bindings[i]);
            p2PowerPadButtons[i] = im.isPressed(m_powerPadInfo.bindings[i]);
        }

        int zapperX = -1;
        int zapperY = -1;
        int mouseDeltaX = 0;
        int mouseDeltaY = 0;
        float arkanoidNesPosition = m_arkanoidNesPosition;
        float arkanoidFamicomPosition = m_arkanoidFamicomPosition;
        bool p1ZapperTrigger = false;
        bool p2ZapperTrigger = false;
        bool bandaiTrigger = false;
        bool mousePrimaryButton = false;
        bool mouseSecondaryButton = false;
        {
            int mx;
            int my;
            Uint32 buttons = SDL_GetMouseState(&mx, &my);

            auto [nesX, nesY] = getNesCursor(mx, my);
            const bool useSnesMouse = isSnesMouseActive() || isSuborMouseActive();
            const bool useArkanoid = isArkanoidActive();
            const bool pointerGrabActive = m_snesMouseGrabActive || m_arkanoidGrabActive;

            const bool mouseAllowed = (!m_imGuiWantsMouse || pointerGrabActive) && !m_touch->buttons().anyPressed();
            bool leftClick = mouseAllowed && (buttons & SDL_BUTTON(SDL_BUTTON_LEFT));
            bool rightClick = mouseAllowed && (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT));

            if(m_snesMouseSuppressClickUntilRelease) {
                if(!leftClick && !rightClick) {
                    m_snesMouseSuppressClickUntilRelease = false;
                } else if(useSnesMouse) {
                    leftClick = false;
                    rightClick = false;
                }
            }
            if(m_arkanoidSuppressClickUntilRelease) {
                if(!leftClick && !rightClick) {
                    m_arkanoidSuppressClickUntilRelease = false;
                } else if(useArkanoid) {
                    leftClick = false;
                    rightClick = false;
                }
            }

            if(mouseAllowed && useSnesMouse && m_snesMouseGrabActive) {
                int relativeX = 0;
                int relativeY = 0;
                buttons = SDL_GetRelativeMouseState(&relativeX, &relativeY);
                const float snesMouseSensitivity = std::clamp(AppSettings::instance().data.input.snesMouse.sensitivity, 0.01f, 4.0f);
                mouseDeltaX = static_cast<int>(std::lround(static_cast<float>(relativeX) * snesMouseSensitivity));
                mouseDeltaY = static_cast<int>(std::lround(static_cast<float>(relativeY) * snesMouseSensitivity));
            } else if(mouseAllowed && useArkanoid && m_arkanoidGrabActive) {
                int relativeX = 0;
                int relativeY = 0;
                buttons = SDL_GetRelativeMouseState(&relativeX, &relativeY);
                const float arkanoidSensitivity = std::clamp(AppSettings::instance().data.input.arkanoid.sensitivity, 0.05f, 4.0f);
                arkanoidNesPosition = std::clamp(arkanoidNesPosition + (static_cast<float>(relativeX) * (arkanoidSensitivity / 512.0f)), 0.0f, 1.0f);
                arkanoidFamicomPosition = std::clamp(arkanoidFamicomPosition + (static_cast<float>(relativeX) * (arkanoidSensitivity / 512.0f)), 0.0f, 1.0f);
            } else if(mouseAllowed && !useSnesMouse) {
                if(!m_hasLastMousePosition) {
                    m_lastMouseX = mx;
                    m_lastMouseY = my;
                    m_hasLastMousePosition = true;
                }
                mouseDeltaX = mx - m_lastMouseX;
                mouseDeltaY = my - m_lastMouseY;
                m_lastMouseX = mx;
                m_lastMouseY = my;
            } else if(!pointerGrabActive) {
                m_hasLastMousePosition = false;
            }

            zapperX = mouseAllowed ? (rightClick ? -1 : nesX) : -1;
            zapperY = mouseAllowed ? (rightClick ? -1 : nesY) : -1;
            p1ZapperTrigger = rightClick;
            p2ZapperTrigger = leftClick || rightClick;
            bandaiTrigger = leftClick || rightClick;
            mousePrimaryButton = leftClick;
            mouseSecondaryButton = rightClick;
        }
        m_arkanoidNesPosition = std::clamp(arkanoidNesPosition, 0.0f, 1.0f);
        m_arkanoidFamicomPosition = std::clamp(arkanoidFamicomPosition, 0.0f, 1.0f);

        EmulationHost::InputState inputState;
        inputState.p1A = p1PrimaryA;
        inputState.p1B = p1PrimaryB;
        inputState.p1Select = p1PrimarySelect;
        inputState.p1Start = p1PrimaryStart;
        inputState.p1Up = p1PrimaryUp;
        inputState.p1Down = p1PrimaryDown;
        inputState.p1Left = p1PrimaryLeft;
        inputState.p1Right = p1PrimaryRight;
        inputState.p1X = p1X;
        inputState.p1Y = p1Y;
        inputState.p1L = p1PrimaryL;
        inputState.p1R = p1PrimaryR;
        inputState.p1Up2 = p1Up2;
        inputState.p1Down2 = p1Down2;
        inputState.p1Left2 = p1Left2;
        inputState.p1Right2 = p1Right2;
        inputState.p2A = p2PrimaryA;
        inputState.p2B = p2PrimaryB;
        inputState.p2Select = p2PrimarySelect;
        inputState.p2Start = p2PrimaryStart;
        inputState.p2Up = p2PrimaryUp;
        inputState.p2Down = p2PrimaryDown;
        inputState.p2Left = p2PrimaryLeft;
        inputState.p2Right = p2PrimaryRight;
        inputState.p2X = p2X;
        inputState.p2Y = p2Y;
        inputState.p2L = p2PrimaryL;
        inputState.p2R = p2PrimaryR;
        inputState.p2Up2 = p2Up2;
        inputState.p2Down2 = p2Down2;
        inputState.p2Left2 = p2Left2;
        inputState.p2Right2 = p2Right2;
        inputState.p3A = p3A;
        inputState.p3B = p3B;
        inputState.p3Select = p3Select;
        inputState.p3Start = p3Start;
        inputState.p3Up = p3Up;
        inputState.p3Down = p3Down;
        inputState.p3Left = p3Left;
        inputState.p3Right = p3Right;
        inputState.p4A = p4A;
        inputState.p4B = p4B;
        inputState.p4Select = p4Select;
        inputState.p4Start = p4Start;
        inputState.p4Up = p4Up;
        inputState.p4Down = p4Down;
        inputState.p4Left = p4Left;
        inputState.p4Right = p4Right;
        inputState.p1PowerPadButtons = p1PowerPadButtons;
        inputState.p2PowerPadButtons = p2PowerPadButtons;
        if(isSuborKeyboardActive()) {
            inputState.suborKeyboardKeys = captureSuborKeyboardState();
        }
        if(isFamilyBasicKeyboardActive()) {
            inputState.familyBasicKeyboardKeys = captureFamilyBasicKeyboardState();
        }
        inputState.zapperX = zapperX;
        inputState.zapperY = zapperY;
        inputState.mouseDeltaX = mouseDeltaX;
        inputState.mouseDeltaY = mouseDeltaY;
        inputState.arkanoidNesPosition = arkanoidNesPosition;
        inputState.arkanoidFamicomPosition = arkanoidFamicomPosition;
        inputState.zapperP1Trigger = p1ZapperTrigger;
        inputState.zapperP2Trigger = p2ZapperTrigger;
        inputState.bandaiTrigger = bandaiTrigger;
        inputState.konamiP1Run = konamiP1Run;
        inputState.konamiP1Jump = konamiP1Jump;
        inputState.konamiP2Run = konamiP2Run;
        inputState.konamiP2Jump = konamiP2Jump;
        inputState.mousePrimaryButton = mousePrimaryButton;
        inputState.mouseSecondaryButton = mouseSecondaryButton;
        inputState.rewind = !keyboardExpansionActive && (
            im.isPressed(m_systemInput.rewind) ||
            m_touch->buttons().rewind
        );
        inputState.speedBoost = !keyboardExpansionActive && (
            im.isPressed(m_systemInput.speed)
        );
        {
            std::scoped_lock stateLock(m_netplayInputStateMutex);
            m_netplayLatestInputState = inputState;
        }
        m_emu.setPendingInput(inputState);
    } else {
        m_emu.setPendingInput({});
    }
}

bool GeraNESApp::initGL()
{
    m_defaultCursor = SdlCursor::getDefault();
    m_crossCursor = SdlCursor::createSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
    m_sizeWECursor = SdlCursor::createSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);

    this->setFullScreen(m_fullScreen, m_fullScreenMode == 1);

    if(SDL_Init(SDL_INIT_TIMER) < 0) {
        Logger::instance().log("SDL_Init error", Logger::Type::ERROR);
        return false;
    }

#ifndef __EMSCRIPTEN__
    GLenum err = glewInit();
    if(GLEW_OK != err) {
        Logger::instance().log((const char*)(glewGetErrorString(err)), Logger::Type::ERROR);
        return false;
    }
#endif

    updateVSyncConfig();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifndef __EMSCRIPTEN__
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    setBordered(!m_customWindowChromeEnabled);
#endif

    {
        auto fs = cmrc::resources::get_filesystem();
        const char* fontPath = nullptr;
        if(fs.exists("resources/fonts/DejaVuSans.ttf")) fontPath = "resources/fonts/DejaVuSans.ttf";
        else if(fs.exists("fonts/DejaVuSans.ttf")) fontPath = "fonts/DejaVuSans.ttf";

        if(fontPath != nullptr) {
            auto file = fs.open(fontPath);
            m_embeddedUiFontData.assign(file.begin(), file.end());

            io.Fonts->Clear();
            io.FontDefault = io.Fonts->AddFontDefault();

            ImFontConfig cfg{};
            cfg.FontDataOwnedByAtlas = false;
            cfg.OversampleH = 4;
            cfg.OversampleV = 2;
            cfg.PixelSnapH = false;

#ifdef ENABLE_NSF_PLAYER
            m_fontNsfTitle = io.Fonts->AddFontFromMemoryTTF(m_embeddedUiFontData.data(), static_cast<int>(m_embeddedUiFontData.size()), 34.0f, &cfg);
            m_fontNsfSubtitle = io.Fonts->AddFontFromMemoryTTF(m_embeddedUiFontData.data(), static_cast<int>(m_embeddedUiFontData.size()), 20.0f, &cfg);
#endif
            m_fontToast = io.Fonts->AddFontFromMemoryTTF(m_embeddedUiFontData.data(), static_cast<int>(m_embeddedUiFontData.size()), 24.0f, &cfg);
            m_fontFps = io.Fonts->AddFontFromMemoryTTF(m_embeddedUiFontData.data(), static_cast<int>(m_embeddedUiFontData.size()), 32.0f, &cfg);

            if(
#ifdef ENABLE_NSF_PLAYER
                m_fontNsfTitle == nullptr || m_fontNsfSubtitle == nullptr ||
#endif
                m_fontToast == nullptr || m_fontFps == nullptr) {
                io.FontDefault = io.Fonts->AddFontDefault();
                Logger::instance().log("Embedded overlay fonts failed to load completely; using ImGui default where needed.", Logger::Type::WARNING);
            } else {
                Logger::instance().log(std::string("Embedded UI font loaded from cmrc: ") + fontPath, Logger::Type::INFO);
            }
        } else {
            Logger::instance().log("Embedded font not found in cmrc (tried resources/fonts/DejaVuSans.ttf and fonts/DejaVuSans.ttf).", Logger::Type::WARNING);
        }
    }

    ApplyImGuiTheme();

    const char* glsl_version = "#version 100";
    ImGui_ImplSDL2_InitForOpenGL(this->sdlWindow(), this->glContext());
    ImGui_ImplOpenGL3_Init(glsl_version);
#ifdef __EMSCRIPTEN__
    emcriptenInstallImGuiClipboardBackend();
#endif

    glClearColor(0.0, 0.0, 0.0, 0.0);

    updateShaderConfig();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_DEPTH_TEST);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);

    updateBuffers();

    if(!m_vao.isCreated()) {
        m_vao.create();
    }

    m_vao.bind();
    m_vbo.bind();

    GLsizei stride = 4 * sizeof(GLfloat);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(GLfloat)));

    glDisableVertexAttribArray(2);
    glVertexAttrib4f(2, 1.0f, 1.0f, 1.0f, 1.0f);

    m_vbo.release();
    m_vao.release();

    if(!m_postProcessVbo.isCreated()) {
        m_postProcessVbo.create();
    }
    if(!m_postProcessVao.isCreated()) {
        m_postProcessVao.create();
    }

    const std::array<GLfloat, 16> fullscreenQuad = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };
    m_postProcessVbo.allocate(fullscreenQuad.data(), static_cast<int>(sizeof(fullscreenQuad)));

    m_postProcessVao.bind();
    m_postProcessVbo.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(GLfloat)));
    glDisableVertexAttribArray(2);
    glVertexAttrib4f(2, 1.0f, 1.0f, 1.0f, 1.0f);
    m_postProcessVbo.release();
    m_postProcessVao.release();

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    updateFilterConfig();
    updateMVP();

    m_touch = std::make_unique<TouchControls>(this->width(), this->height(), GetWindowDPI());

    return true;
}

bool GeraNESApp::loadShader(const std::string& path)
{
    if(m_shaderPasses.empty()) {
        m_shaderPasses.emplace_back();
    }
    return compileShaderProgram(m_shaderPasses.front().program, path, nullptr, &m_shaderPasses.front().parameters);
}

std::vector<GeraNESApp::ShaderPass::Parameter> GeraNESApp::parseShaderParameters(const std::string& shaderText) const
{
    std::vector<ShaderPass::Parameter> parameters;
    std::regex parameterPattern(R"shader(^\s*#pragma\s+parameter\s+([A-Za-z_][A-Za-z0-9_]*)\s+"([^"]*)"\s+([-+]?\d*\.?\d+)\s+([-+]?\d*\.?\d+)\s+([-+]?\d*\.?\d+)\s+([-+]?\d*\.?\d+)\s*$)shader");

    std::istringstream stream(shaderText);
    std::string line;
    while(std::getline(stream, line)) {
        std::smatch match;
        if(!std::regex_match(line, match, parameterPattern)) continue;

        ShaderPass::Parameter parameter;
        parameter.name = match[1].str();
        parameter.label = match[2].str();
        parameter.defaultValue = std::stof(match[3].str());
        parameter.minValue = std::stof(match[4].str());
        parameter.maxValue = std::stof(match[5].str());
        parameter.step = std::stof(match[6].str());
        parameter.value = parameter.defaultValue;
        parameters.push_back(std::move(parameter));
    }

    return parameters;
}

std::string GeraNESApp::sanitizeShaderTextForCompilation(const std::string& shaderText) const
{
    std::regex pragmaPattern(R"(^\s*#pragma\s+parameter[^\n]*\n?)", std::regex::icase | std::regex::multiline);
    return std::regex_replace(shaderText, pragmaPattern, "");
}

bool GeraNESApp::compileShaderProgram(GLShaderProgram& program, const std::string& path, const std::map<std::string, float>* parameterValues, std::vector<ShaderPass::Parameter>* outParameters)
{
    auto fs2 = cmrc::resources::get_filesystem();
    auto shaderFile = fs2.open("resources/default.glsl");
    std::string shaderText(shaderFile.begin(), shaderFile.end());

    if(path != "") {
        std::ifstream file(path);
        shaderText = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    std::vector<ShaderPass::Parameter> parameters = parseShaderParameters(shaderText);
    for(ShaderPass::Parameter& parameter : parameters) {
        if(parameterValues == nullptr) continue;
        auto it = parameterValues->find(parameter.name);
        if(it != parameterValues->end()) {
            parameter.value = std::clamp(it->second, parameter.minValue, parameter.maxValue);
        }
    }
    if(outParameters != nullptr) {
        *outParameters = parameters;
    }

    shaderText = sanitizeShaderTextForCompilation(shaderText);

    const std::string shaderLabel = path.empty() ? "default shader" : path;
    auto shaderErrorText = [&shaderLabel](const char* stage, const std::string& driverLog) {
        if(driverLog.empty()) {
            return std::string("Failed to compile ") + stage + " for " + shaderLabel +
                ". The graphics driver returned an empty error log.";
        }
        return std::string("Failed to compile ") + stage + " for " + shaderLabel + ":\n" + driverLog;
    };

    auto shaderLinkErrorText = [&shaderLabel](const std::string& driverLog) {
        if(driverLog.empty()) {
            return std::string("Failed to link shader program for ") + shaderLabel +
                ". The graphics driver returned an empty error log.";
        }
        return std::string("Failed to link shader program for ") + shaderLabel + ":\n" + driverLog;
    };

    std::string vertexText;
    std::string fragmentText;

    std::regex versionPattern(R"(^#version[^\n]*\n)", std::regex::icase);

    if(std::regex_search(shaderText, versionPattern)) {
        const std::string parameterDefine = parameters.empty() ? "" : "#define PARAMETER_UNIFORM\n";
        vertexText = std::regex_replace(shaderText, versionPattern, std::string("$&") + parameterDefine + "#define VERTEX\n");
        fragmentText = std::regex_replace(shaderText, versionPattern, std::string("$&") + parameterDefine + "#define FRAGMENT\n");
    } else {
        const std::string parameterDefine = parameters.empty() ? "" : "#define PARAMETER_UNIFORM\n";
        vertexText = parameterDefine + "#define VERTEX\n" + shaderText;
        fragmentText = parameterDefine + "#define FRAGMENT\n" + shaderText;
    }

    program.destroy();
    program.create();

    if(!program.addShaderFromSourceCode(GLShaderProgram::Vertex, vertexText.c_str())) {
        Logger::instance().log(shaderErrorText("vertex shader", program.lastError()), Logger::Type::ERROR);
        program.destroy();
        return false;
    }

    if(!program.addShaderFromSourceCode(GLShaderProgram::Fragment, fragmentText.c_str())) {
        Logger::instance().log(shaderErrorText("fragment shader", program.lastError()), Logger::Type::ERROR);
        program.destroy();
        return false;
    }

    program.bindAttributeLocation("VertexCoord", 0);
    program.bindAttributeLocation("TexCoord", 1);
    program.bindAttributeLocation("COLOR", 2);

    if(!program.link()) {
        Logger::instance().log(shaderLinkErrorText(program.lastError()), Logger::Type::ERROR);
        program.destroy();
        return false;
    }

    return true;
}

void GeraNESApp::destroyPostProcessTargets()
{
    for(PostProcessTarget& target : m_postProcessTargets) {
        if(target.fbo != 0) {
            glDeleteFramebuffers(1, &target.fbo);
            target.fbo = 0;
        }
        if(target.texture != 0) {
            glDeleteTextures(1, &target.texture);
            target.texture = 0;
        }
        target.width = 0;
        target.height = 0;
    }
}

bool GeraNESApp::ensurePostProcessTargets(int width, int height)
{
    if(width <= 0 || height <= 0) return false;

    for(PostProcessTarget& target : m_postProcessTargets) {
        if(target.fbo != 0 && target.texture != 0 && target.width == width && target.height == height) {
            continue;
        }

        if(target.fbo == 0) glGenFramebuffers(1, &target.fbo);
        if(target.texture == 0) glGenTextures(1, &target.texture);

        glBindTexture(GL_TEXTURE_2D, target.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            Logger::instance().log("Failed to create post-process framebuffer.", Logger::Type::ERROR);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            return false;
        }

        target.width = width;
        target.height = height;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void GeraNESApp::updateBuffers()
{
    if(!m_vbo.isCreated()) {
        m_vbo.create();
    }

    std::array<GLfloat, 16> data = {};
    auto setVertex = [&data](size_t vertexIndex, GLfloat x, GLfloat y, GLfloat u, GLfloat v) {
        const size_t base = vertexIndex * 4u;
        data[base + 0u] = x;
        data[base + 1u] = y;
        data[base + 2u] = u;
        data[base + 3u] = v;
    };
    SDL_Rect clientArea = emulatorClientArea();
    int drawableW = 0;
    int drawableH = 0;
    SDL_GL_GetDrawableSize(sdlWindow(), &drawableW, &drawableH);
    const GLfloat drawableScaleX = this->width() > 0 ? static_cast<GLfloat>(drawableW) / static_cast<GLfloat>(this->width()) : 1.0f;
    const GLfloat drawableScaleY = this->height() > 0 ? static_cast<GLfloat>(drawableH) / static_cast<GLfloat>(this->height()) : 1.0f;
    const int clientDrawableX = static_cast<int>(std::round(clientArea.x * drawableScaleX));
    const int clientDrawableY = static_cast<int>(std::round(clientArea.y * drawableScaleY));
    const int clientDrawableW = std::max(0, static_cast<int>(std::round(clientArea.w * drawableScaleX)));
    const int clientDrawableH = std::max(0, static_cast<int>(std::round(clientArea.h * drawableScaleY)));
    const GLfloat sourceWidth = static_cast<GLfloat>(PPU::SCREEN_WIDTH);
    const GLfloat sourceHeight = static_cast<GLfloat>(PPU::SCREEN_HEIGHT - 2 * m_clipHeightValue);
    GLfloat drawWidth = 0.0f;
    GLfloat drawHeight = 0.0f;

    auto pixelPerfectScale = [this, clientDrawableW, clientDrawableH](int fixedScale) {
        if(fixedScale > 0) return fixedScale;
        const int maxScaleX = clientDrawableW / PPU::SCREEN_WIDTH;
        const int maxScaleY = clientDrawableH / (PPU::SCREEN_HEIGHT - 2 * m_clipHeightValue);
        return std::max(1, std::min(maxScaleX, maxScaleY));
    };

    switch(m_videoScaleMode) {
        case ASPECT_FIT:
            if(clientArea.w / sourceWidth >= clientArea.h / sourceHeight) {
                drawHeight = static_cast<GLfloat>(clientArea.h);
                drawWidth = sourceWidth / sourceHeight * drawHeight;
            } else {
                drawWidth = static_cast<GLfloat>(clientArea.w);
                drawHeight = sourceHeight / sourceWidth * drawWidth;
            }
            break;
        case STRETCH_TO_FILL:
            if(clientArea.w / sourceWidth >= clientArea.h / sourceHeight) {
                drawWidth = static_cast<GLfloat>(clientArea.w);
                drawHeight = static_cast<GLfloat>(clientArea.h);
            } else {
                drawWidth = static_cast<GLfloat>(clientArea.w);
                drawHeight = sourceHeight / sourceWidth * drawWidth;
            }
            break;
        case PIXEL_PERFECT:
        case PIXEL_PERFECT_BEST_FIT: {
            const int fixedScale = m_videoScaleMode == PIXEL_PERFECT ? m_pixelPerfectScale : 0;
            const int scale = pixelPerfectScale(fixedScale);
            drawWidth = sourceWidth * static_cast<GLfloat>(scale) / drawableScaleX;
            drawHeight = sourceHeight * static_cast<GLfloat>(scale) / drawableScaleY;
            break;
        }
    }

    GLfloat minX = clientArea.x + (clientArea.w - drawWidth) / 2.0f;
    GLfloat minY = clientArea.y + (clientArea.h - drawHeight) / 2.0f;
    GLfloat maxX = minX + drawWidth;
    GLfloat maxY = minY + drawHeight;

    if(m_videoScaleMode == PIXEL_PERFECT || m_videoScaleMode == PIXEL_PERFECT_BEST_FIT) {
        const int minDrawableX = clientDrawableX + (clientDrawableW - static_cast<int>(std::round(drawWidth * drawableScaleX))) / 2;
        const int minDrawableY = clientDrawableY + (clientDrawableH - static_cast<int>(std::round(drawHeight * drawableScaleY))) / 2;
        const int maxDrawableX = minDrawableX + static_cast<int>(std::round(drawWidth * drawableScaleX));
        const int maxDrawableY = minDrawableY + static_cast<int>(std::round(drawHeight * drawableScaleY));
        minX = static_cast<GLfloat>(minDrawableX) / drawableScaleX;
        minY = static_cast<GLfloat>(minDrawableY) / drawableScaleY;
        maxX = static_cast<GLfloat>(maxDrawableX) / drawableScaleX;
        maxY = static_cast<GLfloat>(maxDrawableY) / drawableScaleY;
    }

    m_nesScreenRect.min = glm::vec2(minX, minY);
    m_nesScreenRect.max = glm::vec2(maxX, maxY);

    setVertex(0u, minX, minY, 0.0f, m_clipHeightValue / 256.0f);
    setVertex(1u, minX, maxY, 0.0f, 240.0f / 256.0f - m_clipHeightValue / 256.0f);
    setVertex(2u, maxX, minY, 1.0f, m_clipHeightValue / 256.0f);
    setVertex(3u, maxX, maxY, 1.0f, 240.0f / 256.0f - m_clipHeightValue / 256.0f);

    m_vbo.bind();
    if(static_cast<size_t>(m_vbo.size()) != sizeof(data)) {
        m_vbo.allocate(data.data(), sizeof(data));
    } else {
        m_vbo.write(0, data.data(), sizeof(data));
    }
    m_vbo.release();

    if(m_touch) m_touch->setTopMargin(emulatorClientArea().y);
}

bool GeraNESApp::onEvent(SDL_Event& event)
{
    const bool pointerGrabActive = m_snesMouseGrabActive || m_arkanoidGrabActive;
    const bool isMouseEvent =
        event.type == SDL_MOUSEMOTION ||
        event.type == SDL_MOUSEBUTTONDOWN ||
        event.type == SDL_MOUSEBUTTONUP ||
        event.type == SDL_MOUSEWHEEL;

    if(!(pointerGrabActive && isMouseEvent)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
    }

    ImGuiIO& io = ImGui::GetIO();

    m_imGuiWantsMouse = pointerGrabActive ? false : io.WantCaptureMouse;
    bool imGuiWantsKeyboard = io.WantCaptureKeyboard;
    if(m_forceImGuiMouseResync && !pointerGrabActive) {
        int mx = 0;
        int my = 0;
        const Uint32 buttons = SDL_GetMouseState(&mx, &my);
        io.AddMousePosEvent(static_cast<float>(mx), static_cast<float>(my));
        io.AddMouseButtonEvent(0, (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0);
        io.AddMouseButtonEvent(1, (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0);
        io.AddMouseButtonEvent(2, (buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0);
        m_forceImGuiMouseResync = false;
        m_imGuiWantsMouse = io.WantCaptureMouse;
    }

    switch(event.type) {
        case SDL_KEYDOWN: {
            std::string keyName = SDL_GetKeyName(event.key.keysym.sym);
            const bool hasAltModifier = (event.key.keysym.mod & KMOD_ALT) != 0;
            if(hasAltModifier) keyName = "Alt+" + keyName;
            const bool allowGlobalShortcutWhileImGuiFocused = hasAltModifier || keyName == "Escape";

            if(imGuiWantsKeyboard && !allowGlobalShortcutWhileImGuiFocused) break;

            m_shortcuts.invokeShortcut(keyName);

            if(keyName == "Escape" && m_emuInputEnabled) {
                if(m_snesMouseGrabActive) {
                    setSnesMouseGrab(false);
                } else if(m_arkanoidGrabActive) {
                    setArkanoidGrab(false);
                } else {
                    m_showMenuBar = !m_showMenuBar;
                    m_updateObjectsFlag = true;
                }
            }

            break;
        }

        case SDL_MOUSEBUTTONDOWN:
            if(!m_imGuiWantsMouse && !m_touch->buttons().anyPressed() && (isSnesMouseActive() || isSuborMouseActive())) {
                if(event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT) {
                    if(pointInRect(glm::vec2(static_cast<float>(event.button.x), static_cast<float>(event.button.y)), m_nesScreenRect)) {
                        setSnesMouseGrab(true);
                    }
                }
            }
            if(!m_imGuiWantsMouse && !m_touch->buttons().anyPressed() && isArkanoidActive()) {
                if(event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT) {
                    if(pointInRect(glm::vec2(static_cast<float>(event.button.x), static_cast<float>(event.button.y)), m_nesScreenRect)) {
                        setArkanoidGrab(true);
                    }
                }
            }
            break;

        case SDL_WINDOWEVENT:
            switch(event.window.event) {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    updateMVP();
                    m_touch->onResize(this->width(), this->height());
                    m_updateObjectsFlag = true;
                    break;

                case SDL_WINDOWEVENT_MINIMIZED:
                    break;

                case SDL_WINDOWEVENT_RESTORED:
                case SDL_WINDOWEVENT_MAXIMIZED:
                case SDL_WINDOWEVENT_SHOWN:
                    break;
            }
            break;
    }

    const bool isFingerEvent =
        event.type == SDL_FINGERDOWN ||
        event.type == SDL_FINGERUP ||
        event.type == SDL_FINGERMOTION;
    if(isFingerEvent || !m_imGuiWantsMouse) m_touch->onEvent(event);

    return SDLOpenGLWindow::onEvent(event);
}

void GeraNESApp::onWindowsTitleBarInteractionChanged(bool active)
{
    m_emu.setPresenterLockActive(!active);
}

void GeraNESApp::onWindowDisplayChanged(int displayIndex)
{
    (void)displayIndex;
    updateVSyncConfig();
    m_mainLoopLastCounter = currentMainLoopCounter();
    m_mainLoopCounterFrequency = currentMainLoopCounterFrequency();
    m_mainLoopCounterRemainder = 0;
    m_presenterFrameAccumScaled = 0;
    m_presenterStepRemainder = 0;
}

void GeraNESApp::mainLoop()
{
    updateCursor();

    const Uint64 counterNow = currentMainLoopCounter();
    Uint64 counterFreq = currentMainLoopCounterFrequency();
    if(counterFreq == 0) {
        counterFreq = 1;
    }
    if(m_mainLoopCounterFrequency != counterFreq) {
        m_mainLoopCounterFrequency = counterFreq;
        m_mainLoopCounterRemainder = 0;
    }
    if(m_mainLoopLastCounter == 0) {
        m_mainLoopLastCounter = counterNow;
        return;
    }

    const Uint64 counterDelta = counterNow - m_mainLoopLastCounter;
    m_mainLoopLastCounter = counterNow;
    const Uint64 scaledMs = counterDelta * 1000u + m_mainLoopCounterRemainder;
    Uint64 dt = scaledMs / m_mainLoopCounterFrequency;
    m_mainLoopCounterRemainder = scaledMs % m_mainLoopCounterFrequency;

    if(dt > 0) {
        m_lastMainLoopDtMs = dt;
    }

    m_touch->update(dt);
    dispatch_queued_calls();
    applyEffectiveRewindSettings();
    pollAndPrepareInput();

    m_fpsTimer += dt;

    if(m_fpsTimer >= 1000) {
        int cycles = m_fpsTimer / 1000;
        m_fps = m_frameCounter / cycles;
        m_frameCounter = 0;
        m_fpsTimer %= 1000;
    }

    bool netplayPacingOverrideActive = false;
    netplayPacingOverrideActive = m_netplayRuntime.uiSnapshot().active;
    const uint32_t pacingDtMs = static_cast<uint32_t>(std::min<Uint64>(dt, UINT32_MAX));
    const bool minimized = isMinimized();
    const bool allowPresenterPacing =
        !minimized &&
        !isWindowsTitleBarInteractionActive();
    if(!allowPresenterPacing) {
        m_presenterFrameAccumScaled = 0;
        m_presenterStepRemainder = 0;
        if(!netplayPacingOverrideActive) {
            m_emu.setSimulationSuspended(false);
        }
        const bool advanced = m_emu.update(dt);
        m_netplayRuntime.recordFramePacing(
            pacingDtMs,
            advanced ? 1u : 0u,
            0u,
            netplayPacingOverrideActive,
            false
        );
        if(advanced) render();
    } else {
        const uint32_t emuFps = std::max<uint32_t>(1u, m_emu.getRegionFPS());
        const bool vsyncEnabled = m_vsyncMode != OFF;
        const int displayFrameRate = vsyncEnabled ? this->getDisplayFrameRate() : 0;
        bool monitorCadenceMatchesEmu =
            vsyncEnabled &&
            displayFrameRate > 0 &&
            std::abs(displayFrameRate - static_cast<int>(emuFps)) <= 1;
#ifdef __EMSCRIPTEN__
        // On mobile web, refresh rate can jump (e.g. 60 <-> 120) during touch.
        // Keep pacing time-driven to avoid cadence-path oscillation jitter.
        monitorCadenceMatchesEmu = false;
#endif

        if(monitorCadenceMatchesEmu) {
            const uint32_t stepNumerator = m_presenterStepRemainder + 1000u;
            uint32_t stepDtMs = stepNumerator / emuFps;
            m_presenterStepRemainder = stepNumerator % emuFps;
            stepDtMs = std::max<uint32_t>(1u, stepDtMs);
            m_presenterFrameAccumScaled = 0;
            m_emu.updateUntilFrame(stepDtMs);
            render();
            m_frameCounter++;
            m_netplayRuntime.recordFramePacing(
                pacingDtMs,
                1u,
                0u,
                netplayPacingOverrideActive,
                true
            );
            if(AppSettings::instance().data.debug.cpuDebuggerEnabled ||
               m_pendingEnableCpuDebuggerAfterNetplayDisconnect) {
                syncCpuDebugRuntimeState();
            }
            return;
        }

        const uint64_t pacingScaleDenominator = std::max<uint64_t>(1u, m_mainLoopCounterFrequency);
        m_presenterFrameAccumScaled += counterDelta * static_cast<uint64_t>(emuFps);

        const int MAX_FRAMES_TO_ADVANCE = 30;
        uint32_t framesToAdvance = 0u;
        while(m_presenterFrameAccumScaled >= pacingScaleDenominator && framesToAdvance < MAX_FRAMES_TO_ADVANCE) {
            ++framesToAdvance;
            m_presenterFrameAccumScaled -= pacingScaleDenominator;
        }

        if(framesToAdvance == MAX_FRAMES_TO_ADVANCE && m_presenterFrameAccumScaled > pacingScaleDenominator) {
            m_presenterFrameAccumScaled = pacingScaleDenominator;
        }

        for(uint32_t i = 0u; i < framesToAdvance; ++i) {
            const uint32_t stepNumerator = m_presenterStepRemainder + 1000u;
            uint32_t stepDtMs = stepNumerator / emuFps;
            m_presenterStepRemainder = stepNumerator % emuFps;
            stepDtMs = std::max<uint32_t>(1u, stepDtMs);
            m_emu.updateUntilFrame(stepDtMs);
        }
        m_netplayRuntime.recordFramePacing(
            pacingDtMs,
            framesToAdvance,
            framesToAdvance > 1u ? framesToAdvance : 0u,
            netplayPacingOverrideActive,
            false
        );
        if(framesToAdvance > 0u) {
            render();
        }
    }
    if(AppSettings::instance().data.debug.cpuDebuggerEnabled ||
       m_pendingEnableCpuDebuggerAfterNetplayDisconnect) {
        syncCpuDebugRuntimeState();
    }

    m_frameCounter++;
}

#include "GeraNESApp/GeraNESApp.MenuUI.inl"
#include "GeraNESApp/GeraNESApp.Render.inl"
#include "GeraNESApp/GeraNESApp.WindowsUI.inl"
