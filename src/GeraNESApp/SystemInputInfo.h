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

    void mapInit();

public:
    SystemInputInfo();
    ~SystemInputInfo() override;
    SystemInputInfo(const SystemInputInfo& other);
    SystemInputInfo& operator=(const SystemInputInfo& other);

    const std::string& getByButtonName(const std::string& name);
    void setByButtonIndex(int index, const std::string& input);

    size_t bindingCount() const override;
    const char* bindingLabel(size_t index) const override;
    const std::string& getBinding(size_t index) const override;
    void setBinding(size_t index, const std::string& input) override;
};
