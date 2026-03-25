#pragma once

#include <cstdint>

namespace Netplay {

using FrameNumber = uint32_t;
using ParticipantId = uint8_t;
using PlayerSlot = uint8_t;

constexpr ParticipantId kInvalidParticipantId = 0xFF;
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

} // namespace Netplay
