#pragma once

#include <cstdint>
#include <chrono>
#include <deque>
#include <list>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "DesyncMonitor.h"
#include "RemoteInputStallMonitor.h"
#include "InputTimeline.h"
#include "Diagnostics.h"
#include "NetplayInputFrame.h"
#include "NetSerialization.h"
#include "NetSession.h"
#include "NetTransport.h"
#include "NetplayConfig.h"

namespace ConsoleNetplay {

class NetplayCoordinator
{
public:
    struct LinearScanStats
    {
        uint64_t calls = 0;
        uint64_t hits = 0;
        uint64_t misses = 0;
        uint64_t totalScannedEntries = 0;
        uint64_t maxScannedEntries = 0;
        uint64_t lastScannedEntries = 0;

        void record(bool hit, size_t scannedEntries)
        {
            ++calls;
            if(hit) {
                ++hits;
            } else {
                ++misses;
            }
            const uint64_t scanned = static_cast<uint64_t>(scannedEntries);
            lastScannedEntries = scanned;
            totalScannedEntries += scanned;
            if(scanned > maxScannedEntries) {
                maxScannedEntries = scanned;
            }
        }
    };

    struct PerformanceDiagnostics
    {
        LinearScanStats confirmedFrameFind;
        LinearScanStats confirmedFrameStore;
    };

    struct ConfirmedFrameInputs
    {
        FrameNumber frame = 0;
        uint64_t authoritativeFrameStartClockMicros = 0;
        std::array<uint64_t, kMaxAssignedPlayerSlot + 1> buttonMaskLo = {};
        std::array<uint64_t, kMaxAssignedPlayerSlot + 1> buttonMaskHi = {};
        NetplayInputFrame netplayFrame = {};
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

    struct PendingHostResyncRequest
    {
        FrameNumber frame = 0;
        ResyncReason reason = ResyncReason::Unspecified;
        ParticipantId participantId = kInvalidParticipantId;
    };

private:
    struct DelayedPacketEvent
    {
        std::chrono::steady_clock::time_point releaseAt = {};
        NetTransport::Event event;
    };

    struct PendingKickDisconnect
    {
        NetTransport::PeerHandle peer = NetTransport::kInvalidPeerHandle;
        ParticipantId participantId = kInvalidParticipantId;
        std::chrono::steady_clock::time_point disconnectAt = {};
    };

    struct PendingClockSyncRequest
    {
        uint32_t sequence = 0;
        int64_t clientSendMicros = 0;
        std::chrono::steady_clock::time_point sentAt = {};
    };

    enum class LocalTeardownMode : uint8_t
    {
        Graceful,
        Immediate,
        Unload
    };

