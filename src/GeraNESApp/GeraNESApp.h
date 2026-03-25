#pragma once

#include <SDL.h>

//#include <GL/glu.h>
#include <iostream>

#include <filesystem>
namespace fs = std::filesystem;

#include "CppGL/GLHeaders.h"

#include "imgui_include.h"
#include "imgui_util.h"
#include "ImGuiTheme.h"

#include "InputBindingConfigWindow.h"
#include "PowerPadConfigWindow.h"
#include "ShortcutManager.h"

#ifdef __EMSCRIPTEN__
    #include "EmscriptenUtil.h"
#else
    #include <nfd.h>
#endif

#include <vector>

#include "util/SdlCursor.h"

#include <functional>
#include <iterator>
#include <regex>

#include <glm/glm.hpp>

#include <nlohmann/json.hpp>

#include "logger/logger.h"

#include "CppGL/CppGL.h"
#include "SDLOpenGLWindow.h"

#include "GeraNESApp/EmulationHost.h"

#include "GeraNES/defines.h"

#ifdef __EMSCRIPTEN__
    #include "GeraNESApp/OpenALAudioOutput.h"
    typedef OpenALAudioOutput AudioOutput;
#else   
    #include "GeraNESApp/SDLAudioOutput.h"
    typedef SDLAudioOutput AudioOutput;
#endif


#include "GeraNESApp/InputManager.h"
#include "GeraNESApp/ControllerInfo.h"
#include "GeraNESApp/AppSettings.h"

#include "GeraNES/util/CircularBuffer.h"

#include "signal/signal.h"

#include "cmrc/cmrc.hpp"
#include "util/const_util.h"

CMRC_DECLARE(resources);

#include "TouchControls.h"
#include "UserToastNotifier.h"
#ifdef ENABLE_NSF_PLAYER
#include "NsfVisualizerUI.h"
#endif

const std::string LOG_FILE = "log.txt";

class GeraNESApp : public SDLOpenGLWindow, public SigSlot::SigSlotBase {

private:

    InputBindingConfigWindow m_inputBindingConfigWindow;
    PowerPadConfigWindow m_powerPadConfigWindow;

    std::vector<std::string> m_audioDevices;

    bool m_horizontalStretch = false;
    int m_clipHeightValue = 8;

    GLVertexArrayObject m_vao;
    GLVertexBufferObject m_vbo;

    GLShaderProgram m_shaderProgram;

    bool m_updateObjectsFlag = true;

    GLuint m_texture = 0;

    bool m_fullScreen = false;

    glm::mat4x4 m_mvp = glm::mat4x4(1.0f);

    AudioOutput m_audioOutput;

    EmulationHost m_emu;

    ControllerInfo m_controller1;
    ControllerInfo m_controller2;
    ControllerInfo m_controller3;
    ControllerInfo m_controller4;
    PowerPadInfo m_powerPadInfo;
    SnesControllerInfo m_snesController1;
    SnesControllerInfo m_snesController2;
    VirtualBoyControllerInfo m_virtualBoyController1;
    VirtualBoyControllerInfo m_virtualBoyController2;
    KonamiHyperShotInfo m_konamiHyperShot;
    SystemInputInfo m_systemInput;

    bool m_emuInputEnabled = true;

    enum VSyncMode {OFF, SYNCRONIZED, ADAPTATIVE};
    VSyncMode m_vsyncMode = OFF;

    enum FilterMode {NEAREST, BILINEAR};
    FilterMode m_filterMode = NEAREST;

    bool m_showImprovementsWindow = false;
    bool m_showAboutWindow = false;
    bool m_showRomDatabaseWindow = false;
    bool m_showArkanoidNesConfigWindow = false;
    bool m_showArkanoidFamicomConfigWindow = false;
    bool m_showSnesMouseConfigWindow = false;
    bool m_showKonamiHyperShotConfigWindow = false;

    bool m_showMenuBar = true;

    std::string m_errorMessage = "";
    bool m_showErrorWindow = false;

    ShortcutManager m_shortcuts;

    int m_menuBarHeight = 0;

    std::vector<char> m_logBuf = {'\0'};
    std::string m_log = "";
    bool m_showLogWindow = false;
    UserToastNotifier m_userToast;

    std::vector<uint8_t> m_embeddedUiFontData;
    ImFont* m_fontNsfTitle = nullptr;
    ImFont* m_fontNsfSubtitle = nullptr;
    ImFont* m_fontToast = nullptr;
    ImFont* m_fontFps = nullptr;
#ifdef ENABLE_NSF_PLAYER
    NsfVisualizerUI m_nsfVisualizer;
#endif

