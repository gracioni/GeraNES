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

#include "ControllerConfigWindow.h"
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

#include "GeraNES/GeraNESEmu.h"

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
#include "NsfVisualizerUI.h"

const std::string LOG_FILE = "log.txt";

class GeraNESApp : public SDLOpenGLWindow, public SigSlot::SigSlotBase {

private:

    ControllerConfigWindow m_controllerConfigWindow;

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

    GeraNESEmu m_emu;

    ControllerInfo m_controller1;
    ControllerInfo m_controller2; 

    bool m_emuInputEnabled = true;

    enum VSyncMode {OFF, SYNCRONIZED, ADAPTATIVE};
    VSyncMode m_vsyncMode = OFF;

    enum FilterMode {NEAREST, BILINEAR};
    FilterMode m_filterMode = NEAREST;

    bool m_showImprovementsWindow = false;
    bool m_showAboutWindow = false;
    bool m_showRomDatabaseWindow = false;

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
    NsfVisualizerUI m_nsfVisualizer;

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

        Cartridge& cart = m_emu.getConsole().cartridge();
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

        int nesX = ((screenX - m_nesScreenRect.min.x) * m_emu.getConsole().ppu().SCREEN_WIDTH) / m_nesScreenRect.getWidth();
        int clipTop = m_clipHeightValue;
        int visibleNES = m_emu.getConsole().ppu().SCREEN_HEIGHT - 2*m_clipHeightValue;
        int nesY = clipTop + ((screenY - m_nesScreenRect.min.y) * visibleNES) / m_nesScreenRect.getHeight();

        return std::make_tuple(nesX, nesY);
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

            // Player1
            m_emu.setController1Buttons(
                p1A,
                p1B,
                p1Select,
                p1Start,
                p1Up,
                p1Down,
                p1Left,
                p1Right
            );

            if(im.isJustPressed(m_controller1.saveState)) m_emu.saveState();
            if(im.isJustPressed(m_controller1.loadState)) m_emu.loadState();            

            // Player 2
            m_emu.setController2Buttons(
                im.isPressed(m_controller2.a), im.isPressed(m_controller2.b),
                im.isPressed(m_controller2.select), im.isPressed(m_controller2.start),
                im.isPressed(m_controller2.up), im.isPressed(m_controller2.down),
                im.isPressed(m_controller2.left), im.isPressed(m_controller2.right)
            );

            m_emu.setBandaiHyperShotButtons(
                im.isPressed(m_controller2.a), im.isPressed(m_controller2.b),
                im.isPressed(m_controller2.select), im.isPressed(m_controller2.start),
                im.isPressed(m_controller2.up), im.isPressed(m_controller2.down),
                im.isPressed(m_controller2.left), im.isPressed(m_controller2.right)
            );

            {
                int mx, my;
                Uint32 buttons = SDL_GetMouseState(&mx, &my);

                auto [nesX, nesY] = getNesCursor(mx, my);

                const bool mouseAllowed = !m_imGuiWantsMouse && !m_touch->buttons().anyPressed();
                const bool leftClick = mouseAllowed && (buttons & SDL_BUTTON(SDL_BUTTON_LEFT));
                const bool rightClick = mouseAllowed && (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT));

                const int zapperX = rightClick ? -1 : nesX;
                const int zapperY = rightClick ? -1 : nesY;

