#pragma once

#include <array>

#include "ConsoleNetplay/NetplayTypes.h"

namespace GeraNESNetplay {

using ConsoleNetplay::PlayerSlot;

inline constexpr PlayerSlot kPort1PlayerSlot = 0;
inline constexpr PlayerSlot kPort2PlayerSlot = 1;
inline constexpr PlayerSlot kExpansionPlayerSlot = 2;
inline constexpr PlayerSlot kMultitapP1PlayerSlot = 3;
inline constexpr PlayerSlot kMultitapP2PlayerSlot = 4;
inline constexpr PlayerSlot kMultitapP3PlayerSlot = 5;
inline constexpr PlayerSlot kMultitapP4PlayerSlot = 6;

inline constexpr std::array<PlayerSlot, 4> kMultitapPlayerSlots = {
    kMultitapP1PlayerSlot,
    kMultitapP2PlayerSlot,
    kMultitapP3PlayerSlot,
    kMultitapP4PlayerSlot,
};

inline constexpr std::array<PlayerSlot, 7> kAllGeraNESPlayerSlots = {
    kPort1PlayerSlot,
    kPort2PlayerSlot,
    kExpansionPlayerSlot,
    kMultitapP1PlayerSlot,
    kMultitapP2PlayerSlot,
    kMultitapP3PlayerSlot,
    kMultitapP4PlayerSlot,
};

constexpr bool isGeraNESMultitapPlayerSlot(PlayerSlot slot)
{
    return slot == kMultitapP1PlayerSlot ||
           slot == kMultitapP2PlayerSlot ||
           slot == kMultitapP3PlayerSlot ||
           slot == kMultitapP4PlayerSlot;
}

} // namespace GeraNESNetplay
