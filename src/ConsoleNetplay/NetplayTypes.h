#pragma once

#include <cstdint>

namespace ConsoleNetplay {

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

enum class CrcSubmissionSource : uint8_t
{
    Unknown = 0,
    FrameReady,
    LiveCanonical
};

} // namespace ConsoleNetplay