    NetTransport m_transport;
    NetSession m_session;
    InputTimeline m_localInputs;
    InputTimeline m_remoteInputs;
    std::list<ConfirmedFrameInputs> m_confirmedFrames;
    std::unordered_map<FrameNumber, std::list<ConfirmedFrameInputs>::iterator> m_confirmedFrameIndex;
    std::vector<std::string> m_eventLog;
    std::vector<std::string> m_loggedAdvertisedIceServers;
    NetTransport::PeerHandle m_serverPeer = NetTransport::kInvalidPeerHandle;
    ParticipantId m_localParticipantId = kInvalidParticipantId;
    std::string m_localDisplayName;
    uint64_t m_localReconnectToken = 0;
    bool m_pendingJoinRomLoaded = false;
    RomValidationData m_pendingJoinRomValidation = {};
    std::string m_localEmulatorVersion;
    bool m_disconnectExpectedAfterJoinReject = false;
    bool m_disconnectExpectedAfterHostShutdown = false;
    bool m_debugMode = false;
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
    std::optional<PendingHostResyncRequest> m_pendingHostResyncFrame;
    std::chrono::steady_clock::time_point m_lastHostResyncRequestSentAt = {};
    ResyncReason m_lastHostResyncRequestSentReason = ResyncReason::Unspecified;
    uint16_t m_lastHostResyncRequestSentSource = 0;
    FrameNumber m_lastBroadcastConfirmedFrame = 0;
    uint8_t m_lastBroadcastInputDelayFrames = 0;
    DesyncMonitor m_desyncMonitor;
    FrameNumber m_localSimulationFrame = 0;
    uint32_t m_nextResyncId = 1;
    uint32_t m_activeResyncExpectedStateCrc32 = 0;
    std::optional<IncomingResyncTransfer> m_incomingResync;
    std::optional<PendingResyncApply> m_pendingResyncApply;
    std::optional<ParticipantId> m_pendingHostLateJoinResyncParticipant;
    RemoteInputStallMonitor m_remoteInputStallMonitor;
    std::vector<ParticipantId> m_pendingResyncAcks;
    std::vector<ParticipantId> m_pendingSequenceResetParticipants;
    std::chrono::steady_clock::time_point m_lastPeerHealthBroadcast = {};
    std::unordered_map<ParticipantId, std::chrono::steady_clock::time_point> m_reconnectReservationDeadlines;
    std::unordered_map<ParticipantId, std::string> m_participantDisplayNameCache;
    std::string m_lastJoinHostName;
    uint16_t m_lastJoinPort = 0;
    bool m_reconnectPending = false;
    bool m_reconnectAttemptInFlight = false;
    bool m_reconnectFailureToastShown = false;
    bool m_awaitingReconnectInitialSync = false;
    bool m_suppressReconnectPresenceToasts = false;
    uint16_t m_reconnectSecondsRemaining = 0;
    std::chrono::steady_clock::time_point m_nextReconnectAttempt = {};
    std::chrono::steady_clock::time_point m_reconnectDeadline = {};
    uint32_t m_gameplayReceiveDelayMs = 0;
    std::chrono::milliseconds m_remoteInputSuspendTimeout = std::chrono::milliseconds(1000);
    std::unordered_map<ParticipantId, std::chrono::steady_clock::time_point> m_lastRemoteInputAt;
    std::unordered_map<ParticipantId, std::chrono::steady_clock::time_point> m_lastPeerHealthAt;
    std::deque<DelayedPacketEvent> m_delayedPacketEvents;
    std::vector<PendingKickDisconnect> m_pendingKickDisconnects;
    std::chrono::steady_clock::time_point m_activeResyncAckDeadline = {};
    std::unordered_map<uint16_t, uint32_t> m_dropIncomingMessageCounts;
    std::chrono::seconds m_reconnectReservationDuration = std::chrono::seconds(300);
    ParticipantId m_activeResyncTargetParticipantId = kInvalidParticipantId;
    SessionState m_activeResyncResumeState = SessionState::Lobby;
    uint32_t m_activeTargetedResyncId = 0;
    FrameNumber m_activeTargetedResyncFrame = 0;
    uint32_t m_activeTargetedResyncExpectedStateCrc32 = 0;
    std::chrono::steady_clock::time_point m_lastClockSyncRequestAt = {};
    uint32_t m_nextClockSyncSequence = 1;
    std::unordered_map<uint32_t, PendingClockSyncRequest> m_pendingClockSyncRequests;
    int64_t m_sharedClockOffsetMicros = 0;
    uint64_t m_sharedClockRttMicros = 0;
    uint64_t m_bestClockSyncDelayMicros = 0;
    bool m_sharedClockSynchronized = false;
    std::unordered_map<FrameNumber, uint64_t> m_authoritativeFrameStartClockMicros;
    std::deque<FrameNumber> m_authoritativeFrameStartClockOrder;
    mutable PerformanceDiagnostics m_performanceDiagnostics;

    static std::string defaultDisplayName();
    static uint32_t generateSessionId();
    static uint64_t generateReconnectToken();
    static std::string messageTypeLabel(MessageType type);

