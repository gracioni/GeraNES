#pragma once

#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#include <filesystem>
namespace fs = std::filesystem;

#include "ControllerInfo.h"
#include "KonamiHyperShotInfo.h"
#include "PowerPadInfo.h"
#include "SnesControllerInfo.h"
#include "SystemInputInfo.h"
#include "VirtualBoyControllerInfo.h"

#include "logger/logger.h"

enum class ButtonsMode {Absolute, Column};
enum class DigitaPadMode {Absolute, CentralizeOnTouch, Relative};

static constexpr std::array<const char*, 2> ButtonsModeLabels {"Absolute", "Column"};
static constexpr std::array<const char*, 3> DigitaPadModeLabels {"Absolute", "CentralizeOnTouch", "Relative"};

class AppSettings {

public:

    static fs::path& storageDirectory()
    {
        static fs::path path = fs::current_path();
        return path;
    }

    static void setStorageDirectory(const fs::path& path)
    {
        if(path.empty()) return;
        storageDirectory() = path;
    }

    static fs::path settingsFilePath()
    {
        return storageDirectory() / "settings.json";
    }

    struct Improvements {   

        bool disableSpritesLimit = false;
        bool overclock = false;
        int maxRewindTime = 10;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Improvements, disableSpritesLimit, overclock, maxRewindTime)
    };

    struct Video {

        int vsyncMode = 1;
        int filterMode = 0;
        std::string shaderName = "";  
        bool horizontalStretch = false;
        bool fullScreen = false;
        

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Video, vsyncMode, filterMode, shaderName, horizontalStretch, fullScreen)
    };    

    struct TouchControls {

        bool enabled = false;
        ButtonsMode buttonsMode = ButtonsMode::Absolute;
        DigitaPadMode digitalPadMode = DigitaPadMode::Absolute;
        float transparency = 0.5f;        

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TouchControls, enabled, buttonsMode, digitalPadMode, transparency)
    };

    struct Input {

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

        bool getControllerInfo(int index, ControllerInfo& result) {

            if(controller.count(std::to_string(index))) {
                result = controller[std::to_string(index)];
                return true;
            }

            return false;
        }

        bool getSnesControllerInfo(int index, SnesControllerInfo& result) {

            if(snesController.count(std::to_string(index))) {
                result = snesController[std::to_string(index)];
                return true;
            }

            return false;
        }

        bool getVirtualBoyControllerInfo(int index, VirtualBoyControllerInfo& result) {

            if(virtualBoyController.count(std::to_string(index))) {
                result = virtualBoyController[std::to_string(index)];
                return true;
            }

            return false;
        }

        void setControllerInfo(int index, const ControllerInfo& _if) {

            if(controller.count(std::to_string(index)) > 0)
                controller[std::to_string(index)] = _if;
            else
                controller.insert(std::make_pair(std::to_string(index), _if));

        }

        void setSnesControllerInfo(int index, const SnesControllerInfo& info) {

            if(snesController.count(std::to_string(index)) > 0)
                snesController[std::to_string(index)] = info;
            else
                snesController.insert(std::make_pair(std::to_string(index), info));

        }

        void setVirtualBoyControllerInfo(int index, const VirtualBoyControllerInfo& info) {

            if(virtualBoyController.count(std::to_string(index)) > 0)
                virtualBoyController[std::to_string(index)] = info;
            else
                virtualBoyController.insert(std::make_pair(std::to_string(index), info));

        }

        void sanitizeDefaults() {

            if(controller.count("0") == 0) {

                ControllerInfo i;

                i.a = "Left Alt";
                i.b = "Left Ctrl";
                i.select = "Space";
                i.start = "Return";        
                i.up = "Up";
                i.down = "Down";
                i.left = "Left";
                i.right = "Right";

                controller.insert(std::make_pair("0", i));
            }

            if(controller.count("1") == 0) {

                ControllerInfo i;

                i.a = "H";
                i.b = "G";
                i.select = "T";
                i.start = "Y";        
                i.up = "I";
                i.down = "K";
                i.left = "J";
                i.right = "L";

                controller.insert(std::make_pair("1", i)); 
            }

            if(controller.count("2") == 0) {

                ControllerInfo i;
                controller.insert(std::make_pair("2", i));
            }

            if(controller.count("3") == 0) {

                ControllerInfo i;
                controller.insert(std::make_pair("3", i));
            }

            if(snesController.count("0") == 0) {

                SnesControllerInfo i;

                i.a = "Left Alt";
                i.b = "Left Ctrl";
                i.x = "X";
                i.y = "Z";
                i.l = "A";
                i.r = "S";
                i.select = "Space";
                i.start = "Return";
                i.up = "Up";
                i.down = "Down";
                i.left = "Left";
                i.right = "Right";

                snesController.insert(std::make_pair("0", i));
            }

            if(snesController.count("1") == 0) {

                SnesControllerInfo i;

                i.a = "H";
                i.b = "G";
                i.x = "U";
                i.y = "Y";
                i.l = "R";
                i.r = "T";
                i.select = "T";
                i.start = "Y";
                i.up = "I";
                i.down = "K";
                i.left = "J";
                i.right = "L";

                snesController.insert(std::make_pair("1", i));
            }

            if(virtualBoyController.count("0") == 0) {

                VirtualBoyControllerInfo i;

                i.a = "Left Alt";
                i.b = "Left Ctrl";
                i.l = "Q";
                i.r = "E";
                i.select = "Space";
                i.start = "Return";
                i.up = "Up";
                i.down = "Down";
                i.left = "Left";
                i.right = "Right";
                i.up2 = "W";
                i.down2 = "S";
                i.left2 = "A";
                i.right2 = "D";

                virtualBoyController.insert(std::make_pair("0", i));
            }

            if(virtualBoyController.count("1") == 0) {

                VirtualBoyControllerInfo i;

                i.a = "H";
                i.b = "G";
                i.l = "U";
                i.r = "O";
                i.select = "T";
                i.start = "Y";
                i.up = "I";
                i.down = "K";
                i.left = "J";
                i.right = "L";
                i.up2 = "Keypad 8";
                i.down2 = "Keypad 5";
                i.left2 = "Keypad 4";
                i.right2 = "Keypad 6";

                virtualBoyController.insert(std::make_pair("1", i));
            }

            if(system.saveState.empty()) system.saveState = "F5";
            if(system.loadState.empty()) system.loadState = "F6";
            if(system.rewind.empty()) system.rewind = "Backspace";
            if(system.speed.empty()) system.speed = "+";

            if(konamiHyperShot.p1Run.empty()) konamiHyperShot.p1Run = "Left Alt";
            if(konamiHyperShot.p1Jump.empty()) konamiHyperShot.p1Jump = "Left Ctrl";
            if(konamiHyperShot.p2Run.empty()) konamiHyperShot.p2Run = "H";
            if(konamiHyperShot.p2Jump.empty()) konamiHyperShot.p2Jump = "G";

            const std::array<const char*, 12> defaultBindings {"1","2","3","4","Q","W","E","R","A","S","D","F"};
            for(size_t i = 0; i < powerPad.bindings.size(); ++i) {
                if(powerPad.bindings[i].empty()) {
                    powerPad.bindings[i] = defaultBindings[i];
                }
            }
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Input, touchControls, arkanoid, snesMouse, powerPad, konamiHyperShot, system, controller, snesController, virtualBoyController)
    };

    struct Audio {

        std::string audioDevice = "";
        float volume = 0.8f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Audio, audioDevice, volume)
    };

    struct Debug {

        bool showFps = false;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Debug, showFps)
    };

    const int MAX_RECENT_FILES = 10;    

    struct Data {

    private:

        std::vector<std::string> recentFiles;
        std::string lastFolder;
        
    public:
                
        Input input; 
        Improvements improvements;
        Video video;
        Audio audio;
        Debug debug;

        void addRecentFile(const std::string path) {

            auto it = std::remove_if(recentFiles.begin(), recentFiles.end(),
            [path](const std::string& str) { return str.find(path) != std::string::npos; } );

            recentFiles.erase(it, recentFiles.end());

            recentFiles.insert(recentFiles.begin(), path);

            while(recentFiles.size() > 10) {
                recentFiles.pop_back();
            }
        }

        const std::string& getLastFolder() {
            return lastFolder;
        }

        void setLastFolder(const std::string& path) {
            fs::path p = path;
            lastFolder = p.parent_path().string();
        }

        const std::vector<std::string> getRecentFiles() {
            return recentFiles;
        }

        void sanitizeDefaults() {
            if(lastFolder == "") lastFolder = AppSettings::storageDirectory().string();
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Data, input, recentFiles, lastFolder, improvements, video, audio, debug)
    } data;

    AppSettings(const AppSettings&) = delete;
    AppSettings& operator = (const AppSettings&) = delete;    

    AppSettings() {
        load();
    }    
  
    void load() {

        Logger::instance().log("Loading settings...", Logger::Type::INFO);

        std::ifstream file(settingsFilePath());

        if(file.is_open()) {
            nlohmann::json auxData = nlohmann::json::parse(file);
            auxData.get_to(data);
        }

        sanitizeDefaults();
    }

    void sanitizeDefaults() {        
        data.input.sanitizeDefaults();        
        data.sanitizeDefaults();  
    }

    static AppSettings& instance() {
        static AppSettings _instance;
        return _instance;
    }

    ~AppSettings() {
        save();
    }
  
    void save() {
        Logger::instance().log("Saving settings...", Logger::Type::INFO);
        std::ofstream file(settingsFilePath());
        file << std::setw(4) << nlohmann::json(data);
    }  
};
