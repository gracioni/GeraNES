#ifndef DB_H
#define DB_H

#include <string>
#include <map>
#include <fstream>
#include <regex>
#include <map>
#include <cassert>

#include "Logger.h"
#include "util/StringTrim.h"

class Db {

public:

    enum class MirroringType {
        DEFAULT,
        HORIZONTAL,
        VERTICAL,
        SINGLE_SCREEN_A,
        SINGLE_SCREEN_B,
        FOUR_SCREEN
    };

    enum class BusConflictType {
        DEFAULT,
        YES,
        NO
    };

    struct DataRaw {
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
        std::string Battery;
        std::string Mirroring;
        std::string ControllerType;
        std::string BusConflicts;
        std::string SubMapper;
        std::string VsSystemType;
        std::string PpuModel;
    };

    struct Data {
        std::string PrgChrCrc32;
        std::string System;
        std::string Board;
        std::string PCB;
        std::string Chip;
        int Mapper;
        std::string PrgRomSize;
        std::string ChrRomSize;
        std::string ChrRamSize;
        std::string WorkRamSize;
        std::string SaveRamSize;
        std::string Battery;
        MirroringType Mirroring;
        std::string ControllerType;
        BusConflictType BusConflicts;
        std::string SubMapper;
        std::string VsSystemType;
        std::string PpuModel;
    };

    MirroringType getMirroringType(const std::string& str) {

        if(str == "h") return MirroringType::HORIZONTAL;
        else if(str== "v") return MirroringType::VERTICAL;
        else if(str == "4") return MirroringType::FOUR_SCREEN;
        else if(str == "0") return MirroringType::SINGLE_SCREEN_A;
        else if(str == "1") return MirroringType::SINGLE_SCREEN_B;

        return MirroringType::DEFAULT;
    }

    BusConflictType getBusConflictType(const std::string& str) {

        if(str == "Y") return BusConflictType::YES;
        else if(str== "N") return BusConflictType::NO;

        return BusConflictType::DEFAULT;
    }

    int getMapper(const std::string& str) {
        
        if(!str.empty()) return std::stoi(str);
        return -1;
    }

private:

    std::map<std::string, Data> m_map;

    Db(const Db&) = delete;
    Db& operator = (const Db&) = delete;    

    Db() {
        load();
    }

    static bool validate(const std::string& str, std::vector<std::string> values) {
        
        for(auto item : values) {
            if(item == str) return true;
        }

        return false;
    }

    void load() {

        const std::string filename = "db.txt";

        std::ifstream file(filename);

        if (!file.is_open()) {
            Logger::instance().log(std::string("Database: ") + filename + " not found", Logger::Type::WARNING);
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

                DataRaw rawData;

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
                        case 12: rawData.Battery = trim(match[i]); break;
                        case 13: rawData.Mirroring = trim(match[i]); break;
                        case 14: rawData.ControllerType = trim(match[i]); break;
                        case 15: rawData.BusConflicts = trim(match[i]); break;
                        case 16: rawData.SubMapper = trim(match[i]); break;
                        case 17: rawData.VsSystemType = trim(match[i]); break;
                        case 18: rawData.PpuModel = trim(match[i]); break;
                        default:
                            assert(false); //should never occur
                            break;
                    }                    
                }

                if(!validate(rawData.Battery, {"", "0", "1"})) {
                    std::string msg = "[DB] Invalid Battery value: '" + rawData.Battery + "' at line " + std::to_string(lineCounter);
                    Logger::instance().log(msg, Logger::Type::INFO);
                    continue;
                } 

                /*
                if(!validate(rawData.ControllerType, {"", "0", "1"})) {
                    std::string msg = "[DB] Invalid ControllerType value: '" + rawData.ControllerType + "' at line " + std::to_string(lineCounter);
                    Logger::instance().log(msg, Logger::Type::INFO);
                    continue;
                }
                */

                if(!validate(rawData.Mirroring, {"", "h", "v", "4", "1"})) {
                    std::string msg = "[DB] Invalid Mirroring value: '" + rawData.Mirroring + "' at line " + std::to_string(lineCounter);
                    Logger::instance().log(msg, Logger::Type::INFO);
                    continue;
                }

                Data data;

                data.PrgChrCrc32 = rawData.PrgChrCrc32;
                data.System = rawData.System;
                data.Board = rawData.Board;
                data.PCB = rawData.PCB;
                data.Chip = rawData.Chip;
                data.Mapper = getMapper(rawData.Mapper);
                data.PrgRomSize = rawData.PrgRomSize;
                data.ChrRomSize = rawData.ChrRomSize;
                data.ChrRamSize = rawData.ChrRamSize;
                data.WorkRamSize = rawData.WorkRamSize;
                data.SaveRamSize = rawData.SaveRamSize;
                data.Battery = rawData.Battery;
                data.Mirroring = getMirroringType(rawData.Mirroring);
                data.ControllerType = rawData.ControllerType;
                data.BusConflicts = getBusConflictType(rawData.BusConflicts);
                data.SubMapper = rawData.SubMapper;
                data.VsSystemType = rawData.VsSystemType;
                data.PpuModel = rawData.PpuModel;

                m_map.insert(std::make_pair(rawData.PrgChrCrc32, data));
            }
        }

        Logger::instance().log(std::to_string(m_map.size()) + " titles loaded", Logger::Type::INFO);

        file.close();
    }


public:    

    static Db& instance() {
        static Db _instance;
        return _instance;
    }

    Data* find(const std::string crc) {

        auto it = m_map.find(crc);

        if( it != m_map.end()) {
            return &it->second;
        }

        return nullptr;
    }    

};

#endif
