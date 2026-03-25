#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "NetplayTypes.h"

namespace Netplay {

constexpr uint8_t kProtocolVersion = 1;
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
    ParticipantJoined,
    ParticipantLeft,
    ChatMessage,
    SetRole,
    AssignController,
    SetReady,
    RequestController,
    SelectRom,
    RomValidationResult,
    StartSession,
    PauseSession,
    ResumeSession,
    EndSession,

    InputFrame = 100,
    InputAck,
    FrameStatus,
    PeerHealth,
    CrcReport,

    ResyncBegin = 200,
    ResyncChunk,
    ResyncComplete,
    ResyncAck,
    ResyncAbort,

    SpectatorSyncBegin = 220,
    SpectatorSyncChunk,
    SpectatorSyncComplete,
    SpectatorSyncAck
};

enum class ParticipantRole : uint8_t
{
    Host,
    Player,
    Observer
};

struct PacketHeader
{
    uint8_t protocolVersion = kProtocolVersion;
    MessageType type = MessageType::CreateRoom;
    uint32_t sessionId = 0;
};

struct JoinRoomData
{
    uint64_t reconnectToken = 0;
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

struct RomValidationResultData
{
    ParticipantId participantId = kInvalidParticipantId;
    uint8_t romLoaded = 0;
    uint8_t romCompatible = 0;
    RomValidationData romValidation = {};
};

struct InputFrameData
{
    uint32_t timelineEpoch = 0;
    FrameNumber frame = 0;
    ParticipantId participantId = kInvalidParticipantId;
    PlayerSlot playerSlot = kObserverPlayerSlot;
    uint64_t buttonMaskLo = 0;
    uint64_t buttonMaskHi = 0;
    uint32_t sequence = 0;
};

struct FrameStatusData
{
    uint32_t timelineEpoch = 0;
    FrameNumber currentFrame = 0;
    FrameNumber lastConfirmedFrame = 0;
    uint8_t inputDelayFrames = 0;
};

struct AssignControllerData
{
    ParticipantId participantId = kInvalidParticipantId;
    PlayerSlot controllerAssignment = kObserverPlayerSlot;
};

struct ParticipantLeftData
{
    ParticipantId participantId = kInvalidParticipantId;
};

struct SetRoleData
{
    ParticipantId participantId = kInvalidParticipantId;
    ParticipantRole role = ParticipantRole::Observer;
};

struct SetReadyData
{
    ParticipantId participantId = kInvalidParticipantId;
    uint8_t ready = 0;
};

struct RequestControllerData
{
    ParticipantId participantId = kInvalidParticipantId;
    PlayerSlot requestedSlot = kObserverPlayerSlot;
    uint8_t clearRequest = 0;
};

struct StartSessionData
{
    SessionState state = SessionState::Lobby;
    uint8_t inputDelayFrames = 0;
};

struct PeerHealthData
{
    ParticipantId participantId = kInvalidParticipantId;
    FrameNumber currentFrame = 0;
    FrameNumber lastConfirmedFrame = 0;
    uint16_t pingMs = 0;
    uint16_t jitterMs = 0;
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
    uint32_t payloadSize = 0;
    uint32_t payloadCrc32 = 0;
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

using SpectatorSyncBeginData = ResyncBeginData;
using SpectatorSyncChunkData = ResyncChunkData;
using SpectatorSyncCompleteData = ResyncCompleteData;
using SpectatorSyncAckData = ResyncAckData;

} // namespace Netplay
