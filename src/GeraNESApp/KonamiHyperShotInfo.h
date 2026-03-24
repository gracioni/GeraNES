#pragma once

#include <array>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "GeraNESApp/InputBindingInfo.h"

class KonamiHyperShotInfo : public InputBindingInfo
{
public:
    static constexpr std::array<const char*, 4> BUTTONS {"P1 Run", "P1 Jump", "P2 Run", "P2 Jump"};

    std::string p1Run;
    std::string p1Jump;
    std::string p2Run;
    std::string p2Jump;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(KonamiHyperShotInfo, p1Run, p1Jump, p2Run, p2Jump)

private:
    std::map<const std::string, std::string*>* m_map = nullptr;

    void mapInit()
    {
        if(m_map != nullptr) return;

        m_map = new std::map<const std::string, std::string*>;
        m_map->insert(std::make_pair(BUTTONS[0], &p1Run));
        m_map->insert(std::make_pair(BUTTONS[1], &p1Jump));
        m_map->insert(std::make_pair(BUTTONS[2], &p2Run));
        m_map->insert(std::make_pair(BUTTONS[3], &p2Jump));
    }

public:
    KonamiHyperShotInfo() = default;

    ~KonamiHyperShotInfo()
    {
        if(m_map != nullptr) delete m_map;
    }

    KonamiHyperShotInfo(const KonamiHyperShotInfo& other)
    {
        *this = other;
    }

    KonamiHyperShotInfo& operator=(const KonamiHyperShotInfo& other)
    {
        p1Run = other.p1Run;
        p1Jump = other.p1Jump;
        p2Run = other.p2Run;
        p2Jump = other.p2Jump;
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
            case 0: p1Run = input; break;
            case 1: p1Jump = input; break;
            case 2: p2Run = input; break;
            case 3: p2Jump = input; break;
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
        return const_cast<KonamiHyperShotInfo*>(this)->getByButtonName(BUTTONS[index]);
    }

    void setBinding(size_t index, const std::string& input) override
    {
        setByButtonIndex(static_cast<int>(index), input);
    }
};
