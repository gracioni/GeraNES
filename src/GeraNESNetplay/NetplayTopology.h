#pragma once

#include <cstdint>

namespace Netplay {

enum class PortDevice : uint8_t
{
    CONTROLLER = 0,
    ZAPPER = 1,
    ARKANOID_CONTROLLER = 2,
    BANDAI_HYPERSHOT = 3,
    SNES_MOUSE = 4,
    SNES_CONTROLLER = 5,
    POWER_PAD_SIDE_A = 6,
    POWER_PAD_SIDE_B = 7,
    FAMICOM_CONTROLLER = 8,
    SUBOR_MOUSE = 9,
    NONE = 10,
    VIRTUAL_BOY_CONTROLLER = 11
};

enum class ExpansionDevice : uint8_t
{
    NONE = 0,
    STANDARD_CONTROLLER_FAMICOM = 1,
    BANDAI_HYPERSHOT = 2,
    KONAMI_HYPERSHOT = 3,
    ARKANOID_CONTROLLER = 4,
    FAMILY_TRAINER_SIDE_A = 5,
    FAMILY_TRAINER_SIDE_B = 6,
    SUBOR_KEYBOARD = 7,
    FAMILY_BASIC_KEYBOARD = 8
};

enum class NesMultitapDevice : uint8_t
{
    NONE = 0,
    FOUR_SCORE = 1
};

enum class FamicomMultitapDevice : uint8_t
{
    NONE = 0,
    HORI_ADAPTER = 1
};

} // namespace Netplay
