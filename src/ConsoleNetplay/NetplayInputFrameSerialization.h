#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ConsoleNetplay/NetplayInputFrame.h"

namespace ConsoleNetplay {

std::vector<uint8_t> serializeNetplayInputFrame(const NetplayInputFrame& frame);
size_t serializedNetplayInputFrameSize(const NetplayInputFrame& frame);
bool deserializeNetplayInputFrame(const uint8_t* data, size_t size, NetplayInputFrame& frame);

} // namespace ConsoleNetplay
