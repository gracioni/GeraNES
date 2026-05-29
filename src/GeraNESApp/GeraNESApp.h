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
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

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
#include "ConsoleNetplay/NetplayAppRuntime.h"
#include "GeraNESNetplay/GeraNESNetplayRuntimeDriver.h"
#include "GeraNESNetplay/GeraNESNetplayMenuHelpers.h"

#include "GeraNES/defines.h"
#include "GeraNES/CPU2A03Debug.h"

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
#include "GeraNESApp/ModManager.h"
#include "GeraNESApp/ReplayManager.h"

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

enum class CpuDebugSymbolKind {
    Unknown,
    Function,
    Label,
    Data
};

struct CpuDebugSymbol {
    std::string name;
    CpuDebugSymbolKind kind = CpuDebugSymbolKind::Unknown;
};

class GeraNESApp : public SDLOpenGLWindow, public SigSlot::SigSlotBase {

private:

    InputBindingConfigWindow m_inputBindingConfigWindow;
    PowerPadConfigWindow m_powerPadConfigWindow;

    std::vector<std::string> m_audioDevices;

    ModManager::OverscanConfig m_overscanConfig = { true, 8, 0, 8, 0 };

    GLVertexArrayObject m_vao;
    GLVertexBufferObject m_vbo;
    GLVertexArrayObject m_postProcessVao;
    GLVertexBufferObject m_postProcessVbo;

    struct ShaderPass {
        struct Parameter {
            std::string name;
            std::string label;
            float value = 0.0f;
            float defaultValue = 0.0f;
            float minValue = 0.0f;
            float maxValue = 1.0f;
            float step = 0.1f;
        };

        std::string label;
        std::string path;
        bool enabled = true;
        std::vector<Parameter> parameters;
        GLShaderProgram program;
    };

    struct PostProcessTarget {
        GLuint fbo = 0;
        GLuint texture = 0;
        int width = 0;
        int height = 0;
    };
    std::vector<ShaderPass> m_shaderPasses;
    std::array<PostProcessTarget, 2> m_postProcessTargets = {};

    bool m_updateObjectsFlag = true;

