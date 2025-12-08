#ifndef GAME_DATABASE_H
#define GAME_DATABASE_H

#include <string>
#include <map>
#include <fstream>
#include <regex>
#include <map>
#include <cassert>

#include <functional>
#include <vector>

#include "logger/logger.h"
#include "util/StringTrim.h"

class GameDatabase {

public:

    enum class System
    {
        NesNtsc,
        NesPal,
        Famicom,
        Dendy,
        VsSystem,
        Playchoice,
        FDS,
        FamicomNetworkSystem,
        Unknown
    };

    enum class Battery {
        Default,
        Yes,
        No
    };

    enum class MirroringType {
        DEFAULT,
        HORIZONTAL,
        VERTICAL,
        SINGLE_SCREEN_A,
        SINGLE_SCREEN_B,
        FOUR_SCREEN
    };

    enum class InputType
    {
        Unspecified = 0,
        StandardControllers = 1,
        FourScore = 2,
        FourPlayerAdapter = 3,
        VsSystem = 4,
        VsSystemSwapped = 5,
        VsSystemSwapAB = 6,
        VsZapper = 7,
        Zapper = 8,
        TwoZappers = 9,
        BandaiHypershot = 0x0A,
        PowerPadSideA = 0x0B,
        PowerPadSideB = 0x0C,
        FamilyTrainerSideA = 0x0D,
        FamilyTrainerSideB = 0x0E,
        ArkanoidControllerNes = 0x0F,
        ArkanoidControllerFamicom = 0x10,
        DoubleArkanoidController = 0x11,
        KonamiHyperShot = 0x12,
        PachinkoController = 0x13,
        ExcitingBoxing = 0x14,
        JissenMahjong = 0x15,
        PartyTap = 0x16,
        OekaKidsTablet = 0x17,
        BarcodeBattler = 0x18,
        MiraclePiano = 0x19, //not supported yet
        PokkunMoguraa = 0x1A, //not supported yet
        TopRider = 0x1B, //not supported yet
        DoubleFisted = 0x1C, //not supported yet
        Famicom3dSystem = 0x1D, //not supported yet
        DoremikkoKeyboard = 0x1E, //not supported yet
        ROB = 0x1F, //not supported yet
        FamicomDataRecorder = 0x20,
        TurboFile = 0x21,
        BattleBox = 0x22,
        FamilyBasicKeyboard = 0x23,
        Pec586Keyboard = 0x24, //not supported yet
        Bit79Keyboard = 0x25, //not supported yet
        SuborKeyboard = 0x26,
        SuborKeyboardMouse1 = 0x27,
        SuborKeyboardMouse2 = 0x28,
        SnesMouse = 0x29,
        GenericMulticart = 0x2A, //not supported yet
        SnesControllers = 0x2B,
        RacermateBicycle = 0x2C, //not supported yet
        UForce = 0x2D, //not supported yet
        LastEntry
    };

    enum class BusConflictType {
        DEFAULT,
        YES,
        NO
    };

    enum class VsSystemType
    {
        Default = 0,
        RbiBaseballProtection = 1,
        TkoBoxingProtection = 2,
        SuperXeviousProtection = 3,
        IceClimberProtection = 4,
        VsDualSystem = 5,
        RaidOnBungelingBayProtection = 6,
    };

    enum class PpuModel
    {
        Ppu2C02 = 0,
        Ppu2C03 = 1,
        Ppu2C04A = 2,
        Ppu2C04B = 3,
        Ppu2C04C = 4,
        Ppu2C04D = 5,
        Ppu2C05A = 6,
        Ppu2C05B = 7,
        Ppu2C05C = 8,
        Ppu2C05D = 9,
        Ppu2C05E = 10
    };

    struct RawItem {
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
    };