    void resetSessionState();
    void queuePendingHostResync(FrameNumber frame, ResyncReason reason, ParticipantId participantId = kInvalidParticipantId);
    void pushLog(const std::string& message);
    void pushToast(const std::string& message);
    ParticipantInfo& ensureParticipant(ParticipantId id, const std::string& displayName);
    void rememberParticipantDisplayName(const ParticipantInfo& participant);
    std::string participantLabelForDisconnect(ParticipantId participantId) const;
    ParticipantId participantIdFromPeer(NetTransport::PeerHandle peer) const;
    NetTransport::PeerHandle peerFromParticipantId(ParticipantId participantId) const;
    ParticipantInfo* findParticipantByReconnectToken(uint64_t reconnectToken);
    void removeParticipant(ParticipantId participantId);
    bool activeResyncIsTargeted() const;
    bool sendCurrentSessionStateToPeer(NetTransport::PeerHandle peer);
    bool sendConfirmedFramesToPeer(NetTransport::PeerHandle peer, FrameNumber startFrame);
    void clearActiveResyncTracking(SessionState resumeState);
    void clearTargetedResyncTracking();
    void finalizeActiveResyncIfReady();
    void cancelTargetedResync(const std::string& reason);
    bool assignmentMutationBlocked(std::string* reason = nullptr) const;
    void clearReconnectAttemptState();
    void finalizeLocalTeardown(LocalTeardownMode mode);
    void completeLocalDisconnect(bool shutdownTransport = true);
    void processPendingKickDisconnects();
    void clearPendingKickDisconnect(NetTransport::PeerHandle peer);
    void scheduleReconnectAttempt();
    void notifyReconnectFailure(const std::string& message);
    void processPendingReconnect();
    std::vector<uint8_t> buildJoinRoomPacket() const;
    std::vector<uint8_t> buildJoinRejectedPacket(JoinRejectReason reason,
                                                 const std::string& gameName,
                                                 const RomValidationData& romValidation,
                                                 const std::string& expectedEmulatorVersion = {}) const;
    std::vector<uint8_t> buildParticipantJoinedPacket(const ParticipantInfo& participant, uint64_t reconnectToken) const;
    std::vector<uint8_t> buildSelectRomPacket(const std::string& gameName, const RomValidationData& romValidation) const;
    std::vector<uint8_t> buildRomValidationResultPacket(const RomValidationResultData& result) const;
    std::vector<uint8_t> buildParticipantLeftPacket(ParticipantId participantId, uint32_t disconnectReason = 0) const;
    std::vector<uint8_t> buildLeaveRoomPacket(ParticipantId participantId) const;
    std::vector<uint8_t> buildResyncBeginPacket(const ResyncBeginData& data) const;
    std::vector<uint8_t> buildResyncChunkPacket(const ResyncChunkData& data, std::span<const uint8_t> payloadChunk) const;
    std::vector<uint8_t> buildResyncCompletePacket(const ResyncCompleteData& data) const;
    std::vector<uint8_t> buildResyncAckPacket(const ResyncAckData& data) const;
    std::vector<uint8_t> buildResyncAbortPacket(const ResyncAbortData& data) const;
    std::vector<uint8_t> buildResyncRequestPacket(const ResyncRequestData& data) const;
    std::vector<uint8_t> buildClockSyncRequestPacket(const ClockSyncRequestData& data) const;
    std::vector<uint8_t> buildClockSyncResponsePacket(const ClockSyncResponseData& data) const;
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
    bool handleResyncRequest(NetTransport::PeerHandle peer, PacketReader& reader);
    bool handleClockSyncRequest(NetTransport::PeerHandle peer, PacketReader& reader);
    bool handleClockSyncResponse(NetTransport::PeerHandle peer, PacketReader& reader);
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
                                   uint32_t inputSequenceBase = 0,
                                   bool preserveConfirmedInputs = true);
    static bool preserveConfirmedInputsAcrossRealignment(ResyncReason reason);
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
    void synthesizeSuspendedRemoteInputsUpTo(FrameNumber targetFrame);
    bool synthesizePredictionLimitFallbackInput(FrameNumber targetFrame, ParticipantInfo& participant, PlayerSlot slot);
    bool tryBuildPlaybackFrameInternal(FrameNumber frame,
                                       bool allowPrediction,
                                       bool allowHostFallback,
                                       ConfirmedFrameInputs& outFrame);
    // Frame terminology used by the coordinator:
    // - local simulation frame: last frame this peer has actually simulated.
    // - host input-confirmed frame: highest frame for which the host has
    //   contiguous authoritative inputs from every assigned participant in the
    //   active epoch.
    // - published confirmed frame: highest confirmed frame bundle already
    //   stored/broadcast by this coordinator.
    // - room last confirmed frame: room-wide advertised confirmed checkpoint.
    // - resync target frame: authoritative frame a hard resync is loading.
    // Host-only: highest frame for which every assigned participant has a
    // contiguous confirmed input contribution available in the current epoch.
    FrameNumber computeHostInputConfirmedFrame() const;
    FrameNumber computeHostConfirmedFrame() const;
    void broadcastFrameStatusIfNeeded();
    void broadcastPeerHealthIfNeeded();
    void processClockSyncIfNeeded(const std::chrono::steady_clock::time_point& now);
    static int64_t monotonicNowMicros();
    bool allRequiredParticipantsRomCompatible() const;
    void refreshHostRoomState();
    void updatePeerHealthFromTransport();
    void updateReconnectReservations();
    void resetRuntimeTimelineStateForSessionStart();
    void discardTimelineStateAfter(FrameNumber frame);
    void seedNeutralInputBaseline(ParticipantId participantId, PlayerSlot slot, FrameNumber frame);
    void scheduleResyncRetry(FrameNumber targetFrame, const std::string& reason);
    bool ejectParticipantForResyncFailure(ParticipantId participantId, const std::string& reason);
    static const char* recoveryInputModeLabel(RecoveryInputMode mode);
    void setRecoveryInputMode(RecoveryInputMode mode,
                              const char* reason,
                              FrameNumber frameContext,
                              uint32_t stabilizationFrames = 0);
    void noteDroppedGameplayInputDuringRecovery(const char* source,
                                                FrameNumber frame,
                                                ParticipantId participantId,
                                                PlayerSlot slot);
    void advanceRecoveryStabilization(FrameNumber observedFrame);

public:
    NetplayCoordinator();
    static bool romValidationMatches(const RomValidationData& a, const RomValidationData& b);
    static std::string resyncReasonToast(ResyncReason reason);
    uint32_t unresolvedPredictedRemoteFrameCount() const;
    FrameNumber latestPredictedRemoteFrame() const;
    void setRoomInputTopology(std::vector<InputSlotDescriptor> inputTopology,
                              std::optional<ParticipantId> preservedParticipantId = std::nullopt,
                              PlayerSlot preservedAssignment = kObserverPlayerSlot);

