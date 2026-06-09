#pragma once

#include "util/const_util.h"

#if defined(_MSC_VER)
    #define GERANES_INLINE __forceinline
#elif defined(__GNUC__)
    #define GERANES_INLINE __attribute__((always_inline)) inline
#else
    #define GERANES_INLINE inline
#endif

#if defined(_MSC_VER)
    #define GERANES_HOT
#elif defined(__GNUC__)
    #define GERANES_HOT __attribute__ ((hot))
#else
    #define GERANES_HOT
#endif

#define GERANES_INLINE_HOT GERANES_INLINE GERANES_HOT

#include <cstdint>
#include <string>

namespace GeraNES {

static constexpr const char* GERANES_NAME = "GeraNES";
static constexpr const char* GERANES_VERSION = "2.2";

static constexpr uint32_t SAVE_STATE_MAGIC = makeMagic('G','N','E','S');
static constexpr uint32_t SAVE_STATE_VERSION = 12;

static constexpr const char* STATES_FOLDER  = "states/";

static constexpr const char* SRAM_FOLDER = "sram/";

} // namespace GeraNES