                // Right click = off-screen shot. Keep previous trigger mapping for P1 and
                // allow off-screen reload on P2 (left or right).
                m_emu.setZapper(Settings::Port::P_1, zapperX, zapperY, rightClick);
                m_emu.setZapper(Settings::Port::P_2, zapperX, zapperY, leftClick || rightClick);
                m_emu.setBandaiHyperShot(zapperX, zapperY, leftClick || rightClick);
            }

            if(im.isJustPressed(m_controller2.saveState)) m_emu.saveState();
            if(im.isJustPressed(m_controller2.loadState)) m_emu.loadState();

            // Rewind
            m_emu.setRewind(
                im.isPressed(m_controller1.rewind) ||
                im.isPressed(m_controller2.rewind) ||
                m_touch->buttons().rewind
            );

            // Hold-to-speed (fast forward)
            m_emu.setSpeedBoost(
                im.isPressed(m_controller1.speed) ||
                im.isPressed(m_controller2.speed)
            );
        }
        else {
            m_emu.setRewind(false);
            m_emu.setSpeedBoost(false);
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

        m_audioOutput.config(cfg.audio.audioDevice);      
        cfg.audio.audioDevice = m_audioOutput.currentDeviceName(); 
        m_audioOutput.setVolume(cfg.audio.volume);
        
        cfg.input.getControllerInfo(0, m_controller1);
        cfg.input.getControllerInfo(1, m_controller2);

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
            m_emu.saveState();
        }});

        m_shortcuts.add(ShortcutManager::Data{"loadState", "Load State", "Alt+L", [this]() {
            m_emu.loadState();
        }});

        m_shortcuts.add(ShortcutManager::Data{"pause", "Pause", "Alt+P", [this]() {
            m_emu.togglePaused();
        }});
    }

    void updateCursor() {

        int mx, my;
        SDL_GetMouseState(&mx, &my);

        bool inside = pointInRect(glm::vec2(mx,my), m_nesScreenRect);

        bool useZapper = m_emu.getPortDevice(Settings::Port::P_1) == std::optional<Settings::Device>(Settings::Device::ZAPPER) ||
        m_emu.getPortDevice(Settings::Port::P_2) == std::optional<Settings::Device>(Settings::Device::ZAPPER) ||
        m_emu.getExpansionDevice() == Settings::ExpansionDevice::BANDAI_HYPERSHOT;

        if(!m_imGuiWantsMouse && inside && useZapper) {
            if(m_crossCursor.has_value() && !m_crossCursor->isCurrent()) {
                m_crossCursor->setAsCurrent();
            }
        }
        else {
            if(m_defaultCursor.has_value() && !m_defaultCursor->isCurrent())
                m_defaultCursor->setAsCurrent();
        }
    }

public:

    GeraNESApp() : m_emu(m_audioOutput) {

        //reset log file content
        std::ofstream file(LOG_FILE);
        file.close();

        Logger::instance().signalLog.bind(&GeraNESApp::onLog, this);
        m_emu.signalFrameStart.bind(&GeraNESApp::onFrameStart, this);    

        m_controllerConfigWindow.signalShow.bind(&GeraNESApp::onCaptureBegin, this);
        m_controllerConfigWindow.signalClose.bind(&GeraNESApp::onCaptureEnd, this);

        m_audioDevices = m_audioOutput.getAudioList();

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
        m_audioOutput.restart();
    }

    void onSessionImportComplete() {
        syncSettings();
    }

