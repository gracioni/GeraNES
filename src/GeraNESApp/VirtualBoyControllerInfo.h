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

    void mapInit();

public:
    VirtualBoyControllerInfo();
    ~VirtualBoyControllerInfo() override;
    VirtualBoyControllerInfo(const VirtualBoyControllerInfo& other);
    VirtualBoyControllerInfo& operator=(const VirtualBoyControllerInfo& other);

    const std::string& getByButtonName(const std::string& name);
    void setByButtonIndex(int index, const std::string& input);

    size_t bindingCount() const override;
    const char* bindingLabel(size_t index) const override;
    const std::string& getBinding(size_t index) const override;
    void setBinding(size_t index, const std::string& input) override;
};
