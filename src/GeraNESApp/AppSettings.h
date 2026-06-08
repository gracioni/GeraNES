#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <vector>
#include <string>

#include <nlohmann/json.hpp>

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#endif

#include <filesystem>
namespace fs = std::filesystem;

#include "ControllerInfo.h"
#include "KonamiHyperShotInfo.h"
#include "PowerPadInfo.h"
#include "SnesControllerInfo.h"
#include "SystemInputInfo.h"
#include "VirtualBoyControllerInfo.h"

class AppSettings {

public:

#ifdef __EMSCRIPTEN__
    static int loadUrlSettingsOverrideJson(char* buffer, int bufferSize)
    {
        if(buffer == nullptr || bufferSize <= 0) return 0;
        return EM_ASM_INT({
            const outBuffer = $0;
            const outCapacity = $1;

            function writeResult(text) {
                if(typeof text !== 'string' || outCapacity <= 0) {
                    if(outCapacity > 0) HEAPU8[outBuffer] = 0;
                    return 0;
                }
                const bytesNeeded = lengthBytesUTF8(text) + 1;
                if(bytesNeeded > outCapacity) {
                    if(outCapacity > 0) HEAPU8[outBuffer] = 0;
                    return -bytesNeeded;
                }
                stringToUTF8(text, outBuffer, outCapacity);
                return bytesNeeded - 1;
            }

            function decodeSettingsParam(rawValue) {
                if(!rawValue) return '';

                const normalized = rawValue.replace(/-/g, '+').replace(/_/g, '/');
                const padded = normalized + '='.repeat((4 - (normalized.length % 4)) % 4);
                try {
                    const binary = atob(padded);
                    const bytes = new Uint8Array(binary.length);
                    for(let i = 0; i < binary.length; ++i) {
                        bytes[i] = binary.charCodeAt(i);
                    }
                    return new TextDecoder().decode(bytes);
                } catch(_) {
                }

                try {
                    return decodeURIComponent(rawValue);
                } catch(_) {
                }

                return rawValue;
            }

            try {
                const params = new URLSearchParams(window.location.search || '');
                const rawSettings = params.get('settings');
                return writeResult(decodeSettingsParam(rawSettings));
            } catch(_) {
                if(outCapacity > 0) HEAPU8[outBuffer] = 0;
                return 0;
            }
        }, buffer, bufferSize);
    }
#endif

    enum class TouchControlsTarget { Port1Controller, Port2Controller, Expansion, MultitapP1, MultitapP2, MultitapP3, MultitapP4 };

    static fs::path& storageDirectory();

    static void setStorageDirectory(const fs::path& path);

    static fs::path& contentDirectory();

    static void setContentDirectory(const fs::path& path);

    static fs::path settingsFilePath();

    struct Improvements {   

