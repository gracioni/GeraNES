#pragma once

#ifndef __EMSCRIPTEN__

#include <cstdint>
#include <chrono>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "InputTimeline.h"
#include "Diagnostics.h"
#include "NetSerialization.h"
#include "NetSession.h"
#include "NetTransport.h"

namespace Netplay {

class NetplayCoordinator
{
private:
    struct IncomingResyncTransfer
    {
        uint32_t resyncId = 0;
        FrameNumber targetFrame = 0;
        uint32_t expectedPayloadCrc32 = 0;
        std::vector<uint8_t> payload;
        std::vector<uint8_t> receivedMask;
    };

public:
    struct PendingResyncApply
    {
        uint32_t resyncId = 0;
        FrameNumber targetFrame = 0;
        uint32_t expectedPayloadCrc32 = 0;
        std::vector<uint8_t> payload;
    };

private:
    NetTransport m_transport;
    NetSession m_session;
    InputTimeline m_localInputs;
    InputTimeline m_remoteInputs;
    std::vector<std::string> m_eventLog;
    ENetPeer* m_serverPeer = nullptr;
    ParticipantId m_localParticipantId = kInvalidParticipantId;
    std::string m_localDisplayName;
    uint64_t m_localReconnectToken = 0;
    std::string m_lastError;
    bool m_hosting = false;
    bool m_connected = false;
    ParticipantId m_nextAssignedParticipantId = 1;
    uint32_t m_localInputSequence = 0;
    RollbackStats m_predictionStats;
    std::optional<FrameNumber> m_pendingRollbackFrame;
    std::optional<FrameNumber> m_pendingHostResyncFrame;
    FrameNumber m_lastBroadcastConfirmedFrame = 0;
    uint8_t m_lastBroadcastInputDelayFrames = 0;
    FrameNumber m_lastLocalCrcFrame = 0;
    uint32_t m_lastLocalCrc32 = 0;
    std::deque<std::pair<FrameNumber, uint32_t>> m_recentLocalCrcHistory;
    FrameNumber m_lastCrcMismatchFrame = 0;
    uint8_t m_consecutiveCrcMismatchCount = 0;
    FrameNumber m_localSimulationFrame = 0;
    uint32_t m_nextResyncId = 1;
    std::optional<IncomingResyncTransfer> m_incomingResync;
    std::optional<IncomingResyncTransfer> m_incomingSpectatorSync;
    std::optional<PendingResyncApply> m_pendingResyncApply;
    std::optional<PendingResyncApply> m_pendingSpectatorSyncApply;
    std::optional<ParticipantId> m_pendingHostSpectatorSyncParticipant;
    std::vector<ParticipantId> m_pendingResyncAcks;
    std::chrono::steady_clock::time_point m_lastPeerHealthBroadcast = {};
    std::unordered_map<ParticipantId, std::chrono::steady_clock::time_point> m_reconnectReservationDeadlines;
    bool m_requiresSpectatorSync = false;

    static std::string defaultDisplayName();
    static uint32_t generateSessionId();
    static uint64_t generateReconnectToken();
    static std::string messageTypeLabel(MessageType type);

