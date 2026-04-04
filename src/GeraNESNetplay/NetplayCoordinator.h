#pragma once

#include <cstdint>
#include <chrono>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "GeraNES/InputBuffer.h"
#include "GeraNES/Settings.h"
#include "DesyncMonitor.h"
#include "ImplicitStallRecoveryMonitor.h"
#include "InputTimeline.h"
#include "Diagnostics.h"
#include "NetSerialization.h"
#include "NetSession.h"
#include "NetTransport.h"
#include "NetplayConfig.h"

namespace Netplay {

class NetplayCoordinator
{
public:
    struct ConfirmedFrameInputs
    {
        FrameNumber frame = 0;
        std::array<uint64_t, kMaxAssignedPlayerSlot + 1> buttonMaskLo = {};
        std::array<uint64_t, kMaxAssignedPlayerSlot + 1> buttonMaskHi = {};
        InputFrame inputFrame = {};
        bool predicted = false;
    };

private:
    struct IncomingResyncTransfer
    {
        uint32_t resyncId = 0;
        FrameNumber targetFrame = 0;
        uint32_t expectedPayloadCrc32 = 0;
        std::vector<uint8_t> payload;
        std::vector<uint8_t> receivedMask;
        std::chrono::steady_clock::time_point lastActivityAt = {};
    };

public:
    struct PendingResyncApply
    {
        uint32_t resyncId = 0;
        FrameNumber targetFrame = 0;
        FrameNumber confirmedFrame = 0;
        FrameNumber frameReadyFrame = 0;
        uint32_t expectedPayloadCrc32 = 0;
        uint32_t frameReadyCrc32 = 0;
        uint32_t inputSequenceBase = 0;
        ResyncReason reason = ResyncReason::Unspecified;
        std::vector<uint8_t> payload;
    };

private:
    struct DelayedPacketEvent
    {
        std::chrono::steady_clock::time_point releaseAt = {};
        NetTransport::Event event;
    };

    NetTransport m_transport;
    NetSession m_session;
    InputTimeline m_localInputs;
    InputTimeline m_remoteInputs;
    std::deque<ConfirmedFrameInputs> m_confirmedFrames;
    std::vector<std::string> m_eventLog;
    NetTransport::PeerHandle m_serverPeer = NetTransport::kInvalidPeerHandle;
    ParticipantId m_localParticipantId = kInvalidParticipantId;
    std::string m_localDisplayName;
    uint64_t m_localReconnectToken = 0;
    bool m_pendingJoinRomLoaded = false;
    RomValidationData m_pendingJoinRomValidation = {};
    std::string m_localEmulatorVersion;
    bool m_disconnectExpectedAfterJoinReject = false;
    bool m_disconnectExpectedAfterHostShutdown = false;
    bool m_gracefulDisconnectPending = false;
    std::chrono::steady_clock::time_point m_gracefulDisconnectDeadline = {};
    std::string m_lastError;
    std::string m_lastTransportError;
    bool m_hosting = false;
    bool m_connected = false;
    ParticipantId m_nextAssignedParticipantId = 1;
    uint32_t m_localInputSequence = 0;
    RollbackStats m_predictionStats;
    std::optional<FrameNumber> m_pendingRollbackFrame;
    std::optional<FrameNumber> m_pendingHostResyncFrame;
    FrameNumber m_lastBroadcastConfirmedFrame = 0;
    uint8_t m_lastBroadcastInputDelayFrames = 0;
    DesyncMonitor m_desyncMonitor;
    FrameNumber m_localSimulationFrame = 0;
    uint32_t m_nextResyncId = 1;
    uint32_t m_activeResyncExpectedStateCrc32 = 0;
    std::optional<IncomingResyncTransfer> m_incomingResync;
    std::optional<PendingResyncApply> m_pendingResyncApply;
    std::optional<ParticipantId> m_pendingHostLateJoinResyncParticipant;
    ImplicitStallRecoveryMonitor m_implicitRecoveryMonitor;
    std::vector<ParticipantId> m_pendingResyncAcks;
    std::vector<ParticipantId> m_pendingSequenceResetParticipants;
    std::chrono::steady_clock::time_point m_lastPeerHealthBroadcast = {};
    std::unordered_map<ParticipantId, std::chrono::steady_clock::time_point> m_reconnectReservationDeadlines;
    std::string m_lastJoinHostName;
    uint16_t m_lastJoinPort = 0;
    bool m_reconnectPending = false;
    bool m_reconnectAttemptInFlight = false;
    uint16_t m_reconnectSecondsRemaining = 0;
    std::chrono::steady_clock::time_point m_nextReconnectAttempt = {};
    std::chrono::steady_clock::time_point m_reconnectDeadline = {};
    uint32_t m_gameplayReceiveDelayMs = 0;
    std::deque<DelayedPacketEvent> m_delayedPacketEvents;
    std::unordered_map<uint16_t, uint32_t> m_dropIncomingMessageCounts;
    std::chrono::seconds m_reconnectReservationDuration = std::chrono::seconds(300);

