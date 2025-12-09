#pragma once

#include "util/const_util.h"

#if defined( __GNUC__ )
    #define GERANES_INLINE __attribute__((always_inline)) inline
#else
    #define GERANES_INLINE inline
#endif

#if defined( __GNUC__ )
    #define GERANES_HOT __attribute__ ((hot))
#else
    #define GERANES_HOT
#endif

#define GERANES_INLINE_HOT GERANES_INLINE GERANES_HOT

#include <cstdint>
#include <string>

static constexpr std::string GERANES_NAME = "GeraNES";
static constexpr std::string GERANES_VERSION = "1.6.0";

static constexpr uint32_t SAVE_STATE_MAGIC = makeMagic('G','N','E','S');
static constexpr uint32_t SAVE_STATE_VERSION = 0;

static constexpr std::string STATES_FOLDER  = "states/";

static constexpr std::string SRAM_FOLDER = "sram/";

enum class MirroringType {
    HORIZONTAL=0, VERTICAL=1, SINGLE_SCREEN_A=2, SINGLE_SCREEN_B=3, FOUR_SCREEN=4, CUSTOM=5
};
