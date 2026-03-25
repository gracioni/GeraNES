#pragma once

#include <array>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "GeraNESApp/InputBindingInfo.h"

class VirtualBoyControllerInfo : public InputBindingInfo
{
public:
    static constexpr std::array<const char*, 14> BUTTONS {
        "A", "B", "L", "R", "Select", "Start",
        "Up", "Down", "Left", "Right",
        "Up 2", "Down 2", "Left 2", "Right 2"
    };

    std::string a;
    std::string b;
    std::string l;
    std::string r;
    std::string select;
    std::string start;
    std::string up;
    std::string down;
    std::string left;
    std::string right;
    std::string up2;
    std::string down2;
    std::string left2;
    std::string right2;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(VirtualBoyControllerInfo, a, b, l, r, select, start, up, down, left, right, up2, down2, left2, right2)

private:
    std::map<const std::string, std::string*>* m_map = nullptr;

    void mapInit()
    {
        if(m_map != nullptr) return;

        m_map = new std::map<const std::string, std::string*>;
        m_map->insert(std::make_pair(BUTTONS[0], &a));
        m_map->insert(std::make_pair(BUTTONS[1], &b));
        m_map->insert(std::make_pair(BUTTONS[2], &l));
        m_map->insert(std::make_pair(BUTTONS[3], &r));
        m_map->insert(std::make_pair(BUTTONS[4], &select));
        m_map->insert(std::make_pair(BUTTONS[5], &start));
        m_map->insert(std::make_pair(BUTTONS[6], &up));
        m_map->insert(std::make_pair(BUTTONS[7], &down));
        m_map->insert(std::make_pair(BUTTONS[8], &left));
        m_map->insert(std::make_pair(BUTTONS[9], &right));
        m_map->insert(std::make_pair(BUTTONS[10], &up2));
        m_map->insert(std::make_pair(BUTTONS[11], &down2));
        m_map->insert(std::make_pair(BUTTONS[12], &left2));
        m_map->insert(std::make_pair(BUTTONS[13], &right2));
    }

public:
    VirtualBoyControllerInfo() = default;

    ~VirtualBoyControllerInfo()
    {
        if(m_map != nullptr) delete m_map;
    }

    VirtualBoyControllerInfo(const VirtualBoyControllerInfo& other)
    {
        *this = other;
    }

    VirtualBoyControllerInfo& operator=(const VirtualBoyControllerInfo& other)
    {
        a = other.a;
        b = other.b;
        l = other.l;
        r = other.r;
        select = other.select;
        start = other.start;
        up = other.up;
        down = other.down;
        left = other.left;
        right = other.right;
        up2 = other.up2;
        down2 = other.down2;
        left2 = other.left2;
        right2 = other.right2;
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
            case 2: l = input; break;
            case 3: r = input; break;
            case 4: select = input; break;
            case 5: start = input; break;
            case 6: up = input; break;
            case 7: down = input; break;
            case 8: left = input; break;
            case 9: right = input; break;
            case 10: up2 = input; break;
            case 11: down2 = input; break;
            case 12: left2 = input; break;
            case 13: right2 = input; break;
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
        return const_cast<VirtualBoyControllerInfo*>(this)->getByButtonName(BUTTONS[index]);
    }

    void setBinding(size_t index, const std::string& input) override
    {
        setByButtonIndex(static_cast<int>(index), input);
    }
};
