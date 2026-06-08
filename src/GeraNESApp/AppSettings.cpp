#include "GeraNESApp/AppSettings.h"

#include <array>
#include <exception>
#include <fstream>
#include <iomanip>
#include "logger/logger.h"

fs::path& AppSettings::storageDirectory()
{
    static fs::path path = fs::current_path();
    return path;
}

fs::path& AppSettings::contentDirectory()
{
    static fs::path path = storageDirectory();
    return path;
}

void AppSettings::setStorageDirectory(const fs::path& path)
{
    if(path.empty()) return;
    storageDirectory() = path;
}

void AppSettings::setContentDirectory(const fs::path& path)
{
    if(path.empty()) return;
    contentDirectory() = path;
}

fs::path AppSettings::settingsFilePath()
{
    return contentDirectory() / "settings.json";
}

bool AppSettings::Input::getControllerInfo(int index, ControllerInfo& result)
{
    if(controller.count(std::to_string(index))) {
        result = controller[std::to_string(index)];
        return true;
    }

    return false;
}

bool AppSettings::Input::getSnesControllerInfo(int index, SnesControllerInfo& result)
{
    if(snesController.count(std::to_string(index))) {
        result = snesController[std::to_string(index)];
        return true;
    }

    return false;
}

bool AppSettings::Input::getVirtualBoyControllerInfo(int index, VirtualBoyControllerInfo& result)
{
    if(virtualBoyController.count(std::to_string(index))) {
        result = virtualBoyController[std::to_string(index)];
        return true;
    }

    return false;
}

void AppSettings::Input::setControllerInfo(int index, const ControllerInfo& _if)
{
    if(controller.count(std::to_string(index)) > 0)
        controller[std::to_string(index)] = _if;
    else
        controller.insert(std::make_pair(std::to_string(index), _if));
}

void AppSettings::Input::setSnesControllerInfo(int index, const SnesControllerInfo& info)
{
    if(snesController.count(std::to_string(index)) > 0)
        snesController[std::to_string(index)] = info;
    else
        snesController.insert(std::make_pair(std::to_string(index), info));
}

void AppSettings::Input::setVirtualBoyControllerInfo(int index, const VirtualBoyControllerInfo& info)
{
    if(virtualBoyController.count(std::to_string(index)) > 0)
        virtualBoyController[std::to_string(index)] = info;
    else
        virtualBoyController.insert(std::make_pair(std::to_string(index), info));
}