    static std::string defaultDisplayName();
    static uint32_t generateSessionId();
    static uint64_t generateReconnectToken();
    static std::string messageTypeLabel(MessageType type);

    void resetSessionState();
    void pushLog(const std::string& message);
    ParticipantInfo& ensureParticipant(ParticipantId id, const std::string& displayName);
    ParticipantId participantIdFromPeer(NetTransport::PeerHandle peer) const;
    ParticipantInfo* findParticipantByReconnectToken(uint64_t reconnectToken);
    void removeParticipant(ParticipantId participantId);
    void clearReconnectAttemptState();
    void completeLocalDisconnect();
    void scheduleReconnectAttempt();
    void processPendingReconnect();
    std::vector<uint8_t> buildJoinRoomPacket() const;
    std::vector<uint8_t> buildJoinRejectedPacket(JoinRejectReason reason,
                                                 const std::string& gameName,
                                                 const RomValidationData& romValidation,
                                                 const std::string& expectedEmulatorVersion = {}) const;
    std::vector<uint8_t> buildParticipantJoinedPacket(const ParticipantInfo& participant, uint64_t reconnectToken) const;
    std::vector<uint8_t> buildSelectRomPacket(const std::string& gameName, const RomValidationData& romValidation) const;
    std::vector<uint8_t> buildRomValidationResultPacket(const RomValidationResultData& result) const;
    std::vector<uint8_t> buildParticipantLeftPacket(ParticipantId participantId) const;
    std::vector<uint8_t> buildLeaveRoomPacket(ParticipantId participantId) const;
    std::vector<uint8_t> buildResyncBeginPacket(const ResyncBeginData& data) const;
    std::vector<uint8_t> buildResyncChunkPacket(const ResyncChunkData& data, std::span<const uint8_t> payloadChunk) const;
    std::vector<uint8_t> buildResyncCompletePacket(const ResyncCompleteData& data) const;
    std::vector<uint8_t> buildResyncAckPacket(const ResyncAckData& data) const;
    std::vector<uint8_t> buildResyncAbortPacket(const ResyncAbortData& data) const;
    std::vector<uint8_t> buildPeerHealthPacket(const PeerHealthData& data, uint32_t sessionId) const;
    bool handleControlPacket(NetTransport::PeerHandle peer, const std::vector<uint8_t>& payload);
    bool handleJoinRoom(NetTransport::PeerHandle peer, PacketReader& reader);
    bool handleJoinRejected(PacketReader& reader);
    bool handleParticipantJoined(PacketReader& reader);
    bool handleSelectRom(PacketReader& reader);
    bool handleRomValidationResult(NetTransport::PeerHandle peer, PacketReader& reader);
    bool handleParticipantLeft(PacketReader& reader);
    bool handleLeaveRoom(NetTransport::PeerHandle peer, PacketReader& reader);
    bool handleResyncBegin(PacketReader& reader);
    bool handleResyncChunk(PacketReader& reader);
    bool handleResyncComplete(PacketReader& reader);
    bool handleResyncAck(PacketReader& reader);
    bool handleResyncAbort(PacketReader& reader);
    bool handlePeerHealth(NetTransport::PeerHandle peer, PacketReader& reader);
    bool handleInputFrame(NetTransport::PeerHandle peer, PacketReader& reader);
    bool handleConfirmedInputFrames(PacketReader& reader);
    bool handleInputAck(PacketReader& reader);
    bool handleFrameStatus(PacketReader& reader);
    bool handleCrcReport(PacketReader& reader);
    void applyDesyncMonitorUpdate(const DesyncMonitor::Update& update, const char* source);
    bool handleAssignController(PacketReader& reader);
    bool handleStartSession(PacketReader& reader);
    std::optional<uint32_t> findRecentLocalCrc(FrameNumber frame) const;
    void realignAuthoritativeState(FrameNumber loadedFrame,
                                   bool resetInputSequences = false,
                                   uint32_t inputSequenceBase = 0);
    void recordMissingInputGap(ParticipantInfo& participant, FrameNumber missingFrame, FrameNumber receivedFrame, PlayerSlot slot);
    void advanceParticipantContiguousInputFrame(ParticipantInfo& participant, PlayerSlot slot);
    void storeConfirmedFrame(const ConfirmedFrameInputs& frame);
    bool tryAssembleConfirmedFrame(FrameNumber frame, ConfirmedFrameInputs& outFrame) const;
    void publishConfirmedFramesIfReady();
    void handleResolvedPredictedInput(ParticipantId participantId, FrameNumber inputFrame, PlayerSlot slot, bool predictionMatched);
    bool predictRemoteInputFrame(FrameNumber frame, ParticipantId participantId, PlayerSlot slot);
    void noteImplicitRemoteInputStall(ParticipantId participantId, PlayerSlot slot, FrameNumber frame);
    void clearImplicitRemoteInputStall(ParticipantId participantId, FrameNumber recoveredThroughFrame);
    void tryScheduleImplicitRecoveryResync(ParticipantInfo& participant);
    bool tryBuildPlaybackFrameInternal(FrameNumber frame, bool allowPrediction, ConfirmedFrameInputs& outFrame);
    FrameNumber computeHostConfirmedFrame() const;
    void broadcastFrameStatusIfNeeded();
    void broadcastPeerHealthIfNeeded();
    bool allRequiredParticipantsRomCompatible() const;
    void refreshHostRoomState();
    void updatePeerHealthFromTransport();
    void updateReconnectReservations();
    void resetRuntimeTimelineStateForSessionStart();
    void discardTimelineStateAfter(FrameNumber frame);
    void seedNeutralInputBaseline(ParticipantId participantId, PlayerSlot slot, FrameNumber frame);
    void scheduleResyncRetry(FrameNumber targetFrame, const std::string& reason);

public:
    NetplayCoordinator();
    static bool romValidationMatches(const RomValidationData& a, const RomValidationData& b);
    static std::string resyncReasonToast(ResyncReason reason);
    uint32_t unresolvedPredictedRemoteFrameCount() const;
    FrameNumber latestPredictedRemoteFrame() const;
    void setRoomInputTopology(std::optional<Settings::Device> port1Device,
                              std::optional<Settings::Device> port2Device,
                              Settings::ExpansionDevice expansionDevice,
                              Settings::NesMultitapDevice nesMultitapDevice,
                              Settings::FamicomMultitapDevice famicomMultitapDevice,
                              std::optional<ParticipantId> preservedParticipantId = std::nullopt,
                              PlayerSlot preservedAssignment = kObserverPlayerSlot);

