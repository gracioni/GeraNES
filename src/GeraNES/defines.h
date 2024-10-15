#ifndef INCLUDE_TYPES
#define INCLUDE_TYPES

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

#include "stdint.h"

#define GERANES_NAME "GeraNES"
#define GERANES_VERSION "1.5.2"

#define STATES_FOLDER "states/"

#define SRAM_FOLDER "sram/"

enum class MirroringType {
    HORIZONTAL=0, VERTICAL=1, SINGLE_SCREEN_A=2, SINGLE_SCREEN_B=3, FOUR_SCREEN=4, CUSTOM=5
};

#endif