#endif

    void onCaptureBegin() {
        m_emuInputEnabled = false;
    }

    void onCaptureEnd() {

        m_emuInputEnabled = true;        

        //save both
        AppSettings::instance().data.input.setControllerInfo(0, m_controller1);
        AppSettings::instance().data.input.setControllerInfo(1, m_controller2);
    }

    virtual ~GeraNESApp() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    void openRom() {

        #ifndef __EMSCRIPTEN__
        if(isFullScreen()) minimizeWindow();
        #endif

        #ifdef __EMSCRIPTEN__
            emcriptenFileDialog(reinterpret_cast<intptr_t>(this));
        #else

        NFD_Init();     

        nfdchar_t *outPath;
        nfdfilteritem_t filterItem[] = { { "NES/NSF", "nes,nsf,zip" }, "Patch", "ips,ups,bps" };

        nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, sizeof(filterItem)/sizeof(nfdfilteritem_t),
            (AppSettings::instance().data.getLastFolder()).c_str());

        if (result == NFD_OKAY)
        {
            openFile(outPath);                        
            NFD_FreePath(outPath);
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

        #endif

        if(m_fullScreen) restoreWindow();
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
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        {
            auto fs = cmrc::resources::get_filesystem();
            const char* fontPath = nullptr;
            if(fs.exists("data/fonts/DejaVuSans.ttf")) fontPath = "data/fonts/DejaVuSans.ttf";
            else if(fs.exists("fonts/DejaVuSans.ttf")) fontPath = "fonts/DejaVuSans.ttf";
            else if(fs.exists("resources/fonts/DejaVuSans.ttf")) fontPath = "resources/fonts/DejaVuSans.ttf";

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

                m_fontNsfTitle = io.Fonts->AddFontFromMemoryTTF(m_embeddedUiFontData.data(), static_cast<int>(m_embeddedUiFontData.size()), 34.0f, &cfg);
                m_fontNsfSubtitle = io.Fonts->AddFontFromMemoryTTF(m_embeddedUiFontData.data(), static_cast<int>(m_embeddedUiFontData.size()), 20.0f, &cfg);
                m_fontToast = io.Fonts->AddFontFromMemoryTTF(m_embeddedUiFontData.data(), static_cast<int>(m_embeddedUiFontData.size()), 24.0f, &cfg);
                m_fontFps = io.Fonts->AddFontFromMemoryTTF(m_embeddedUiFontData.data(), static_cast<int>(m_embeddedUiFontData.size()), 32.0f, &cfg);

                if(m_fontNsfTitle == nullptr || m_fontNsfSubtitle == nullptr || m_fontToast == nullptr || m_fontFps == nullptr) {
                    io.FontDefault = io.Fonts->AddFontDefault();
                    Logger::instance().log("Embedded overlay fonts failed to load completely; using ImGui default where needed.", Logger::Type::WARNING);
                } else {
                    Logger::instance().log(std::string("Embedded UI font loaded from cmrc: ") + fontPath, Logger::Type::INFO);
                }
            } else {
                Logger::instance().log("Embedded font not found in cmrc (tried data/fonts/DejaVuSans.ttf, fonts/DejaVuSans.ttf and resources/fonts/DejaVuSans.ttf).", Logger::Type::WARNING);
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

        ImGui_ImplSDL2_ProcessEvent(&event);

        ImGuiIO &io = ImGui::GetIO();

        m_imGuiWantsMouse = io.WantCaptureMouse;
        bool imGuiWantsKeyboard = io.WantCaptureKeyboard;

        switch(event.type) {

            case SDL_KEYDOWN: {

                if(imGuiWantsKeyboard) break;

                std::string keyName = SDL_GetKeyName(event.key.keysym.sym);

                if(event.key.keysym.mod & KMOD_ALT) keyName = "Alt+" + keyName;

                m_shortcuts.invokeShortcut(keyName);

                if(keyName == "Escape" && m_emuInputEnabled) {
                    m_showMenuBar = !m_showMenuBar;
                    m_updateObjectsFlag = true;
                }

                break;
            }                

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

    void mainLoop()
    {
        updateCursor();

        int displayFrameRate = getDisplayFrameRate();

        Uint64 tempTime = SDL_GetTicks64();

        Uint64 dt = tempTime - m_mainLoopLastTime;

        if(dt == 0) return;

        m_touch->update(dt);

        m_mainLoopLastTime = tempTime;
 
        m_fpsTimer += dt;        

        if(m_fpsTimer >= 1000)
        {
            int cycles = m_fpsTimer / 1000;
            m_fps = m_frameCounter / cycles;
            m_frameCounter = 0;
            m_fpsTimer %= 1000; 
        }

        if(m_vsyncMode == OFF || displayFrameRate != m_emu.getRegionFPS()) {          
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
    void drawNsfPlayerVisualizer();

    virtual void paintGL() override;

    void showGui();
    void showOverlay();
 
};

#include "GeraNESApp.MenuUI.inl"
#include "GeraNESApp.Render.inl"
#include "GeraNESApp.WindowsUI.inl"
