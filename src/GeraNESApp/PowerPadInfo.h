#pragma once

#include <array>
#include <string>

#include <nlohmann/json.hpp>

#include "GeraNESApp/InputBindingInfo.h"

class PowerPadInfo : public InputBindingInfo
{
public:
    static constexpr std::array<const char*, 12> BUTTONS {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"};

    std::array<std::string, 12> bindings = {};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PowerPadInfo, bindings)

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
        return bindings[index];
    }

    void setBinding(size_t index, const std::string& input) override
    {
        bindings[index] = input;
    }
};
