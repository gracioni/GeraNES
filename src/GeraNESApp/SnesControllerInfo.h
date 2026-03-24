#pragma once

#include <array>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "GeraNESApp/InputBindingInfo.h"

class SnesControllerInfo : public InputBindingInfo
{
public:
    static constexpr std::array<const char*, 16> BUTTONS {"A", "B", "X", "Y", "L", "R", "Select", "Start", "Up", "Down", "Left", "Right", "Save", "Load", "Rewind", "Speed"};

    std::string a;
    std::string b;
    std::string x;
    std::string y;
    std::string l;
    std::string r;
    std::string select;
    std::string start;
    std::string up;
    std::string down;
    std::string left;
    std::string right;
    std::string saveState;
    std::string loadState;
    std::string rewind;
    std::string speed;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SnesControllerInfo, a, b, x, y, l, r, select, start, up, down, left, right, saveState, loadState, rewind, speed)

private:
    std::map<const std::string, std::string*>* m_map = nullptr;

    void mapInit()
    {
        if(m_map != nullptr) return;

        m_map = new std::map<const std::string, std::string*>;
        m_map->insert(std::make_pair(BUTTONS[0], &a));
        m_map->insert(std::make_pair(BUTTONS[1], &b));
        m_map->insert(std::make_pair(BUTTONS[2], &x));
        m_map->insert(std::make_pair(BUTTONS[3], &y));
        m_map->insert(std::make_pair(BUTTONS[4], &l));
        m_map->insert(std::make_pair(BUTTONS[5], &r));
        m_map->insert(std::make_pair(BUTTONS[6], &select));
        m_map->insert(std::make_pair(BUTTONS[7], &start));
        m_map->insert(std::make_pair(BUTTONS[8], &up));
        m_map->insert(std::make_pair(BUTTONS[9], &down));
        m_map->insert(std::make_pair(BUTTONS[10], &left));
        m_map->insert(std::make_pair(BUTTONS[11], &right));
        m_map->insert(std::make_pair(BUTTONS[12], &saveState));
        m_map->insert(std::make_pair(BUTTONS[13], &loadState));
        m_map->insert(std::make_pair(BUTTONS[14], &rewind));
        m_map->insert(std::make_pair(BUTTONS[15], &speed));
    }

public:
    SnesControllerInfo() = default;

    ~SnesControllerInfo()
    {
        if(m_map != nullptr) delete m_map;
    }

    SnesControllerInfo(const SnesControllerInfo& other)
    {
        *this = other;
    }

    SnesControllerInfo& operator=(const SnesControllerInfo& other)
    {
        a = other.a;
        b = other.b;
        x = other.x;
        y = other.y;
        l = other.l;
        r = other.r;
        select = other.select;
        start = other.start;
        up = other.up;
        down = other.down;
        left = other.left;
        right = other.right;
        saveState = other.saveState;
        loadState = other.loadState;
        rewind = other.rewind;
        speed = other.speed;
        return *this;
    }

    const std::string& getByButtonName(const std::string& name)
    {
        static std::string empty = "";

        mapInit();
        if(m_map->count(name)) {
            return *(*m_map)[name];
        }
        return empty;
    }

    void setByButtonIndex(int index, const std::string& input)
    {
        switch(index) {
            case 0: a = input; break;
            case 1: b = input; break;
            case 2: x = input; break;
            case 3: y = input; break;
            case 4: l = input; break;
            case 5: r = input; break;
            case 6: select = input; break;
            case 7: start = input; break;
            case 8: up = input; break;
            case 9: down = input; break;
            case 10: left = input; break;
            case 11: right = input; break;
            case 12: saveState = input; break;
            case 13: loadState = input; break;
            case 14: rewind = input; break;
            case 15: speed = input; break;
        }
    }

    size_t bindingCount() const override
    {
        return BUTTONS.size();
    }

    const char* bindingLabel(size_t index) const override
    {
        return BUTTONS[index];
    }

    const std::string& getBinding(size_t index) const override
    {
        return const_cast<SnesControllerInfo*>(this)->getByButtonName(BUTTONS[index]);
    }

    void setBinding(size_t index, const std::string& input) override
    {
        setByButtonIndex(static_cast<int>(index), input);
    }
};
