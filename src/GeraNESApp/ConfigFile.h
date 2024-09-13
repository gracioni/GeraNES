#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#if __GNUC__
    #if __GNUC__ >= 8 || defined(__EMSCRIPTEN__)
        #include <filesystem>
        namespace fs = std::filesystem;
    #else
        #include <experimental/filesystem>
        namespace fs = std::experimental::filesystem;
    #endif
#else
    #include <filesystem>
    namespace fs = std::filesystem;
#endif

#include "InputInfo.h"

#include "GeraNES/Logger.h"


class ConfigFile {

private:

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

    struct Audio {

        std::string audioDevice = "";
        float volume = 0.5f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Audio, audioDevice, volume)
    };

    const char* FILENAME = "settings.json";
    const int MAX_RECENT_FILES = 10;    

    struct Data {
        std::map<std::string, InputInfo> input;
        std::vector<std::string> recentFiles;
        std::string lastFolder;    
        Improvements improvements;
        Video video;
        Audio audio;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Data, input, recentFiles, lastFolder, improvements, video, audio)
    } m_data;

    ConfigFile(const ConfigFile&) = delete;
    ConfigFile& operator = (const ConfigFile&) = delete;    

    ConfigFile() {
        load();
    }    
  
    void load() {

        Logger::instance().log("Loading settings...", Logger::Type::INFO);

        std::ifstream file(FILENAME);

        if(file.is_open()) {
            nlohmann::json data = nlohmann::json::parse(file);
            data.get_to(m_data);
        }

        sanitizeDefaults();
    }

    void sanitizeDefaults() {

        if(m_data.input.count("0") == 0) {

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

            m_data.input.insert(std::make_pair("0", i));
        }

        if(m_data.input.count("1") == 0) {

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

            m_data.input.insert(std::make_pair("1", i)); 
        }      
         
        if(m_data.lastFolder == "") m_data.lastFolder = fs::current_path().string();
    }

public:    

    static ConfigFile& instance() {

        static std::unique_ptr<ConfigFile> _instance;

        if (!_instance) {
            _instance.reset(new ConfigFile());
        }

        return *_instance;
    }

    ~ConfigFile() {
        save();
    }
  
    void save() {
        Logger::instance().log("Saving settings...", Logger::Type::INFO);
        std::ofstream file(FILENAME);
        file << std::setw(4) << nlohmann::json(m_data);
    }    

    bool getInputInfo(int index, InputInfo& result) {

        if(m_data.input.count(std::to_string(index))) {
            result = m_data.input[std::to_string(index)];
            return true;
        }

        return false;
    }

    void setInputInfo(int index, const InputInfo& _if) {

        if(m_data.input.count(std::to_string(index)) > 0)
            m_data.input[std::to_string(index)] = _if;
        else
            m_data.input.insert(std::make_pair(std::to_string(index), _if));

    }

    void addRecentFile(const std::string path) {

        auto it = std::remove_if(m_data.recentFiles.begin(), m_data.recentFiles.end(),
        [path](const std::string& str) { return str.find(path) != std::string::npos; } );

        m_data.recentFiles.erase(it, m_data.recentFiles.end());

        m_data.recentFiles.insert(m_data.recentFiles.begin(), path);

        while(m_data.recentFiles.size() > 10) {
            m_data.recentFiles.pop_back();
        }
    }

    const std::vector<std::string> getRecentFiles() {
        return m_data.recentFiles;
    }

    void setLastFolder(const std::string& path) {
        fs::path p = path;
        m_data.lastFolder = p.parent_path().string();
    }

    const std::string& getLastFolder() {
        return m_data.lastFolder;
    }

    const std::string& getAudioDevice() {
        return m_data.audio.audioDevice;
    }

    void setAudioDevice(const std::string& name) {
        m_data.audio.audioDevice = name;
    }

    void setDisableSpritesLimit(bool state) {
        m_data.improvements.disableSpritesLimit = state;
    }

    bool getDisableSpritesLimit() {
        return m_data.improvements.disableSpritesLimit;
    }

    void setOverclock(bool state) {
        m_data.improvements.overclock = state;
    }

    bool getOverclock() {
        return m_data.improvements.overclock;
    }

    void setMaxRewindTime(int seconds) {
        m_data.improvements.maxRewindTime = seconds;
    }

    int getMaxRewindTime() {
        return m_data.improvements.maxRewindTime;
    }

    int getVSyncMode() {
        return m_data.video.vsyncMode;
    }

    void setVSyncMode(int mode) {
        m_data.video.vsyncMode = mode;
    }

    int getFilterMode() {
        return m_data.video.filterMode;
    }

    void setFilterMode(int mode) {
        m_data.video.filterMode = mode;
    }

    bool getHorizontalStretch() {
        return m_data.video.horizontalStretch;
    }

    void setHorizontalStretch(bool state) {
        m_data.video.horizontalStretch = state;
    }

    bool getFullScreen() {
        return m_data.video.fullScreen;
    }

    void setFullScreen(bool state) {
        m_data.video.fullScreen = state;
    }

    void setShaderName(const std::string& shader) {
        m_data.video.shaderName = shader;
    }

    const std::string& getShaderName() {
        return m_data.video.shaderName;
    }

    void setAudioVolume(float volume) {
        m_data.audio.volume = volume;
    }

    float getAudioVolume() {
        return m_data.audio.volume;
    }

    
    
};

#endif