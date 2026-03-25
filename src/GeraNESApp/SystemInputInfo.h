#pragma once

#include <array>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "GeraNESApp/InputBindingInfo.h"

class SystemInputInfo : public InputBindingInfo
{
public:
    static constexpr std::array<const char*, 4> BUTTONS {"Save", "Load", "Rewind", "Speed"};

    std::string saveState;
    std::string loadState;
    std::string rewind;
    std::string speed;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SystemInputInfo, saveState, loadState, rewind, speed)

private:
    std::map<const std::string, std::string*>* m_map = nullptr;

    void mapInit()
    {
        if(m_map != nullptr) return;

        m_map = new std::map<const std::string, std::string*>;
        m_map->insert(std::make_pair(BUTTONS[0], &saveState));
        m_map->insert(std::make_pair(BUTTONS[1], &loadState));
        m_map->insert(std::make_pair(BUTTONS[2], &rewind));
        m_map->insert(std::make_pair(BUTTONS[3], &speed));
    }

public:
    SystemInputInfo() = default;

    ~SystemInputInfo()
    {
        if(m_map != nullptr) delete m_map;
    }

    SystemInputInfo(const SystemInputInfo& other)
    {
        *this = other;
    }

    SystemInputInfo& operator=(const SystemInputInfo& other)
    {
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
            case 0: saveState = input; break;
            case 1: loadState = input; break;
            case 2: rewind = input; break;
            case 3: speed = input; break;
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
        return const_cast<SystemInputInfo*>(this)->getByButtonName(BUTTONS[index]);
    }

    void setBinding(size_t index, const std::string& input) override
    {
        setByButtonIndex(static_cast<int>(index), input);
    }
};
