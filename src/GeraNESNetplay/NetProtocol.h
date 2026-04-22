#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "GeraNES/Settings.h"
#include "NetplayTypes.h"

namespace Netplay {

constexpr uint8_t kProtocolVersion = 6;
constexpr size_t kMaxRomHashBytes = 32;
constexpr size_t kMaxDisplayNameBytes = 32;
constexpr size_t kMaxChatMessageBytes = 256;

enum class Channel : uint8_t
{
    Control = 0,
    Gameplay = 1,
    Diagnostics = 2
};

enum class MessageType : uint16_t
{
    CreateRoom = 1,
    JoinRoom,
    JoinRejected,
    ParticipantJoined,
    ParticipantLeft,
    LeaveRoom,
    ChatMessage,
    AssignController,
    SelectRom,
    RomValidationResult,
    StartSession,
    PauseSession,
    ResumeSession,
    EndSession,

    InputFrame = 100,
    ConfirmedInputFrames,
    InputAck,
    FrameStatus,
    PeerHealth,
    CrcReport,
    ClockSyncRequest,
    ClockSyncResponse,

    ResyncBegin = 200,
    ResyncChunk,
    ResyncComplete,
    ResyncAck,
    ResyncAbort,
    ResyncRequest
};

enum class ParticipantRole : uint8_t
{
    SessionOwner,
    SessionParticipant,
    Observer
};

enum class JoinRejectReason : uint8_t
{
    Unknown = 0,
    RomMismatch = 1,
    EmulatorVersionMismatch = 2
};

enum class ResyncReason : uint8_t
{
    Unspecified = 0,
    InitialSessionSync,
    ConfirmedDesync,
    AssignmentChanged,
    ManualForce,
    HostReset,
    HostLoadedState,
    ObserverVisibilityRestore
};

struct PacketHeader
{
    uint8_t protocolVersion = kProtocolVersion;
    MessageType type = MessageType::CreateRoom;
    uint32_t sessionId = 0;
};

struct RomValidationData
{
    uint32_t romCrc32 = 0;
    uint16_t mapperId = 0;
    uint16_t subMapperId = 0;
    uint32_t prgRomSize = 0;
    uint32_t chrRomSize = 0;
    uint32_t chrRamSize = 0;
    uint32_t fileSize = 0;
    std::array<uint8_t, kMaxRomHashBytes> contentHash = {};
};

struct JoinRoomData
{
    uint64_t reconnectToken = 0;
    uint8_t romLoaded = 0;
    RomValidationData romValidation = {};
};

struct JoinRejectedData
{
    JoinRejectReason reason = JoinRejectReason::Unknown;
    RomValidationData romValidation = {};
};

struct RomValidationResultData
{
    ParticipantId participantId = kInvalidParticipantId;
    uint8_t romLoaded = 0;
    uint8_t romCompatible = 0;
    RomValidationData romValidation = {};
};

struct InputTopologyData
{
    Settings::Device port1Device = Settings::Device::NONE;
    Settings::Device port2Device = Settings::Device::NONE;
    Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
    Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
    Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;
};

struct InputFrameData
{
    uint32_t timelineEpoch = 0;
    FrameNumber frame = 0;
    uint64_t authoritativeFrameStartClockMicros = 0;
    ParticipantId participantId = kInvalidParticipantId;
    PlayerSlot playerSlot = kObserverPlayerSlot;
    uint64_t buttonMaskLo = 0;
    uint64_t buttonMaskHi = 0;
    uint32_t sequence = 0;
    uint16_t payloadSize = 0;
};

struct ConfirmedInputFramesData
{
    uint32_t timelineEpoch = 0;
    FrameNumber startFrame = 0;
    uint16_t frameCount = 0;
};

struct ConfirmedInputFrameEntry
{
    uint64_t authoritativeFrameStartClockMicros = 0;
    std::array<uint64_t, kMaxAssignedPlayerSlot + 1> buttonMaskLo = {};
    std::array<uint64_t, kMaxAssignedPlayerSlot + 1> buttonMaskHi = {};
    uint16_t payloadSize = 0;
};

struct InputAckData
{
    uint32_t timelineEpoch = 0;
    ParticipantId participantId = kInvalidParticipantId;
    PlayerSlot playerSlot = kObserverPlayerSlot;
    FrameNumber contiguousFrame = 0;
    uint32_t sequence = 0;
};

struct FrameStatusData
{
    uint32_t timelineEpoch = 0;
    FrameNumber currentFrame = 0;
    FrameNumber lastConfirmedFrame = 0;
    uint8_t inputDelayFrames = 0;
    uint8_t predictFrames = 0;
    InputTopologyData topology = {};
};

struct AssignControllerData
{
    ParticipantId participantId = kInvalidParticipantId;
    uint8_t assignmentCount = 0;
    std::array<PlayerSlot, kMaxAssignedPlayerSlot + 1> controllerAssignments = {};
};

struct ParticipantLeftData
{
    ParticipantId participantId = kInvalidParticipantId;
};

struct LeaveRoomData
{
    ParticipantId participantId = kInvalidParticipantId;
};

struct StartSessionData
{
    SessionState state = SessionState::Lobby;
    uint8_t inputDelayFrames = 0;
    uint8_t predictFrames = 0;
    InputTopologyData topology = {};
};

struct PeerHealthData
{
    ParticipantId participantId = kInvalidParticipantId;
    FrameNumber currentFrame = 0;
    FrameNumber lastConfirmedFrame = 0;
    uint16_t pingMs = 0;
    uint16_t jitterMs = 0;
    uint64_t sharedClockMicros = 0;
    uint64_t clockSyncRttMicros = 0;
    uint8_t sharedClockSynchronized = 0;
};

struct ClockSyncRequestData
{
    uint32_t sequence = 0;
    uint64_t clientSendMicros = 0;
};

struct ClockSyncResponseData
{
    uint32_t sequence = 0;
    uint64_t clientSendMicros = 0;
    uint64_t hostReceiveMicros = 0;
    uint64_t hostSendMicros = 0;
};

struct CrcReportData
{
    uint32_t timelineEpoch = 0;
    FrameNumber frame = 0;
    uint32_t crc32 = 0;
    DesyncSeverity severity = DesyncSeverity::NoIssue;
};

struct ResyncBeginData
{
    uint32_t resyncId = 0;
    uint32_t timelineEpoch = 0;
    FrameNumber targetFrame = 0;
    FrameNumber confirmedFrame = 0;
    FrameNumber frameReadyFrame = 0;
    uint32_t payloadSize = 0;
    uint32_t payloadCrc32 = 0;
    uint32_t stateCrc32 = 0;
    uint32_t frameReadyCrc32 = 0;
    uint32_t inputSequenceBase = 0;
    ResyncReason reason = ResyncReason::Unspecified;
};

struct ResyncChunkData
{
    uint32_t resyncId = 0;
    uint32_t offset = 0;
    uint16_t size = 0;
};

struct ResyncCompleteData
{
    uint32_t resyncId = 0;
};

struct ResyncAckData
{
    uint32_t resyncId = 0;
    ParticipantId participantId = kInvalidParticipantId;
    FrameNumber loadedFrame = 0;
    uint32_t crc32 = 0;
    uint8_t success = 0;
};

struct ResyncAbortData
{
    uint32_t resyncId = 0;
    ParticipantId participantId = kInvalidParticipantId;
    uint8_t reason = 0;
};

struct ResyncRequestData
{
    ParticipantId participantId = kInvalidParticipantId;
    ResyncReason reason = ResyncReason::Unspecified;
    FrameNumber localFrame = 0;
    FrameNumber estimatedHostFrame = 0;
    FrameNumber confirmedThroughFrame = 0;
    uint16_t lagFrames = 0;
    uint16_t catchupBudgetFrames = 0;
    uint16_t source = 0;
    uint16_t flags = 0;
};

constexpr uint16_t kResyncRequestFlagRollbackReplayBuildFailure = 1u << 0;
constexpr uint16_t kResyncRequestFlagRollbackReplayEnqueueFailure = 1u << 1;
constexpr uint16_t kResyncRequestFlagRollbackReplayAdvanceFailure = 1u << 2;

} // namespace Netplay