    struct RomDatabaseEditorData {
        bool loaded = false;
        bool foundInDatabase = false;
        std::string statusMessage = "No ROM loaded";

        std::string PrgChrCrc32;
        std::string System;
        std::string Board;
        std::string PCB;
        std::string Chip;
        std::string Mapper;
        std::string PrgRomSize;
        std::string ChrRomSize;
        std::string ChrRamSize;
        std::string WorkRamSize;
        std::string SaveRamSize;
        std::string HasBattery;
        std::string Mirroring;
        std::string InputType;
        std::string BusConflicts;
        std::string SubMapperId;
        std::string VsSystemType;
        std::string VsPpuModel;
    } m_romDbEditor;
    RomDatabaseEditorData m_romDbSaved;

    Uint64 m_mainLoopLastTime = 0;

    Rect m_nesScreenRect = {{0,0}, {1,1}};

    bool m_imGuiWantsMouse = false;
    bool m_cursorVisible = true;
    bool m_arkanoidGrabActive = false;
    bool m_arkanoidSuppressClickUntilRelease = false;
    bool m_snesMouseGrabActive = false;
    bool m_snesMouseSuppressClickUntilRelease = false;
    bool m_forceImGuiMouseResync = false;
    bool m_hasLastMousePosition = false;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
    float m_arkanoidNesPosition = 0.5f;
    float m_arkanoidFamicomPosition = 0.5f;

    // FPS vars    
    Uint64 m_fpsTimer = 0;
    int m_fps = 0;
    int m_frameCounter = 0;

    static constexpr std::array<const char*, 3> VSYNC_TYPE_LABELS {"Off", "Syncronized", "Adaptative"};
    static constexpr std::array<const char*, 3> FILTER_TYPE_LABELS {"Nearest", "Bilinear"};    

    struct AudioChannelControl {
        std::string source;
        std::string id;
        std::string label;
        float volume = 1.0f;
        float min = 0.0f;
        float max = 1.0f;
    };

    struct ShaderItem {
        std::string label;
        std::string path;
    };

    std::vector<ShaderItem> shaderList;

    std::unique_ptr<TouchControls> m_touch;

    std::optional<SdlCursor> m_defaultCursor;
    std::optional<SdlCursor> m_crossCursor;
    std::optional<SdlCursor> m_sizeWECursor;
    static void setIfNegative(std::string& dst, int value)
    {
        dst = value >= 0 ? std::to_string(value) : "";
    }

    static void setIfNegativeKb(std::string& dst, int bytesValue)
    {
        dst = bytesValue >= 0 ? std::to_string(bytesValue / 1024) : "";
    }

    void loadRomDatabaseEditorFromCurrentRom()
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