    GLuint m_texture = 0;
    std::vector<uint32_t> m_framebufferUploadCopy;
    std::vector<uint32_t> m_textureUploadBuffer;
    int m_renderTextureWidth = 256;
    int m_renderTextureHeight = 256;
    GLuint m_modPixelInspectorTexture = 0;
    std::vector<uint32_t> m_modPixelInspectorTextureUploadBuffer;
    int m_modPixelInspectorTextureWidth = 256;
    int m_modPixelInspectorTextureHeight = 256;
    GLuint m_ppuNametableTexture = 0;
    GLuint m_ppuChrTexture = 0;
    GLuint m_ppuEventTexture = 0;
    std::vector<uint32_t> m_ppuNametableBuffer;
    std::vector<uint32_t> m_ppuChrBuffer;
    std::vector<uint32_t> m_ppuChrExportBuffer;
    std::vector<uint32_t> m_ppuEventBuffer;
    std::vector<GeraNESEmu::PpuViewerScanlineState> m_ppuViewerScanlineStates;
    std::vector<GeraNESEmu::PpuViewerScanlineSnapshot> m_ppuViewerScanlineSnapshots;
    uint32_t m_ppuViewerCachedFrame = UINT32_MAX;
    int m_ppuViewerCachedScanline = -1;
    int m_ppuViewerCachedCycle = -1;
    int m_ppuViewerChrPaletteMode = 0;
    int m_ppuViewerCachedChrPaletteMode = -1;
    bool m_ppuViewerScanlineTraceActive = false;
    uint32_t m_eventViewerCachedFrame = UINT32_MAX;
    bool m_eventViewerCachedTraceEnabled = false;
    std::vector<GeraNESEmu::PpuRegisterAccessEvent> m_cachedPpuEvents;

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
    bool m_showPaletteWindow = false;
    bool m_showShaderWindow = false;
    bool m_showOverscanWindow = false;
    bool m_showReplayWindow = false;
    bool m_showCpuDebuggerWindow = false;
    bool m_showCpuBreakpointsWindow = false;
    bool m_showMemoryViewerWindow = false;
    bool m_showMemoryCompareWindow = false;
    bool m_showPpuViewerWindow = false;
    bool m_showModPixelInspectorWindow = false;
    bool m_modPixelInspectorPpuCaptureEnabled = false;
    bool m_showOriginalGraphicsInsteadOfModFramebuffer = false;
    float m_modPixelInspectorZoom = 1.0f;
    float m_modPixelInspectorBlend = 1.0f;
    bool m_modPixelInspectorShowSprites = true;
    bool m_modPixelInspectorShowBackground = true;
    std::string m_modPixelInspectorLastDebugText;
    std::string m_modPixelInspectorFilter;
    bool m_modPixelInspectorInspectMod = false;
    bool m_showEventViewerWindow = false;
    bool m_ppuEventViewerEnabled = false;
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
    uint64_t m_lastSeenCpuBreakpointSequence = 0;
    bool m_pendingEnableCpuDebuggerAfterNetplayDisconnect = false;
    bool m_cpuDebuggerAutoPaused = false;
    std::unordered_map<uint16_t, CpuDebugSymbol> m_cpuDebugSymbols;
    std::string m_cpuDebugSymbolsPath;
    std::string m_cpuDebugSymbolsStatus;
    uint16_t m_cpuDebuggerViewAddress = 0;
    bool m_cpuDebuggerFollowPc = true;
    bool m_cpuDebuggerScrollToViewAddress = true;
    std::vector<uint16_t> m_cpuDebuggerHistory;
    size_t m_cpuDebuggerHistoryIndex = 0;
    std::array<char, 128> m_cpuDebuggerSymbolSearch = {};
    bool m_cpuDebuggerFocused = false;
    bool m_cpuBreakpointsFocused = false;
    bool m_cpuBreakpointsRequestFocus = false;
    uint16_t m_cpuDebuggerSelectedAddress = 0;
    uint16_t m_cpuDebuggerSelectionAnchor = 0;
    bool m_cpuDebuggerHasSelection = false;
    int m_memoryViewerType = 0;
    uint16_t m_memoryViewerAddress = 0;
    uint16_t m_memoryViewerGotoAddress = 0;
    char m_memoryViewerGotoText[5] = "0000";
    int m_memoryViewerColumns = 16;
    bool m_memoryViewerShowAscii = true;
    uint32_t m_memoryViewerScrollToAddress = UINT32_MAX;
    int m_memoryViewerScrollPendingFrames = 0;
    bool m_memoryViewerEditOpen = false;
    uint32_t m_memoryViewerEditAddress = 0;
    uint8_t m_memoryViewerEditValue = 0;
    char m_memoryViewerEditText[3] = "00";
    int m_memoryCompareType = 0;
    int m_memoryCompareFilter = 1;
    char m_memoryCompareFromText[3] = "01";
    char m_memoryCompareToText[3] = "02";
    bool m_memoryCompareAutoRefresh = true;
    bool m_memoryCompareMaskActive = false;
    std::vector<uint8_t> m_memoryCompareBaseline;
    std::vector<uint8_t> m_memoryCompareCurrent;
    std::vector<size_t> m_memoryCompareMask;
    std::vector<std::vector<size_t>> m_memoryCompareMaskStack;
    std::string m_memoryCompareStatus;
    int m_selectedPpuEventIndex = -1;
    uint32_t m_selectedPpuEventFrame = 0;

    std::vector<uint8_t> m_embeddedUiFontData;
    std::vector<uint8_t> m_embeddedIconFontData;
    bool m_fontAwesomeIconsLoaded = false;
#ifdef ENABLE_NSF_PLAYER
    ImFont* m_fontNsfTitle = nullptr;
    ImFont* m_fontNsfSubtitle = nullptr;
#endif
    ImFont* m_fontToast = nullptr;
    ImFont* m_fontFps = nullptr;
#ifdef ENABLE_NSF_PLAYER
    NsfVisualizerUI m_nsfVisualizer;
#endif

    ConsoleNetplay::NetplayAppRuntime m_netplayRuntime;
    ModManager m_modManager;
    mutable std::mutex m_netplayInputStateMutex;
    IEmulationHost::InputState m_netplayLatestInputState = {};
    IEmulationHost::InputState m_replayLiveInputState = {};
    ReplayManager m_replayManager;
    int m_replaySliderValue = 0;
    bool m_replaySliderDragging = false;
    bool m_replaySeekInProgress = false;
    bool m_replayAutoPlayAfterSeek = false;
    std::thread m_replaySeekThread;
    std::atomic<bool> m_replaySeekTaskRunning = false;
    std::atomic<bool> m_replaySeekTaskCompleted = false;
    std::atomic<bool> m_replaySeekTaskSucceeded = false;
    uint32_t m_replaySeekTargetFrame = 0;
    size_t m_selectedEmulationSpeedIndex = 2;
    size_t m_lastEffectiveEmulationSpeedIndex = 2;

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
    bool m_customWindowChromeEnabled = true;
    bool m_customChromeDragging = false;
    int m_customChromeDragStartMouseX = 0;
    int m_customChromeDragStartMouseY = 0;
    int m_customChromeDragStartWindowX = 0;
    int m_customChromeDragStartWindowY = 0;

