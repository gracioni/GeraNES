#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "NetplayTypes.h"
#include "NetplayTopology.h"

namespace ConsoleNetplay {

class PacketWriter;
class PacketReader;

constexpr uint8_t kProtocolVersion = 14;
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
    HostStallRecovery,
    ClientStallRecovery,
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

    static constexpr size_t serializedSize()
    {
        return sizeof(uint8_t) + sizeof(MessageType) + sizeof(uint32_t);
    }
    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, PacketHeader& header);
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

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, RomValidationData& data);
};

struct JoinRoomData
{
    uint64_t reconnectToken = 0;
    uint8_t romLoaded = 0;
    RomValidationData romValidation = {};

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, JoinRoomData& data);
};

struct JoinRejectedData
{
    JoinRejectReason reason = JoinRejectReason::Unknown;
    RomValidationData romValidation = {};

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, JoinRejectedData& data);
};

struct RomValidationResultData
{
    ParticipantId participantId = kInvalidParticipantId;
    uint8_t romLoaded = 0;
    uint8_t romCompatible = 0;
    RomValidationData romValidation = {};

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, RomValidationResultData& data);
};

struct InputTopologyData
{
    struct Slot
    {
        PlayerSlot slot = kObserverPlayerSlot;
        uint8_t assignable = 0;
        InputGroupId groupId = 0;
        InputDeviceId deviceId = kNoInputDevice;
        std::string groupLabel;
        std::string inputLabel;

        void serialize(PacketWriter& writer) const;
        static bool deserialize(PacketReader& reader, Slot& slot);
        size_t serializedSize() const;
    };

    std::vector<Slot> slots;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, InputTopologyData& data);
    size_t serializedSize() const;
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

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, InputFrameData& data);
};

struct ConfirmedInputFramesData
{
    uint32_t timelineEpoch = 0;
    FrameNumber startFrame = 0;
    uint16_t frameCount = 0;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, ConfirmedInputFramesData& data);
};

struct ConfirmedInputFrameEntry
{
    struct SlotMask
    {
        PlayerSlot slot = kObserverPlayerSlot;
        uint64_t buttonMaskLo = 0;
        uint64_t buttonMaskHi = 0;

        void serialize(PacketWriter& writer) const;
        static bool deserialize(PacketReader& reader, SlotMask& slotMask);
    };

    uint64_t authoritativeFrameStartClockMicros = 0;
    std::vector<SlotMask> slotMasks;
    uint16_t payloadSize = 0;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, ConfirmedInputFrameEntry& entry);
    size_t serializedSize() const;
};

struct InputAckData
{
    uint32_t timelineEpoch = 0;
    ParticipantId participantId = kInvalidParticipantId;
    PlayerSlot playerSlot = kObserverPlayerSlot;
    FrameNumber contiguousFrame = 0;
    uint32_t sequence = 0;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, InputAckData& data);
};

struct FrameStatusData
{
    uint32_t timelineEpoch = 0;
    FrameNumber currentFrame = 0;
    FrameNumber lastConfirmedFrame = 0;
    uint8_t inputDelayFrames = 0;
    uint8_t predictFrames = 0;
    InputTopologyData topology = {};

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, FrameStatusData& data);
    size_t serializedSize() const;
};

struct AssignControllerData
{
    ParticipantId participantId = kInvalidParticipantId;
    std::vector<PlayerSlot> controllerAssignments;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, AssignControllerData& data);
    size_t serializedSize() const;
};

struct ParticipantLeftData
{
    ParticipantId participantId = kInvalidParticipantId;
    uint32_t disconnectReason = 0;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, ParticipantLeftData& data);
};

struct LeaveRoomData
{
    ParticipantId participantId = kInvalidParticipantId;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, LeaveRoomData& data);
};

struct StartSessionData
{
    SessionState state = SessionState::Lobby;
    uint8_t inputDelayFrames = 0;
    uint8_t predictFrames = 0;
    InputTopologyData topology = {};

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, StartSessionData& data);
    size_t serializedSize() const;
};

struct PeerHealthData
{
    ParticipantId participantId = kInvalidParticipantId;
    FrameNumber currentFrame = 0;
    FrameNumber lastConfirmedFrame = 0;
    FrameNumber lastProducedLocalInputFrame = 0;
    uint32_t lastProducedLocalInputSequence = 0;
    uint8_t localAssignmentCount = 0;
    uint16_t pingMs = 0;
    uint16_t jitterMs = 0;
    uint64_t sharedClockMicros = 0;
    uint64_t clockSyncRttMicros = 0;
    uint8_t sharedClockSynchronized = 0;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, PeerHealthData& data);
};

struct ClockSyncRequestData
{
    uint32_t sequence = 0;
    uint64_t clientSendMicros = 0;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, ClockSyncRequestData& data);
};

struct ClockSyncResponseData
{
    uint32_t sequence = 0;
    uint64_t clientSendMicros = 0;
    uint64_t hostReceiveMicros = 0;
    uint64_t hostSendMicros = 0;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, ClockSyncResponseData& data);
};

struct CrcReportData
{
    uint32_t timelineEpoch = 0;
    FrameNumber frame = 0;
    uint32_t crc32 = 0;
    DesyncSeverity severity = DesyncSeverity::NoIssue;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, CrcReportData& data);
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

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, ResyncBeginData& data);
};

struct ResyncChunkData
{
    uint32_t resyncId = 0;
    uint32_t offset = 0;
    uint16_t size = 0;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, ResyncChunkData& data);
};

struct ResyncCompleteData
{
    uint32_t resyncId = 0;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, ResyncCompleteData& data);
};

struct ResyncAckData
{
    uint32_t resyncId = 0;
    ParticipantId participantId = kInvalidParticipantId;
    FrameNumber loadedFrame = 0;
    uint32_t crc32 = 0;
    uint8_t success = 0;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, ResyncAckData& data);
};

struct ResyncAbortData
{
    uint32_t resyncId = 0;
    ParticipantId participantId = kInvalidParticipantId;
    uint8_t reason = 0;

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, ResyncAbortData& data);
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

    void serialize(PacketWriter& writer) const;
    static bool deserialize(PacketReader& reader, ResyncRequestData& data);
};

constexpr uint16_t kResyncRequestFlagRollbackReplayBuildFailure = 1u << 0;
constexpr uint16_t kResyncRequestFlagRollbackReplayEnqueueFailure = 1u << 1;
constexpr uint16_t kResyncRequestFlagRollbackReplayAdvanceFailure = 1u << 2;

} // namespace ConsoleNetplay