    void saveRomDatabaseEditor()
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
        }
        else {
            if(error.empty()) error = "Failed to save ROM database entry";
            Logger::instance().log(error, Logger::Type::ERROR);
        }
    }

    void removeRomDatabaseEditor()
    {
        if(!m_romDbEditor.loaded || !m_romDbEditor.foundInDatabase) return;

        std::string error;
        if(GameDatabase::instance().removeByCrc(m_romDbEditor.PrgChrCrc32, &error)) {
            Logger::instance().log("ROM database entry removed", Logger::Type::USER);
            loadRomDatabaseEditorFromCurrentRom();
        }
        else {
            if(error.empty()) error = "Failed to remove ROM database entry";
            Logger::instance().log(error, Logger::Type::ERROR);
        }
    }

    void updateMVP() {
        glm::mat4 proj = glm::ortho(0.0f, (float)width(), (float)height(), 0.0f, -1.0f, 1.0f);           
        m_mvp = proj * glm::mat4(1.0f);
    }    

    void onLog(const std::string& msg, Logger::Type type) {
        if(type == Logger::Type::USER) {
            m_userToast.show(msg);
            return;
        }

        std::ofstream file(LOG_FILE, std::ios_base::app);
        file << msg << std::endl;
        std::cout << msg << std::endl;

        if(type == Logger::Type::ERROR) {
            m_errorMessage = msg;
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

        m_log += msgType + msg + "\n";

        size_t needed = m_log.size() + 1;
        if (m_logBuf.capacity() < needed) {
            m_logBuf.reserve( std::max(needed, m_logBuf.capacity() * 2) );
        }

        m_logBuf.resize(needed);
        memcpy(m_logBuf.data(), m_log.c_str(), needed);
    }

    std::tuple<int, int> getNesCursor(int screenX, int screenY) {

        int nesX = ((screenX - m_nesScreenRect.min.x) * PPU::SCREEN_WIDTH) / m_nesScreenRect.getWidth();
        int clipTop = m_clipHeightValue;
        int visibleNES = PPU::SCREEN_HEIGHT - 2*m_clipHeightValue;
        int nesY = clipTop + ((screenY - m_nesScreenRect.min.y) * visibleNES) / m_nesScreenRect.getHeight();

        return std::make_tuple(nesX, nesY);
    }

    std::tuple<int, int> getClampedNesCursor(int screenX, int screenY)
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

    bool isSnesMouseActive() const
    {
        return m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::SNES_MOUSE) ||
               m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::SNES_MOUSE);
    }

    bool isSuborMouseActive() const
    {
        return m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE) ||
               m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::SUBOR_MOUSE);
    }

    bool isSuborKeyboardActive() const
    {
        return m_emu.getExpansionDevice() == Settings::ExpansionDevice::SUBOR_KEYBOARD;
    }

    bool isFamilyBasicKeyboardActive() const
    {
        return m_emu.getExpansionDevice() == Settings::ExpansionDevice::FAMILY_BASIC_KEYBOARD;
    }

    static IExpansionDevice::SuborKeyboardKeys captureSuborKeyboardState()
    {
        IExpansionDevice::SuborKeyboardKeys keys = {};
        const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);

        auto pressed = [&](SDL_Scancode scancode) {
            return keyboardState[scancode] != 0;
        };

        auto pressedAny = [&](std::initializer_list<SDL_Scancode> scancodes) {
            for(SDL_Scancode scancode : scancodes) {
                if(keyboardState[scancode] != 0) return true;
            }
            return false;
        };

        auto set = [&](SuborKeyboard::Button button, bool value) {
            keys[static_cast<size_t>(button)] = value;
        };

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

    static IExpansionDevice::FamilyBasicKeyboardKeys captureFamilyBasicKeyboardState()
    {
        IExpansionDevice::FamilyBasicKeyboardKeys keys = {};
        const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);

        auto pressed = [&](SDL_Scancode scancode) {
            return keyboardState[scancode] != 0;
        };

        auto pressedAny = [&](std::initializer_list<SDL_Scancode> scancodes) {
            for(SDL_Scancode scancode : scancodes) {
                if(keyboardState[scancode] != 0) return true;
            }
            return false;
        };

        auto set = [&](FamilyBasicKeyboard::Button button, bool value) {
            keys[static_cast<size_t>(button)] = value;
        };

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

    bool isArkanoidActive() const
    {
        return m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) ||
               m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) ||
               m_emu.getExpansionDevice() == Settings::ExpansionDevice::ARKANOID_CONTROLLER;
    }

    void setArkanoidGrab(bool active)
    {
        if(m_arkanoidGrabActive == active) return;
        m_arkanoidGrabActive = active;
#ifndef __EMSCRIPTEN__
        SDL_SetWindowGrab(sdlWindow(), active ? SDL_TRUE : SDL_FALSE);
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

    void setSnesMouseGrab(bool active)
    {
        if(m_snesMouseGrabActive == active) return;
        m_snesMouseGrabActive = active;
#ifndef __EMSCRIPTEN__
        SDL_SetWindowGrab(sdlWindow(), active ? SDL_TRUE : SDL_FALSE);
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

    void onFrameStart() { 

        if(m_emuInputEnabled) {

            InputManager& im = InputManager::instance();        

            im.updateInputs();

            const bool p1A = im.isPressed(m_controller1.a) || (!m_imGuiWantsMouse && m_touch->buttons().a);
            const bool p1B = im.isPressed(m_controller1.b) || (!m_imGuiWantsMouse &&m_touch->buttons().b);
            const bool p1Select = im.isPressed(m_controller1.select) || (!m_imGuiWantsMouse &&m_touch->buttons().select);
            const bool p1Start = im.isPressed(m_controller1.start) || (!m_imGuiWantsMouse &&m_touch->buttons().start);
            const bool p1Up = im.isPressed(m_controller1.up) || (!m_imGuiWantsMouse &&m_touch->buttons().up);
            const bool p1Down = im.isPressed(m_controller1.down) || (!m_imGuiWantsMouse &&m_touch->buttons().down);
            const bool p1Left = im.isPressed(m_controller1.left) || (!m_imGuiWantsMouse &&m_touch->buttons().left);
            const bool p1Right = im.isPressed(m_controller1.right) || (!m_imGuiWantsMouse &&m_touch->buttons().right);
            const bool p1X = im.isPressed(m_snesController1.x);
            const bool p1Y = im.isPressed(m_snesController1.y);
            const bool p1L = im.isPressed(m_snesController1.l);
            const bool p1R = im.isPressed(m_snesController1.r);

            const bool p2A = im.isPressed(m_controller2.a);
            const bool p2B = im.isPressed(m_controller2.b);
            const bool p2Select = im.isPressed(m_controller2.select);
            const bool p2Start = im.isPressed(m_controller2.start);
            const bool p2Up = im.isPressed(m_controller2.up);
            const bool p2Down = im.isPressed(m_controller2.down);
            const bool p2Left = im.isPressed(m_controller2.left);
            const bool p2Right = im.isPressed(m_controller2.right);
            const bool p2X = im.isPressed(m_snesController2.x);
            const bool p2Y = im.isPressed(m_snesController2.y);
            const bool p2L = im.isPressed(m_snesController2.l);
            const bool p2R = im.isPressed(m_snesController2.r);
            const bool p3A = im.isPressed(m_controller3.a);
            const bool p3B = im.isPressed(m_controller3.b);
            const bool p3Select = im.isPressed(m_controller3.select);
            const bool p3Start = im.isPressed(m_controller3.start);
            const bool p3Up = im.isPressed(m_controller3.up);
            const bool p3Down = im.isPressed(m_controller3.down);
            const bool p3Left = im.isPressed(m_controller3.left);
            const bool p3Right = im.isPressed(m_controller3.right);
            const bool p4A = im.isPressed(m_controller4.a);
            const bool p4B = im.isPressed(m_controller4.b);
            const bool p4Select = im.isPressed(m_controller4.select);
            const bool p4Start = im.isPressed(m_controller4.start);
            const bool p4Up = im.isPressed(m_controller4.up);
            const bool p4Down = im.isPressed(m_controller4.down);
            const bool p4Left = im.isPressed(m_controller4.left);
            const bool p4Right = im.isPressed(m_controller4.right);
            const bool konamiP1Run = im.isPressed(m_konamiHyperShot.p1Run);
            const bool konamiP1Jump = im.isPressed(m_konamiHyperShot.p1Jump);
            const bool konamiP2Run = im.isPressed(m_konamiHyperShot.p2Run);
            const bool konamiP2Jump = im.isPressed(m_konamiHyperShot.p2Jump);
            const bool p1UsesSnesController = m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::SNES_CONTROLLER);
            const bool p2UsesSnesController = m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::SNES_CONTROLLER);
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
                if(im.isJustPressed(m_systemInput.saveState)) m_emu.saveState();
                if(im.isJustPressed(m_systemInput.loadState)) m_emu.loadState();
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
                int mx, my;
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
                } else if(!useSnesMouse) {
                    if(!m_hasLastMousePosition) {
                        m_lastMouseX = mx;
                        m_lastMouseY = my;
                        m_hasLastMousePosition = true;
                    }
                    mouseDeltaX = mx - m_lastMouseX;
                    mouseDeltaY = my - m_lastMouseY;
                    m_lastMouseX = mx;
                    m_lastMouseY = my;
                }

                zapperX = rightClick ? -1 : nesX;
                zapperY = rightClick ? -1 : nesY;
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
            m_emu.setPendingInput(inputState);
        }
        else {
            m_emu.setPendingInput({});
        }

    }

    void openFile(const char* path) {

        AppSettings::instance().data.addRecentFile(path);
        AppSettings::instance().data.setLastFolder(path);
        if(m_emu.open(path)) {
            const std::string filename = fs::path(path).filename().string();
            setTitle((std::string("GeraNES (") + filename + ")").c_str());
            Logger::instance().log("Rom loaded", Logger::Type::USER);
        }
        else {
            Logger::instance().log("Failed to load ROM", Logger::Type::USER);
        }
    }

    void syncSettings() {

        AppSettings::instance().load();

        auto cfg = AppSettings::instance().data;

        m_emu.configAudioDevice(cfg.audio.audioDevice);
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

        m_emu.setupRewindSystem(cfg.improvements.maxRewindTime > 0, cfg.improvements.maxRewindTime);
        m_emu.disableSpriteLimit(cfg.improvements.disableSpritesLimit);
        m_emu.enableOverclock(cfg.improvements.overclock);

        m_vsyncMode = (VSyncMode)cfg.video.vsyncMode;
        m_filterMode = (FilterMode)cfg.video.filterMode;
        m_horizontalStretch = cfg.video.horizontalStretch;
        m_fullScreen = cfg.video.fullScreen;
    }

    void createShortcuts() {
        
        // std::string key;
        // std::string label;
        // std::string shortcut;
        // std::function<void()> action;
        m_shortcuts.add(ShortcutManager::Data{"fullscreen", "Fullscreen", "Alt+F", [this]() {
            m_fullScreen = !isFullScreen();
            setFullScreen(m_fullScreen);            
            AppSettings::instance().data.video.fullScreen = m_fullScreen;
        }});

        m_shortcuts.add(ShortcutManager::Data{"openRom", "Open Rom", "Alt+O", [this]() {
            openRom();
        }});

        #ifndef __EMSCRIPTEN__
        m_shortcuts.add(ShortcutManager::Data{"quit", "Quit", "Alt+Q", [this]() {
            quit();
        }});
        #endif

        m_shortcuts.add(ShortcutManager::Data{"horizontalStretch", "Horizontal Stretch", "Alt+H", [this]() {
            m_horizontalStretch = !m_horizontalStretch;
            AppSettings::instance().data.video.horizontalStretch = m_horizontalStretch;
            m_updateObjectsFlag = true;
        }});

        m_shortcuts.add(ShortcutManager::Data{"saveState", "Save State", "Alt+S", [this]() {
            if(!m_emu.valid()) return;
            m_emu.saveState();
        }});

        m_shortcuts.add(ShortcutManager::Data{"loadState", "Load State", "Alt+L", [this]() {
            if(!m_emu.valid()) return;
            m_emu.loadState();
        }});

        m_shortcuts.add(ShortcutManager::Data{"pause", "Pause", "Alt+P", [this]() {
            if(!m_emu.valid()) return;
            m_emu.togglePaused();
        }});
    }

    void updateCursor() {

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

        int mx, my;
        SDL_GetMouseState(&mx, &my);

        bool inside = pointInRect(glm::vec2(mx,my), m_nesScreenRect);
        const bool useArkanoidCursor =
            m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) ||
            m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ARKANOID_CONTROLLER) ||
            m_emu.getExpansionDevice() == Settings::ExpansionDevice::ARKANOID_CONTROLLER;

        bool usePointerDevice =
            m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ZAPPER) ||
            m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ZAPPER) ||
            m_emu.getExpansionDevice() == Settings::ExpansionDevice::BANDAI_HYPERSHOT ||
            useArkanoidCursor;

        auto setCursorVisibility = [this](bool visible)
        {
            if(m_cursorVisible == visible) return;
            SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
            m_cursorVisible = visible;
        };

        if(!m_imGuiWantsMouse && useArkanoidCursor && inside) {
            setCursorVisibility(false);
        }
        else if(!m_imGuiWantsMouse && inside && usePointerDevice) {
            setCursorVisibility(true);
            if(m_crossCursor.has_value() && !m_crossCursor->isCurrent()) {
                m_crossCursor->setAsCurrent();
            }
        }
        else {
            setCursorVisibility(true);
            if(m_defaultCursor.has_value() && !m_defaultCursor->isCurrent())
                m_defaultCursor->setAsCurrent();
        }
    }

