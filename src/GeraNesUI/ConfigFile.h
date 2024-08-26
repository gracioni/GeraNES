#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <experimental/filesystem>

#include <nlohmann/json.hpp>
#include "InputInfo.h"

#include "GeraNes/Logger.h"

namespace fs = std::experimental::filesystem;

class ConfigFile {

private:

    struct Improvements {   

        bool disableSpritesLimit = false;
        bool overclock = false;
        int maxRewindTime = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Improvements, disableSpritesLimit, overclock, maxRewindTime)
    };

    struct Video {

        int vsyncMode = 0;
        int filterMode = 0;
        std::string shader = "";  
        bool horizontalStretch = false;
        bool fullScreen = false;
        

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Video, vsyncMode, filterMode, shader, horizontalStretch, fullScreen)
    };

    struct Audio {

        std::string audioDevice = "";

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Audio, audioDevice)
    };

    const char* FILENAME = "settings.json";
    const int MAX_RECENT_FILES = 10;

    static std::unique_ptr<ConfigFile> _instance;

    std::map<std::string, InputInfo> input;
    std::vector<std::string> recentFiles;
    std::string lastFolder;    
    Improvements improvements;
    Video video;
    Audio audio;

    //ConfigFile(const ConfigFile&) = delete;
    ConfigFile& operator = (const ConfigFile&) = delete;

    ConfigFile();  
  
    void load() {

        Logger::instance().log("Loading settings...", Logger::INFO);

        std::ifstream file(FILENAME);

        if(file.is_open()) {
            nlohmann::json data = nlohmann::json::parse(file);
            data.get_to(*this);
        }

        sanitizeDefaults();
    }

    void sanitizeDefaults() {

        if(input.count("0") == 0) {

            InputInfo i;

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

            input.insert(std::make_pair("0", i));
        }

        if(input.count("1") == 0) {

            InputInfo i;

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

            input.insert(std::make_pair("1", i)); 
        }      
         
        if(lastFolder == "") lastFolder = fs::current_path().string();
    }

public:

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ConfigFile, input, recentFiles, lastFolder, improvements, video, audio)

    ~ConfigFile();

    /*
    void to_json(nlohmann::json& j, const ConfigFile& p)
    {
        j["inputInfo"] = p.inputInfo;
        j["recentFiles"] =  p.recentFiles;
    }

    void from_json(const nlohmann::json& j, const ConfigFile& p)
    {
        p.cbor      = j.at("cbor").get< std::string >();
        p.hex       = j.at("hex").get< std::string >();
        p.roundtrip = j.at("roundtrip").get< bool >();
        
        // if we also allow "null" values, then we need to add an "is_string()"
        // check
        if (j.count("diagnostic") != 0)
        {
            p.diagnostic = j.at("diagnostic").get< std::string >();
        }
    }
    */    

    static ConfigFile& instance();    

    void save() {
        Logger::instance().log("Saving settings...", Logger::INFO);
        std::ofstream file(FILENAME);
        file << std::setw(4) << nlohmann::json(*this);
    }    

    bool getInputInfo(int index, InputInfo& result) {

        if(input.count(std::to_string(index))) {
            result = input[std::to_string(index)];
            return true;
        }

        return false;
    }

    void setInputInfo(int index, const InputInfo& _if) {

        if(input.count(std::to_string(index)) > 0)
            input[std::to_string(index)] = _if;
        else
            input.insert(std::make_pair(std::to_string(index), _if));

    }

    void addRecentFile(const std::string path) {

        auto it = std::remove_if(recentFiles.begin(), recentFiles.end(),
        [path](const std::string& str) { return str.find(path) != std::string::npos; } );

        recentFiles.erase(it, recentFiles.end());

        recentFiles.insert(recentFiles.begin(), path);

        while(recentFiles.size() > 10) {
            recentFiles.pop_back();
        }
    }

    const std::vector<std::string> getRecentFiles() {
        return recentFiles;
    }

    void setLastFolder(const std::string& path) {
        fs::path p = path;
        lastFolder = p.parent_path().string();
    }

    const std::string& getLastFolder() {
        return lastFolder;
    }

    const std::string& getAudioDevice() {
        return audio.audioDevice;
    }

    void setAudioDevice(const std::string& name) {
        audio.audioDevice = name;
    }

    void setDisableSpritesLimit(bool state) {
        improvements.disableSpritesLimit = state;
    }

    bool getDisableSpritesLimit() {
        return improvements.disableSpritesLimit;
    }

    void setOverclock(bool state) {
        improvements.overclock = state;
    }

    bool getOverclock() {
        return improvements.overclock;
    }

    int setMaxRewindTime(int seconds) {
        improvements.maxRewindTime = seconds;
    }

    int getMaxRewindTime() {
        return improvements.maxRewindTime;
    }

    int getVSyncMode() {
        return video.vsyncMode;
    }

    void setVSyncMode(int mode) {
        video.vsyncMode = mode;
    }

    int getFilterMode() {
        return video.filterMode;
    }

    void setFilterMode(int mode) {
        video.filterMode = mode;
    }

    bool getHorizontalStretch() {
        return video.horizontalStretch;
    }

    void setHorizontalStretch(bool state) {
        video.horizontalStretch = state;
    }

    bool getFullScreen() {
        return video.fullScreen;
    }

    void setFullScreen(bool state) {
        video.fullScreen = state;
    }

    void setShader(const std::string& shader) {
        video.shader = shader;
    }

    const std::string& getShader() {
        return video.shader;
    }

    
    
};

#endif