    bool host(uint16_t port, size_t maxPeers, const std::string& displayName);
    bool join(const std::string& hostName, uint16_t port, const std::string& displayName);
    void disconnect();
    void update(uint32_t timeoutMs = 0);
    bool setTransportBackend(NetTransportBackend backend);
    void setTransportOptions(const NetTransportOptions& options);
    const NetTransportOptions& transportOptions() const;
    NetTransportBackend transportBackend() const;

    bool isActive() const;
    bool isHosting() const;
    bool isConnected() const;
    bool reconnectPending() const;
    uint16_t reconnectSecondsRemaining() const;
    void setPendingJoinRomValidation(bool romLoaded, const RomValidationData& romValidation);
    uint32_t gameplayReceiveDelayMs() const;
    void setGameplayReceiveDelayMs(uint32_t delayMs);
    void dropNextIncomingMessages(MessageType type, uint32_t count);
    void clearIncomingMessageDrops();
    void setReconnectReservationDurationForTests(uint32_t seconds);
    void setLocalEmulatorVersionForTests(const std::string& version);
    void simulateTransportFailureForTests();
    ParticipantId localParticipantId() const;
    const std::string& localDisplayName() const;
    uint64_t localReconnectToken() const;
    void setLocalReconnectToken(uint64_t token);
    const std::string& lastError() const;
    const NetSession& session() const;
    const std::vector<std::string>& eventLog() const;
    const RollbackStats& predictionStats() const;
    void recordPlaybackStop(FrameNumber frame, bool predictionLimitReached);
    void setLocalSimulationFrame(FrameNumber frame);
    void rescheduleRollbackFrame(FrameNumber frame);
    std::optional<FrameNumber> consumePendingRollbackFrame();
    void discardTimelineAfter(FrameNumber frame);
    std::optional<FrameNumber> consumePendingHostResyncFrame();
    std::optional<ParticipantId> consumePendingHostLateJoinResyncParticipant();
    const InputTimeline& localInputs() const;
    const InputTimeline& remoteInputs() const;
    const ConfirmedFrameInputs* findConfirmedFrame(FrameNumber frame) const;
    FrameNumber latestConfirmedFrame() const;
    uint8_t predictFrames() const;
    void recordLocalInputFrame(FrameNumber frame, PlayerSlot slot, const InputFrame& contribution);
    void recordLocalInputFrame(FrameNumber frame, PlayerSlot slot, uint64_t buttonMaskLo, uint64_t buttonMaskHi = 0);
    void predictRemoteInputsForFrame(FrameNumber frame);
    bool tryBuildPlaybackFrame(FrameNumber frame, bool allowPrediction, ConfirmedFrameInputs& outFrame);
    void submitLocalCrc(FrameNumber frame, uint32_t crc32);
    void invalidateLocalCrcHistoryAfter(FrameNumber frame);
    bool beginResync(FrameNumber targetFrame,
                     const std::vector<uint8_t>& payload,
                     uint32_t payloadCrc32,
                     uint32_t stateCrc32,
                     ResyncReason reason = ResyncReason::Unspecified);
    std::optional<PendingResyncApply> consumePendingResyncApply();
    bool acknowledgeResync(uint32_t resyncId, FrameNumber loadedFrame, uint32_t crc32, bool success);
    bool selectRom(const std::string& gameName, const RomValidationData& romValidation);
    bool submitLocalRomValidation(bool romLoaded, bool romCompatible, const RomValidationData& romValidation);
    bool assignController(ParticipantId participantId, PlayerSlot slot);
    bool addControllerAssignment(ParticipantId participantId, PlayerSlot slot);
    bool removeControllerAssignment(ParticipantId participantId, PlayerSlot slot);
    bool clearControllerAssignments(ParticipantId participantId);
    bool kickParticipant(ParticipantId participantId);
    bool removeReconnectReservation(ParticipantId participantId);
    bool setInputDelayFrames(uint8_t frames);
    bool setPredictFrames(uint8_t frames);
    bool startSession();
    bool pauseSession();
    bool resumeSession();
    bool endSession();
};

} // namespace Netplay