public:

    GeraNESApp() : m_emu(m_audioOutput) {

        //reset log file content
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
        
        #ifdef __EMSCRIPTEN__
            emcriptenRegisterAudioReset(reinterpret_cast<intptr_t>(this));
        #endif
        
    }

    void loadShaderList() {

        const char* SHADER_DIR = "shaders/";

        std::string dir = fs::path(SHADER_DIR).parent_path().string();
        if(!fs::exists(dir)) fs::create_directory(dir);

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (fs::is_regular_file(entry.path())) {
                ShaderItem item = {entry.path().filename().string(), entry.path().string()};         
                shaderList.push_back(item);
            }
        }

        std::sort(shaderList.begin(), shaderList.end(), [](const ShaderItem& a, const ShaderItem& b) {
        return a.label < b.label;
    });
    }

#ifdef __EMSCRIPTEN__

    void processUploadedFile(const char* fileName, size_t fileSize, const uint8_t* fileContent) {

        FILE* file = fopen(fileName, "w");

        if (file) {
            
            size_t written = fwrite(fileContent, sizeof(uint8_t), fileSize, file);

            if (written != fileSize) {
                Logger::instance().log("Failed writing file in processUploadedFile call", Logger::Type::ERROR);
            }

            fclose(file);

            openFile(fileName);

        } else {
            Logger::instance().log("Failed to open file for writing in processUploadedFile call", Logger::Type::ERROR);
        }

    }

    void restartAudioModule() {
        m_emu.restartAudio();
    }

    void onSessionImportComplete() {
        syncSettings();
    }