    struct Item {
        uint32_t PrgChrCrc32;
        System System;
        std::string Board;
        std::string PCB;
        std::string Chip;
        int MapperId;
        int PrgRomSize;
        int ChrRomSize;
        int ChrRamSize;
        int WorkRamSize;
        int SaveRamSize;
        Battery HasBattery;
        MirroringType Mirroring;
        InputType InputType;
        BusConflictType BusConflicts;
        int SubmapperId;
        VsSystemType VsType;
        PpuModel VsPpuModel;
    };

    uint32_t getCrc32(const std::string& str) {
        return (uint32_t)std::stoll(str.c_str(), nullptr, 16);
    }

    System getGameSystem(const std::string& str)
    {
        if(str == "NesNtsc") {
            return System::NesNtsc;
        } else if(str == "NesPal") {
            return System::NesPal;
        } else if(str == "Famicom") {
            return System::Famicom;
        } else if(str == "VsSystem") {
            return System::VsSystem;
        } else if(str == "Dendy") {
            return System::Dendy;
        } else if(str == "Playchoice") {
            return System::Playchoice;
        }
        
        return System::Unknown;
    }

    Battery getBattery(const std::string& str) {
        
        if(!str.empty()) {
            if(str == "0") return Battery::No;
            else if(str == "1") return Battery::Yes;
        }

        return Battery::Default;
    }

    MirroringType getMirroringType(const std::string& str) {

        if(str == "h") return MirroringType::HORIZONTAL;
        else if(str== "v") return MirroringType::VERTICAL;
        else if(str == "4") return MirroringType::FOUR_SCREEN;
        else if(str == "0") return MirroringType::SINGLE_SCREEN_A;
        else if(str == "1") return MirroringType::SINGLE_SCREEN_B;

        return MirroringType::DEFAULT;
    }

    InputType getInputType(const std::string& str) {

        if(!str.empty()) return static_cast<InputType>(std::stoi(str));
        return InputType::Unspecified;
    }

    BusConflictType getBusConflictType(const std::string& str) {

        if(str == "Y") return BusConflictType::YES;
        else if(str== "N") return BusConflictType::NO;

        return BusConflictType::DEFAULT;
    }

    VsSystemType getVsSystemType(const std::string& str) {

        if(!str.empty()) {
            return static_cast<VsSystemType>(std::stoi(str));
        }
        
        return VsSystemType::Default;
    }

    int getInt(const std::string& str) {
        
        if(!str.empty()) return std::stoi(str);
        return -1;
    }

    PpuModel getPpuModel(const std::string& str) {
        if(!str.empty()) return static_cast<PpuModel>(std::stoi(str));
        return PpuModel::Ppu2C02;
    }

private:

    std::map<std::string, Item> m_map;

    GameDatabase(const GameDatabase&) = delete;
    GameDatabase& operator = (const GameDatabase&) = delete;    

    GameDatabase() {
        load();
    }

    static bool validate(const std::string& str, std::vector<std::string> values) {
        
        for(auto item : values) {
            if(item == str) return true;
        }

        return false;
    }