    bool m_imGuiWantsMouse = false;
    bool m_imGuiWantsKeyboard = false;
    bool m_imGuiWindowFocusBlocksEmulator = false;
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
    int m_emulatorFps = 0;
    int m_displayFps = 0;
    uint32_t m_generatedFrameCounter = 0;
    uint32_t m_displayLoopCounter = 0;
    uint32_t m_lastObservedEmulationFrame = 0;
    bool m_hasLastObservedEmulationFrame = false;
    uint32_t m_lastModObservedFrame = 0;
    bool m_hasLastModObservedFrame = false;
    uint32_t m_lastMainLoopDtMs = 0;
    uint64_t m_presenterFrameAccumScaled = 0;
    uint32_t m_presenterStepRemainder = 0;
    double m_emulationSpeedFrameAccumulator = 0.0;
    uint32_t m_lastTextureUploadOverscanKey = 0;
    int m_lastTextureUploadModScale = -1;

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

    struct AudioChannelControlCache {
        std::vector<AudioChannelControl> nesChannels;
        std::vector<AudioChannelControl> mapperChannels;
        double lastRefreshTime = -1.0;
        bool valid = false;
        bool emuValid = false;
        GameDatabase::System cartridgeSystem = GameDatabase::System::Unknown;
    };

    struct ShaderItem {
        std::string label;
        std::string path;
    };

    struct PaletteItem {
        std::string name;
        fs::path path;
        std::array<uint32_t, 64> colors = {};
        bool builtIn = false;
    };

    std::vector<ShaderItem> shaderList;
    int m_selectedAvailableShaderIndex = 0;
    int m_selectedShaderStackIndex = -1;
    std::string m_selectedShaderPresetName = "";
    std::string m_shaderPresetNameInput = "";
    AudioChannelControlCache m_audioChannelControlCache;
    std::vector<PaletteItem> m_paletteList;
    std::array<uint32_t, 64> m_editPalette = {};
    std::string m_selectedPaletteName = "";
    std::string m_paletteNameInput = "";
    fs::path m_loadedRomPath;
    struct PendingRomLoad {
        bool active = false;
        fs::path requestedPath;
        std::string effectivePath;
        ModManager::LoadRequest modLoad;
    } m_pendingRomLoad;

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
    void closeRomAction();
    bool finishOpenRomPath(const fs::path& requestedPath, const std::string& effectivePath, const ModManager::LoadRequest& modLoad, bool modDefinitionLoaded);
    void updatePendingRomLoad();
    bool openRomPath(const fs::path& path, bool updateRecentFiles = true, bool clearSelectedMod = true);
    int currentModAudioPreferredChannels() const;
    void syncModAudioOutputChannels(int previousChannelCount);
    void loadModArchive();
    void loadModFolder();
    void clearSelectedMod();
    void resetShowOriginalGraphicsInsteadOfModFramebuffer();
    bool shouldSuppressRewindForNetplay() const;
    void applyEffectiveRewindSettings();
    static bool isTouchCompatibleControllerDevice(const std::optional<Settings::Device>& device);
    static const char* touchDeviceLabel(Settings::Device device);
    static bool isTouchCompatibleExpansionDevice(Settings::ExpansionDevice device);
    static std::string touchTargetMenuLabel(AppSettings::TouchControlsTarget target);
    static AppSettings::TouchControlsTarget preferredTouchTargetForTopology(const IEmulationHost::InputTopologySnapshot& topology);
    AppSettings::TouchControlsTarget effectiveTouchControlsTarget() const;
    void normalizeTouchControlsTargetForCurrentTopology();
    static void setIfNegative(std::string& dst, int value);
    static void setIfNegativeKb(std::string& dst, int bytesValue);


