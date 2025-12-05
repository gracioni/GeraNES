#pragma once

#include <vector>
#include <string>

#include <nlohmann/json.hpp>


class ControllerInfo {

public:

    static constexpr std::array<const char*, 11> BUTTONS {"A", "B", "Select", "Start", "Up", "Down", "Left", "Right", "Save", "Load", "Rewind"};

    std::string a;
    std::string b;
    std::string select;
    std::string start;
    std::string up;
    std::string down;
    std::string left;
    std::string right;
    std::string saveState;
    std::string loadState;
    std::string rewind;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ControllerInfo, a, b, select, start, up, down, left, right, saveState, loadState, rewind)

private:

    std::map<const std::string, std::string*>* _map = nullptr;

    void mapInit() {
        
        if(_map != nullptr) return;

        _map = new std::map<const std::string, std::string*>;

        _map->insert(std::make_pair(BUTTONS[0], &a));
        _map->insert(std::make_pair(BUTTONS[1], &b));
        _map->insert(std::make_pair(BUTTONS[2], &select));
        _map->insert(std::make_pair(BUTTONS[3], &start));
        _map->insert(std::make_pair(BUTTONS[4], &up));
        _map->insert(std::make_pair(BUTTONS[5], &down));
        _map->insert(std::make_pair(BUTTONS[6], &left));
        _map->insert(std::make_pair(BUTTONS[7], &right));
        _map->insert(std::make_pair(BUTTONS[8], &saveState));
        _map->insert(std::make_pair(BUTTONS[9], &loadState));
        _map->insert(std::make_pair(BUTTONS[10], &rewind));
    }

public:    

    ControllerInfo() {
    }

    ~ControllerInfo() {
        if(_map != nullptr) delete _map;
    }

    ControllerInfo(const ControllerInfo& other)
    {
        *this = other;        
    }

    ControllerInfo& operator = (const ControllerInfo& other) {
        a = other.a;
        b = other.b;
        select = other.select;
        start = other.start;
        up = other.up;
        down = other.down;
        left = other.left;
        right = other.right;
        saveState = other.saveState;
        loadState = other.loadState;
        rewind = other.rewind;

        return *this;
    }

    const std::string& getByButtonName(const std::string& name) { 

        static std::string empty = "";

        mapInit();

        if(_map->count(name)) {
            return *(*_map)[name];
        }

        return empty;
        
    }

    const void setByButtonIndex(int index, const std::string& input) {
        
        switch(index){
            case 0: a = input; break;
            case 1: b = input; break;
            case 2: select = input; break;
            case 3: start = input; break;
            case 4: up = input; break;
            case 5: down = input; break;
            case 6: left = input; break;
            case 7: right = input; break;
            case 8: saveState = input; break;
            case 9: loadState = input; break;
            case 10: rewind = input; break;
        }
    }
};
