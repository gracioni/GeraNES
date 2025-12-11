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

#include "logger/logger.h"

enum class ButtonsMode {Absolute, Column};
enum class DigitaPadMode {Absolute, CentralizeOnTouch, Relative};

static constexpr std::array<const char*, 2> ButtonsModeLabels {"Absolute", "Column"};
static constexpr std::array<const char*, 3> DigitaPadModeLabels {"Absolute", "CentralizeOnTouch", "Relative"};

class AppSettings {

public:

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

    private:

        std::map<std::string, ControllerInfo> controller;

    public:

        TouchControls touchControls;

        bool getControllerInfo(int index, ControllerInfo& result) {

            if(controller.count(std::to_string(index))) {
                result = controller[std::to_string(index)];
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
                i.saveState = "F5";
                i.loadState = "F6";
                i.rewind = "Backspace";

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
                i.saveState = "";
                i.loadState = "";
                i.rewind = "";

                controller.insert(std::make_pair("1", i)); 
            }
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Input, touchControls, controller)
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

    const char* FILENAME = "settings.json";
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
            if(lastFolder == "") lastFolder = fs::current_path().string();
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

        std::ifstream file(FILENAME);

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
        std::ofstream file(FILENAME);
        file << std::setw(4) << nlohmann::json(data);
    }  
};
