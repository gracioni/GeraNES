#pragma once

#include <cstdint>
#include "Serialization.h"

namespace GeraNES {

class IRewindable {

public:

    virtual void serialization(SerializationBase& s) = 0;
    virtual void loadStateFromMemory(const std::vector<uint8_t>& data) = 0;
    virtual int getFPS() = 0;

};

} // namespace GeraNES