        bool disableSpritesLimit = false;
        bool overclock = false;
        int maxRewindTime = 10;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Improvements, disableSpritesLimit, overclock, maxRewindTime)
    };

    struct Video {

        struct ShaderPass {
            std::string label = "";
            bool enabled = true;
            std::map<std::string, float> parameters;

            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ShaderPass, label, enabled, parameters)
        };

        int vsyncMode = 1;
        int filterMode = 0;
        std::string shaderName = "";
        std::vector<ShaderPass> shaderStack;
        std::map<std::string, std::vector<ShaderPass>> shaderPresets;
        int scaleMode = 0;
        int pixelPerfectScale = 3;
        bool horizontalStretch = false;
        bool fullScreen = false;
        int fullScreenMode = 0;
        std::string paletteName = "";
        int overscanTop = 8;
        int overscanRight = 0;
        int overscanBottom = 8;
        int overscanLeft = 0;

        friend void to_json(nlohmann::json& j, const Video& value)
        {
            j = nlohmann::json{
                {"vsyncMode", value.vsyncMode},
                {"filterMode", value.filterMode},
                {"shaderName", value.shaderName},
                {"shaderStack", value.shaderStack},
                {"shaderPresets", value.shaderPresets},
                {"scaleMode", value.scaleMode},
                {"pixelPerfectScale", value.pixelPerfectScale},
                {"horizontalStretch", value.horizontalStretch},
                {"fullScreen", value.fullScreen},
                {"fullScreenMode", value.fullScreenMode},
                {"paletteName", value.paletteName},
                {"overscanTop", value.overscanTop},
                {"overscanRight", value.overscanRight},
                {"overscanBottom", value.overscanBottom},
                {"overscanLeft", value.overscanLeft}
            };
        }

        friend void from_json(const nlohmann::json& j, Video& value)
        {
            const Video defaults;
            value.vsyncMode = j.value("vsyncMode", defaults.vsyncMode);
            value.filterMode = j.value("filterMode", defaults.filterMode);
            value.shaderName = defaults.shaderName;
            value.shaderStack.clear();
            value.shaderPresets.clear();
            if(j.contains("shaderStack") && j["shaderStack"].is_array()) {
                bool validShaderStack = true;
                for(const auto& item : j["shaderStack"]) {
                    if(item.is_object()) {
                        value.shaderStack.push_back(item.get<ShaderPass>());
                    } else {
                        validShaderStack = false;
                        break;
                    }
                }
                if(validShaderStack && !value.shaderStack.empty()) {
                    value.shaderName = value.shaderStack.front().label;
                } else {
                    value.shaderStack.clear();
                    value.shaderName.clear();
                }
            }
            if(j.contains("shaderPresets") && j["shaderPresets"].is_object()) {
                for(const auto& [presetName, presetValue] : j["shaderPresets"].items()) {
                    if(presetName.empty() || !presetValue.is_array()) continue;

                    std::vector<ShaderPass> presetStack;
                    bool validPreset = true;
                    for(const auto& item : presetValue) {
                        if(item.is_object()) {
                            presetStack.push_back(item.get<ShaderPass>());
                        } else {
                            validPreset = false;
                            break;
                        }
                    }
                    if(validPreset) {
                        value.shaderPresets[presetName] = std::move(presetStack);
                    }
                }
            }
            value.horizontalStretch = j.value("horizontalStretch", defaults.horizontalStretch);
            value.scaleMode = j.value("scaleMode", value.horizontalStretch ? 1 : defaults.scaleMode);
            value.pixelPerfectScale = j.value("pixelPerfectScale", defaults.pixelPerfectScale);
            if(!j.contains("pixelPerfectScale")) {
                if(value.scaleMode >= 2 && value.scaleMode <= 7) {
                    static constexpr std::array<int, 6> oldScaleModeValues{1, 2, 3, 4, 8, 16};
                    value.pixelPerfectScale = oldScaleModeValues[static_cast<size_t>(value.scaleMode - 2)];
                    value.scaleMode = 2;
                } else if(value.scaleMode == 8) {
                    value.scaleMode = 3;
                }
            }
            value.fullScreen = j.value("fullScreen", defaults.fullScreen);
            value.fullScreenMode = j.value("fullScreenMode", defaults.fullScreenMode);
            value.paletteName = j.value("paletteName", defaults.paletteName);
            value.overscanTop = j.value("overscanTop", defaults.overscanTop);
            value.overscanRight = j.value("overscanRight", defaults.overscanRight);
            value.overscanBottom = j.value("overscanBottom", defaults.overscanBottom);
            value.overscanLeft = j.value("overscanLeft", defaults.overscanLeft);
        }
    };    

    struct TouchControls {

        bool enabled = false;
        TouchControlsTarget target = TouchControlsTarget::Port1Controller;
        float transparency = 0.5f;        

        friend void to_json(nlohmann::json& j, const TouchControls& value)
        {
            j = nlohmann::json{
                {"enabled", value.enabled},
                {"target", static_cast<int>(value.target)},
                {"transparency", value.transparency}
            };
        }

        friend void from_json(const nlohmann::json& j, TouchControls& value)
        {
            const TouchControls defaults;
            value.enabled = j.value("enabled", defaults.enabled);
            value.target = static_cast<TouchControlsTarget>(j.value("target", static_cast<int>(defaults.target)));
            value.transparency = j.value("transparency", defaults.transparency);
        }
    };

    struct Input {

        bool automaticOnRomLoad = true;
        bool showMiniatures = false;

        struct Arkanoid {

            float sensitivity = 0.5f;

            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Arkanoid, sensitivity)
        };

        struct SnesMouse {

            float sensitivity = 0.25f;

            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SnesMouse, sensitivity)
        };

    private:

        std::map<std::string, ControllerInfo> controller;
        std::map<std::string, SnesControllerInfo> snesController;
        std::map<std::string, VirtualBoyControllerInfo> virtualBoyController;

    public:
                
        TouchControls touchControls;
        Arkanoid arkanoid;
        SnesMouse snesMouse;
        PowerPadInfo powerPad;
        KonamiHyperShotInfo konamiHyperShot;
        SystemInputInfo system;

        bool getControllerInfo(int index, ControllerInfo& result);

        bool getSnesControllerInfo(int index, SnesControllerInfo& result);

        bool getVirtualBoyControllerInfo(int index, VirtualBoyControllerInfo& result);

        void setControllerInfo(int index, const ControllerInfo& _if);

        void setSnesControllerInfo(int index, const SnesControllerInfo& info);

        void setVirtualBoyControllerInfo(int index, const VirtualBoyControllerInfo& info);

        void sanitizeDefaults();

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Input, touchControls, automaticOnRomLoad, showMiniatures, arkanoid, snesMouse, powerPad, konamiHyperShot, system, controller, snesController, virtualBoyController)
    };

    struct Audio {

        std::string audioDevice = "";
        int sampleRate = 0;
        int sampleSize = 0;
        float volume = 0.8f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Audio, audioDevice, sampleRate, sampleSize, volume)
    };

    struct Debug {

        bool showFps = false;
        bool cpuDebuggerEnabled = false;
        bool showCpuDebugger = false;
        bool showCpuBreakpoints = false;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Debug, showFps, cpuDebuggerEnabled, showCpuDebugger, showCpuBreakpoints)
    };

    struct Modding {

        bool useModIfAvailable = false;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Modding, useModIfAvailable)
    };

    struct Netplay {

        int snapshotWindowFrames = 600;
        int transportBackend = 0;
#ifdef __EMSCRIPTEN__
        bool useEmbeddedSignalingServer = false;
#else
        bool useEmbeddedSignalingServer = false;
#endif
        std::string signalingUrl = "ws://127.0.0.1:27990";
        int embeddedSignalingPort = 27990;
        std::string signalingRoomId = "default";
        std::string signalingPassword;
        bool autoGameplayTuning = true;
        int inputDelayFrames = 2;
        bool showNetplayDebugLog = false;
        int gameplayReceiveDelayMs = 0;
        std::string displayName = "Participant";
        std::string hostName = "127.0.0.1";
        int port = 27888;
        int maxPeers = 4;

        friend void to_json(nlohmann::json& j, const Netplay& value)
        {
            j = nlohmann::json{
                {"snapshotWindowFrames", value.snapshotWindowFrames},
                {"transportBackend", value.transportBackend},
                {"useEmbeddedSignalingServer", value.useEmbeddedSignalingServer},
                {"signalingUrl", value.signalingUrl},
                {"embeddedSignalingPort", value.embeddedSignalingPort},
                {"signalingRoomId", value.signalingRoomId},
                {"signalingPassword", value.signalingPassword},
                {"autoGameplayTuning", value.autoGameplayTuning},
                {"inputDelayFrames", value.inputDelayFrames},
                {"showNetplayDebugLog", value.showNetplayDebugLog},
                {"gameplayReceiveDelayMs", value.gameplayReceiveDelayMs},
                {"displayName", value.displayName},
                {"hostName", value.hostName},
                {"port", value.port},
                {"maxPeers", value.maxPeers}
            };
        }

        friend void from_json(const nlohmann::json& j, Netplay& value)
        {
            const Netplay defaults;
            value.snapshotWindowFrames = j.value("snapshotWindowFrames", defaults.snapshotWindowFrames);
            value.transportBackend = j.value("transportBackend", defaults.transportBackend);
            value.useEmbeddedSignalingServer = j.value("useEmbeddedSignalingServer", defaults.useEmbeddedSignalingServer);
            value.signalingUrl = j.value("signalingUrl", defaults.signalingUrl);
            value.embeddedSignalingPort = std::clamp(
                j.value("embeddedSignalingPort", defaults.embeddedSignalingPort),
                1,
                65535
            );
            value.signalingRoomId = j.value("signalingRoomId", defaults.signalingRoomId);
            value.signalingPassword = j.value("signalingPassword", defaults.signalingPassword);
            value.autoGameplayTuning = j.value("autoGameplayTuning", defaults.autoGameplayTuning);
            value.inputDelayFrames = j.value("inputDelayFrames", defaults.inputDelayFrames);
            value.showNetplayDebugLog =
                j.value("showNetplayDebugLog",
                        j.value("debugMode", defaults.showNetplayDebugLog));
            value.gameplayReceiveDelayMs = j.value("gameplayReceiveDelayMs", defaults.gameplayReceiveDelayMs);
            value.displayName = j.value("displayName", defaults.displayName);
            value.hostName = j.value("hostName", defaults.hostName);
            value.port = j.value("port", defaults.port);
            value.maxPeers = j.value("maxPeers", defaults.maxPeers);
        }
    };

    const int MAX_RECENT_FILES = 10;    

    struct Data {

    private:

        std::vector<std::string> recentFiles;
        std::string lastFolder;
        
    public:
        int saveStateSlot = 0;
                
        Input input; 
        Improvements improvements;
        Netplay netplay;
        Video video;
        Audio audio;
        Debug debug;
        Modding modding;

        void addRecentFile(const std::string path);

        const std::string& getLastFolder();

        void setLastFolder(const std::string& path);

        const std::vector<std::string> getRecentFiles();

        void sanitizeDefaults();

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Data, input, recentFiles, lastFolder, saveStateSlot, improvements, netplay, video, audio, debug, modding)
    } data;

    AppSettings(const AppSettings&) = delete;
    AppSettings& operator = (const AppSettings&) = delete;    

    AppSettings();
    void load();
    void sanitizeDefaults();
    static AppSettings& instance();
    ~AppSettings();
    void save();
};


