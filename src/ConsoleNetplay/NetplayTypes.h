#pragma once

#include <cstdint>

namespace ConsoleNetplay {

using FrameNumber = uint32_t;
using ParticipantId = uint8_t;
using PlayerSlot = uint8_t;

constexpr ParticipantId kInvalidParticipantId = 0xFF;
constexpr PlayerSlot kPort1PlayerSlot = 0;
constexpr PlayerSlot kPort2PlayerSlot = 1;
constexpr PlayerSlot kExpansionPlayerSlot = 2;
constexpr PlayerSlot kMultitapP1PlayerSlot = 3;
constexpr PlayerSlot kMultitapP2PlayerSlot = 4;
constexpr PlayerSlot kMultitapP3PlayerSlot = 5;
constexpr PlayerSlot kMultitapP4PlayerSlot = 6;
constexpr PlayerSlot kMaxAssignedPlayerSlot = 6;
constexpr PlayerSlot kObserverPlayerSlot = 0xFF;

enum class SessionState : uint8_t
{
    Lobby,
    ValidatingRom,
    ReadyCheck,
    Starting,
    Running,
    Resyncing,
    Paused,
    Ended
};

enum class DesyncSeverity : uint8_t
{
    NoIssue,
    PredictionMismatchOnly,
    RollbackCorrected,
    ConfirmedDesync,
    HardResyncRequired
};

} // namespace ConsoleNetplay
