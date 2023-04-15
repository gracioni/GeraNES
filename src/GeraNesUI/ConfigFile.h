#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#include <map>
#include <vector>
#include <fstream>
#include <iostream>
#include <experimental/filesystem>

#include <nlohmann/json.hpp>
#include "InputInfo.h"

class ConfigFile {

private:

    const char* FILENAME = "config.json";
    const int MAX_RECENT_FILES = 10;

    static std::unique_ptr<ConfigFile> _instance;

    std::map<std::string, InputInfo> inputInfo;
    std::vector<std::string> recentFiles;
    std::string lastFolder;
    std::string audioDevice;

    ConfigFile(const ConfigFile&) = delete;
    ConfigFile& operator = (const ConfigFile&) = delete;

    ConfigFile();  
  
    void load() {

        std::ifstream file(FILENAME);

        if(file.is_open()) {
            nlohmann::json data = nlohmann::json::parse(file);
            data.get_to(*this);
        }

        sanitizeDefaults();
    }

    void sanitizeDefaults() {

        if(inputInfo.count("0") == 0) {

            InputInfo input;

            input.a = "Left Alt";
            input.b = "Left Ctrl";
            input.select = "Space";
            input.start = "Return";        
            input.up = "Up";
            input.down = "Down";
            input.left = "Left";
            input.right = "Right";
            input.saveState = "F5";
            input.loadState = "F6";
            input.rewind = "Backspace";

            inputInfo.insert(std::make_pair("0", input));
        }

        if(inputInfo.count("1") == 0) {

            InputInfo input;

            input.a = "H";
            input.b = "G";
            input.select = "T";
            input.start = "Y";        
            input.up = "I";
            input.down = "K";
            input.left = "J";
            input.right = "L";
            input.saveState = "";
            input.loadState = "";
            input.rewind = "";

            inputInfo.insert(std::make_pair("1", input)); 
        }      
         
        if(lastFolder == "") lastFolder = std::experimental::filesystem::current_path().string();
    }

public:

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ConfigFile, inputInfo, recentFiles, lastFolder, audioDevice)

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
        std::cout << "saving..." << std::endl;
       std::ofstream file(FILENAME);
       file << std::setw(4) << nlohmann::json(*this);
    }    

    bool getInputInfo(int index, InputInfo& result) {

        if(inputInfo.count(std::to_string(index))) {
            result = inputInfo[std::to_string(index)];
            return true;
        }

        return false;
    }

    void setInputInfo(int index, const InputInfo& _if) {

        if(inputInfo.count(std::to_string(index)) > 0)
            inputInfo[std::to_string(index)] = _if;
        else
            inputInfo.insert(std::make_pair(std::to_string(index), _if));

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
        std::experimental::filesystem::path p = path;
        lastFolder = p.parent_path().string();
    }

    const std::string& getLastFolder() {
        return lastFolder;
    }

    const std::string& getAudioDevice() {
        return audioDevice;
    }

    void setAudioDevice(const std::string& name) {
        audioDevice = name;
    }
    
};

#endif