    void load() {
        
        Logger::instance().log(std::string("(DB) Loading database"), Logger::Type::INFO);

        const std::string filename = "db.txt";

        std::ifstream file(filename);

        if (!file.is_open()) {
            Logger::instance().log(std::string("(DB) Database: ") + filename + " not found", Logger::Type::INFO);
            return;
        }

        // Regex to capture 18 columns
        std::regex pattern(R"(^([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^,]*),([^$]*)$)");

        std::string line;

        std::smatch match;

        int lineCounter = 0;
        
        while (std::getline(file, line)) {

            lineCounter++;

            if(line.size() > 0 && line[0] == '#') continue; //skip comments

            if(trim(line).size() == 0) continue; //skip empty

            if (std::regex_match(line, match, pattern)) {

                RawItem rawData;

                for(size_t i = 1; i < match.size(); ++i) {

                    //match[0] = full match
                    switch(i) {
                        case 1: rawData.PrgChrCrc32 = trim(match[i]); break;
                        case 2: rawData.System = trim(match[i]); break;
                        case 3: rawData.Board = trim(match[i]); break;
                        case 4: rawData.PCB = trim(match[i]); break;
                        case 5: rawData.Chip = trim(match[i]); break;
                        case 6: rawData.Mapper = trim(match[i]); break;
                        case 7: rawData.PrgRomSize = trim(match[i]); break;
                        case 8: rawData.ChrRomSize = trim(match[i]); break;
                        case 9: rawData.ChrRamSize = trim(match[i]); break;
                        case 10: rawData.WorkRamSize = trim(match[i]); break;
                        case 11: rawData.SaveRamSize = trim(match[i]); break;
                        case 12: rawData.HasBattery = trim(match[i]); break;
                        case 13: rawData.Mirroring = trim(match[i]); break;
                        case 14: rawData.InputType = trim(match[i]); break;
                        case 15: rawData.BusConflicts = trim(match[i]); break;
                        case 16: rawData.SubMapperId = trim(match[i]); break;
                        case 17: rawData.VsSystemType = trim(match[i]); break;
                        case 18: rawData.VsPpuModel = trim(match[i]); break;
                        default:
                            assert(false); //should never occur
                            break;
                    }                    
                }

                if(!validate(rawData.HasBattery, {"", "0", "1"})) {
                    std::string msg = "(DB) Invalid Battery value: '" + rawData.HasBattery + "' at line " + std::to_string(lineCounter);
                    Logger::instance().log(msg, Logger::Type::INFO);
                    continue;
                } 

                /*
                if(!validate(rawData.ControllerType, {"", "0", "1"})) {
                    std::string msg = "(DB) Invalid ControllerType value: '" + rawData.ControllerType + "' at line " + std::to_string(lineCounter);
                    Logger::instance().log(msg, Logger::Type::INFO);
                    continue;
                }
                */

                if(!validate(rawData.Mirroring, {"", "h", "v", "4", "1"})) {
                    std::string msg = "(DB) Invalid Mirroring value: '" + rawData.Mirroring + "' at line " + std::to_string(lineCounter);
                    Logger::instance().log(msg, Logger::Type::INFO);
                    continue;
                }

                Item data;

                data.PrgChrCrc32 = getCrc32(rawData.PrgChrCrc32);
                data.System = getGameSystem(rawData.System);
                data.Board = rawData.Board;
                data.PCB = rawData.PCB;
                data.Chip = rawData.Chip;
                data.MapperId = getInt(rawData.Mapper);
                data.PrgRomSize = getInt(rawData.PrgRomSize);
                data.ChrRomSize = getInt(rawData.ChrRomSize);
                data.ChrRamSize = getInt(rawData.ChrRamSize);
                data.WorkRamSize = getInt(rawData.WorkRamSize);
                data.SaveRamSize = getInt(rawData.SaveRamSize);
                data.HasBattery = getBattery(rawData.HasBattery);
                data.Mirroring = getMirroringType(rawData.Mirroring);
                data.InputType = getInputType(rawData.InputType);
                data.BusConflicts = getBusConflictType(rawData.BusConflicts);
                data.SubmapperId = getInt(rawData.SubMapperId);
                data.VsType = getVsSystemType(rawData.VsSystemType);
                data.VsPpuModel = getPpuModel(rawData.VsPpuModel);

                m_map.insert(std::make_pair(rawData.PrgChrCrc32, data));
            }
        }

        Logger::instance().log(std::string("(DB) ") + std::to_string(m_map.size()) + " items loaded", Logger::Type::INFO);

        file.close();
    }


public:    

    static GameDatabase& instance() {
        static GameDatabase _instance;
        return _instance;
    }

    Item* findByCrc(const std::string crc) {

        auto it = m_map.find(crc);

        if( it != m_map.end()) {
            return &it->second;
        }

        return nullptr;
    }

    std::vector<Item*> find(const std::function<bool(Item& item)>& condition) {

        std::vector<Item*> ret;

        for(auto& pair : m_map) {
            if(condition(pair.second)) ret.push_back(&pair.second);
        }

        return ret;
    }

};

#endif