    bool host(uint16_t port, size_t maxPeers, const std::string& displayName);
    bool join(const std::string& hostName, uint16_t port, const std::string& displayName);
    void disconnect();
    void disconnectImmediately();
    void shutdownForUnload();
    void update(uint32_t timeoutMs = 0);
    bool setTransportBackend(NetTransportBackend backend);
    void setTransportOptions(const NetTransportOptions& options);
    void setDebugMode(bool enabled);
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
    void setRemoteInputSuspendTimeoutForTests(uint32_t timeoutMs);
    void setLocalEmulatorVersionForTests(const std::string& version);
    void simulateTransportFailureForTests();
    bool injectInputFrameForTests(const InputFrameData& input, const NetplayInputFrame& contribution);
    bool injectConfirmedInputFramesForTests(const ConfirmedInputFramesData& data);
    bool injectConfirmedPlaybackFramesForTests(const ConfirmedInputFramesData& data,
                                               const std::vector<ConfirmedFrameInputs>& frames);
    bool injectInputAckForTests(const InputAckData& ack);
    bool injectFrameStatusForTests(const FrameStatusData& status);
    bool injectCrcReportForTests(const CrcReportData& report);
    bool injectResyncAckForTests(const ResyncAckData& ack);
    ParticipantId localParticipantId() const;
    const std::string& localDisplayName() const;
    uint64_t localReconnectToken() const;
    void setLocalReconnectToken(uint64_t token);
    const std::string& lastError() const;
    const NetSession& session() const;
    const std::vector<std::string>& eventLog() const;
    PerformanceDiagnostics performanceDiagnostics() const;
    void appendNetplayLog(const std::string& message);
    const RollbackStats& predictionStats() const;
    void recordPlaybackStop(FrameNumber frame, bool predictionLimitReached);
    void recordLocalAuthoritativeFrameStart(FrameNumber frame);
    void setLocalSimulationFrame(FrameNumber frame);
    void rescheduleRollbackFrame(FrameNumber frame);
    std::optional<FrameNumber> consumePendingRollbackFrame();
    void discardTimelineAfter(FrameNumber frame, bool preserveLocalInputs = false);
    std::optional<PendingHostResyncRequest> consumePendingHostResyncFrame();
    std::optional<ParticipantId> consumePendingHostLateJoinResyncParticipant();
    const InputTimeline& localInputs() const;
    const InputTimeline& remoteInputs() const;
    FrameNumber localSimulationFrame() const;
    const ConfirmedFrameInputs* findConfirmedFrame(FrameNumber frame) const;
    // Highest confirmed frame bundle already published/stored locally.
    FrameNumber latestPublishedConfirmedFrame() const;
    FrameNumber latestConfirmedFrame() const;
    FrameNumber hostConfirmedFrame() const;
    uint64_t sharedClockNowMicros() const;
    uint64_t authoritativeFrameStartClockMicros(FrameNumber frame) const;
    FrameNumber authoritativeResyncTargetFrame() const;
    uint8_t predictFrames() const;
    void recordLocalInputFrame(FrameNumber frame, PlayerSlot slot, const NetplayInputFrame& contribution);
    void recordLocalInputFrame(FrameNumber frame, PlayerSlot slot, uint64_t buttonMaskLo, uint64_t buttonMaskHi = 0);
    void predictRemoteInputsForFrame(FrameNumber frame);
    bool tryBuildPlaybackFrame(FrameNumber frame,
                               bool allowPrediction,
                               ConfirmedFrameInputs& outFrame,
                               bool allowHostFallback = true);
    void submitLocalCrc(FrameNumber frame, uint32_t crc32);
    void invalidateLocalCrcHistoryAfter(FrameNumber frame);
    bool beginResync(FrameNumber targetFrame,
                     const std::vector<uint8_t>& payload,
                     uint32_t payloadCrc32,
                     uint32_t stateCrc32,
                     ResyncReason reason = ResyncReason::Unspecified,
                     ParticipantId targetParticipantId = kInvalidParticipantId);
    std::optional<PendingResyncApply> consumePendingResyncApply();
    bool acknowledgeResync(uint32_t resyncId, FrameNumber loadedFrame, uint32_t crc32, bool success);
    bool requestHostResync(ResyncReason reason = ResyncReason::ObserverVisibilityRestore);
    bool requestHostResync(const ResyncRequestData& request);
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

} // namespace ConsoleNetplay
