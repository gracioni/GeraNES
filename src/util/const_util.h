#pragma once

#include <cstdint>

static constexpr int compileTimeYear() {
    // __DATE__ format "Mmm dd yyyy"
    return (__DATE__[7] - '0') * 1000 +
           (__DATE__[8] - '0') * 100 +
           (__DATE__[9] - '0') * 10 +
           (__DATE__[10] - '0');
}

static constexpr uint32_t makeMagic(char a, char b, char c, char d)
{
    return (uint32_t(uint8_t(a)) << 24) |
           (uint32_t(uint8_t(b)) << 16) |
           (uint32_t(uint8_t(c)) <<  8) |
            uint32_t(uint8_t(d));
}