void AppSettings::Input::sanitizeDefaults()
{
    if(controller.count("0") == 0) {
        ControllerInfo i;

        i.a = "X";
        i.b = "Z";
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

        i.a = "X";
        i.b = "Z";
        i.x = "S";
        i.y = "A";
        i.l = "Q";
        i.r = "W";
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
        i.x = "Y";
        i.y = "T";
        i.l = "5";
        i.r = "6";
        i.select = "N";
        i.start = "M";
        i.up = "I";
        i.down = "K";
        i.left = "J";
        i.right = "L";

        snesController.insert(std::make_pair("1", i));
    }

    if(snesController.count("2") == 0) {
        SnesControllerInfo i;
        snesController.insert(std::make_pair("2", i));
    }

    if(snesController.count("3") == 0) {
        SnesControllerInfo i;
        snesController.insert(std::make_pair("3", i));
    }

    if(virtualBoyController.count("0") == 0) {
        VirtualBoyControllerInfo i;

        i.a = "X";
        i.b = "Z";
        i.l = "A";
        i.r = "S";
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

    if(konamiHyperShot.p1Run.empty()) konamiHyperShot.p1Run = "X";
    if(konamiHyperShot.p1Jump.empty()) konamiHyperShot.p1Jump = "Z";
    if(konamiHyperShot.p2Run.empty()) konamiHyperShot.p2Run = "H";
    if(konamiHyperShot.p2Jump.empty()) konamiHyperShot.p2Jump = "G";

    const std::array<const char*, 12> defaultBindings{"1", "2", "3", "4", "Q", "W", "E", "R", "A", "S", "D", "F"};
    for(size_t i = 0; i < powerPad.bindings.size(); ++i) {
        if(powerPad.bindings[i].empty()) {
            powerPad.bindings[i] = defaultBindings[i];
        }
    }
}

void AppSettings::Data::addRecentFile(const std::string path)
{
    auto it = std::remove_if(recentFiles.begin(), recentFiles.end(),
                             [path](const std::string& str) { return str == path; });

    recentFiles.erase(it, recentFiles.end());

    recentFiles.insert(recentFiles.begin(), path);

    while(recentFiles.size() > 10) {
        recentFiles.pop_back();
    }
}

const std::string& AppSettings::Data::getLastFolder()
{
    return lastFolder;
}

void AppSettings::Data::setLastFolder(const std::string& path)
{
    fs::path p = path;
    lastFolder = p.parent_path().string();
}

const std::vector<std::string> AppSettings::Data::getRecentFiles()
{
    return recentFiles;
}

void AppSettings::Data::sanitizeDefaults()
{
    if(lastFolder == "") lastFolder = AppSettings::contentDirectory().string();
    saveStateSlot = std::clamp(saveStateSlot, 0, 9);
}

AppSettings::AppSettings()
{
    load();
}

void AppSettings::load()
{
    Logger::instance().log("Loading settings...", Logger::Type::INFO);

    const fs::path path = settingsFilePath();
    bool shouldPersistDefaults = false;

    std::ifstream file(path);

    if(file.is_open()) {
        try {
            nlohmann::json auxData = nlohmann::json::parse(file);
            auxData.get_to(data);
        } catch(const std::exception& ex) {
            Logger::instance().log(
                std::string("Failed to load settings.json, using defaults: ") + ex.what(),
                Logger::Type::WARNING
            );
            data = Data{};
            shouldPersistDefaults = true;
        }
    } else {
        shouldPersistDefaults = true;
    }

#ifdef __EMSCRIPTEN__
    std::array<char, 8192> urlOverrideBuffer{};
    const int urlOverrideLength = loadUrlSettingsOverrideJson(
        urlOverrideBuffer.data(),
        static_cast<int>(urlOverrideBuffer.size())
    );
    if(urlOverrideLength > 0) {
        try {
            const nlohmann::json overrideJson =
                nlohmann::json::parse(std::string(urlOverrideBuffer.data(), static_cast<size_t>(urlOverrideLength)));
            nlohmann::json merged = nlohmann::json(data);
            merged.merge_patch(overrideJson);
            merged.get_to(data);
            Logger::instance().log("Applied URL settings override", Logger::Type::INFO);
        } catch(const std::exception& ex) {
            Logger::instance().log(
                std::string("Failed to parse URL settings override: ") + ex.what(),
                Logger::Type::WARNING
            );
        }
    } else if(urlOverrideLength < 0) {
        Logger::instance().log("URL settings override is too large", Logger::Type::WARNING);
    }
#endif

    sanitizeDefaults();

    if(shouldPersistDefaults) {
        save();
    }
}

void AppSettings::sanitizeDefaults()
{
#ifdef __EMSCRIPTEN__
    data.netplay.transportBackend = 1;
    data.netplay.useEmbeddedSignalingServer = false;
#else
    data.netplay.transportBackend = std::clamp(data.netplay.transportBackend, 0, 1);
#endif
    if(static_cast<int>(data.input.touchControls.target) < static_cast<int>(TouchControlsTarget::Port1Controller) ||
       static_cast<int>(data.input.touchControls.target) > static_cast<int>(TouchControlsTarget::MultitapP4)) {
        data.input.touchControls.target = TouchControlsTarget::Port1Controller;
    }
    data.video.scaleMode = std::clamp(data.video.scaleMode, 0, 3);
    data.video.pixelPerfectScale = std::clamp(data.video.pixelPerfectScale, 1, 16);
    data.video.horizontalStretch = data.video.scaleMode == 1;
    data.video.fullScreenMode = std::clamp(data.video.fullScreenMode, 0, 1);
    data.audio.sampleRate = std::max(0, data.audio.sampleRate);
    data.audio.sampleSize = std::max(0, data.audio.sampleSize);
    data.input.sanitizeDefaults();
    data.sanitizeDefaults();
}

AppSettings& AppSettings::instance()
{
    static AppSettings _instance;
    return _instance;
}

AppSettings::~AppSettings()
{
    save();
}

void AppSettings::save()
{
    Logger::instance().log("Saving settings...", Logger::Type::INFO);
    std::error_code ec;
    fs::create_directories(settingsFilePath().parent_path(), ec);
    std::ofstream file(settingsFilePath());
    file << std::setw(4) << nlohmann::json(data);
}
