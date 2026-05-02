#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "NetProtocol.h"

namespace ConsoleNetplay {

enum class RecoveryInputMode : uint8_t
{
    Normal = 0,
    ResyncLocked = 1,
    PostResyncStabilizing = 2
};

struct ParticipantInfo
{
    ParticipantId id = kInvalidParticipantId;
    std::string displayName;
    uint64_t reconnectToken = 0;
    bool connected = false;
    bool reconnectReserved = false;
    uint16_t reservationSecondsRemaining = 0;
    ParticipantRole role = ParticipantRole::Observer;
    PlayerSlot controllerAssignment = kObserverPlayerSlot;
    std::vector<PlayerSlot> controllerAssignments;
    bool romLoaded = false;
    bool romCompatible = false;
    uint16_t pingMs = 0;
    uint16_t jitterMs = 0;
    uint64_t sharedClockMicros = 0;
    uint64_t clockSyncRttMicros = 0;
    uint64_t sharedClockSampledAtLocalMicros = 0;
    bool sharedClockSynchronized = false;
    FrameNumber lastReportedCurrentFrame = 0;
    FrameNumber lastReportedConfirmedFrame = 0;
    uint32_t peerHealthSerial = 0;
    FrameNumber lastReceivedInputFrame = 0;
    FrameNumber lastContiguousInputFrame = 0;
    uint32_t lastReceivedInputSequence = 0;
    bool sequenceRebasePending = false;
    std::optional<FrameNumber> pendingMissingInputFrom;
    uint32_t missingInputGapCount = 0;
    uint32_t rollbackScheduledCount = 0;
    uint32_t futureFrameMismatchCount = 0;
    uint32_t confirmedFrameConflictCount = 0;
    FrameNumber lastDecisionFrame = 0;
    PlayerSlot lastDecisionSlot = kObserverPlayerSlot;
    std::string lastDecision;
    FrameNumber lastRejectedInputFrame = 0;
    uint32_t lastRejectedInputSequence = 0;
    PlayerSlot lastRejectedInputSlot = kObserverPlayerSlot;
    uint32_t lastRejectedInputEpoch = 0;
    std::string lastRejectedInputReason;
    bool inputSuspended = false;
    bool inputResumeAwaitingResync = false;

    void normalizeControllerAssignments(const std::vector<InputSlotDescriptor>* topology = nullptr);
    bool hasControllerAssignment(PlayerSlot slot) const;
};

struct RoomState
{
    uint32_t sessionId = 0;
    SessionState state = SessionState::Lobby;
    uint32_t timelineEpoch = 0;
    uint8_t inputDelayFrames = 2;
    uint8_t predictFrames = 0;
    // `currentFrame`: latest frame the host has reported as locally simulated.
    FrameNumber currentFrame = 0;
    // `lastConfirmedFrame`: highest frame the authoritative input timeline has
    // confirmed/published for the current epoch.
    FrameNumber lastConfirmedFrame = 0;
    FrameNumber lastRemoteCrcFrame = 0;
    uint32_t lastRemoteCrc32 = 0;
    uint32_t lastAcceptedRemoteEpoch = 0;
    uint32_t lastIgnoredStaleInputEpoch = 0;
    uint32_t lastIgnoredStaleFrameStatusEpoch = 0;
    uint32_t lastIgnoredStaleCrcEpoch = 0;
    uint32_t staleInputPacketCount = 0;
    uint32_t staleFrameStatusPacketCount = 0;
    uint32_t staleCrcPacketCount = 0;
    FrameNumber lastAuthoritativeClockFrame = 0;
    uint64_t lastAuthoritativeClockMicros = 0;
    uint64_t sharedClockMicros = 0;
    uint64_t sharedClockRttMicros = 0;
    int64_t sharedClockOffsetMicros = 0;
    bool sharedClockSynchronized = false;
    FrameNumber autoTuneDelayIncreaseBlockedUntilFrame = 0;
    RecoveryInputMode recoveryInputMode = RecoveryInputMode::Normal;
    uint32_t recoveryModeTransitionCount = 0;
    uint32_t inputsDroppedDuringRecovery = 0;
    FrameNumber recoveryModeEnteredAtFrame = 0;
    uint32_t stabilizationFramesRemaining = 0;
    uint32_t stabilizationCrcPassCount = 0;
    FrameNumber stabilizationAnchorFrame = 0;
    bool stabilizationRetryIssued = false;
    uint32_t stabilizationRetryCount = 0;
    uint32_t activeResyncId = 0;
    FrameNumber resyncTargetFrame = 0;
    FrameNumber resyncConfirmedFrame = 0;
    FrameNumber resyncFrameReadyFrame = 0;
    uint32_t resyncPayloadSize = 0;
    uint32_t resyncPayloadCrc32 = 0;
    uint32_t resyncFrameReadyCrc32 = 0;
    uint32_t resyncInputSequenceBase = 0;
    ResyncReason activeResyncReason = ResyncReason::Unspecified;
    uint32_t pendingResyncAckCount = 0;
    std::string selectedGameName;
    RomValidationData romValidation;
    std::vector<InputSlotDescriptor> inputTopology = defaultInputTopology();
    std::vector<ParticipantInfo> participants;
};

class NetSession
{
private:
    RoomState m_roomState;

public:
    RoomState& roomState();
    const RoomState& roomState() const;
    void reset();
    ParticipantInfo* findParticipant(ParticipantId id);
    const ParticipantInfo* findParticipant(ParticipantId id) const;
};

} // namespace ConsoleNetplay