    void resetSessionState();
    void pushLog(const std::string& message);
    ParticipantInfo& ensureParticipant(ParticipantId id, const std::string& displayName);
    ParticipantId participantIdFromPeer(ENetPeer* peer) const;
    void removeParticipant(ParticipantId participantId);
    std::vector<uint8_t> buildJoinRoomPacket() const;
    std::vector<uint8_t> buildParticipantJoinedPacket(const ParticipantInfo& participant, uint64_t reconnectToken) const;
    std::vector<uint8_t> buildSelectRomPacket(const std::string& gameName, const RomValidationData& romValidation) const;
    std::vector<uint8_t> buildRomValidationResultPacket(const RomValidationResultData& result) const;
    std::vector<uint8_t> buildParticipantLeftPacket(ParticipantId participantId) const;
    std::vector<uint8_t> buildSetRolePacket(const SetRoleData& data) const;
    std::vector<uint8_t> buildResyncBeginPacket(const ResyncBeginData& data) const;
    std::vector<uint8_t> buildResyncChunkPacket(const ResyncChunkData& data, std::span<const uint8_t> payloadChunk) const;
    std::vector<uint8_t> buildResyncCompletePacket(const ResyncCompleteData& data) const;
    std::vector<uint8_t> buildResyncAckPacket(const ResyncAckData& data) const;
    std::vector<uint8_t> buildSpectatorSyncBeginPacket(const SpectatorSyncBeginData& data) const;
    std::vector<uint8_t> buildSpectatorSyncChunkPacket(const SpectatorSyncChunkData& data, std::span<const uint8_t> payloadChunk) const;
    std::vector<uint8_t> buildSpectatorSyncCompletePacket(const SpectatorSyncCompleteData& data) const;
    std::vector<uint8_t> buildSpectatorSyncAckPacket(const SpectatorSyncAckData& data) const;
    std::vector<uint8_t> buildPeerHealthPacket(const PeerHealthData& data, uint32_t sessionId) const;
    bool handleControlPacket(ENetPeer* peer, const std::vector<uint8_t>& payload);
    bool handleJoinRoom(ENetPeer* peer, PacketReader& reader);
    bool handleParticipantJoined(PacketReader& reader);
    bool handleSelectRom(PacketReader& reader);
    bool handleRomValidationResult(ENetPeer* peer, PacketReader& reader);
    bool handleParticipantLeft(PacketReader& reader);
    bool handleSetRole(PacketReader& reader);
    bool handleResyncBegin(PacketReader& reader);
    bool handleResyncChunk(PacketReader& reader);
    bool handleResyncComplete(PacketReader& reader);
    bool handleResyncAck(PacketReader& reader);
    bool handleSpectatorSyncBegin(PacketReader& reader);
    bool handleSpectatorSyncChunk(PacketReader& reader);
    bool handleSpectatorSyncComplete(PacketReader& reader);
    bool handleSpectatorSyncAck(PacketReader& reader);
    bool handlePeerHealth(ENetPeer* peer, PacketReader& reader);
    bool handleInputFrame(ENetPeer* peer, PacketReader& reader);
    bool handleFrameStatus(PacketReader& reader);
    bool handleCrcReport(PacketReader& reader);
    bool handleAssignController(PacketReader& reader);
    bool handleSetReady(ENetPeer* peer, PacketReader& reader);
    bool handleRequestController(ENetPeer* peer, PacketReader& reader);
    bool handleStartSession(PacketReader& reader);
    std::optional<uint32_t> findRecentLocalCrc(FrameNumber frame) const;
    void realignAuthoritativeState(FrameNumber loadedFrame);
    void recordMissingInputGap(ParticipantInfo& participant, FrameNumber missingFrame, FrameNumber receivedFrame, PlayerSlot slot);
    void advanceParticipantContiguousInputFrame(ParticipantInfo& participant, PlayerSlot slot);
    void handleConfirmedInputMismatch(ParticipantId participantId, FrameNumber inputFrame, PlayerSlot slot);
    bool predictRemoteInputFrame(FrameNumber frame, ParticipantId participantId, PlayerSlot slot);
    FrameNumber computeHostConfirmedFrame() const;
    void broadcastFrameStatusIfNeeded();
    void broadcastPeerHealthIfNeeded();
    bool allRequiredParticipantsReady() const;
    bool allRequiredParticipantsRomCompatible() const;
    void refreshHostRoomState();
    void updatePeerHealthFromTransport();
    void updateReconnectReservations();

public:
    NetplayCoordinator();
    static bool romValidationMatches(const RomValidationData& a, const RomValidationData& b);

    bool host(uint16_t port, size_t maxPeers, const std::string& displayName);
    bool join(const std::string& hostName, uint16_t port, const std::string& displayName);
    void disconnect();
    void update(uint32_t timeoutMs = 0);

    bool isActive() const;
    bool isHosting() const;
    bool isConnected() const;
    ParticipantId localParticipantId() const;
    const std::string& localDisplayName() const;
    uint64_t localReconnectToken() const;
    void setLocalReconnectToken(uint64_t token);
    const std::string& lastError() const;
    const NetSession& session() const;
    const std::vector<std::string>& eventLog() const;
    const RollbackStats& predictionStats() const;
    void setLocalSimulationFrame(FrameNumber frame);
    void rescheduleRollbackFrame(FrameNumber frame);
    std::optional<FrameNumber> consumePendingRollbackFrame();
    std::optional<FrameNumber> consumePendingHostResyncFrame();
    std::optional<ParticipantId> consumePendingHostSpectatorSyncParticipant();
    const InputTimeline& localInputs() const;
    const InputTimeline& remoteInputs() const;
    void recordLocalInputFrame(FrameNumber frame, PlayerSlot slot, uint64_t buttonMaskLo, uint64_t buttonMaskHi = 0);
    void predictRemoteInputsForFrame(FrameNumber frame);
    void submitLocalCrc(FrameNumber frame, uint32_t crc32);
    void invalidateLocalCrcHistoryAfter(FrameNumber frame);
    bool beginResync(FrameNumber targetFrame, const std::vector<uint8_t>& payload, uint32_t payloadCrc32);
    bool beginSpectatorSync(ParticipantId participantId, FrameNumber targetFrame, const std::vector<uint8_t>& payload, uint32_t payloadCrc32);
    std::optional<PendingResyncApply> consumePendingResyncApply();
    std::optional<PendingResyncApply> consumePendingSpectatorSyncApply();
    bool acknowledgeResync(uint32_t resyncId, FrameNumber loadedFrame, uint32_t crc32, bool success);
    bool acknowledgeSpectatorSync(uint32_t resyncId, FrameNumber loadedFrame, uint32_t crc32, bool success);
    bool awaitingSpectatorSync() const;
    bool selectRom(const std::string& gameName, const RomValidationData& romValidation);
    bool submitLocalRomValidation(bool romLoaded, bool romCompatible, const RomValidationData& romValidation);
    bool setLocalReady(bool ready);
    bool requestControllerSlot(PlayerSlot slot);
    bool cancelControllerRequest();
    bool approveControllerRequest(ParticipantId participantId);
    bool denyControllerRequest(ParticipantId participantId);
    bool setParticipantRole(ParticipantId participantId, ParticipantRole role);
    bool assignController(ParticipantId participantId, PlayerSlot slot);
    bool kickParticipant(ParticipantId participantId);
    bool removeReconnectReservation(ParticipantId participantId);
    bool setInputDelayFrames(uint8_t frames);
    bool startSession();
    bool pauseSession();
    bool resumeSession();
    bool endSession();
};

} // namespace Netplay

#endif
