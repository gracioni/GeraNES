#pragma once

#include <array>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

#include "GeraNESApp/InputBindingInfo.h"

class ControllerInfo : public InputBindingInfo {

public:

    static constexpr std::array<const char*, 8> BUTTONS {"A", "B", "Select", "Start", "Up", "Down", "Left", "Right"};

    std::string a;
    std::string b;
    std::string select;
    std::string start;
    std::string up;
    std::string down;
    std::string left;
    std::string right;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ControllerInfo, a, b, select, start, up, down, left, right)

private:

    std::map<const std::string, std::string*>* _map = nullptr;

    void mapInit();

public:    

    ControllerInfo();
    ~ControllerInfo() override;
    ControllerInfo(const ControllerInfo& other);
    ControllerInfo& operator = (const ControllerInfo& other);

    const std::string& getByButtonName(const std::string& name);
    void setByButtonIndex(int index, const std::string& input);

    size_t bindingCount() const override;
    const char* bindingLabel(size_t index) const override;
    const std::string& getBinding(size_t index) const override;
    void setBinding(size_t index, const std::string& input) override;
};
