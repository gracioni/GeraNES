#pragma once

#include <cstddef>
#include <string>

class InputBindingInfo
{
public:
    virtual ~InputBindingInfo() = default;

    virtual size_t bindingCount() const = 0;
    virtual const char* bindingLabel(size_t index) const = 0;
    virtual const std::string& getBinding(size_t index) const = 0;
    virtual void setBinding(size_t index, const std::string& input) = 0;
};
