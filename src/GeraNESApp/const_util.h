#pragma once

#ifdef __cplusplus
extern "C" {
#endif

constexpr int compileTimeYear() {
    // __DATE__ format "Mmm dd yyyy"
    return (__DATE__[7] - '0') * 1000 +
           (__DATE__[8] - '0') * 100 +
           (__DATE__[9] - '0') * 10 +
           (__DATE__[10] - '0');
}

#ifdef __cplusplus
}
#endif

