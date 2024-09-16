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

    struct Data {
        std::string CRC;
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

private:

    std::map<std::string, Data> m_map;

    Db(const Db&) = delete;
    Db& operator = (const Db&) = delete;    

    Db() {
        load();
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

                Data data;

                for(size_t i = 1; i < match.size(); ++i) {

                    //match[0] = full match
                    switch(i) {
                        case 1: data.CRC = trim(match[i]); break;
                        case 2: data.System = trim(match[i]); break;
                        case 3: data.Board = trim(match[i]); break;
                        case 4: data.PCB = trim(match[i]); break;
                        case 5: data.Chip = trim(match[i]); break;
                        case 6: data.Mapper = trim(match[i]); break;
                        case 7: data.PrgRomSize = trim(match[i]); break;
                        case 8: data.ChrRomSize = trim(match[i]); break;
                        case 9: data.ChrRamSize = trim(match[i]); break;
                        case 10: data.WorkRamSize = trim(match[i]); break;
                        case 11: data.SaveRamSize = trim(match[i]); break;
                        case 12: data.Battery = trim(match[i]); break;
                        case 13: data.Mirroring = trim(match[i]); break;
                        case 14: data.ControllerType = trim(match[i]); break;
                        case 15: data.BusConflicts = trim(match[i]); break;
                        case 16: data.SubMapper = trim(match[i]); break;
                        case 17: data.VsSystemType = trim(match[i]); break;
                        case 18: data.PpuModel = trim(match[i]); break;
                        default:
                            assert(false); //should never occur
                            break;
                    }                    
                }

                m_map.insert(std::make_pair(data.CRC, data));
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