#endif

    void onCaptureBegin() {
        m_emuInputEnabled = false;
    }

    void onInputBindingCaptureEnd() {
        m_emuInputEnabled = true;        
        AppSettings::instance().data.input.setControllerInfo(0, m_controller1);
        AppSettings::instance().data.input.setControllerInfo(1, m_controller2);
        AppSettings::instance().data.input.setControllerInfo(2, m_controller3);
        AppSettings::instance().data.input.setControllerInfo(3, m_controller4);
        AppSettings::instance().data.input.powerPad = m_powerPadInfo;
        AppSettings::instance().data.input.setSnesControllerInfo(0, m_snesController1);
        AppSettings::instance().data.input.setSnesControllerInfo(1, m_snesController2);
        AppSettings::instance().data.input.setVirtualBoyControllerInfo(0, m_virtualBoyController1);
        AppSettings::instance().data.input.setVirtualBoyControllerInfo(1, m_virtualBoyController2);
        AppSettings::instance().data.input.konamiHyperShot = m_konamiHyperShot;
        AppSettings::instance().data.input.system = m_systemInput;
    }

    virtual ~GeraNESApp() {
        m_emu.shutdown();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    void openRom() {

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

        const bool restoreAfterDialog = isFullScreen();
#ifndef _WIN32
        if(restoreAfterDialog) minimizeWindow();
#endif

        NFD_Init();     

        nfdu8char_t *outPath = nullptr;
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

        nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);

        if (result == NFD_OKAY)
        {
            openFile(outPath);                        
            NFD_FreePathU8(outPath);
        }
        else if (result == NFD_CANCEL)
        {
            //puts("User pressed cancel.");
        }
        else 
        {
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

        if(restoreAfterDialog) restoreWindow();
    } 

    void updateVSyncConfig() {
        switch(m_vsyncMode) {
            case OFF: setVSync(0); break;
            case SYNCRONIZED: setVSync(1); break;
            case ADAPTATIVE: setVSync(-1); break;
        }
    }

    void updateFilterConfig() {
        switch(m_filterMode) {
            case NEAREST:
                glBindTexture(GL_TEXTURE_2D, m_texture); 
                //glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE); //legacy
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                break;
            case BILINEAR:
                glBindTexture(GL_TEXTURE_2D, m_texture); 
                //glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE); //legacy
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;   
        }
    }

    void updateShaderConfig() {

        bool loaded = false;

        const std::string& shaderName = AppSettings::instance().data.video.shaderName;

        if(shaderName != "") {
        
            for(const ShaderItem& item : shaderList) {
                if(item.label == shaderName) {
                    loaded = loadShader(item.path);
                    break;
                }
            }
        }

        if(!loaded) {

            if(shaderName != "") {
                Logger::instance().log("Failed to load shader " + shaderName + ". Using default shader.", Logger::Type::WARNING);
                AppSettings::instance().data.video.shaderName = "";
            }
            loadShader(""); //default
        }
    }

    virtual bool initGL() override {

        m_defaultCursor = SdlCursor::getDefault();
        m_crossCursor = SdlCursor::createSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
        m_sizeWECursor = SdlCursor::createSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);

        setFullScreen(m_fullScreen);

        if (SDL_Init(SDL_INIT_TIMER) < 0) {
            Logger::instance().log("SDL_Init error", Logger::Type::ERROR);
            return false;
        }

        #ifndef __EMSCRIPTEN__
        GLenum err = glewInit();
        if (GLEW_OK != err)
        {
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

        // Setup Platform/Renderer bindings
        ImGui_ImplSDL2_InitForOpenGL(sdlWindow(), glContext());
        ImGui_ImplOpenGL3_Init(glsl_version);


        glClearColor(0.0, 0.0, 0.0, 0.0);

        updateShaderConfig();

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glDisable(GL_DEPTH_TEST);

        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);

        updateBuffers(); //create vbo

        if(!m_vao.isCreated()) {
            m_vao.create();
        }

        m_vao.bind();

        m_vbo.bind();

        GLsizei stride = 4*sizeof(GLfloat);

        glEnableVertexAttribArray(0); //position
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,stride,0);

        glEnableVertexAttribArray(1); //texture uv
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,stride, (void*)(2*sizeof(GLfloat)));

        m_vbo.release();

        m_vao.release();

        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        updateFilterConfig();

        updateMVP();

        m_touch = std::make_unique<TouchControls>(m_controller1, width(), height(), GetWindowDPI());

        return true;
    }

    bool loadShader(const std::string& path)
    {   

        auto fs2 = cmrc::resources::get_filesystem();
        auto shaderFile = fs2.open("resources/default.glsl");
        std::string shaderText(shaderFile.begin(), shaderFile.end());        

        if(path != "") {
            std::ifstream file(path);            
            shaderText = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }     

        std::string vertexText;
        std::string fragmentText;

        std::regex versionPattern(R"(^#version[^\n]*\n)", std::regex::icase);

        if(std::regex_search(shaderText, versionPattern)) {
            vertexText = std::regex_replace(shaderText, versionPattern, R"($&#define VERTEX)" "\n");
            fragmentText = std::regex_replace(shaderText, versionPattern, R"($&#define FRAGMENT)" "\n");
        }
        else {
            vertexText = "#define VERTEX\n" + shaderText;
            fragmentText = "#define FRAGMENT\n" + shaderText;
        }

        m_shaderProgram.destroy();

        m_shaderProgram.create();      

        if(!m_shaderProgram.addShaderFromSourceCode(GLShaderProgram::Vertex, vertexText.c_str())){
            Logger::instance().log(std::string("vertex shader errors:\n") + m_shaderProgram.lastError(), Logger::Type::ERROR);
            m_shaderProgram.destroy();
            return false;
        }

        if(!m_shaderProgram.addShaderFromSourceCode(GLShaderProgram::Fragment, fragmentText.c_str())){
            Logger::instance().log(std::string("fragment shader errors:\n") + m_shaderProgram.lastError(), Logger::Type::ERROR);    
            m_shaderProgram.destroy();
            return false;
        }

        m_shaderProgram.bindAttributeLocation("VertexCoord", 0);
        m_shaderProgram.bindAttributeLocation("TexCoord", 1);

        if(!m_shaderProgram.link()){
            Logger::instance().log(std::string("shader link error: ") + m_shaderProgram.lastError(), Logger::Type::ERROR);   
            m_shaderProgram.destroy();
            return false;
        }

        return true;
    }

    void updateBuffers()
    {
        if(!m_vbo.isCreated()){
            m_vbo.create();
        }

        std::vector<GLfloat> data;

        SDL_Rect clientArea = { 0, m_menuBarHeight, width(), std::max(0, height() - m_menuBarHeight) };

        if( width()/256.0 >= height()/(240.0-2*m_clipHeightValue))
        {
            GLfloat drawWidth = m_horizontalStretch ? clientArea.w : 256.0/(240.0-2*m_clipHeightValue) * clientArea.h;
            GLfloat offsetX = (clientArea.w - drawWidth)/2.0;

            m_nesScreenRect.min = glm::vec2(clientArea.x+offsetX, clientArea.y);
            m_nesScreenRect.max = glm::vec2(m_nesScreenRect.min.x+drawWidth, m_nesScreenRect.min.y+clientArea.h);

            //glVertex2f(offsetX,0);]
            data.push_back(clientArea.x+offsetX);
            data.push_back(clientArea.y);
            //glTexCoord2f(0.0,m_clipHeightValue/256.0 ); //top left
            data.push_back(0.0);
            data.push_back(m_clipHeightValue/256.0);

            //glVertex2f(offsetX,height());
            data.push_back(clientArea.x+offsetX);
            data.push_back(clientArea.y+clientArea.h);
            //glTexCoord2f(0.0,240.0/256.0 - m_clipHeightValue/256.0 ); //bottom left
            data.push_back(0.0);
            data.push_back(240.0/256.0 - m_clipHeightValue/256.0);

            //glVertex2f(offsetX+screenWidth,0);
            data.push_back(clientArea.x+offsetX+drawWidth);
            data.push_back(clientArea.y);
            //glTexCoord2f(1.0, m_clipHeightValue/256.0 ); //top right
            data.push_back(1.0);
            data.push_back(m_clipHeightValue/256.0);

            //glVertex2f(offsetX+screenWidth,height());
            data.push_back(clientArea.x+offsetX+drawWidth);
            data.push_back(clientArea.y+clientArea.h);
            //glTexCoord2f(1.0,240.0/256.0 - m_clipHeightValue/256.0 ); //bottom right
            data.push_back(1.0);
            data.push_back(240.0/256.0 - m_clipHeightValue/256.0);
        }
        else
        {
            GLfloat drawHeight = (240.0-2*m_clipHeightValue)/256.0 * clientArea.w;
            GLfloat offsetY = (clientArea.h - drawHeight)/2.0; 

            m_nesScreenRect.min = glm::vec2(clientArea.x, clientArea.y+offsetY);
            m_nesScreenRect.max = glm::vec2(m_nesScreenRect.min.x+clientArea.w, m_nesScreenRect.min.y+drawHeight);

            //glVertex2f(0,offsetY);
            data.push_back(clientArea.x);
            data.push_back(clientArea.y+offsetY);
            //glTexCoord2f(0.0, m_clipHeightValue/256.0 ); //top left
            data.push_back(0.0);
            data.push_back(m_clipHeightValue/256.0);

            //glVertex2f(0,offsetY+screenHeight);
            data.push_back(clientArea.x);
            data.push_back(clientArea.y+offsetY+drawHeight);
            //glTexCoord2f(0.0,240.0/256.0 - m_clipHeightValue/256.0 ); //bottom left
            data.push_back(0.0);
            data.push_back(240.0/256.0 - m_clipHeightValue/256.0);

            //glVertex2f(width(),offsetY);
            data.push_back(clientArea.x+clientArea.w);
            data.push_back(clientArea.y+offsetY);
            //glTexCoord2f(1.0,0.0 + m_clipHeightValue/256.0 ); //top right
            data.push_back(1.0);
            data.push_back(0.0 + m_clipHeightValue/256.0);

            //glVertex2f(width(),offsetY+screenHeight);
            data.push_back(clientArea.x+clientArea.w);
            data.push_back(clientArea.y+offsetY+drawHeight);
            //glTexCoord2f(1.0,240.0/256.0 - m_clipHeightValue/256.0 ); //bottom right
            data.push_back(1.0);
            data.push_back(240.0/256.0 - m_clipHeightValue/256.0);
        }  

        m_vbo.bind();
        if( (size_t)m_vbo.size() != data.size()*sizeof(GLfloat))
            m_vbo.allocate(&data[0], data.size()*sizeof(GLfloat));
        else m_vbo.write(0,&data[0], data.size()*sizeof(GLfloat));
        m_vbo.release();

        if(m_touch) m_touch->setTopMargin(m_menuBarHeight);

    }

    virtual bool onEvent(SDL_Event& event) override {

        const bool pointerGrabActive = m_snesMouseGrabActive || m_arkanoidGrabActive;
        const bool isMouseEvent =
            event.type == SDL_MOUSEMOTION ||
            event.type == SDL_MOUSEBUTTONDOWN ||
            event.type == SDL_MOUSEBUTTONUP ||
            event.type == SDL_MOUSEWHEEL;

        if(!(pointerGrabActive && isMouseEvent)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
        }

        ImGuiIO &io = ImGui::GetIO();

        m_imGuiWantsMouse = pointerGrabActive ? false : io.WantCaptureMouse;
        bool imGuiWantsKeyboard = io.WantCaptureKeyboard;

        if(m_forceImGuiMouseResync && !pointerGrabActive) {
            int mx = 0, my = 0;
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

                if(imGuiWantsKeyboard) break;

                std::string keyName = SDL_GetKeyName(event.key.keysym.sym);

                if(event.key.keysym.mod & KMOD_ALT) keyName = "Alt+" + keyName;

                m_shortcuts.invokeShortcut(keyName);

                if(keyName == "Escape" && m_emuInputEnabled) {
                    if(m_snesMouseGrabActive) {
                        setSnesMouseGrab(false);
                    }
                    else if(m_arkanoidGrabActive) {
                        setArkanoidGrab(false);
                    }
                    else {
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
                        m_touch->onResize(width(),height());               
                        m_updateObjectsFlag = true;
                        break;
                }
                break;  
                
        }

        if(!m_imGuiWantsMouse) m_touch->onEvent(event);

        return SDLOpenGLWindow::onEvent(event);
    }

    virtual void onWindowsTitleBarInteractionChanged(bool active) override {
        m_emu.setPresenterLockActive(!active);
    }

    void mainLoop()
    {
        updateCursor();

        int displayFrameRate = getDisplayFrameRate();

        Uint64 tempTime = SDL_GetTicks64();

        Uint64 dt = tempTime - m_mainLoopLastTime;

        if(dt == 0) return;

        m_touch->update(dt);
        onFrameStart();
        dispatch_queued_calls();

        m_mainLoopLastTime = tempTime;
 
        m_fpsTimer += dt;        

        if(m_fpsTimer >= 1000)
        {
            int cycles = m_fpsTimer / 1000;
            m_fps = m_frameCounter / cycles;
            m_frameCounter = 0;
            m_fpsTimer %= 1000; 
        }

        const bool allowVsyncLock =
            m_vsyncMode != OFF &&
            displayFrameRate == m_emu.getRegionFPS() &&
            !isWindowsTitleBarInteractionActive();

        if(!allowVsyncLock) {
            if( m_emu.update(dt) ) render();
        }
        else {
            m_emu.updateUntilFrame(dt);
            render();      
        }

        m_frameCounter++;
    }

    void render();

    void menuBar();
    void collectAudioChannelsFromJson(const std::string& jsonStr, const char* source, std::vector<AudioChannelControl>& out);
    void applyAudioChannelVolume(const AudioChannelControl& c, float value);
    void drawAudioChannelDebugControls();
#ifdef ENABLE_NSF_PLAYER
    void drawNsfPlayerVisualizer();
#endif

    virtual void paintGL() override;

    void showGui();
    void showOverlay();
 
};

#include "GeraNESApp.MenuUI.inl"
#include "GeraNESApp.Render.inl"
#include "GeraNESApp.WindowsUI.inl"
