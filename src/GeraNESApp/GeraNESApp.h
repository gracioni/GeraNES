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
#include <array>
#include <deque>
#include <mutex>

#include "util/SdlCursor.h"

#include <functional>
#include <iterator>
#include <optional>
#include <regex>

#include <glm/glm.hpp>

#include <nlohmann/json.hpp>

#include "logger/logger.h"

#include "CppGL/CppGL.h"
#include "SDLOpenGLWindow.h"

#include "GeraNESApp/EmulationHost.h"
#include "GeraNESNetplay/NetplayAppRuntime.h"

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
#include "GeraNES/util/Crc32.h"

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

    int m_clipHeightValue = 8;

    GLVertexArrayObject m_vao;
    GLVertexBufferObject m_vbo;

    GLShaderProgram m_shaderProgram;

    bool m_updateObjectsFlag = true;

    GLuint m_texture = 0;
    std::vector<uint32_t> m_framebufferUploadCopy;

    bool m_fullScreen = false;
    int m_fullScreenMode = 0;

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

    enum VideoScaleMode {
        ASPECT_FIT,
        STRETCH_TO_FILL,
        PIXEL_PERFECT,
        PIXEL_PERFECT_BEST_FIT
    };
    VideoScaleMode m_videoScaleMode = ASPECT_FIT;
    int m_pixelPerfectScale = 3;

    bool m_showImprovementsWindow = false;
    bool m_showAboutWindow = false;
    bool m_showRomDatabaseWindow = false;
    bool m_showNetplayWindow = false;
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
#ifdef ENABLE_NSF_PLAYER
    ImFont* m_fontNsfTitle = nullptr;
    ImFont* m_fontNsfSubtitle = nullptr;
#endif
    ImFont* m_fontToast = nullptr;
    ImFont* m_fontFps = nullptr;
#ifdef ENABLE_NSF_PLAYER
    NsfVisualizerUI m_nsfVisualizer;
#endif

    Netplay::NetplayAppRuntime m_netplayRuntime;

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

    Uint64 m_mainLoopLastCounter = 0;
    Uint64 m_mainLoopCounterFrequency = 0;
    Uint64 m_mainLoopCounterRemainder = 0;

    Rect m_nesScreenRect = {{0,0}, {1,1}};

    bool m_imGuiWantsMouse = false;
    bool m_cursorVisible = true;
    bool m_arkanoidGrabActive = false;
    bool m_arkanoidSuppressClickUntilRelease = false;
    bool m_snesMouseGrabActive = false;
    bool m_snesMouseSuppressClickUntilRelease = false;
    bool m_forceImGuiMouseResync = false;
    bool m_webVisibilitySuspended = false;
    bool m_hasLastMousePosition = false;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
    float m_arkanoidNesPosition = 0.5f;
    float m_arkanoidFamicomPosition = 0.5f;

    // FPS vars    
    Uint64 m_fpsTimer = 0;
    int m_fps = 0;
    int m_frameCounter = 0;
    uint32_t m_lastMainLoopDtMs = 0;
    uint64_t m_presenterFrameAccumScaled = 0;
    uint32_t m_presenterStepRemainder = 0;

    static constexpr std::array<const char*, 3> VSYNC_TYPE_LABELS {"Off", "Syncronized", "Adaptative"};
    static constexpr std::array<const char*, 3> FILTER_TYPE_LABELS {"Nearest", "Bilinear"};    
    static constexpr std::array<const char*, 4> VIDEO_SCALE_MODE_LABELS {
        "Aspect Fit",
        "Stretch to Fill",
        "Pixel Perfect",
        "Best Fit"
    };
    static constexpr std::array<const char*, 2> FULLSCREEN_MODE_LABELS {
        "Desktop",
        "Exclusive"
    };

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

    bool isNetplayClientRestricted() const;
    bool isNetplayRomChangeRestricted() const;
    void notifyNetplayClientRestrictedAction(const char* action);
    void notifyNetplayRomChangeRestrictedAction(const char* action);
    bool canUseNetplaySessionPause() const;
    bool isNetplayPauseRestricted() const;
    void notifyNetplayPauseRestrictedAction();
    void togglePauseAction();
    void resetAction();
    bool shouldSuppressRewindForNetplay() const;
    void applyEffectiveRewindSettings();
    static bool isTouchCompatibleControllerDevice(const std::optional<Settings::Device>& device);
    static const char* touchDeviceLabel(Settings::Device device);
    static bool isTouchCompatibleExpansionDevice(Settings::ExpansionDevice device);
    static std::string touchTargetMenuLabel(AppSettings::TouchControlsTarget target);
    static AppSettings::TouchControlsTarget preferredTouchTargetForTopology(const IEmulationHost::InputTopologySnapshot& topology);
    void normalizeTouchControlsTargetForCurrentTopology();
    static void setIfNegative(std::string& dst, int value);
    static void setIfNegativeKb(std::string& dst, int bytesValue);


    void loadRomDatabaseEditorFromCurrentRom();
    void saveRomDatabaseEditor();
    void removeRomDatabaseEditor();

    void updateMVP();
    void onLog(const std::string& msg, Logger::Type type);

    std::tuple<int, int> getNesCursor(int screenX, int screenY);
    std::tuple<int, int> getClampedNesCursor(int screenX, int screenY);
    bool isSnesMouseActive() const;
    bool isSuborMouseActive() const;
    bool isSuborKeyboardActive() const;
    bool isFamilyBasicKeyboardActive() const;
    static IExpansionDevice::SuborKeyboardKeys captureSuborKeyboardState();
    static IExpansionDevice::FamilyBasicKeyboardKeys captureFamilyBasicKeyboardState();
    bool isArkanoidActive() const;
    void setArkanoidGrab(bool active);
    void setSnesMouseGrab(bool active);

    void pollAndPrepareInput();

    void openFile(const char* path);
    void syncSettings();
    void createShortcuts();
    void updateCursor();

public:

    GeraNESApp();

    void loadShaderList();

#ifdef __EMSCRIPTEN__

    void processUploadedFile(const char* fileName, size_t fileSize, const uint8_t* fileContent);
    void restartAudioModule();
    void onWebVisibilityChanged(bool visible);
    void onWebAppUnload();
    void onSessionImportComplete();

#endif

    void onCaptureBegin();
    void onInputBindingCaptureEnd();
    virtual ~GeraNESApp();
    void openRom();
    void updateVSyncConfig();
    void updateFilterConfig();
    void updateShaderConfig();

    virtual bool initGL() override;

    bool loadShader(const std::string& path);

    void updateBuffers();

    virtual bool onEvent(SDL_Event& event) override;

    virtual void onWindowsTitleBarInteractionChanged(bool active) override;
    virtual void onWindowDisplayChanged(int displayIndex) override;

    void mainLoop();

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