    void loadRomDatabaseEditorFromCurrentRom();
    void saveRomDatabaseEditor();
    void removeRomDatabaseEditor();
    void loadPaletteList();
    const ShaderItem* findShaderByLabel(const std::string& label) const;
    std::vector<ShaderPass::Parameter> parseShaderParameters(const std::string& shaderText) const;
    std::string sanitizeShaderTextForCompilation(const std::string& shaderText) const;
    bool compileShaderProgram(GLShaderProgram& program, const std::string& path, const std::map<std::string, float>* parameterValues, std::vector<ShaderPass::Parameter>* outParameters);
    void destroyPostProcessTargets();
    bool ensurePostProcessTargets(int width, int height);
    void drawShaderWindow();
    void applyPalette(const std::array<uint32_t, 64>& colors, const std::string& name);
    void saveCurrentPalette();
    void exportPpuViewerChrPng(const std::vector<uint32_t>& pixels, int width, int height);
    void createNewPalette();
    void deleteCurrentPalette();
    void drawPaletteWindow();
    void drawOverscanWindow();
    void drawReplayWindow();
    void drawPpuViewerWindow();
    void drawModPixelInspectorWindow();
    void drawEventViewerWindow();
    void drawImprovementsWindow();
    void drawAboutWindow();
    bool openDocumentation();
    void drawArkanoidNesConfigWindow();
    void drawArkanoidFamicomConfigWindow();
    void drawSnesMouseConfigWindow();
    void drawRomDatabaseWindow();
    void drawConfirmSaveRomDatabaseEntryPopup();
    void drawConfirmRemoveRomDatabaseEntryPopup();
    void drawErrorWindow();
    void drawLogWindow();
    void exportCurrentPpuChrPng();
    void drawCpuDebuggerWindow();
    void drawCpuBreakpointsWindow();
    void drawMemoryViewerWindow();
    void drawMemoryViewerEditPopup(uint32_t regionBaseAddress, int regionSource);
    void drawMemoryCompareWindow();
    void loadCpuDebuggerSymbols();
    bool loadCpuDebuggerSymbolsFromFile(const std::string& path);
    void syncCpuDebugRuntimeState();
    void disableCpuDebugging();
    void requestEnableCpuDebugger();
    bool isNetplayBlockingCpuDebug() const;
    void fillNoRomStaticFramebuffer();

    void updateMVP();
    void onLog(const std::string& msg, Logger::Type type);
    void onEmuResetForModAudio(uint32_t frame);
    void onEmuLoadForModAssets(uint32_t frame);
    void refreshModFrameCaptureHook();
    ModManager::OverscanConfig effectiveOverscan() const;
    static uint32_t overscanKey(const ModManager::OverscanConfig& overscan);

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
    void persistSettingsForShutdown();
    void createShortcuts();
    void updateCursor();
    bool isReplayRestricted() const;
    bool isNetplaySpeedRestricted() const;
    bool isReplayRecordingActive() const;
    bool isReplaySessionInteractionLocked() const;
    static constexpr size_t emulationSpeedOptionCount() { return 6; }
    static const char* emulationSpeedLabel(size_t index);
    static bool emulationSpeedIsMax(size_t index);
    static double emulationSpeedMultiplier(size_t index);
    size_t effectiveEmulationSpeedIndex(bool maxSpeedRequested) const;
    void resetEmulationSpeedPacing();
    void cycleEmulationSpeedSelection(int direction);
    void notifyReplaySessionInteractionLocked(const char* action);
    void refreshReplayFrameInputResolver();
    void stopReplayPlayback(bool pauseEmulation);
    void clearReplaySession(bool keepWindowOpen = true);
    void applyReplayInputTopology(const IEmulationHost::InputTopologySnapshot& topology);
    static InputFrame buildReplayRecordedFrame(const IEmulationHost::InputTopologySnapshot& topology,
                                               uint32_t frameNumber,
                                               const IEmulationHost::InputState& input);
    std::string currentRomCrc32() const;
    bool openReplayDialog();
    bool saveReplayDialog(fs::path& path);
    bool startReplayRecording();
    bool continueReplayRecordingFromCurrentCursor();
    void stopReplayRecording();
    bool openReplayFile(const fs::path& path);
    bool seekReplayToFrame(uint32_t frame);
    bool stopReplayToStart();
    void startReplayPlayback();
    bool beginReplaySeekCatchup(uint32_t targetFrame,
                                std::vector<InputFrame> replayFrames,
                                std::optional<uint32_t> expectedCurrentStateCrc32);
    void finishReplaySeekTask();
    void waitForReplaySeekTask();
    void syncReplayRuntimeState();

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
    virtual void onQuitRequested() override;

    virtual void onWindowsTitleBarInteractionChanged(bool active) override;
    virtual void onWindowDisplayChanged(int displayIndex) override;

    void mainLoop();

    void render();

    void menuBar();
    void collectAudioChannelsFromJson(const std::string& jsonStr, const char* source, std::vector<AudioChannelControl>& out);
    void applyAudioChannelVolume(const AudioChannelControl& c, float value);
    void drawAudioChannelDebugControls();
#ifdef ENABLE_NSF_PLAYER
    void drawNsfPlayerWindow();
    void drawNsfPlayerVisualizer();
#endif

    virtual bool paintGL() override;

    void showGui();
    void showOverlay();
    void drawPendingRomLoadOverlay();
    bool useCustomWindowChrome();
    float customTitleBarHeight();
    float customSideFrameWidth();
    float customBottomFrameHeight();
    float customContentTopPadding();
    SDL_Rect emulatorClientArea();
    void drawCustomWindowChrome();
 
};

