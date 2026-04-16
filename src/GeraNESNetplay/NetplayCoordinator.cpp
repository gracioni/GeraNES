#include "GeraNESNetplay/NetplayCoordinator.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <sstream>

#ifdef ERROR
#undef ERROR
#endif

#include "GeraNESNetplay/NetplayInputAssignment.h"
#include "GeraNES/defines.h"
#include "GeraNES/util/Crc32.h"
#include "GeraNES/Serialization.h"
#include "logger/logger.h"

namespace {

constexpr size_t kResyncChunkPayloadBytes = 1024;
constexpr size_t kMaxIncomingResyncPayloadBytes = 64u * 1024u * 1024u;
constexpr size_t kConfirmedFrameHistoryCapacity = 16384;
constexpr uint16_t kMaxConfirmedFramesPerPacket = 64;
constexpr auto kReconnectRetryDelay = std::chrono::milliseconds(750);
constexpr auto kGracefulDisconnectTimeout = std::chrono::milliseconds(750);
constexpr auto kIncomingResyncTimeout = std::chrono::milliseconds(750);
constexpr auto kResyncAckTimeout = std::chrono::seconds(5);
constexpr auto kKickDisconnectGrace = std::chrono::milliseconds(250);
constexpr uint32_t kDisconnectReasonKicked = 1u;
constexpr uint32_t kRecoveryStabilizationFrames = 2;
constexpr uint32_t kRecoveryStabilizationFailTimeoutFrames = 120;

std::string participantLabel(const Netplay::ParticipantInfo& participant)
{
    if(!participant.displayName.empty()) {
        return participant.displayName;
    }
    return std::to_string(static_cast<int>(participant.id));
}

std::string controllerAssignmentToast(Netplay::PlayerSlot slot, const std::string& participantName)
{
    return Netplay::inputAssignmentLabel(slot, Netplay::RoomState{}) + " assigned to " + participantName;
}

bool hasReconnectTarget(Netplay::NetTransportBackend backend,
                        const Netplay::NetTransportOptions& options,
                        const std::string& hostName,
                        uint16_t port)
{
    if(backend == Netplay::NetTransportBackend::WebRTC) {
        return options.webRtcSignaling.has_value() && options.webRtcSignaling->valid();
    }
    return !hostName.empty() && port != 0;
}

std::string describeConnectTarget(Netplay::NetTransportBackend backend,
                                  const Netplay::NetTransportOptions& options,
                                  const std::string& hostName,
                                  uint16_t port)
{
    if(backend == Netplay::NetTransportBackend::WebRTC) {
        if(options.webRtcSignaling.has_value() && options.webRtcSignaling->valid()) {
            return "room " + options.webRtcSignaling->roomId + " via " + options.webRtcSignaling->url;
        }
        return "configured WebRTC room";
    }
    return hostName + ":" + std::to_string(port);
}

std::string describeHostTarget(Netplay::NetTransportBackend backend,
                               const Netplay::NetTransportOptions& options,
                               uint16_t port,
                               size_t maxPeers)
{
    std::ostringstream oss;
    if(backend == Netplay::NetTransportBackend::WebRTC) {
        if(options.webRtcSignaling.has_value() && options.webRtcSignaling->valid()) {
            oss << "Hosting WebRTC room " << options.webRtcSignaling->roomId
                << " via " << options.webRtcSignaling->url;
        } else {
            oss << "Hosting WebRTC room";
        }
    } else {
        oss << "Hosting on port " << port;
    }
    oss << " for up to " << maxPeers << " participants";
    return oss.str();
}

Netplay::InputTopologyData makeTopologyData(const Netplay::RoomState& room)
{
    Netplay::InputTopologyData data;
    data.port1Device = room.port1Device.value_or(Settings::Device::NONE);
    data.port2Device = room.port2Device.value_or(Settings::Device::NONE);
    data.expansionDevice = room.expansionDevice;
    data.nesMultitapDevice = room.nesMultitapDevice;
    data.famicomMultitapDevice = room.famicomMultitapDevice;
    return data;
}

void applyTopologyData(Netplay::RoomState& room, const Netplay::InputTopologyData& data)
{
    room.port1Device = data.port1Device;
    room.port2Device = data.port2Device;
    room.expansionDevice = data.expansionDevice;
    room.nesMultitapDevice = data.nesMultitapDevice;
    room.famicomMultitapDevice = data.famicomMultitapDevice;
}

std::vector<uint8_t> serializeInputFrame(const InputFrame& inputFrame)
{
    Serialize serializer;
    InputFrame copy = inputFrame;
    copy.serialization(serializer);
    return serializer.getData();
}


bool deserializeInputFrame(const uint8_t* data, size_t size, InputFrame& inputFrame)
{
    Deserialize deserializer;
    deserializer.setData(data, size);
    inputFrame = {};
    inputFrame.serialization(deserializer);
    return !deserializer.error();
}

bool inputFramesEqual(const InputFrame& a, const InputFrame& b)
{
    return serializeInputFrame(a) == serializeInputFrame(b);
}

bool isPlayableSlot(Netplay::PlayerSlot slot)
{
    return slot <= Netplay::kMaxAssignedPlayerSlot;
}

std::string controllerAssignmentToast(Netplay::PlayerSlot slot,
                                      const Netplay::RoomState& room,
                                      const std::string& participantName)
{
    return Netplay::inputAssignmentLabel(slot, room) + " assigned to " + participantName;
}

Netplay::AssignControllerData makeAssignControllerData(const Netplay::ParticipantInfo& participant)
{
    Netplay::AssignControllerData data;
    data.participantId = participant.id;
    const size_t count = std::min(participant.controllerAssignments.size(), data.controllerAssignments.size());
    data.assignmentCount = static_cast<uint8_t>(count);
    for(size_t i = 0; i < count; ++i) {
        data.controllerAssignments[i] = participant.controllerAssignments[i];
    }
    return data;
}

}

namespace Netplay {

NetplayCoordinator::NetplayCoordinator()
    : m_localDisplayName(defaultDisplayName())
    , m_localEmulatorVersion(GERANES_VERSION)
{
    m_localInputs.configure(2400);
    m_remoteInputs.configure(2400);
}

std::string NetplayCoordinator::defaultDisplayName()
{
    return "Participant";
}

uint32_t NetplayCoordinator::generateSessionId()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto value = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    return static_cast<uint32_t>(value ^ (value >> 32));
}

uint64_t NetplayCoordinator::generateReconnectToken()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    const uint64_t mixed = static_cast<uint64_t>(value) ^ (static_cast<uint64_t>(value) << 13) ^ 0x9E3779B97F4A7C15ull;
    return mixed != 0 ? mixed : 1ull;
}

int64_t NetplayCoordinator::monotonicNowMicros()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

uint64_t NetplayCoordinator::sharedClockNowMicros() const
{
    const int64_t nowMicros = monotonicNowMicros();
    if(m_hosting) {
        return nowMicros > 0 ? static_cast<uint64_t>(nowMicros) : 0u;
    }
    if(!m_sharedClockSynchronized) {
        return 0u;
    }
    const int64_t adjusted = nowMicros + m_sharedClockOffsetMicros;
    return adjusted > 0 ? static_cast<uint64_t>(adjusted) : 0u;
}

uint64_t NetplayCoordinator::authoritativeFrameStartClockMicros(FrameNumber frame) const
{
    const auto it = m_authoritativeFrameStartClockMicros.find(frame);
    if(it != m_authoritativeFrameStartClockMicros.end()) {
        return it->second;
    }
    return 0u;
}

static const char* transportBackendLabel(NetTransportBackend backend)
{
    return netTransportBackendLabel(backend);
}

std::string NetplayCoordinator::messageTypeLabel(MessageType type)
{
    switch(type) {
        case MessageType::CreateRoom: return "CreateRoom";
        case MessageType::JoinRoom: return "JoinRoom";
        case MessageType::JoinRejected: return "JoinRejected";
        case MessageType::ParticipantJoined: return "ParticipantJoined";
        case MessageType::ParticipantLeft: return "ParticipantLeft";
        case MessageType::LeaveRoom: return "LeaveRoom";
        case MessageType::ChatMessage: return "ChatMessage";
        case MessageType::AssignController: return "AssignController";
        case MessageType::SelectRom: return "SelectRom";
        case MessageType::RomValidationResult: return "RomValidationResult";
        case MessageType::StartSession: return "StartSession";
        case MessageType::PauseSession: return "PauseSession";
        case MessageType::ResumeSession: return "ResumeSession";
        case MessageType::EndSession: return "EndSession";
        case MessageType::InputFrame: return "InputFrame";
        case MessageType::ConfirmedInputFrames: return "ConfirmedInputFrames";
        case MessageType::InputAck: return "InputAck";
        case MessageType::FrameStatus: return "FrameStatus";
        case MessageType::PeerHealth: return "PeerHealth";
        case MessageType::CrcReport: return "CrcReport";
        case MessageType::ClockSyncRequest: return "ClockSyncRequest";
        case MessageType::ClockSyncResponse: return "ClockSyncResponse";
        case MessageType::ResyncBegin: return "ResyncBegin";
        case MessageType::ResyncChunk: return "ResyncChunk";
        case MessageType::ResyncComplete: return "ResyncComplete";
        case MessageType::ResyncAck: return "ResyncAck";
        case MessageType::ResyncAbort: return "ResyncAbort";
        case MessageType::ResyncRequest: return "ResyncRequest";
        default: return "Unknown";
    }
}

std::string NetplayCoordinator::resyncReasonToast(ResyncReason reason)
{
    switch(reason) {
        case ResyncReason::HostReset: return "Owner reset the game";
        case ResyncReason::HostLoadedState: return "Owner loaded state";
        default: return {};
    }
}

static const char* resyncReasonLabel(ResyncReason reason)
{
    switch(reason) {
        case ResyncReason::Unspecified: return "Unspecified";
        case ResyncReason::InitialSessionSync: return "InitialSessionSync";
        case ResyncReason::ConfirmedDesync: return "ConfirmedDesync";
        case ResyncReason::AssignmentChanged: return "AssignmentChanged";
        case ResyncReason::ManualForce: return "ManualForce";
        case ResyncReason::HostReset: return "HostReset";
        case ResyncReason::HostLoadedState: return "HostLoadedState";
        case ResyncReason::ObserverVisibilityRestore: return "ObserverVisibilityRestore";
        default: return "Unknown";
    }
}

void NetplayCoordinator::resetSessionState()
{
    m_serverPeer = NetTransport::kInvalidPeerHandle;
    m_localParticipantId = kInvalidParticipantId;
    m_localReconnectToken = 0;
    m_pendingJoinRomLoaded = false;
    m_pendingJoinRomValidation = {};
    m_disconnectExpectedAfterJoinReject = false;
    m_disconnectExpectedAfterHostShutdown = false;
    m_gracefulDisconnectPending = false;
    m_gracefulDisconnectDeadline = {};
    m_session.reset();
    m_localInputs.clear();
    m_remoteInputs.clear();
    m_confirmedFrames.clear();
    m_loggedAdvertisedIceServers.clear();
    m_lastError.clear();
    m_hosting = false;
    m_connected = false;
    m_nextAssignedParticipantId = 1;
    m_localInputSequence = 0;
    m_predictionStats = {};
    m_pendingRollbackFrame.reset();
    m_pendingHostResyncFrame.reset();
    m_lastBroadcastConfirmedFrame = 0;
    m_lastBroadcastInputDelayFrames = 0;
    m_desyncMonitor.reset();
    m_localSimulationFrame = 0;
    m_nextResyncId = 1;
    m_incomingResync.reset();
    m_pendingResyncApply.reset();
    m_pendingHostLateJoinResyncParticipant.reset();
    m_implicitRecoveryMonitor.reset();
    m_pendingResyncAcks.clear();
    m_activeResyncTargetParticipantId = kInvalidParticipantId;
    m_activeResyncResumeState = SessionState::Lobby;
    m_activeTargetedResyncId = 0;
    m_activeTargetedResyncFrame = 0;
    m_activeTargetedResyncExpectedStateCrc32 = 0;
    m_reconnectReservationDeadlines.clear();
    m_lastRemoteInputAt.clear();
    m_lastPeerHealthAt.clear();
    m_lastTransportError.clear();
    clearReconnectAttemptState();
    m_delayedPacketEvents.clear();
    m_pendingKickDisconnects.clear();
    m_activeResyncAckDeadline = {};
    m_lastClockSyncRequestAt = {};
    m_nextClockSyncSequence = 1;
    m_pendingClockSyncRequests.clear();
    m_sharedClockOffsetMicros = 0;
    m_sharedClockRttMicros = 0;
    m_bestClockSyncDelayMicros = std::numeric_limits<uint64_t>::max();
    m_sharedClockSynchronized = false;
    m_authoritativeFrameStartClockMicros.clear();
    m_authoritativeFrameStartClockOrder.clear();
    m_session.roomState().sharedClockMicros = 0;
    m_session.roomState().sharedClockRttMicros = 0;
    m_session.roomState().sharedClockOffsetMicros = 0;
    m_session.roomState().sharedClockSynchronized = false;
    m_session.roomState().lastAuthoritativeClockFrame = 0;
    m_session.roomState().lastAuthoritativeClockMicros = 0;
}

void NetplayCoordinator::queuePendingHostResync(FrameNumber frame, ResyncReason reason, ParticipantId participantId)
{
    if(!m_hosting) return;

    if(!m_pendingHostResyncFrame.has_value() || frame < m_pendingHostResyncFrame->frame) {
        m_pendingHostResyncFrame = PendingHostResyncRequest{frame, reason, participantId};
        return;
    }

    if(frame == m_pendingHostResyncFrame->frame &&
       m_pendingHostResyncFrame->reason == ResyncReason::Unspecified) {
        m_pendingHostResyncFrame->reason = reason;
    }
    if(frame == m_pendingHostResyncFrame->frame &&
       m_pendingHostResyncFrame->participantId == kInvalidParticipantId) {
        m_pendingHostResyncFrame->participantId = participantId;
    }
}

void NetplayCoordinator::pushLog(const std::string& message)
{
    m_eventLog.push_back(message);

    constexpr size_t MAX_LOG_LINES = 256;
    if(m_eventLog.size() > MAX_LOG_LINES) {
        m_eventLog.erase(m_eventLog.begin(), m_eventLog.begin() + (m_eventLog.size() - MAX_LOG_LINES));
    }
}

void NetplayCoordinator::pushToast(const std::string& message)
{
    if(message.empty()) {
        return;
    }
    pushLog(message);
    Logger::instance().log(message, Logger::Type::USER);
}

ParticipantInfo& NetplayCoordinator::ensureParticipant(ParticipantId id, const std::string& displayName)
{
    if(ParticipantInfo* existing = m_session.findParticipant(id)) {
        if(!displayName.empty()) existing->displayName = displayName;
        return *existing;
    }

    ParticipantInfo participant;
    participant.id = id;
    participant.displayName = displayName;
    participant.inputSuspended = false;
    participant.inputResumeAwaitingResync = false;
    m_session.roomState().participants.push_back(participant);
    m_lastRemoteInputAt[id] = std::chrono::steady_clock::now();
    return m_session.roomState().participants.back();
}

ParticipantInfo* NetplayCoordinator::findParticipantByReconnectToken(uint64_t reconnectToken)
{
    if(reconnectToken == 0) return nullptr;
    for(auto& participant : m_session.roomState().participants) {
        if(participant.reconnectToken == reconnectToken) {
            return &participant;
        }
    }
    return nullptr;
}

ParticipantId NetplayCoordinator::participantIdFromPeer(NetTransport::PeerHandle peer) const
{
    const uintptr_t stored = m_transport.peerTag(peer);
    if(stored == 0) return kInvalidParticipantId;
    return static_cast<ParticipantId>(stored - 1u);
}

NetTransport::PeerHandle NetplayCoordinator::peerFromParticipantId(ParticipantId participantId) const
{
    if(participantId == kInvalidParticipantId) return NetTransport::kInvalidPeerHandle;

    for(const NetTransport::PeerHandle peer : m_transport.connectedPeers()) {
        if(participantIdFromPeer(peer) == participantId) {
            return peer;
        }
    }

    return NetTransport::kInvalidPeerHandle;
}

void NetplayCoordinator::removeParticipant(ParticipantId participantId)
{
    auto& participants = m_session.roomState().participants;
    participants.erase(
        std::remove_if(participants.begin(), participants.end(), [participantId](const ParticipantInfo& participant) {
            return participant.id == participantId;
        }),
        participants.end()
    );
    m_reconnectReservationDeadlines.erase(participantId);
    m_lastRemoteInputAt.erase(participantId);
    m_lastPeerHealthAt.erase(participantId);
    m_pendingResyncAcks.erase(
        std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), participantId),
        m_pendingResyncAcks.end()
    );
    m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
    if(!m_pendingResyncAcks.empty()) {
        m_activeResyncAckDeadline = std::chrono::steady_clock::now() + kResyncAckTimeout;
    }
    if(m_implicitRecoveryMonitor.pending().has_value() &&
       m_implicitRecoveryMonitor.pending()->participantId == participantId) {
        m_implicitRecoveryMonitor.reset();
    }

    if(m_activeResyncTargetParticipantId == participantId) {
        cancelTargetedResync(
            "Targeted resync participant left before completion: " + std::to_string(static_cast<int>(participantId))
        );
    } else {
        finalizeActiveResyncIfReady();
    }
}

bool NetplayCoordinator::activeResyncIsTargeted() const
{
    return m_activeTargetedResyncId != 0 && m_activeResyncTargetParticipantId != kInvalidParticipantId;
}

bool NetplayCoordinator::sendCurrentSessionStateToPeer(NetTransport::PeerHandle peer)
{
    if(peer == NetTransport::kInvalidPeerHandle) return false;

    const SessionState state = m_session.roomState().state;
    if(state == SessionState::Paused) {
        PacketWriter writer;
        PacketHeader header;
        header.type = MessageType::PauseSession;
        header.sessionId = m_session.roomState().sessionId;
        writer.writePod(header);
        StartSessionData startData;
        startData.state = SessionState::Paused;
        startData.inputDelayFrames = m_session.roomState().inputDelayFrames;
        startData.predictFrames = m_session.roomState().predictFrames;
        startData.topology = makeTopologyData(m_session.roomState());
        writer.writePod(startData);
        return m_transport.sendReliable(peer, Channel::Control, writer.data());
    }

    if(state == SessionState::Running) {
        PacketWriter writer;
        PacketHeader header;
        header.type = MessageType::ResumeSession;
        header.sessionId = m_session.roomState().sessionId;
        writer.writePod(header);
        StartSessionData startData;
        startData.state = SessionState::Running;
        startData.inputDelayFrames = m_session.roomState().inputDelayFrames;
        startData.predictFrames = m_session.roomState().predictFrames;
        startData.topology = makeTopologyData(m_session.roomState());
        writer.writePod(startData);
        return m_transport.sendReliable(peer, Channel::Control, writer.data());
    }

    return true;
}

bool NetplayCoordinator::sendConfirmedFramesToPeer(NetTransport::PeerHandle peer, FrameNumber startFrame)
{
    if(peer == NetTransport::kInvalidPeerHandle) return false;

    const auto buildPacket = [this](const std::vector<ConfirmedFrameInputs>& frames) {
        PacketWriter writer;
        PacketHeader header;
        header.type = MessageType::ConfirmedInputFrames;
        header.sessionId = m_session.roomState().sessionId;
        writer.writePod(header);

        ConfirmedInputFramesData data;
        data.timelineEpoch = m_session.roomState().timelineEpoch;
        data.startFrame = frames.front().frame;
        data.frameCount = static_cast<uint16_t>(frames.size());
        writer.writePod(data);

        for(const auto& frame : frames) {
            ConfirmedInputFrameEntry entry;
            entry.authoritativeFrameStartClockMicros = frame.authoritativeFrameStartClockMicros;
            entry.buttonMaskLo = frame.buttonMaskLo;
            entry.buttonMaskHi = frame.buttonMaskHi;
            const std::vector<uint8_t> payload = serializeInputFrame(frame.inputFrame);
            entry.payloadSize = static_cast<uint16_t>(payload.size());
            writer.writePod(entry);
            writer.writeBytes(std::span<const uint8_t>(payload.data(), payload.size()));
        }

        return writer.data();
    };

    std::vector<ConfirmedFrameInputs> chunk;
    chunk.reserve(kMaxConfirmedFramesPerPacket);
    FrameNumber frame = startFrame;
    const FrameNumber latestFrame = latestPublishedConfirmedFrame();
    while(frame <= latestFrame) {
        const ConfirmedFrameInputs* confirmed = findConfirmedFrame(frame);
        if(confirmed == nullptr) {
            break;
        }

        if(chunk.empty()) {
            chunk.push_back(*confirmed);
        } else if(chunk.size() < kMaxConfirmedFramesPerPacket &&
                  confirmed->frame == chunk.back().frame + 1u) {
            chunk.push_back(*confirmed);
        } else {
            if(!m_transport.sendReliable(peer, Channel::Gameplay, buildPacket(chunk))) {
                return false;
            }
            chunk.clear();
            chunk.push_back(*confirmed);
        }

        ++frame;
    }

    if(!chunk.empty()) {
        return m_transport.sendReliable(peer, Channel::Gameplay, buildPacket(chunk));
    }

    return true;
}

void NetplayCoordinator::clearActiveResyncTracking(SessionState resumeState)
{
    m_session.roomState().state = resumeState;
    m_session.roomState().pendingResyncAckCount = 0;
    m_session.roomState().activeResyncId = 0;
    m_session.roomState().resyncTargetFrame = 0;
    m_session.roomState().resyncConfirmedFrame = 0;
    m_session.roomState().resyncFrameReadyFrame = 0;
    m_session.roomState().resyncPayloadSize = 0;
    m_session.roomState().resyncPayloadCrc32 = 0;
    m_session.roomState().resyncFrameReadyCrc32 = 0;
    m_session.roomState().resyncInputSequenceBase = 0;
    m_session.roomState().activeResyncReason = ResyncReason::Unspecified;
    m_activeResyncExpectedStateCrc32 = 0;
    m_pendingResyncAcks.clear();
    m_activeResyncTargetParticipantId = kInvalidParticipantId;
    m_activeResyncResumeState = resumeState;
    m_activeTargetedResyncId = 0;
    m_activeTargetedResyncFrame = 0;
    m_activeTargetedResyncExpectedStateCrc32 = 0;
    m_activeResyncAckDeadline = {};
}

void NetplayCoordinator::clearTargetedResyncTracking()
{
    m_pendingResyncAcks.clear();
    m_activeResyncTargetParticipantId = kInvalidParticipantId;
    m_activeResyncResumeState = SessionState::Lobby;
    m_activeTargetedResyncId = 0;
    m_activeTargetedResyncFrame = 0;
    m_activeTargetedResyncExpectedStateCrc32 = 0;
    m_activeResyncAckDeadline = {};
}

void NetplayCoordinator::finalizeActiveResyncIfReady()
{
    if(!m_hosting || !m_pendingResyncAcks.empty()) return;
    if(!activeResyncIsTargeted() && m_session.roomState().activeResyncId == 0) return;

    const bool targeted = activeResyncIsTargeted();
    const ParticipantId targetParticipantId = m_activeResyncTargetParticipantId;
    const SessionState resumeState = targeted ? m_activeResyncResumeState : SessionState::Running;
    const FrameNumber recoveryFrame =
        targeted ? m_activeTargetedResyncFrame : m_session.roomState().resyncTargetFrame;
    if(!targeted) {
        setRecoveryInputMode(
            RecoveryInputMode::PostResyncStabilizing,
            "all-resync-acks-received",
            recoveryFrame,
            kRecoveryStabilizationFrames
        );
    }
    if(targeted) {
        clearTargetedResyncTracking();
    } else {
        clearActiveResyncTracking(resumeState);
    }
    for(ParticipantInfo& participant : m_session.roomState().participants) {
        if(participant.id == m_localParticipantId) continue;
        participant.inputSuspended = false;
        participant.inputResumeAwaitingResync = false;
    }

    if(targeted) {
        (void)sendConfirmedFramesToPeer(peerFromParticipantId(targetParticipantId), recoveryFrame + 1u);
        (void)sendCurrentSessionStateToPeer(peerFromParticipantId(targetParticipantId));
        return;
    }

    PacketWriter writer;
    PacketHeader header;
    header.type = MessageType::ResumeSession;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    StartSessionData startData;
    startData.state = SessionState::Running;
    startData.inputDelayFrames = m_session.roomState().inputDelayFrames;
    startData.predictFrames = m_session.roomState().predictFrames;
    startData.topology = makeTopologyData(m_session.roomState());
    writer.writePod(startData);
    m_transport.broadcastReliable(Channel::Control, writer.data());
}

void NetplayCoordinator::cancelTargetedResync(const std::string& reason)
{
    if(!m_hosting || !activeResyncIsTargeted()) return;

    pushLog(reason);
    const ParticipantId targetParticipantId = m_activeResyncTargetParticipantId;
    clearTargetedResyncTracking();
    (void)sendCurrentSessionStateToPeer(peerFromParticipantId(targetParticipantId));
}

void NetplayCoordinator::clearReconnectAttemptState()
{
    m_reconnectPending = false;
    m_reconnectAttemptInFlight = false;
    m_reconnectSecondsRemaining = 0;
    m_nextReconnectAttempt = {};
    m_reconnectDeadline = {};
}

bool NetplayCoordinator::assignmentMutationBlocked(std::string* reason) const
{
    const RoomState& room = m_session.roomState();
    if(room.state == SessionState::Resyncing) {
        if(reason != nullptr) {
            *reason = "assignment update blocked during active resync";
        }
        return true;
    }
    if(room.activeResyncId != 0 || room.pendingResyncAckCount != 0) {
        if(reason != nullptr) {
            *reason = "assignment update blocked while resync acknowledgement is pending";
        }
        return true;
    }
    if(room.recoveryInputMode != RecoveryInputMode::Normal) {
        if(reason != nullptr) {
            *reason = "assignment update blocked during recovery stabilization";
        }
        return true;
    }
    return false;
}

void NetplayCoordinator::finalizeLocalTeardown(LocalTeardownMode mode)
{
    clearReconnectAttemptState();

    if(m_transport.isActive()) {
        switch(mode) {
            case LocalTeardownMode::Graceful:
                m_transport.disconnectAll();
                break;
            case LocalTeardownMode::Immediate:
                m_transport.shutdown();
                break;
            case LocalTeardownMode::Unload:
                m_transport.shutdownForUnload();
                break;
            default:
                break;
        }
    }

    resetSessionState();
}

void NetplayCoordinator::completeLocalDisconnect(bool shutdownTransport)
{
    finalizeLocalTeardown(shutdownTransport ? LocalTeardownMode::Immediate
                                            : LocalTeardownMode::Graceful);
}

void NetplayCoordinator::clearPendingKickDisconnect(NetTransport::PeerHandle peer)
{
    if(peer == NetTransport::kInvalidPeerHandle) return;
    m_pendingKickDisconnects.erase(
        std::remove_if(
            m_pendingKickDisconnects.begin(),
            m_pendingKickDisconnects.end(),
            [peer](const PendingKickDisconnect& pending) {
                return pending.peer == peer;
            }
        ),
        m_pendingKickDisconnects.end()
    );
}

void NetplayCoordinator::processPendingKickDisconnects()
{
    if(!m_hosting || m_pendingKickDisconnects.empty()) return;

    const auto now = std::chrono::steady_clock::now();
    std::vector<PendingKickDisconnect> remaining;
    remaining.reserve(m_pendingKickDisconnects.size());
    for(const PendingKickDisconnect& pending : m_pendingKickDisconnects) {
        if(pending.disconnectAt != std::chrono::steady_clock::time_point{} &&
           now >= pending.disconnectAt) {
            m_transport.disconnectPeer(pending.peer, kDisconnectReasonKicked);
            pushLog("Forced disconnect for kicked participant " + std::to_string(static_cast<int>(pending.participantId)));
            continue;
        }
        remaining.push_back(pending);
    }
    m_pendingKickDisconnects.swap(remaining);
}

void NetplayCoordinator::scheduleReconnectAttempt()
{
    if(m_hosting ||
       m_localReconnectToken == 0 ||
       !hasReconnectTarget(m_transport.backend(), m_transport.options(), m_lastJoinHostName, m_lastJoinPort)) {
        clearReconnectAttemptState();
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if(!m_reconnectPending) {
        m_reconnectDeadline = now + m_reconnectReservationDuration;
        pushLog("Connection lost; attempting automatic reconnect");
    }

    m_reconnectPending = true;
    m_reconnectAttemptInFlight = false;
    m_nextReconnectAttempt = now;
    m_reconnectSecondsRemaining = static_cast<uint16_t>(std::clamp<int64_t>(m_reconnectReservationDuration.count(), 1, 65535));
    m_lastError = "Disconnected from host; reconnecting...";
}

void NetplayCoordinator::processPendingReconnect()
{
    if(m_hosting || !m_reconnectPending) return;

    const auto now = std::chrono::steady_clock::now();
    if(m_reconnectDeadline != std::chrono::steady_clock::time_point{}) {
        if(now >= m_reconnectDeadline) {
            pushLog("Reconnect window expired");
            m_lastError = "Reconnect window expired";
            clearReconnectAttemptState();
            return;
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(m_reconnectDeadline - now);
        m_reconnectSecondsRemaining = static_cast<uint16_t>(std::max<int64_t>(1, remaining.count() + 1));
    }

    if(m_reconnectAttemptInFlight || now < m_nextReconnectAttempt) return;

    const uint64_t reconnectToken = m_localReconnectToken;
    const std::string displayName = m_localDisplayName;
    const std::string hostName = m_lastJoinHostName;
    const uint16_t port = m_lastJoinPort;
    const bool pendingJoinRomLoaded = m_pendingJoinRomLoaded;
    const RomValidationData pendingJoinRomValidation = m_pendingJoinRomValidation;
    const auto reconnectDeadline = m_reconnectDeadline;
    const uint16_t reconnectSecondsRemaining = m_reconnectSecondsRemaining;

    finalizeLocalTeardown(LocalTeardownMode::Immediate);
    m_localReconnectToken = reconnectToken;
    m_localDisplayName = displayName;
    m_pendingJoinRomLoaded = pendingJoinRomLoaded;
    m_pendingJoinRomValidation = pendingJoinRomValidation;
    m_lastJoinHostName = hostName;
    m_lastJoinPort = port;
    m_reconnectPending = true;
    m_reconnectAttemptInFlight = true;
    m_reconnectDeadline = reconnectDeadline;
    m_reconnectSecondsRemaining = reconnectSecondsRemaining;
    m_nextReconnectAttempt = now + kReconnectRetryDelay;
    m_session.roomState().state = SessionState::Lobby;

    if(!m_transport.connectToHost(hostName, port)) {
        m_reconnectAttemptInFlight = false;
        m_lastError = "Failed to reconnect";
        pushLog(m_lastError);
        return;
    }

    pushLog("Attempting reconnect");
}

std::vector<uint8_t> NetplayCoordinator::buildJoinRoomPacket() const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::JoinRoom;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    JoinRoomData joinData;
    joinData.reconnectToken = m_localReconnectToken;
    joinData.romLoaded = m_pendingJoinRomLoaded ? 1 : 0;
    joinData.romValidation = m_pendingJoinRomValidation;
    writer.writePod(joinData);
    writer.writeString(m_localDisplayName);
    writer.writeString(m_localEmulatorVersion);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildParticipantJoinedPacket(const ParticipantInfo& participant, uint64_t reconnectToken) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::ParticipantJoined;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(participant.id);
    writer.writePod(reconnectToken);
    writer.writePod(static_cast<uint8_t>(participant.connected ? 1 : 0));
    writer.writePod(static_cast<uint8_t>(participant.reconnectReserved ? 1 : 0));
    writer.writePod(participant.reservationSecondsRemaining);
    writer.writePod(participant.role);
    const uint8_t assignmentCount = static_cast<uint8_t>(
        std::min(participant.controllerAssignments.size(), static_cast<size_t>(kMaxAssignedPlayerSlot + 1))
    );
    writer.writePod(assignmentCount);
    for(uint8_t index = 0; index < assignmentCount; ++index) {
        writer.writePod(participant.controllerAssignments[index]);
    }
    writer.writeString(participant.displayName);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildJoinRejectedPacket(JoinRejectReason reason,
                                                                 const std::string& gameName,
                                                                 const RomValidationData& romValidation,
                                                                 const std::string& expectedEmulatorVersion) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::JoinRejected;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);

    JoinRejectedData data;
    data.reason = reason;
    data.romValidation = romValidation;
    writer.writePod(data);
    writer.writeString(gameName);
    writer.writeString(expectedEmulatorVersion);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildSelectRomPacket(const std::string& gameName, const RomValidationData& romValidation) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::SelectRom;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writeString(gameName);
    writer.writePod(romValidation);
    writer.writePod(makeTopologyData(m_session.roomState()));

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildRomValidationResultPacket(const RomValidationResultData& result) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::RomValidationResult;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(result);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildParticipantLeftPacket(ParticipantId participantId) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::ParticipantLeft;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);

    ParticipantLeftData data;
    data.participantId = participantId;
    writer.writePod(data);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildLeaveRoomPacket(ParticipantId participantId) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::LeaveRoom;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);

    LeaveRoomData data;
    data.participantId = participantId;
    writer.writePod(data);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildResyncBeginPacket(const ResyncBeginData& data) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::ResyncBegin;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(data);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildResyncChunkPacket(const ResyncChunkData& data, std::span<const uint8_t> payloadChunk) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::ResyncChunk;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(data);
    writer.writeBytes(payloadChunk);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildResyncCompletePacket(const ResyncCompleteData& data) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::ResyncComplete;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(data);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildResyncAckPacket(const ResyncAckData& data) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::ResyncAck;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(data);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildResyncAbortPacket(const ResyncAbortData& data) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::ResyncAbort;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(data);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildResyncRequestPacket(const ResyncRequestData& data) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::ResyncRequest;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(data);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildClockSyncRequestPacket(const ClockSyncRequestData& data) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::ClockSyncRequest;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(data);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildClockSyncResponsePacket(const ClockSyncResponseData& data) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::ClockSyncResponse;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(data);

    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildPeerHealthPacket(const PeerHealthData& data, uint32_t sessionId) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::PeerHealth;
    header.sessionId = sessionId;
    writer.writePod(header);
    writer.writePod(data);

    return writer.data();
}

static std::vector<uint8_t> buildInputFramePacket(const InputFrameData& input, const InputFrame& inputFrame)
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::InputFrame;
    header.sessionId = 0;
    writer.writePod(header);
    writer.writePod(input);
    const std::vector<uint8_t> payload = serializeInputFrame(inputFrame);
    writer.writeBytes(std::span<const uint8_t>(payload.data(), payload.size()));

    return writer.data();
}

static std::vector<uint8_t> buildConfirmedInputFramesPacket(const ConfirmedInputFramesData& data,
                                                            std::span<const NetplayCoordinator::ConfirmedFrameInputs> frames,
                                                            uint32_t sessionId)
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::ConfirmedInputFrames;
    header.sessionId = sessionId;
    writer.writePod(header);
    writer.writePod(data);
    for(const auto& frame : frames) {
        ConfirmedInputFrameEntry entry;
        entry.authoritativeFrameStartClockMicros = frame.authoritativeFrameStartClockMicros;
        entry.buttonMaskLo = frame.buttonMaskLo;
        entry.buttonMaskHi = frame.buttonMaskHi;
        const std::vector<uint8_t> payload = serializeInputFrame(frame.inputFrame);
        entry.payloadSize = static_cast<uint16_t>(payload.size());
        writer.writePod(entry);
        writer.writeBytes(std::span<const uint8_t>(payload.data(), payload.size()));
    }

    return writer.data();
}

static std::vector<uint8_t> buildInputAckPacket(const InputAckData& ack)
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::InputAck;
    header.sessionId = 0;
    writer.writePod(header);
    writer.writePod(ack);

    return writer.data();
}

static std::vector<uint8_t> buildFrameStatusPacket(const FrameStatusData& status, uint32_t sessionId)
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::FrameStatus;
    header.sessionId = sessionId;
    writer.writePod(header);
    writer.writePod(status);

    return writer.data();
}

static std::vector<uint8_t> buildCrcReportPacket(const CrcReportData& report, uint32_t sessionId)
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::CrcReport;
    header.sessionId = sessionId;
    writer.writePod(header);
    writer.writePod(report);

    return writer.data();
}

static std::vector<uint8_t> buildAssignControllerPacket(const AssignControllerData& data, uint32_t sessionId)
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::AssignController;
    header.sessionId = sessionId;
    writer.writePod(header);
    writer.writePod(data);

    return writer.data();
}

static std::vector<uint8_t> buildStartSessionPacket(const StartSessionData& data, uint32_t sessionId)
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::StartSession;
    header.sessionId = sessionId;
    writer.writePod(header);
    writer.writePod(data);

    return writer.data();
}

static std::vector<uint8_t> buildSessionStatePacket(MessageType type, SessionState state, uint32_t sessionId)
{
    PacketWriter writer;

    PacketHeader header;
    header.type = type;
    header.sessionId = sessionId;
    writer.writePod(header);

    StartSessionData data;
    data.state = state;
    data.inputDelayFrames = 0;
    data.predictFrames = 0;
    data.topology = {};
    writer.writePod(data);

    return writer.data();
}

bool NetplayCoordinator::handleInputFrame(NetTransport::PeerHandle peer, PacketReader& reader)
{
    InputFrameData input;
    if(!reader.readPod(input)) return false;
    std::vector<uint8_t> payload;
    if(!reader.readBytes(payload, input.payloadSize)) return false;
    InputFrame inputFrame;
    if(!deserializeInputFrame(payload.data(), payload.size(), inputFrame)) return false;
    if(!m_hosting &&
       input.authoritativeFrameStartClockMicros != 0u &&
       input.frame >= m_session.roomState().lastAuthoritativeClockFrame) {
        m_session.roomState().lastAuthoritativeClockFrame = input.frame;
        m_session.roomState().lastAuthoritativeClockMicros = input.authoritativeFrameStartClockMicros;
    }
    if(input.timelineEpoch != m_session.roomState().timelineEpoch) {
        if(input.timelineEpoch < m_session.roomState().timelineEpoch) {
            m_session.roomState().lastIgnoredStaleInputEpoch = input.timelineEpoch;
            ++m_session.roomState().staleInputPacketCount;
            std::ostringstream oss;
            oss << "Ignored stale input from previous timeline epoch"
                << " frame " << input.frame
                << " seq " << input.sequence
                << " epoch " << input.timelineEpoch
                << " (current " << m_session.roomState().timelineEpoch << ")";
            pushLog(oss.str());
            return true;
        }
        return false;
    }
    m_session.roomState().lastAcceptedRemoteEpoch = input.timelineEpoch;
    if(input.playerSlot == kObserverPlayerSlot) {
        return true;
    }
    if(!isPlayableSlot(input.playerSlot)) {
        pushLog("Ignored input frame with invalid player slot");
        return true;
    }
    if(m_session.roomState().recoveryInputMode == RecoveryInputMode::ResyncLocked) {
        noteDroppedGameplayInputDuringRecovery("input_frame", input.frame, input.participantId, input.playerSlot);
        return true;
    }

    ParticipantInfo* participant = m_session.findParticipant(input.participantId);
    uint32_t previousReceivedSequence = 0;
    if(participant != nullptr) {
        if(m_hosting && participant->id != m_localParticipantId) {
            // Any incoming packet from the remote participant (even stale) is
            // activity and should prevent suspend-timeout resync loops.
            m_lastRemoteInputAt[participant->id] = std::chrono::steady_clock::now();
        }
        previousReceivedSequence = participant->lastReceivedInputSequence;
        if(!participantHasAssignment(*participant, input.playerSlot)) {
            if(participantIsObserver(*participant)) {
                // Observers can still emit local input events depending on platform/UI.
                // Ignore silently to avoid flooding logs every frame.
                return true;
            }
            std::ostringstream oss;
            oss << "Ignored input for unexpected assignment from " << participant->displayName
                << ": got " << inputAssignmentLabel(input.playerSlot, m_session.roomState());
            oss << ", expected one of " << participantAssignmentsLabel(*participant, m_session.roomState());
            pushLog(oss.str());
            return true;
        }

        if(m_hosting && participant->id != m_localParticipantId) {
            if(participant->inputResumeAwaitingResync) {
                std::ostringstream oss;
                oss << "Ignoring resumed participant input until authoritative resync completes: "
                    << participant->displayName
                    << " frame " << input.frame
                    << " seq " << input.sequence;
                pushLog(oss.str());
                return true;
            }

            if(participant->inputSuspended) {
                participant->inputSuspended = false;
                participant->inputResumeAwaitingResync = true;
                participant->sequenceRebasePending = true;
                m_lastRemoteInputAt[participant->id] = std::chrono::steady_clock::now();

                const FrameNumber resyncFrame =
                    std::min(m_localSimulationFrame, m_session.roomState().lastConfirmedFrame);
                queuePendingHostResync(resyncFrame, ResyncReason::ConfirmedDesync);

                std::ostringstream oss;
                oss << "Participant input resumed after suspension: " << participant->displayName
                    << " frame " << input.frame
                    << " seq " << input.sequence
                    << "; scheduling authoritative resync from frame " << resyncFrame
                    << " classification=suspended_input_resume";
                pushLog(oss.str());
                return true;
            }
        }

        if(input.sequence <= participant->lastReceivedInputSequence) {
            std::ostringstream oss;
            oss << "Ignored stale/duplicate input from " << participant->displayName
                << " frame " << input.frame
                << " seq " << input.sequence
                << " lastSeq " << participant->lastReceivedInputSequence;
            pushLog(oss.str());
            return true;
        }

        const uint32_t expectedSequence = participant->lastReceivedInputSequence + 1u;
        const FrameNumber expectedFrame = participant->lastContiguousInputFrame + 1u;
        const bool allowSequenceRebase =
            participant->sequenceRebasePending &&
            input.frame >= expectedFrame &&
            input.sequence > participant->lastReceivedInputSequence;
        const bool allowClientResyncRebase =
            !m_hosting &&
            participant->sequenceRebasePending &&
            input.frame >= expectedFrame &&
            input.sequence > participant->lastReceivedInputSequence;
        if(input.sequence != expectedSequence && !allowSequenceRebase && !allowClientResyncRebase) {
            std::ostringstream oss;
            oss << "Rejected non-sequential input sequence from " << participant->displayName
                << " seq " << input.sequence
                << " expectedSeq " << expectedSequence
                << " frame " << input.frame;
            pushLog(oss.str());
            return true;
        }

        if(input.frame != expectedFrame && !allowClientResyncRebase) {
            std::ostringstream oss;
            oss << "Rejected non-sequential input from " << participant->displayName
                << " frame " << input.frame
                << " expectedFrame " << expectedFrame
                << " seq " << input.sequence;
            pushLog(oss.str());
            return true;
        }
        if((allowSequenceRebase || allowClientResyncRebase) && input.frame > expectedFrame) {
            // Re-anchor contiguous frame tracking when a participant resumes
            // after a reset/recovery window and starts from a later frame.
            participant->lastContiguousInputFrame = input.frame - 1u;
            participant->pendingMissingInputFrom.reset();
            std::ostringstream oss;
            oss << "Accepted input rebase from " << participant->displayName
                << " frame " << input.frame
                << " expectedFrame " << expectedFrame
                << " seq " << input.sequence
                << " expectedSeq " << expectedSequence;
            pushLog(oss.str());
        }

        participant->lastReceivedInputFrame = std::max(participant->lastReceivedInputFrame, input.frame);
        participant->lastReceivedInputSequence = std::max(participant->lastReceivedInputSequence, input.sequence);
        participant->sequenceRebasePending = false;
    }

    InputTimeline* destinationTimeline = &m_remoteInputs;
    if(!m_hosting && input.participantId == m_localParticipantId) {
        destinationTimeline = &m_localInputs;
    }

    const TimelineInputEntry* existing = destinationTimeline->find(input.frame, input.participantId, input.playerSlot);
    if(existing != nullptr && existing->predicted) {
        const bool predictionHit = inputFramesEqual(existing->inputFrame, inputFrame);
        const FrameNumber currentFrame = m_localSimulationFrame;
        if(input.frame <= currentFrame) {
            m_predictionStats.recordPrediction(predictionHit);
            handleResolvedPredictedInput(input.participantId, input.frame, input.playerSlot, predictionHit);
        } else if(participant != nullptr) {
            m_predictionStats.recordPrediction(predictionHit);
            participant->lastDecision = predictionHit ? "Future prediction validated" : "Future prediction corrected";
            participant->lastDecisionFrame = input.frame;
            participant->lastDecisionSlot = input.playerSlot;
        }
    }

    TimelineInputEntry entry;
    entry.frame = input.frame;
    entry.participantId = input.participantId;
    entry.playerSlot = input.playerSlot;
    entry.buttonMaskLo = input.buttonMaskLo;
    entry.buttonMaskHi = input.buttonMaskHi;
    entry.inputFrame = inputFrame;
    entry.sequence = input.sequence;
    entry.confirmed = true;
    entry.predicted = false;
    destinationTimeline->push(entry);

    if(participant != nullptr) {
        if(previousReceivedSequence > 0 && input.sequence > previousReceivedSequence + 1u) {
            std::ostringstream oss;
            oss << "Input sequence gap from " << participant->displayName
                << " expected seq " << (previousReceivedSequence + 1u)
                << " got " << input.sequence
                << " frame " << input.frame;
            pushLog(oss.str());
        }

        if(input.frame > participant->lastContiguousInputFrame + 1u) {
            const FrameNumber missingFrame = participant->lastContiguousInputFrame + 1u;
            if(!participant->pendingMissingInputFrom.has_value() ||
               *participant->pendingMissingInputFrom != missingFrame) {
                participant->pendingMissingInputFrom = missingFrame;
                recordMissingInputGap(*participant, missingFrame, input.frame, input.playerSlot);
            }
        }
        advanceParticipantContiguousInputFrame(*participant, input.playerSlot);
        if(participant->pendingMissingInputFrom.has_value() &&
           participant->lastContiguousInputFrame >= *participant->pendingMissingInputFrom) {
            std::ostringstream oss;
            oss << "Recovered missing input gap from " << participant->displayName
                << " at frame " << *participant->pendingMissingInputFrom
                << " using received frame " << input.frame;
            pushLog(oss.str());
            participant->pendingMissingInputFrom.reset();
        }

        clearImplicitRemoteInputStall(participant->id, participant->lastContiguousInputFrame);
    }

    if(m_hosting) {
        InputFrameData relayedInput = input;
        relayedInput.authoritativeFrameStartClockMicros =
            authoritativeFrameStartClockMicros(input.frame);
        m_transport.broadcastReliable(Channel::Gameplay, buildInputFramePacket(relayedInput, inputFrame), peer);
        publishConfirmedFramesIfReady();
    }

    return true;
}

bool NetplayCoordinator::handleConfirmedInputFrames(PacketReader& reader)
{
    ConfirmedInputFramesData data;
    if(!reader.readPod(data)) return false;
    if(data.timelineEpoch != m_session.roomState().timelineEpoch) {
        if(data.timelineEpoch < m_session.roomState().timelineEpoch) {
            return true;
        }
        return false;
    }
    m_session.roomState().lastAcceptedRemoteEpoch = data.timelineEpoch;
    if(m_session.roomState().recoveryInputMode == RecoveryInputMode::ResyncLocked) {
        noteDroppedGameplayInputDuringRecovery(
            "confirmed_input_frames",
            data.startFrame,
            m_localParticipantId,
            kObserverPlayerSlot
        );
        return true;
    }

    for(uint16_t i = 0; i < data.frameCount; ++i) {
        ConfirmedInputFrameEntry entry;
        if(!reader.readPod(entry)) return false;
        std::vector<uint8_t> payload;
        if(!reader.readBytes(payload, entry.payloadSize)) return false;

        ConfirmedFrameInputs frame;
        frame.frame = data.startFrame + static_cast<FrameNumber>(i);
        frame.authoritativeFrameStartClockMicros = entry.authoritativeFrameStartClockMicros;
        frame.buttonMaskLo = entry.buttonMaskLo;
        frame.buttonMaskHi = entry.buttonMaskHi;
        if(!deserializeInputFrame(payload.data(), payload.size(), frame.inputFrame)) return false;
        storeConfirmedFrame(frame);
        if(frame.authoritativeFrameStartClockMicros != 0u) {
            m_session.roomState().lastAuthoritativeClockFrame = frame.frame;
            m_session.roomState().lastAuthoritativeClockMicros = frame.authoritativeFrameStartClockMicros;
        }
    }

    if(data.frameCount > 0) {
        const FrameNumber lastFrame = data.startFrame + static_cast<FrameNumber>(data.frameCount - 1u);
        m_session.roomState().lastConfirmedFrame = std::max(m_session.roomState().lastConfirmedFrame, lastFrame);

        ParticipantInfo* localParticipant = m_session.findParticipant(m_localParticipantId);
        if(localParticipant != nullptr && !participantIsObserver(*localParticipant)) {
            for(FrameNumber frame = data.startFrame; frame <= lastFrame; ++frame) {
                for(PlayerSlot slot : participantAssignments(*localParticipant)) {
                    TimelineInputEntry* localEntry =
                        m_localInputs.findMutable(frame, m_localParticipantId, slot);
                    if(localEntry != nullptr) {
                        localEntry->confirmed = true;
                    }
                }
            }
            localParticipant->lastContiguousInputFrame = std::max(localParticipant->lastContiguousInputFrame, lastFrame);
            localParticipant->lastReceivedInputFrame = std::max(localParticipant->lastReceivedInputFrame, lastFrame);
        }

    }

    return true;
}

bool NetplayCoordinator::handleInputAck(PacketReader& reader)
{
    InputAckData ack;
    if(!reader.readPod(ack)) return false;
    if(ack.timelineEpoch != m_session.roomState().timelineEpoch) {
        if(ack.timelineEpoch < m_session.roomState().timelineEpoch) {
            return true;
        }
        return false;
    }
    m_session.roomState().lastAcceptedRemoteEpoch = ack.timelineEpoch;
    if(ack.playerSlot == kObserverPlayerSlot) {
        return true;
    }
    if(!isPlayableSlot(ack.playerSlot)) {
        pushLog("Ignored input ACK with invalid player slot");
        return true;
    }
    if(m_session.roomState().recoveryInputMode == RecoveryInputMode::ResyncLocked) {
        noteDroppedGameplayInputDuringRecovery("input_ack", ack.contiguousFrame, ack.participantId, ack.playerSlot);
        return true;
    }

    if(m_hosting) {
        return true;
    }

    if(ack.participantId != m_localParticipantId) {
        return true;
    }

    ParticipantInfo* participant = m_session.findParticipant(ack.participantId);
    if(participant != nullptr) {
        participant->lastReceivedInputFrame = std::max(participant->lastReceivedInputFrame, ack.contiguousFrame);
        participant->lastContiguousInputFrame = std::max(participant->lastContiguousInputFrame, ack.contiguousFrame);
    }

    for(FrameNumber frame = 1; frame <= ack.contiguousFrame; ++frame) {
        TimelineInputEntry* entry = m_localInputs.findMutable(frame, ack.participantId, ack.playerSlot);
        if(entry == nullptr) continue;
        entry->confirmed = true;
    }

    return true;
}

void NetplayCoordinator::recordMissingInputGap(ParticipantInfo& participant, FrameNumber missingFrame, FrameNumber receivedFrame, PlayerSlot slot)
{
    ++participant.missingInputGapCount;
    participant.lastDecision = "Missing input gap";
    participant.lastDecisionFrame = missingFrame;
    participant.lastDecisionSlot = slot;
    m_predictionStats.recordMissingInputGap(missingFrame, slot);

    std::ostringstream oss;
    oss << "Missing input gap from " << participant.displayName
        << " starting at frame " << missingFrame
        << " after receiving frame " << receivedFrame
        << " slot " << static_cast<unsigned>(slot) + 1u;
    pushLog(oss.str());
}

void NetplayCoordinator::advanceParticipantContiguousInputFrame(ParticipantInfo& participant, PlayerSlot slot)
{
    (void)slot;
    while(true) {
        const FrameNumber nextFrame = participant.lastContiguousInputFrame + 1u;
        bool haveAllAssignments = true;
        for(PlayerSlot assignedSlot : participantAssignments(participant)) {
            const TimelineInputEntry* entry = m_remoteInputs.find(nextFrame, participant.id, assignedSlot);
            if(entry == nullptr || !entry->confirmed) {
                haveAllAssignments = false;
                break;
            }
        }
        if(!haveAllAssignments) {
            break;
        }
        participant.lastContiguousInputFrame = nextFrame;
    }
}

void NetplayCoordinator::scheduleResyncRetry(FrameNumber targetFrame, const std::string& reason)
{
    if(!m_hosting) return;

    const ResyncReason retryReason =
        m_session.roomState().activeResyncReason != ResyncReason::Unspecified
            ? m_session.roomState().activeResyncReason
            : ResyncReason::ConfirmedDesync;
    queuePendingHostResync(targetFrame, retryReason);

    m_pendingResyncAcks.clear();
    m_activeResyncTargetParticipantId = kInvalidParticipantId;
    m_activeResyncResumeState = SessionState::Paused;
    m_activeTargetedResyncId = 0;
    m_activeTargetedResyncFrame = 0;
    m_activeTargetedResyncExpectedStateCrc32 = 0;
    m_activeResyncAckDeadline = {};
    m_session.roomState().pendingResyncAckCount = 0;
    m_session.roomState().activeResyncId = 0;
    m_session.roomState().resyncTargetFrame = 0;
    m_session.roomState().resyncConfirmedFrame = 0;
    m_session.roomState().resyncFrameReadyFrame = 0;
    m_session.roomState().resyncPayloadSize = 0;
    m_session.roomState().resyncPayloadCrc32 = 0;
    m_session.roomState().resyncFrameReadyCrc32 = 0;
    m_session.roomState().resyncInputSequenceBase = 0;
    m_session.roomState().activeResyncReason = ResyncReason::Unspecified;
    m_activeResyncExpectedStateCrc32 = 0;
    m_implicitRecoveryMonitor.reset();
    if(m_session.roomState().state == SessionState::Running ||
       m_session.roomState().state == SessionState::Resyncing) {
        m_session.roomState().state = SessionState::Running;
    }
    setRecoveryInputMode(RecoveryInputMode::Normal, "resync-retry-scheduled", targetFrame);
    pushLog(reason);
}

bool NetplayCoordinator::ejectParticipantForResyncFailure(ParticipantId participantId, const std::string& reason)
{
    if(!m_hosting || participantId == kInvalidParticipantId || participantId == m_localParticipantId) {
        return false;
    }

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr) {
        return false;
    }
    if(participantIsObserver(*participant)) {
        return false;
    }

    pushLog(reason);
    if(participant->reconnectReserved) {
        (void)removeReconnectReservation(participantId);
    } else {
        (void)kickParticipant(participantId);
    }

    m_pendingResyncAcks.erase(
        std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), participantId),
        m_pendingResyncAcks.end()
    );
    m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
    if(m_pendingResyncAcks.empty()) {
        finalizeActiveResyncIfReady();
    } else {
        m_activeResyncAckDeadline = std::chrono::steady_clock::now() + kResyncAckTimeout;
    }
    return true;
}

const char* NetplayCoordinator::recoveryInputModeLabel(RecoveryInputMode mode)
{
    switch(mode) {
        case RecoveryInputMode::Normal: return "Normal";
        case RecoveryInputMode::ResyncLocked: return "ResyncLocked";
        case RecoveryInputMode::PostResyncStabilizing: return "PostResyncStabilizing";
        default: return "Unknown";
    }
}

void NetplayCoordinator::setRecoveryInputMode(RecoveryInputMode mode,
                                              const char* reason,
                                              FrameNumber frameContext,
                                              uint32_t stabilizationFrames)
{
    RoomState& room = m_session.roomState();
    const RecoveryInputMode previousMode = room.recoveryInputMode;
    if(previousMode == mode &&
       (mode != RecoveryInputMode::PostResyncStabilizing || room.stabilizationFramesRemaining == stabilizationFrames)) {
        return;
    }

    room.recoveryInputMode = mode;
    room.recoveryModeEnteredAtFrame = frameContext;
    ++room.recoveryModeTransitionCount;
    if(mode == RecoveryInputMode::PostResyncStabilizing) {
        room.stabilizationFramesRemaining = stabilizationFrames;
        room.stabilizationAnchorFrame = frameContext;
        room.stabilizationCrcPassCount = 0;
        room.stabilizationRetryIssued = false;
    } else if(mode == RecoveryInputMode::Normal) {
        room.stabilizationFramesRemaining = 0;
        room.stabilizationAnchorFrame = frameContext;
        room.stabilizationCrcPassCount = 0;
        room.stabilizationRetryIssued = false;
    }

    std::ostringstream oss;
    oss << "Recovery input mode "
        << recoveryInputModeLabel(previousMode)
        << " -> "
        << recoveryInputModeLabel(mode)
        << " frame "
        << frameContext;
    if(reason != nullptr && *reason != '\0') {
        oss << " reason " << reason;
    }
    if(mode == RecoveryInputMode::PostResyncStabilizing) {
        oss << " stabilizationFrames " << room.stabilizationFramesRemaining;
    }
    pushLog(oss.str());
}

void NetplayCoordinator::noteDroppedGameplayInputDuringRecovery(const char* source,
                                                                FrameNumber frame,
                                                                ParticipantId participantId,
                                                                PlayerSlot slot)
{
    RoomState& room = m_session.roomState();
    ++room.inputsDroppedDuringRecovery;
    if((room.inputsDroppedDuringRecovery % 32u) != 1u) {
        return;
    }

    std::ostringstream oss;
    oss << "Dropped gameplay input during recovery"
        << " source " << (source != nullptr ? source : "unknown")
        << " frame " << frame
        << " participant " << static_cast<unsigned>(participantId)
        << " slot " << static_cast<unsigned>(slot)
        << " mode " << recoveryInputModeLabel(room.recoveryInputMode)
        << " droppedCount " << room.inputsDroppedDuringRecovery;
    pushLog(oss.str());
}

void NetplayCoordinator::advanceRecoveryStabilization(FrameNumber observedFrame)
{
    RoomState& room = m_session.roomState();
    if(room.recoveryInputMode != RecoveryInputMode::PostResyncStabilizing) return;
    if(room.state != SessionState::Running) return;

    const bool confirmedCheckpointReached = room.lastConfirmedFrame >= room.recoveryModeEnteredAtFrame;
    const bool firstPostRecoveryCrcPassed = room.stabilizationCrcPassCount > 0u;

    if(room.stabilizationFramesRemaining > 0u && observedFrame > room.stabilizationAnchorFrame) {
        const FrameNumber advancedFrames = observedFrame - room.stabilizationAnchorFrame;
        const uint32_t consume = std::min<uint32_t>(room.stabilizationFramesRemaining, advancedFrames);
        room.stabilizationFramesRemaining -= consume;
        room.stabilizationAnchorFrame = observedFrame;
    }

    if(room.stabilizationFramesRemaining == 0u &&
       confirmedCheckpointReached &&
       firstPostRecoveryCrcPassed) {
        setRecoveryInputMode(RecoveryInputMode::Normal, "stabilization-window-complete", observedFrame);
        return;
    }

    const FrameNumber stabilizationElapsedFrames =
        observedFrame > room.recoveryModeEnteredAtFrame
            ? (observedFrame - room.recoveryModeEnteredAtFrame)
            : 0u;
    if(stabilizationElapsedFrames < kRecoveryStabilizationFailTimeoutFrames) {
        return;
    }
    if(room.stabilizationRetryIssued) {
        return;
    }

    room.stabilizationRetryIssued = true;
    ++room.stabilizationRetryCount;
    if(!m_hosting) {
        pushLog("Stabilization timeout reached on client; waiting for authoritative recovery");
        return;
    }

    const FrameNumber retryFrame =
        std::min(m_localSimulationFrame, room.lastConfirmedFrame);
    const std::string reason =
        "Post-resync stabilization failed to validate (confirmed checkpoint and CRC pass); scheduling controlled retry";
    scheduleResyncRetry(retryFrame, reason);
    queuePendingHostResync(retryFrame, ResyncReason::ConfirmedDesync);
}

void NetplayCoordinator::noteImplicitRemoteInputStall(ParticipantId participantId, PlayerSlot slot, FrameNumber frame)
{
    if(!m_hosting || m_session.roomState().state != SessionState::Running) return;
    if(participantId == kInvalidParticipantId || participantId == m_localParticipantId) return;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr || !participant->connected || participantIsObserver(*participant)) return;

    const auto update = m_implicitRecoveryMonitor.noteStall(
        participantId,
        slot,
        frame,
        participant->peerHealthSerial
    );
    if(!update.newlyTracked) return;

    participant->lastDecisionFrame = frame;
    participant->lastDecisionSlot = slot;
    participant->lastDecision = "Implicit stall detected; waiting for peer health";

    std::ostringstream oss;
    oss << "Implicit input stall detected for " << participant->displayName
        << " at frame " << frame
        << "; waiting for fresh peer health before recovery resync";
    pushLog(oss.str());
}

void NetplayCoordinator::clearImplicitRemoteInputStall(ParticipantId participantId, FrameNumber recoveredThroughFrame)
{
    const auto update = m_implicitRecoveryMonitor.clearRecovered(participantId, recoveredThroughFrame);
    if(!update.cleared) return;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant != nullptr) {
        std::ostringstream oss;
        oss << "Implicit input stall recovered for " << participant->displayName
            << " by input frame " << recoveredThroughFrame;
        pushLog(oss.str());
    }
}

void NetplayCoordinator::tryScheduleImplicitRecoveryResync(ParticipantInfo& participant)
{
    if(!m_hosting) return;
    if(m_session.roomState().state != SessionState::Running) return;
    if(m_session.roomState().activeResyncId != 0 || m_session.roomState().pendingResyncAckCount != 0) return;
    if(participant.inputSuspended || participant.inputResumeAwaitingResync) return;
    const auto update = m_implicitRecoveryMonitor.onPeerHealth(participant.id, participant.peerHealthSerial);
    if(!update.shouldScheduleResync) return;

    const FrameNumber resyncFrame =
        m_session.roomState().lastConfirmedFrame > 0
            ? std::min(m_localSimulationFrame, m_session.roomState().lastConfirmedFrame)
            : m_localSimulationFrame;

    queuePendingHostResync(resyncFrame, ResyncReason::ConfirmedDesync);

    std::ostringstream oss;
    oss << "Fresh peer health received from " << participant.displayName
        << " after implicit input stall; scheduling recovery resync from frame "
        << resyncFrame
        << " classification=stall_based_recovery";
    pushLog(oss.str());
}

void NetplayCoordinator::synthesizeSuspendedRemoteInputsUpTo(FrameNumber targetFrame)
{
    if(!m_hosting || m_session.roomState().state != SessionState::Running) return;

    for(ParticipantInfo& participant : m_session.roomState().participants) {
        const bool participatesViaReservation =
            !participant.connected && participant.reconnectReserved;
        if(participant.id == m_localParticipantId ||
           (!participant.connected && !participatesViaReservation) ||
           participantIsObserver(participant) ||
           (!participant.inputSuspended && !participant.inputResumeAwaitingResync)) {
            continue;
        }

        for(PlayerSlot slot : participantAssignments(participant)) {
            const TimelineInputEntry* latestConfirmed = m_remoteInputs.latestConfirmedFor(participant.id, slot);
            if(latestConfirmed == nullptr) {
                continue;
            }

            FrameNumber nextFrame = participant.lastContiguousInputFrame + 1u;
            while(nextFrame <= targetFrame) {
                const TimelineInputEntry* existing =
                    m_remoteInputs.find(nextFrame, participant.id, slot);
                if(existing != nullptr && existing->confirmed) {
                    ++nextFrame;
                    continue;
                }

                TimelineInputEntry synthetic = *latestConfirmed;
                synthetic.frame = nextFrame;
                synthetic.inputFrame = InputFrame::repeatedFrom(latestConfirmed->inputFrame, nextFrame);
                synthetic.predicted = false;
                synthetic.confirmed = true;
                if(existing != nullptr) {
                    synthetic.sequence = existing->sequence;
                }
                m_remoteInputs.push(synthetic);
                latestConfirmed = m_remoteInputs.find(nextFrame, participant.id, slot);
                ++nextFrame;
            }
        }

        for(PlayerSlot slot : participantAssignments(participant)) {
            advanceParticipantContiguousInputFrame(participant, slot);
        }
    }
}

void NetplayCoordinator::processRemoteInputSuspension(const std::chrono::steady_clock::time_point& now)
{
    if(!m_hosting || m_session.roomState().state != SessionState::Running) {
        return;
    }

    const bool resyncBusy =
        m_session.roomState().activeResyncId != 0u ||
        m_session.roomState().pendingResyncAckCount != 0u;
    if(resyncBusy) {
        return;
    }

    for(ParticipantInfo& participant : m_session.roomState().participants) {
        if(participant.id == m_localParticipantId ||
           !participant.connected ||
           participantIsObserver(participant)) {
            continue;
        }

        const auto inputIt = m_lastRemoteInputAt.find(participant.id);
        const auto healthIt = m_lastPeerHealthAt.find(participant.id);
        if(inputIt == m_lastRemoteInputAt.end() && healthIt == m_lastPeerHealthAt.end()) {
            continue;
        }

        if(participant.inputSuspended || participant.inputResumeAwaitingResync) {
            continue;
        }

        std::chrono::steady_clock::time_point latestActivity = {};
        if(inputIt != m_lastRemoteInputAt.end()) {
            latestActivity = inputIt->second;
        }
        if(healthIt != m_lastPeerHealthAt.end() &&
           (latestActivity.time_since_epoch().count() == 0 || healthIt->second > latestActivity)) {
            latestActivity = healthIt->second;
        }
        if(latestActivity.time_since_epoch().count() == 0) {
            continue;
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - latestActivity);
        if(elapsed < m_remoteInputSuspendTimeout) {
            continue;
        }

        bool hasBaselineInput = true;
        for(PlayerSlot slot : participantAssignments(participant)) {
            if(m_remoteInputs.latestConfirmedFor(participant.id, slot) == nullptr) {
                hasBaselineInput = false;
                break;
            }
        }
        if(!hasBaselineInput) {
            continue;
        }

        participant.inputSuspended = true;
        participant.inputResumeAwaitingResync = false;

        std::ostringstream oss;
        oss << "Participant input suspended due to timeout: " << participant.displayName
            << " timeoutMs=" << m_remoteInputSuspendTimeout.count()
            << " lastFrame=" << participant.lastContiguousInputFrame
            << " classification=suspended_input_timeout";
        pushLog(oss.str());

        // Keep the authoritative input stream contiguous for everyone else.
        synthesizeSuspendedRemoteInputsUpTo(m_localSimulationFrame);
    }
}

void NetplayCoordinator::handleResolvedPredictedInput(ParticipantId participantId,
                                                      FrameNumber inputFrame,
                                                      PlayerSlot slot,
                                                      bool predictionMatched)
{
    ParticipantInfo* participant = m_session.findParticipant(participantId);
    auto predictionMessage = [&]() {
        std::ostringstream oss;
        oss << "Prediction mismatch";
        if(participant != nullptr && !participant->displayName.empty()) {
            oss << " from " << participant->displayName;
        } else {
            oss << " from participant " << static_cast<unsigned>(participantId);
        }
        oss << " on frame " << inputFrame
            << " slot " << static_cast<unsigned>(slot) + 1u;
        return oss.str();
    };

    auto recordParticipantDecision = [&](const std::string& label) {
        if(participant == nullptr) return;
        participant->lastDecision = label;
        participant->lastDecisionFrame = inputFrame;
        participant->lastDecisionSlot = slot;
    };

    const FrameNumber confirmedFrame = m_session.roomState().lastConfirmedFrame;
    const FrameNumber currentFrame = m_localSimulationFrame;

    if(inputFrame <= confirmedFrame) {
        m_predictionStats.recordConfirmedFrameConflict(inputFrame, slot);
        if(participant != nullptr) {
            ++participant->confirmedFrameConflictCount;
        }
        recordParticipantDecision("Confirmed-frame conflict");
        pushLog(predictionMessage() + " after confirmed window");

        if(m_hosting &&
           m_session.roomState().state != SessionState::Resyncing) {
            queuePendingHostResync(inputFrame, ResyncReason::ConfirmedDesync);
        }
        return;
    }

    FrameNumber rollbackFrame = inputFrame > 0 ? (inputFrame - 1) : 0;
    if(rollbackFrame < confirmedFrame) {
        rollbackFrame = confirmedFrame;
    }

    if(!m_pendingRollbackFrame.has_value() || rollbackFrame < *m_pendingRollbackFrame) {
        m_pendingRollbackFrame = rollbackFrame;
    }

    m_predictionStats.recordRollbackScheduled(inputFrame, slot);
    if(participant != nullptr) {
        ++participant->rollbackScheduledCount;
    }
    if(inputFrame > currentFrame) {
        if(!predictionMatched) {
            m_predictionStats.recordFutureFrameMismatch(inputFrame, slot);
            if(participant != nullptr) {
                ++participant->futureFrameMismatchCount;
            }
            recordParticipantDecision("Future-frame mismatch");
        } else {
            recordParticipantDecision("Future prediction validated");
        }
        if(!predictionMatched) {
            pushLog(predictionMessage() + " before local simulation reached that frame");
        }
        return;
    }

    recordParticipantDecision("Rollback scheduled");
    if(!predictionMatched) {
        pushLog(predictionMessage() + " classification=speculative_mismatch_corrected_by_rollback");
    }
}

bool NetplayCoordinator::handleFrameStatus(PacketReader& reader)
{
    FrameStatusData status;
    if(!reader.readPod(status)) return false;
    if(status.timelineEpoch != m_session.roomState().timelineEpoch) {
        if(status.timelineEpoch < m_session.roomState().timelineEpoch) {
            m_session.roomState().lastIgnoredStaleFrameStatusEpoch = status.timelineEpoch;
            ++m_session.roomState().staleFrameStatusPacketCount;
            return true;
        }
        return true;
    }
    m_session.roomState().lastAcceptedRemoteEpoch = status.timelineEpoch;

    if(m_hosting) {
        return true;
    }

    m_session.roomState().currentFrame = status.currentFrame;
    m_session.roomState().lastConfirmedFrame = status.lastConfirmedFrame;
    m_session.roomState().inputDelayFrames = status.inputDelayFrames;
    m_session.roomState().predictFrames = status.predictFrames;
    applyTopologyData(m_session.roomState(), status.topology);
    advanceRecoveryStabilization(status.currentFrame);
    return true;
}

bool NetplayCoordinator::handleCrcReport(PacketReader& reader)
{
    CrcReportData report;
    if(!reader.readPod(report)) return false;
    if(!kDesyncMonitorEnabled) return true;
    if(report.timelineEpoch != m_session.roomState().timelineEpoch) {
        if(report.timelineEpoch < m_session.roomState().timelineEpoch) {
            m_session.roomState().lastIgnoredStaleCrcEpoch = report.timelineEpoch;
            ++m_session.roomState().staleCrcPacketCount;
            pushLog("Ignored stale CRC report from previous timeline epoch");
            return true;
        }
        return true;
    }
    m_session.roomState().lastAcceptedRemoteEpoch = report.timelineEpoch;

    if(m_session.roomState().state != SessionState::Running) {
        return true;
    }

    m_session.roomState().lastRemoteCrcFrame = report.frame;
    m_session.roomState().lastRemoteCrc32 = report.crc32;
    applyDesyncMonitorUpdate(m_desyncMonitor.submitRemoteCrc(report.frame, report.crc32), "remote CRC report");

    return true;
}

void NetplayCoordinator::applyDesyncMonitorUpdate(const DesyncMonitor::Update& update, const char* source)
{
    if(!update.mismatchDetected) return;

    std::ostringstream oss;
    oss << "CRC mismatch detected on frame " << update.frame;
    if(source != nullptr && *source != '\0') {
        oss << " via " << source;
    }
    oss << " consecutive=" << static_cast<uint32_t>(update.consecutiveMismatchCount)
        << " classification=confirmed_crc_mismatch";
    pushLog(oss.str());

    if(!m_hosting) return;
    if(m_session.roomState().state != SessionState::Running) return;
    for(const ParticipantInfo& participant : m_session.roomState().participants) {
        if(participant.id == m_localParticipantId) continue;
        if(!participant.connected || participantIsObserver(participant)) continue;
        if(participant.inputSuspended || participant.inputResumeAwaitingResync) {
            pushLog("Deferred hard resync escalation while participant input recovery is in progress");
            return;
        }
    }
    if(m_session.roomState().recoveryInputMode == RecoveryInputMode::PostResyncStabilizing) {
        pushLog("Deferred hard resync escalation during post-resync stabilization");
        return;
    }
    if(m_session.roomState().activeResyncId != 0 || m_session.roomState().pendingResyncAckCount != 0) return;
    queuePendingHostResync(update.frame, ResyncReason::ConfirmedDesync);
}

bool NetplayCoordinator::preserveConfirmedInputsAcrossRealignment(ResyncReason reason)
{
    // Ordinary rollback/resync keeps the confirmed baseline anchored to the
    // same causal timeline. Manual host load/reset replaces that timeline, so
    // old confirmed inputs must not be projected onto the loaded frame.
    switch(reason) {
        case ResyncReason::HostReset:
        case ResyncReason::HostLoadedState:
            return false;
        default:
            return true;
    }
}

void NetplayCoordinator::realignAuthoritativeState(FrameNumber loadedFrame,
                                                   bool resetInputSequences,
                                                   uint32_t inputSequenceBase,
                                                   bool preserveConfirmedInputs)
{
    std::vector<TimelineInputEntry> preservedLocalInputs;
    std::vector<TimelineInputEntry> preservedRemoteInputs;
    preservedLocalInputs.reserve(m_session.roomState().participants.size());
    preservedRemoteInputs.reserve(m_session.roomState().participants.size());

    const auto preserveLatestConfirmedInput = [&](const InputTimeline& timeline,
                                                  ParticipantId participantId,
                                                  PlayerSlot slot,
                                                  std::vector<TimelineInputEntry>& preserved) {
        for(auto it = timeline.entries().rbegin(); it != timeline.entries().rend(); ++it) {
            if(it->participantId != participantId || it->playerSlot != slot) continue;
            if(!it->confirmed || it->frame > loadedFrame) continue;

            TimelineInputEntry entry = *it;
            entry.frame = loadedFrame;
            entry.predicted = false;
            entry.confirmed = true;
            if(resetInputSequences) {
                entry.sequence = inputSequenceBase;
            }
            preserved.push_back(std::move(entry));
            break;
        }
    };

    if(preserveConfirmedInputs) {
        for(const ParticipantInfo& participant : m_session.roomState().participants) {
            if(participantIsObserver(participant)) continue;

            for(PlayerSlot slot : participantAssignments(participant)) {
                if(participant.id == m_localParticipantId) {
                    preserveLatestConfirmedInput(m_localInputs,
                                                 participant.id,
                                                 slot,
                                                 preservedLocalInputs);
                } else {
                    preserveLatestConfirmedInput(m_remoteInputs,
                                                 participant.id,
                                                 slot,
                                                 preservedRemoteInputs);
                }
            }
        }
    } else {
        for(const ParticipantInfo& participant : m_session.roomState().participants) {
            if(participantIsObserver(participant)) continue;

            for(PlayerSlot slot : participantAssignments(participant)) {
                TimelineInputEntry entry;
                entry.frame = loadedFrame;
                entry.participantId = participant.id;
                entry.playerSlot = slot;
                entry.buttonMaskLo = 0;
                entry.buttonMaskHi = 0;
                entry.inputFrame = makeContributionBase(makeRoomTopologyBaseFrame(loadedFrame, m_session.roomState()));
                entry.sequence = resetInputSequences ? inputSequenceBase : 0u;
                entry.predicted = false;
                entry.confirmed = true;
                if(participant.id == m_localParticipantId) {
                    preservedLocalInputs.push_back(std::move(entry));
                } else {
                    preservedRemoteInputs.push_back(std::move(entry));
                }
            }
        }
    }

    m_localInputs.clear();
    m_remoteInputs.clear();
    m_confirmedFrames.clear();
    for(const TimelineInputEntry& entry : preservedLocalInputs) {
        m_localInputs.push(entry);
    }
    for(const TimelineInputEntry& entry : preservedRemoteInputs) {
        m_remoteInputs.push(entry);
    }
    m_pendingRollbackFrame.reset();
    m_predictionStats.lastDecision.clear();
    m_predictionStats.lastDecisionFrame = loadedFrame;
    m_predictionStats.lastDecisionSlot = kObserverPlayerSlot;
    m_desyncMonitor.reset();
    m_session.roomState().currentFrame = loadedFrame;
    m_session.roomState().lastConfirmedFrame = loadedFrame;
    m_session.roomState().lastRemoteCrcFrame = 0;
    m_session.roomState().lastRemoteCrc32 = 0;
    m_lastBroadcastConfirmedFrame = loadedFrame;

    const auto hasPendingSequenceReset = [&](ParticipantId participantId) -> bool {
        return std::find(
                   m_pendingSequenceResetParticipants.begin(),
                   m_pendingSequenceResetParticipants.end(),
                   participantId
               ) != m_pendingSequenceResetParticipants.end();
    };

    const auto latestSequenceForParticipant = [&](ParticipantId participantId) -> uint32_t {
        if(resetInputSequences) {
            return inputSequenceBase;
        }
        if(hasPendingSequenceReset(participantId)) {
            return 0;
        }

        const InputTimeline& timeline =
            participantId == m_localParticipantId ? m_localInputs : m_remoteInputs;
        uint32_t latestSequence = 0;
        for(const TimelineInputEntry& entry : timeline.entries()) {
            if(entry.participantId != participantId) continue;
            latestSequence = std::max(latestSequence, entry.sequence);
        }
        return latestSequence;
    };

    const uint32_t rebuiltLocalInputSequence = latestSequenceForParticipant(m_localParticipantId);
#if defined(__EMSCRIPTEN__)
    // The browser client can enter authoritative realignment before it has
    // received echoes/acks for every local input it already sent. If we rewind
    // the local sequence counter to the preserved timeline's latest confirmed
    // sequence, the next locally produced inputs reuse old sequence numbers and
    // the host rejects them as stale/duplicate. Keep the sequence monotonic on
    // web unless an explicit sequence reset was requested.
    if(resetInputSequences) {
        m_localInputSequence = inputSequenceBase;
    } else {
        m_localInputSequence = hasPendingSequenceReset(m_localParticipantId)
            ? 0u
            : std::max(m_localInputSequence, rebuiltLocalInputSequence);
    }
#else
    m_localInputSequence = rebuiltLocalInputSequence;
#endif

    if(preserveConfirmedInputs) {
        ConfirmedFrameInputs confirmedFrame;
        confirmedFrame.frame = loadedFrame;
        confirmedFrame.inputFrame = makeRoomTopologyBaseFrame(loadedFrame, m_session.roomState());
        bool haveConfirmedFrame = false;
        for(const ParticipantInfo& participant : m_session.roomState().participants) {
            for(PlayerSlot slot : participantAssignments(participant)) {
                if(!isPlayableSlot(slot)) {
                    continue;
                }
                haveConfirmedFrame = true;

                const TimelineInputEntry* preserved =
                    participant.id == m_localParticipantId
                        ? m_localInputs.find(loadedFrame, participant.id, slot)
                        : m_remoteInputs.find(loadedFrame, participant.id, slot);
                if(preserved != nullptr) {
                    confirmedFrame.buttonMaskLo[slot] = preserved->buttonMaskLo;
                    confirmedFrame.buttonMaskHi[slot] = preserved->buttonMaskHi;
                    applyAssignedContribution(confirmedFrame.inputFrame, slot, preserved->inputFrame);
                }
            }
        }
        if(haveConfirmedFrame) {
            storeConfirmedFrame(confirmedFrame);
        }
    }

    for(ParticipantInfo& participant : m_session.roomState().participants) {
        participant.lastReceivedInputFrame = loadedFrame;
        participant.lastContiguousInputFrame = loadedFrame;
        participant.lastReceivedInputSequence = latestSequenceForParticipant(participant.id);
        participant.sequenceRebasePending = resetInputSequences;
        participant.pendingMissingInputFrom.reset();
        participant.lastDecisionFrame = loadedFrame;
        participant.lastDecisionSlot = kObserverPlayerSlot;
        participant.lastDecision.clear();
        participant.inputSuspended = false;
        participant.inputResumeAwaitingResync = false;
        m_lastRemoteInputAt[participant.id] = std::chrono::steady_clock::now();
    }

    m_localSimulationFrame = loadedFrame;
    m_authoritativeFrameStartClockMicros.clear();
    m_authoritativeFrameStartClockOrder.clear();
    if(m_hosting) {
        const uint64_t loadedFrameClockMicros = sharedClockNowMicros();
        m_authoritativeFrameStartClockMicros[loadedFrame] = loadedFrameClockMicros;
        m_authoritativeFrameStartClockOrder.push_back(loadedFrame);
        m_session.roomState().lastAuthoritativeClockFrame = loadedFrame;
        m_session.roomState().lastAuthoritativeClockMicros = loadedFrameClockMicros;
    } else {
        m_session.roomState().lastAuthoritativeClockFrame = loadedFrame;
        m_session.roomState().lastAuthoritativeClockMicros = 0;
    }
    m_pendingSequenceResetParticipants.clear();

    std::ostringstream oss;
    oss << "Authoritative state realigned"
        << " loadedFrame " << loadedFrame
        << " preserveConfirmedInputs " << (preserveConfirmedInputs ? "1" : "0")
        << " resetInputSequences " << (resetInputSequences ? "1" : "0")
        << " inputSequenceBase " << inputSequenceBase;
    pushLog(oss.str());
}

void NetplayCoordinator::resetRuntimeTimelineStateForSessionStart()
{
    m_localInputs.clear();
    m_remoteInputs.clear();
    m_confirmedFrames.clear();
    m_pendingRollbackFrame.reset();
    m_pendingHostResyncFrame.reset();
    m_pendingHostLateJoinResyncParticipant.reset();
    m_implicitRecoveryMonitor.reset();
    m_pendingResyncApply.reset();
    m_pendingResyncAcks.clear();
    m_activeResyncExpectedStateCrc32 = 0;
    m_activeResyncTargetParticipantId = kInvalidParticipantId;
    m_activeResyncResumeState = SessionState::Lobby;
    m_activeTargetedResyncId = 0;
    m_activeTargetedResyncFrame = 0;
    m_activeTargetedResyncExpectedStateCrc32 = 0;
    m_desyncMonitor.reset();
    m_lastBroadcastConfirmedFrame = 0;
    m_localInputSequence = 0;
    m_localSimulationFrame = 0;
    m_authoritativeFrameStartClockMicros.clear();
    m_authoritativeFrameStartClockOrder.clear();
    m_predictionStats.lastDecision.clear();
    m_predictionStats.lastDecisionFrame = 0;
    m_predictionStats.lastDecisionSlot = kObserverPlayerSlot;
    m_session.roomState().currentFrame = 0;
    m_session.roomState().lastConfirmedFrame = 0;
    m_session.roomState().lastAuthoritativeClockFrame = 0;
    m_session.roomState().lastAuthoritativeClockMicros = 0;
    m_session.roomState().activeResyncId = 0;
    m_session.roomState().resyncTargetFrame = 0;
    m_session.roomState().resyncConfirmedFrame = 0;
    m_session.roomState().resyncFrameReadyFrame = 0;
    m_session.roomState().resyncPayloadSize = 0;
    m_session.roomState().resyncPayloadCrc32 = 0;
    m_session.roomState().resyncFrameReadyCrc32 = 0;
    m_session.roomState().resyncInputSequenceBase = 0;
    m_session.roomState().pendingResyncAckCount = 0;

    for(ParticipantInfo& participant : m_session.roomState().participants) {
        participant.lastReceivedInputFrame = 0;
        participant.lastContiguousInputFrame = 0;
        participant.lastReceivedInputSequence = 0;
        participant.sequenceRebasePending = false;
        participant.pendingMissingInputFrom.reset();
        participant.lastDecision.clear();
        participant.lastDecisionFrame = 0;
        participant.lastDecisionSlot = kObserverPlayerSlot;
        participant.inputSuspended = false;
        participant.inputResumeAwaitingResync = false;
        m_lastRemoteInputAt[participant.id] = std::chrono::steady_clock::now();
    }
}

void NetplayCoordinator::discardTimelineStateAfter(FrameNumber frame)
{
    m_localInputs.eraseFramesAfter(frame);
    m_remoteInputs.eraseFramesAfter(frame);

    while(!m_confirmedFrames.empty() && m_confirmedFrames.back().frame > frame) {
        m_confirmedFrames.pop_back();
    }

    m_lastBroadcastConfirmedFrame = std::min(m_lastBroadcastConfirmedFrame, frame);
    m_session.roomState().lastConfirmedFrame = std::min(m_session.roomState().lastConfirmedFrame, frame);
    for(auto it = m_authoritativeFrameStartClockMicros.begin();
        it != m_authoritativeFrameStartClockMicros.end();) {
        if(it->first > frame) {
            it = m_authoritativeFrameStartClockMicros.erase(it);
        } else {
            ++it;
        }
    }
    while(!m_authoritativeFrameStartClockOrder.empty() &&
          m_authoritativeFrameStartClockOrder.back() > frame) {
        m_authoritativeFrameStartClockOrder.pop_back();
    }
    if(m_session.roomState().lastAuthoritativeClockFrame > frame) {
        m_session.roomState().lastAuthoritativeClockFrame = frame;
        m_session.roomState().lastAuthoritativeClockMicros =
            authoritativeFrameStartClockMicros(frame);
    }

    for(ParticipantInfo& participant : m_session.roomState().participants) {
        const InputTimeline& timeline =
            participant.id == m_localParticipantId ? m_localInputs : m_remoteInputs;

        const TimelineInputEntry* latestForParticipant = nullptr;
        for(auto it = timeline.entries().rbegin(); it != timeline.entries().rend(); ++it) {
            if(it->participantId != participant.id) continue;
            latestForParticipant = &(*it);
            break;
        }

        if(latestForParticipant != nullptr) {
            participant.lastReceivedInputFrame = latestForParticipant->frame;
            participant.lastContiguousInputFrame = latestForParticipant->frame;
            participant.lastReceivedInputSequence = latestForParticipant->sequence;
        } else {
            participant.lastReceivedInputFrame = frame;
            participant.lastContiguousInputFrame = frame;
            participant.lastReceivedInputSequence = 0;
        }

        participant.sequenceRebasePending = false;
        participant.pendingMissingInputFrom.reset();
        participant.inputSuspended = false;
        participant.inputResumeAwaitingResync = false;
        m_lastRemoteInputAt[participant.id] = std::chrono::steady_clock::now();
    }
}

void NetplayCoordinator::seedNeutralInputBaseline(ParticipantId participantId, PlayerSlot slot, FrameNumber frame)
{
    if(participantId == kInvalidParticipantId || slot == kObserverPlayerSlot || !isPlayableSlot(slot)) return;

    InputTimeline* timeline = &m_remoteInputs;
    if(participantId == m_localParticipantId) {
        timeline = &m_localInputs;
    }

    if(timeline->find(frame, participantId, slot) != nullptr) {
        return;
    }

    TimelineInputEntry entry;
    entry.frame = frame;
    entry.participantId = participantId;
    entry.playerSlot = slot;
    entry.buttonMaskLo = 0;
    entry.buttonMaskHi = 0;
    entry.inputFrame = makeContributionBase(makeRoomTopologyBaseFrame(frame, m_session.roomState()));
    entry.sequence = 0;
    entry.predicted = false;
    entry.confirmed = true;
    timeline->push(entry);
}

bool NetplayCoordinator::handleAssignController(PacketReader& reader)
{
    AssignControllerData data;
    if(!reader.readPod(data)) return false;

    if(ParticipantInfo* participant = m_session.findParticipant(data.participantId)) {
        const FrameNumber assignmentBaselineFrame =
            std::max({m_localSimulationFrame,
                      m_session.roomState().currentFrame,
                      m_session.roomState().lastConfirmedFrame});
        const std::vector<PlayerSlot> previousAssignments = participant->controllerAssignments;
        participant->controllerAssignments.assign(
            data.controllerAssignments.begin(),
            data.controllerAssignments.begin() + std::min<size_t>(data.assignmentCount, data.controllerAssignments.size())
        );
        participant->controllerAssignments.erase(
            std::remove_if(
                participant->controllerAssignments.begin(),
                participant->controllerAssignments.end(),
                [](PlayerSlot slot) {
                    return slot != kObserverPlayerSlot && slot > kMultitapP4PlayerSlot;
                }
            ),
            participant->controllerAssignments.end()
        );
        participant->normalizeControllerAssignments();
        const bool keepHostRole = participant->id == m_localParticipantId && m_hosting;
        syncParticipantRoleWithAssignments(*participant, keepHostRole);
        const bool assignmentChanged = previousAssignments != participant->controllerAssignments;
        if(assignmentChanged) {
            discardTimelineStateAfter(assignmentBaselineFrame);
            participant->lastReceivedInputFrame = assignmentBaselineFrame;
            participant->lastContiguousInputFrame = assignmentBaselineFrame;
            participant->lastReceivedInputSequence = 0;
            // Assignment changes realign the input baseline. The sender may
            // continue with a monotonic sequence counter, so allow a one-time
            // sequence rebase on the first post-assignment frame.
            participant->sequenceRebasePending = true;
            participant->pendingMissingInputFrom.reset();
            if(data.participantId == m_localParticipantId) {
                m_localInputSequence = 0;
            }
        }
        for(PlayerSlot slot : participant->controllerAssignments) {
            participant->lastReceivedInputFrame = assignmentBaselineFrame;
            participant->lastContiguousInputFrame = assignmentBaselineFrame;
            seedNeutralInputBaseline(data.participantId, slot, assignmentBaselineFrame);
        }
        if(participant->controllerAssignments.empty()) {
            pushLog(controllerAssignmentToast(kObserverPlayerSlot, m_session.roomState(), participantLabel(*participant)));
        } else {
            for(PlayerSlot slot : participant->controllerAssignments) {
                pushLog(controllerAssignmentToast(slot, m_session.roomState(), participantLabel(*participant)));
            }
        }
        return true;
    }

    return false;
}

bool NetplayCoordinator::handleSelectRom(PacketReader& reader)
{
    std::string gameName;
    RomValidationData romValidation;
    if(!reader.readString(gameName)) return false;
    if(!reader.readPod(romValidation)) return false;
    InputTopologyData topology;
    if(!reader.readPod(topology)) return false;

    const bool activeSession =
        m_session.roomState().state == SessionState::Starting ||
        m_session.roomState().state == SessionState::Running ||
        m_session.roomState().state == SessionState::Paused ||
        m_session.roomState().state == SessionState::Resyncing;
    m_session.roomState().selectedGameName = gameName;
    m_session.roomState().romValidation = romValidation;
    applyTopologyData(m_session.roomState(), topology);
    if(!activeSession) {
        m_session.roomState().state = SessionState::ValidatingRom;
        for(ParticipantInfo& participant : m_session.roomState().participants) {
            if(participant.id != m_localParticipantId) {
                participant.romLoaded = false;
                participant.romCompatible = false;
            }
        }
    }

    pushLog("Selected ROM: " + gameName);
    return true;
}

bool NetplayCoordinator::handleRomValidationResult(NetTransport::PeerHandle peer, PacketReader& reader)
{
    RomValidationResultData result;
    if(!reader.readPod(result)) return false;

    if(ParticipantInfo* participant = m_session.findParticipant(result.participantId)) {
        participant->romLoaded = result.romLoaded != 0;
        participant->romCompatible = result.romCompatible != 0;

        if(m_hosting) {
            m_transport.broadcastReliable(Channel::Control, buildRomValidationResultPacket(result), peer);
            refreshHostRoomState();
            if(m_pendingHostLateJoinResyncParticipant.has_value() &&
               *m_pendingHostLateJoinResyncParticipant == participant->id &&
               participant->connected &&
               participant->romLoaded &&
               participant->romCompatible &&
               (m_session.roomState().state == SessionState::Starting ||
                m_session.roomState().state == SessionState::Running ||
                m_session.roomState().state == SessionState::Paused ||
                m_session.roomState().state == SessionState::Resyncing)) {
                m_pendingHostLateJoinResyncParticipant.reset();
                const FrameNumber resyncFrame =
                    std::min<FrameNumber>(m_localSimulationFrame, m_session.roomState().lastConfirmedFrame);
                queuePendingHostResync(resyncFrame, ResyncReason::InitialSessionSync);
                pushLog("Late join validated; scheduling automatic resync");
            }
        }
        return true;
    }

    return false;
}

bool NetplayCoordinator::handleParticipantLeft(PacketReader& reader)
{
    ParticipantLeftData data;
    if(!reader.readPod(data)) return false;

    const ParticipantInfo* participant = m_session.findParticipant(data.participantId);
    const std::string participantName =
        participant != nullptr && !participant->displayName.empty()
            ? participant->displayName
            : std::to_string(static_cast<int>(data.participantId));

    const bool hostLeft =
        !m_hosting &&
        data.participantId != kInvalidParticipantId &&
        data.participantId != m_localParticipantId &&
        data.participantId == 0;
    const bool localParticipantKicked =
        !m_hosting &&
        data.participantId != kInvalidParticipantId &&
        data.participantId == m_localParticipantId;

    removeParticipant(data.participantId);
    if(hostLeft) {
        pushLog("Owner left the room");
        pushToast("Owner left the room");
        m_serverPeer = NetTransport::kInvalidPeerHandle;
        m_connected = false;
        m_session.roomState().state = SessionState::Ended;
        m_lastError = "Owner closed the room";
        clearReconnectAttemptState();
    } else if(localParticipantKicked) {
        pushLog("Removed from room by host");
        pushToast("Removed from room by host");
        m_connected = false;
        m_session.roomState().state = SessionState::Ended;
        m_lastError = "Removed from room by host";
        clearReconnectAttemptState();
        completeLocalDisconnect();
        m_lastError = "Removed from room by host";
    } else {
        pushLog("Participant left: " + participantName);
        pushToast(participantName + " left");
    }
    return true;
}

bool NetplayCoordinator::handleLeaveRoom(NetTransport::PeerHandle peer, PacketReader& reader)
{
    LeaveRoomData data;
    if(!reader.readPod(data)) return false;
    if(!m_hosting) return true;

    const ParticipantId participantId = participantIdFromPeer(peer);
    if(participantId == kInvalidParticipantId || participantId != data.participantId) {
        pushLog("Ignored leave-room request with mismatched participant id");
        return false;
    }

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr) {
        return true;
    }

    const std::string name =
        !participant->displayName.empty()
            ? participant->displayName
            : std::to_string(static_cast<int>(participantId));

    removeParticipant(participantId);
    m_reconnectReservationDeadlines.erase(participantId);
    m_transport.broadcastReliable(Channel::Control, buildParticipantLeftPacket(participantId), peer);
    m_transport.flush();
    refreshHostRoomState();
    pushLog("Participant left gracefully: " + name);
    pushToast(name + " left");
    return true;
}

bool NetplayCoordinator::handleResyncBegin(PacketReader& reader)
{
    ResyncBeginData data;
    if(!reader.readPod(data)) return false;
    if(data.payloadSize > kMaxIncomingResyncPayloadBytes) {
        pushLog("Rejected resync begin: payload exceeds safety limit");
        return false;
    }
    if(m_pendingResyncApply.has_value() &&
       m_pendingResyncApply->resyncId != data.resyncId) {
        std::ostringstream oss;
        oss << "Discarding stale pending resync apply"
            << " pendingResyncId " << m_pendingResyncApply->resyncId
            << " incomingResyncId " << data.resyncId;
        pushLog(oss.str());
    }
    m_pendingResyncApply.reset();
    const bool preserveInputSequences = data.reason == ResyncReason::ObserverVisibilityRestore;

    realignAuthoritativeState(
        data.targetFrame,
        !preserveInputSequences,
        data.inputSequenceBase,
        preserveConfirmedInputsAcrossRealignment(data.reason)
    );
    m_incomingResync = IncomingResyncTransfer{};
    m_incomingResync->resyncId = data.resyncId;
    m_incomingResync->targetFrame = data.targetFrame;
    m_incomingResync->expectedPayloadCrc32 = data.payloadCrc32;
    m_incomingResync->payload.resize(data.payloadSize);
    m_incomingResync->receivedMask.assign(data.payloadSize, 0);
    m_incomingResync->lastActivityAt = std::chrono::steady_clock::now();

    m_session.roomState().activeResyncId = data.resyncId;
    m_session.roomState().timelineEpoch = data.timelineEpoch;
    m_session.roomState().resyncTargetFrame = data.targetFrame;
    m_session.roomState().resyncConfirmedFrame = data.confirmedFrame;
    m_session.roomState().resyncFrameReadyFrame = data.frameReadyFrame;
    m_session.roomState().resyncPayloadSize = data.payloadSize;
    m_session.roomState().resyncPayloadCrc32 = data.payloadCrc32;
    m_session.roomState().resyncFrameReadyCrc32 = data.frameReadyCrc32;
    m_session.roomState().resyncInputSequenceBase = data.inputSequenceBase;
    m_session.roomState().activeResyncReason = data.reason;
    m_session.roomState().state = SessionState::Resyncing;
    setRecoveryInputMode(RecoveryInputMode::ResyncLocked, "resync-begin-received", data.targetFrame);

    {
        std::ostringstream oss;
        oss << "Received authoritative resync begin"
            << " reason " << resyncReasonLabel(data.reason)
            << " targetFrame " << data.targetFrame
            << " epoch " << data.timelineEpoch
            << " resyncId " << data.resyncId;
        pushLog(oss.str());
    }

    const std::string toast = resyncReasonToast(data.reason);
    if(!toast.empty()) {
        pushToast(toast);
    }

    return true;
}

bool NetplayCoordinator::handleResyncChunk(PacketReader& reader)
{
    ResyncChunkData data;
    if(!reader.readPod(data)) return false;
    if(!m_incomingResync.has_value() || m_incomingResync->resyncId != data.resyncId) return false;
    const size_t payloadSize = m_incomingResync->payload.size();
    const size_t offset = static_cast<size_t>(data.offset);
    const size_t chunkSize = static_cast<size_t>(data.size);
    if(offset > payloadSize || chunkSize > (payloadSize - offset)) return false;

    std::vector<uint8_t> chunk;
    if(!reader.readBytes(chunk, data.size)) return false;
    m_incomingResync->lastActivityAt = std::chrono::steady_clock::now();

    if(data.size > 0) {
        std::memcpy(m_incomingResync->payload.data() + offset, chunk.data(), chunkSize);
        std::fill_n(m_incomingResync->receivedMask.begin() + offset, chunkSize, uint8_t{1});
    }
    return true;
}

bool NetplayCoordinator::handleResyncComplete(PacketReader& reader)
{
    ResyncCompleteData data;
    if(!reader.readPod(data)) return false;
    if(!m_incomingResync.has_value() || m_incomingResync->resyncId != data.resyncId) return false;

    if(std::find(m_incomingResync->receivedMask.begin(), m_incomingResync->receivedMask.end(), uint8_t{0}) != m_incomingResync->receivedMask.end()) {
        pushLog("Resync payload incomplete");
        if(!m_hosting && m_serverPeer != NetTransport::kInvalidPeerHandle) {
            ResyncAbortData abort;
            abort.resyncId = data.resyncId;
            abort.participantId = m_localParticipantId;
            abort.reason = 1;
            m_transport.sendReliable(m_serverPeer, Channel::Control, buildResyncAbortPacket(abort));
        }
        m_incomingResync.reset();
        return true;
    }

    const uint32_t payloadCrc32 = Crc32::calc(reinterpret_cast<const char*>(m_incomingResync->payload.data()), m_incomingResync->payload.size());
    if(payloadCrc32 != m_incomingResync->expectedPayloadCrc32) {
        pushLog("Resync payload CRC mismatch");
        if(!m_hosting && m_serverPeer != NetTransport::kInvalidPeerHandle) {
            ResyncAbortData abort;
            abort.resyncId = data.resyncId;
            abort.participantId = m_localParticipantId;
            abort.reason = 2;
            m_transport.sendReliable(m_serverPeer, Channel::Control, buildResyncAbortPacket(abort));
        }
        m_incomingResync.reset();
        return true;
    }

    PendingResyncApply pending;
    pending.resyncId = m_incomingResync->resyncId;
    pending.targetFrame = m_incomingResync->targetFrame;
    pending.confirmedFrame = m_session.roomState().resyncConfirmedFrame;
    pending.frameReadyFrame = m_session.roomState().resyncFrameReadyFrame;
    pending.expectedPayloadCrc32 = m_incomingResync->expectedPayloadCrc32;
    pending.frameReadyCrc32 = m_session.roomState().resyncFrameReadyCrc32;
    pending.inputSequenceBase = m_session.roomState().resyncInputSequenceBase;
    pending.reason = m_session.roomState().activeResyncReason;
    pending.payload = std::move(m_incomingResync->payload);
    m_pendingResyncApply = std::move(pending);
    {
        std::ostringstream oss;
        oss << "Queued authoritative resync apply"
            << " reason " << resyncReasonLabel(m_session.roomState().activeResyncReason)
            << " targetFrame " << m_pendingResyncApply->targetFrame
            << " confirmedFrame " << m_pendingResyncApply->confirmedFrame
            << " frameReadyFrame " << m_pendingResyncApply->frameReadyFrame;
        pushLog(oss.str());
    }
    m_incomingResync.reset();
    return true;
}

bool NetplayCoordinator::handleResyncAck(PacketReader& reader)
{
    ResyncAckData data;
    if(!reader.readPod(data)) return false;
    const bool targetedResync = activeResyncIsTargeted();
    const uint32_t activeResyncId =
        targetedResync ? m_activeTargetedResyncId : m_session.roomState().activeResyncId;
    if(!m_hosting || data.resyncId != activeResyncId) return true;
    const ParticipantInfo* ackParticipant = m_session.findParticipant(data.participantId);
    const bool ackFromObserver =
        ackParticipant != nullptr && participantIsObserver(*ackParticipant);

    if(data.success == 0) {
        if(targetedResync) {
            cancelTargetedResync(
                "Targeted resync ACK failure from participant " +
                std::to_string(static_cast<int>(data.participantId)) +
                "; cancelling targeted observer resync"
            );
            return true;
        }
        if(ackFromObserver) {
            m_pendingResyncAcks.erase(
                std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), data.participantId),
                m_pendingResyncAcks.end()
            );
            if(m_pendingResyncAcks.empty()) {
                finalizeActiveResyncIfReady();
            } else {
                m_activeResyncAckDeadline = std::chrono::steady_clock::now() + kResyncAckTimeout;
                m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
            }
            pushLog(
                "Observer resync ACK failure ignored for participant " +
                std::to_string(static_cast<int>(data.participantId)) +
                "; continuing authoritative resync"
            );
            return true;
        }
        (void)ejectParticipantForResyncFailure(
            data.participantId,
            "Resync ACK failure from participant " + std::to_string(static_cast<int>(data.participantId)) +
                "; removing participant to keep room running"
        );
        return true;
    }

    const FrameNumber expectedTargetFrame =
        targetedResync ? m_activeTargetedResyncFrame : m_session.roomState().resyncTargetFrame;
    if(data.loadedFrame != expectedTargetFrame) {
        if(targetedResync) {
            cancelTargetedResync(
                "Targeted resync ACK loaded unexpected frame from participant " +
                std::to_string(static_cast<int>(data.participantId)) +
                "; cancelling targeted observer resync"
            );
            return true;
        }
        if(ackFromObserver) {
            m_pendingResyncAcks.erase(
                std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), data.participantId),
                m_pendingResyncAcks.end()
            );
            if(m_pendingResyncAcks.empty()) {
                finalizeActiveResyncIfReady();
            } else {
                m_activeResyncAckDeadline = std::chrono::steady_clock::now() + kResyncAckTimeout;
                m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
            }
            pushLog(
                "Observer resync ACK frame mismatch ignored for participant " +
                std::to_string(static_cast<int>(data.participantId)) +
                "; continuing authoritative resync"
            );
            return true;
        }
        (void)ejectParticipantForResyncFailure(
            data.participantId,
            "Resync ACK loaded unexpected frame from participant " +
                std::to_string(static_cast<int>(data.participantId)) +
                "; removing participant to keep room running"
        );
        return true;
    }

    const uint32_t expectedStateCrc32 =
        targetedResync ? m_activeTargetedResyncExpectedStateCrc32 : m_activeResyncExpectedStateCrc32;
    if(expectedStateCrc32 != 0 &&
       data.crc32 != expectedStateCrc32) {
        if(targetedResync) {
            cancelTargetedResync(
                "Targeted resync ACK state CRC mismatch from participant " +
                std::to_string(static_cast<int>(data.participantId)) +
                "; cancelling targeted observer resync"
            );
            return true;
        }
        if(ackFromObserver) {
            m_pendingResyncAcks.erase(
                std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), data.participantId),
                m_pendingResyncAcks.end()
            );
            if(m_pendingResyncAcks.empty()) {
                finalizeActiveResyncIfReady();
            } else {
                m_activeResyncAckDeadline = std::chrono::steady_clock::now() + kResyncAckTimeout;
                m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
            }
            pushLog(
                "Observer resync ACK CRC mismatch ignored for participant " +
                std::to_string(static_cast<int>(data.participantId)) +
                "; continuing authoritative resync"
            );
            return true;
        }
        (void)ejectParticipantForResyncFailure(
            data.participantId,
            "Resync ACK state CRC mismatch from participant " +
                std::to_string(static_cast<int>(data.participantId)) +
                "; removing participant to keep room running"
        );
        return true;
    }

    m_pendingResyncAcks.erase(
        std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), data.participantId),
        m_pendingResyncAcks.end()
    );

    if(m_pendingResyncAcks.empty()) {
        finalizeActiveResyncIfReady();
    } else {
        m_activeResyncAckDeadline = std::chrono::steady_clock::now() + kResyncAckTimeout;
        m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
    }

    return true;
}

bool NetplayCoordinator::handleResyncAbort(PacketReader& reader)
{
    ResyncAbortData data;
    if(!reader.readPod(data)) return false;
    if(activeResyncIsTargeted()) {
        if(!m_hosting || data.resyncId != m_activeTargetedResyncId) return true;
        cancelTargetedResync(
            "Targeted resync aborted by participant " +
            std::to_string(static_cast<int>(data.participantId)) +
            "; cancelling targeted observer resync"
        );
        return true;
    }
    if(!m_hosting || data.resyncId != m_session.roomState().activeResyncId) return true;
    if(const ParticipantInfo* participant = m_session.findParticipant(data.participantId);
       participant != nullptr && participantIsObserver(*participant)) {
        m_pendingResyncAcks.erase(
            std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), data.participantId),
            m_pendingResyncAcks.end()
        );
        if(m_pendingResyncAcks.empty()) {
            finalizeActiveResyncIfReady();
        } else {
            m_activeResyncAckDeadline = std::chrono::steady_clock::now() + kResyncAckTimeout;
            m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
        }
        pushLog(
            "Observer resync abort ignored for participant " +
            std::to_string(static_cast<int>(data.participantId)) +
            "; continuing authoritative resync"
        );
        return true;
    }

    (void)ejectParticipantForResyncFailure(
        data.participantId,
        "Resync aborted by participant " + std::to_string(static_cast<int>(data.participantId)) +
            "; removing participant to keep room running"
    );
    return true;
}

bool NetplayCoordinator::handleResyncRequest(NetTransport::PeerHandle peer, PacketReader& reader)
{
    ResyncRequestData data;
    if(!reader.readPod(data)) return false;
    if(!m_hosting) return true;

    ParticipantInfo* participant = m_session.findParticipant(data.participantId);
    if(participant == nullptr || participantIdFromPeer(peer) != data.participantId) {
        pushLog("Ignored resync request from unknown participant");
        return true;
    }

    if(!participant->connected || participant->reconnectReserved) {
        pushLog("Ignored resync request from disconnected participant");
        return true;
    }

    const SessionState state = m_session.roomState().state;
    if(state != SessionState::Running && state != SessionState::Paused) {
        return true;
    }

    if(m_session.roomState().activeResyncId != 0 || m_activeTargetedResyncId != 0) {
        return true;
    }

    const FrameNumber resyncFrame =
        std::min(m_localSimulationFrame, m_session.roomState().lastConfirmedFrame);
    const ParticipantId targetParticipantId =
        data.reason == ResyncReason::ObserverVisibilityRestore && participantIsObserver(*participant)
            ? participant->id
            : kInvalidParticipantId;
    queuePendingHostResync(resyncFrame, data.reason, targetParticipantId);

    std::ostringstream oss;
    oss << "Participant requested authoritative resync: " << participant->displayName
        << " reason " << resyncReasonLabel(data.reason)
        << " frame " << resyncFrame;
    pushLog(oss.str());
    return true;
}

bool NetplayCoordinator::handleClockSyncRequest(NetTransport::PeerHandle peer, PacketReader& reader)
{
    ClockSyncRequestData data;
    if(!reader.readPod(data)) return false;
    if(!m_hosting) return true;

    const int64_t receiveMicros = monotonicNowMicros();
    ClockSyncResponseData response;
    response.sequence = data.sequence;
    response.clientSendMicros = data.clientSendMicros;
    response.hostReceiveMicros = receiveMicros > 0 ? static_cast<uint64_t>(receiveMicros) : 0u;
    const int64_t sendMicros = monotonicNowMicros();
    response.hostSendMicros = sendMicros > 0 ? static_cast<uint64_t>(sendMicros) : 0u;
    m_transport.sendUnreliable(peer, Channel::Diagnostics, buildClockSyncResponsePacket(response));
    return true;
}

bool NetplayCoordinator::handleClockSyncResponse(NetTransport::PeerHandle peer, PacketReader& reader)
{
    ClockSyncResponseData data;
    if(!reader.readPod(data)) return false;
    if(m_hosting || m_serverPeer == NetTransport::kInvalidPeerHandle || peer != m_serverPeer) return true;

    auto it = m_pendingClockSyncRequests.find(data.sequence);
    if(it == m_pendingClockSyncRequests.end()) {
        return true;
    }
    const PendingClockSyncRequest pending = it->second;
    m_pendingClockSyncRequests.erase(it);

    if(data.clientSendMicros != static_cast<uint64_t>(std::max<int64_t>(0, pending.clientSendMicros))) {
        return true;
    }

    const int64_t t1 = pending.clientSendMicros;
    const int64_t t2 = static_cast<int64_t>(data.hostReceiveMicros);
    const int64_t t3 = static_cast<int64_t>(data.hostSendMicros);
    const int64_t t4 = monotonicNowMicros();
    if(t1 <= 0 || t2 <= 0 || t3 <= 0 || t4 <= 0 || t3 < t2 || t4 < t1) {
        return true;
    }

    const int64_t rawDelay = (t4 - t1) - (t3 - t2);
    const uint64_t delayMicros = rawDelay > 0 ? static_cast<uint64_t>(rawDelay) : 0u;
    const int64_t offsetMicros = ((t2 - t1) + (t3 - t4)) / 2;

    if(!m_sharedClockSynchronized) {
        m_sharedClockOffsetMicros = offsetMicros;
        m_bestClockSyncDelayMicros = delayMicros;
        m_sharedClockSynchronized = true;
    } else {
        if(delayMicros < m_bestClockSyncDelayMicros) {
            m_bestClockSyncDelayMicros = delayMicros;
        }
        const bool acceptableSample =
            delayMicros <= (m_bestClockSyncDelayMicros + 2000u) ||
            delayMicros <= (m_sharedClockRttMicros + 2000u);
        if(acceptableSample) {
            m_sharedClockOffsetMicros =
                (m_sharedClockOffsetMicros * 7 + offsetMicros) / 8;
        }
    }
    m_sharedClockRttMicros = delayMicros;
    return true;
}

bool NetplayCoordinator::handlePeerHealth(NetTransport::PeerHandle peer, PacketReader& reader)
{
    PeerHealthData data;
    if(!reader.readPod(data)) return false;

    if(ParticipantInfo* participant = m_session.findParticipant(data.participantId)) {
        m_lastPeerHealthAt[participant->id] = std::chrono::steady_clock::now();
        const FrameNumber previousReportedCurrentFrame = participant->lastReportedCurrentFrame;
        participant->pingMs = data.pingMs;
        participant->jitterMs = data.jitterMs;
        participant->lastReportedCurrentFrame = data.currentFrame;
        participant->lastReportedConfirmedFrame = data.lastConfirmedFrame;
        participant->sharedClockMicros = data.sharedClockMicros;
        participant->clockSyncRttMicros = data.clockSyncRttMicros;
        participant->sharedClockSynchronized = data.sharedClockSynchronized != 0;
        participant->sharedClockSampledAtLocalMicros =
            static_cast<uint64_t>(std::max<int64_t>(0, monotonicNowMicros()));
        ++participant->peerHealthSerial;
        if(m_hosting &&
           participant->connected &&
           !participantIsObserver(*participant) &&
           data.currentFrame > previousReportedCurrentFrame) {
            // If peer health reports forward simulation progress, treat this as
            // fresh activity so input-timeout suspension does not false-trigger
            // during temporary packet ordering/duplication anomalies.
            m_lastRemoteInputAt[participant->id] = std::chrono::steady_clock::now();
        }
        tryScheduleImplicitRecoveryResync(*participant);
    }

    if(m_hosting) {
        m_transport.broadcastUnreliable(
            Channel::Diagnostics,
            buildPeerHealthPacket(data, m_session.roomState().sessionId),
            peer
        );
    }

    return true;
}

bool NetplayCoordinator::handleStartSession(PacketReader& reader)
{
    StartSessionData data;
    if(!reader.readPod(data)) return false;

    const SessionState previousState = m_session.roomState().state;
    if(data.state == SessionState::Starting) {
        resetRuntimeTimelineStateForSessionStart();
    }
    m_session.roomState().inputDelayFrames = data.inputDelayFrames;
    m_session.roomState().predictFrames = data.predictFrames;
    applyTopologyData(m_session.roomState(), data.topology);
    m_session.roomState().state = data.state;
    const bool shouldResetConfirmedState =
        data.state == SessionState::Starting ||
        (data.state == SessionState::Running &&
         previousState != SessionState::Starting &&
         previousState != SessionState::Resyncing);
    if(shouldResetConfirmedState) {
        m_confirmedFrames.clear();
        m_lastBroadcastConfirmedFrame = 0;
        m_session.roomState().lastConfirmedFrame = 0;
    }
    if(data.state == SessionState::Starting) {
        pushLog("Session starting: waiting for authoritative sync");
        setRecoveryInputMode(RecoveryInputMode::ResyncLocked, "session-starting", m_session.roomState().currentFrame);
    } else if(data.state == SessionState::Ended) {
        clearReconnectAttemptState();
        m_disconnectExpectedAfterHostShutdown = true;
        m_lastError = "Owner closed the room";
        pushLog("Owner closed the room");
        pushToast("Owner closed the room");
        setRecoveryInputMode(RecoveryInputMode::Normal, "session-ended", m_session.roomState().currentFrame);
    } else if(data.state == SessionState::Running &&
              (previousState == SessionState::Resyncing || previousState == SessionState::Starting)) {
        const FrameNumber stabilizationAnchor =
            std::max(m_session.roomState().currentFrame, m_session.roomState().lastConfirmedFrame);
        setRecoveryInputMode(
            RecoveryInputMode::PostResyncStabilizing,
            "session-running-after-recovery",
            stabilizationAnchor,
            kRecoveryStabilizationFrames
        );
    } else {
        if(data.state == SessionState::Paused && previousState != SessionState::Paused) {
            pushLog("Owner paused");
            pushToast("Owner paused");
        } else if(data.state == SessionState::Running && previousState == SessionState::Paused) {
            pushLog("Owner resumed");
            pushToast("Owner resumed");
        }
        pushLog(data.state == SessionState::Running ? "Session started" : "Session state updated");
        if(data.state != SessionState::Resyncing && data.state != SessionState::Paused) {
            setRecoveryInputMode(RecoveryInputMode::Normal, "session-state-update", m_session.roomState().currentFrame);
        }
    }
    return true;
}

void NetplayCoordinator::updatePeerHealthFromTransport()
{
    if(!m_transport.isActive()) return;

    for(ParticipantInfo& participant : m_session.roomState().participants) {
        if(participant.id == m_localParticipantId) {
            participant.pingMs = 0;
            participant.jitterMs = 0;
        }
    }

    for(NetTransport::PeerHandle peer : m_transport.connectedPeers()) {
        const ParticipantId participantId = participantIdFromPeer(peer);
        if(ParticipantInfo* participant = m_session.findParticipant(participantId)) {
            participant->pingMs = static_cast<uint16_t>(std::min<uint32_t>(m_transport.peerRoundTripTime(peer), 65535u));
            participant->jitterMs = static_cast<uint16_t>(std::min<uint32_t>(m_transport.peerRoundTripVariance(peer), 65535u));
        }
    }
}

void NetplayCoordinator::broadcastPeerHealthIfNeeded()
{
    if(!m_transport.isActive() || !m_connected || m_localParticipantId == kInvalidParticipantId) return;

    const auto now = std::chrono::steady_clock::now();
    if(m_lastPeerHealthBroadcast.time_since_epoch().count() != 0) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastPeerHealthBroadcast);
        if(elapsed.count() < 500) return;
    }
    m_lastPeerHealthBroadcast = now;

    PeerHealthData data;
    data.participantId = m_localParticipantId;
    data.currentFrame = m_session.roomState().currentFrame;
    data.lastConfirmedFrame = m_session.roomState().lastConfirmedFrame;
    data.sharedClockMicros = sharedClockNowMicros();
    data.clockSyncRttMicros = m_sharedClockRttMicros;
    data.sharedClockSynchronized = (m_hosting || m_sharedClockSynchronized) ? 1u : 0u;
    if(ParticipantInfo* participant = m_session.findParticipant(m_localParticipantId)) {
        participant->sharedClockMicros = data.sharedClockMicros;
        participant->clockSyncRttMicros = data.clockSyncRttMicros;
        participant->sharedClockSynchronized = data.sharedClockSynchronized != 0;
        participant->sharedClockSampledAtLocalMicros =
            static_cast<uint64_t>(std::max<int64_t>(0, monotonicNowMicros()));
    }

    if(m_hosting) {
        m_transport.broadcastUnreliable(
            Channel::Diagnostics,
            buildPeerHealthPacket(data, m_session.roomState().sessionId)
        );
    } else if(m_serverPeer != NetTransport::kInvalidPeerHandle) {
        data.pingMs = static_cast<uint16_t>(std::min<uint32_t>(m_transport.peerRoundTripTime(m_serverPeer), 65535u));
        data.jitterMs = static_cast<uint16_t>(std::min<uint32_t>(m_transport.peerRoundTripVariance(m_serverPeer), 65535u));

        if(ParticipantInfo* participant = m_session.findParticipant(m_localParticipantId)) {
            participant->pingMs = data.pingMs;
            participant->jitterMs = data.jitterMs;
            participant->sharedClockMicros = data.sharedClockMicros;
            participant->clockSyncRttMicros = data.clockSyncRttMicros;
            participant->sharedClockSynchronized = data.sharedClockSynchronized != 0;
            participant->sharedClockSampledAtLocalMicros =
                static_cast<uint64_t>(std::max<int64_t>(0, monotonicNowMicros()));
        }

        m_transport.sendUnreliable(
            m_serverPeer,
            Channel::Diagnostics,
            buildPeerHealthPacket(data, m_session.roomState().sessionId)
        );
    }
}

void NetplayCoordinator::processClockSyncIfNeeded(const std::chrono::steady_clock::time_point& now)
{
    if(!m_transport.isActive() || !m_connected || m_hosting || m_serverPeer == NetTransport::kInvalidPeerHandle) {
        return;
    }

    for(auto it = m_pendingClockSyncRequests.begin(); it != m_pendingClockSyncRequests.end();) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.sentAt);
        if(elapsed.count() >= 10) {
            it = m_pendingClockSyncRequests.erase(it);
        } else {
            ++it;
        }
    }

    const auto interval = m_sharedClockSynchronized
        ? std::chrono::milliseconds(1000)
        : std::chrono::milliseconds(250);
    if(m_lastClockSyncRequestAt.time_since_epoch().count() != 0 &&
       (now - m_lastClockSyncRequestAt) < interval) {
        return;
    }
    m_lastClockSyncRequestAt = now;

    const uint32_t sequence = m_nextClockSyncSequence++;
    const int64_t clientSendMicros = monotonicNowMicros();
    if(clientSendMicros <= 0) {
        return;
    }

    ClockSyncRequestData request;
    request.sequence = sequence;
    request.clientSendMicros = static_cast<uint64_t>(clientSendMicros);

    PendingClockSyncRequest pending;
    pending.sequence = sequence;
    pending.clientSendMicros = clientSendMicros;
    pending.sentAt = now;
    m_pendingClockSyncRequests[sequence] = pending;

    m_transport.sendUnreliable(
        m_serverPeer,
        Channel::Diagnostics,
        buildClockSyncRequestPacket(request)
    );
}

FrameNumber NetplayCoordinator::computeHostInputConfirmedFrame() const
{
    FrameNumber confirmedFrame = 0;
    bool anyAssigned = false;

    for(const ParticipantInfo& participant : m_session.roomState().participants) {
        if(participantIsObserver(participant)) continue;

        FrameNumber latestFrame = 0;
        if(participant.id == m_localParticipantId) {
            bool anyLocalAssigned = false;
            for(PlayerSlot slot : participantAssignments(participant)) {
                if(const TimelineInputEntry* latest = m_localInputs.latestFor(participant.id, slot)) {
                    latestFrame = anyLocalAssigned ? std::min(latestFrame, latest->frame) : latest->frame;
                    anyLocalAssigned = true;
                }
            }
        } else {
            latestFrame = participant.lastContiguousInputFrame;
        }

        if(!anyAssigned) {
            confirmedFrame = latestFrame;
            anyAssigned = true;
        } else {
            confirmedFrame = std::min(confirmedFrame, latestFrame);
        }
    }

    return anyAssigned ? confirmedFrame : m_localSimulationFrame;
}

FrameNumber NetplayCoordinator::computeHostConfirmedFrame() const
{
    return computeHostInputConfirmedFrame();
}

void NetplayCoordinator::broadcastFrameStatusIfNeeded()
{
    if(!m_hosting) return;

    FrameStatusData status;
    status.timelineEpoch = m_session.roomState().timelineEpoch;
    status.currentFrame = m_localSimulationFrame;
    const FrameNumber inputConfirmedFrame = std::max(m_session.roomState().lastConfirmedFrame, computeHostInputConfirmedFrame());
    status.lastConfirmedFrame = inputConfirmedFrame;
    status.inputDelayFrames = m_session.roomState().inputDelayFrames;
    status.predictFrames = m_session.roomState().predictFrames;
    status.topology = makeTopologyData(m_session.roomState());

    const FrameNumber previousCurrentFrame = m_session.roomState().currentFrame;
    const FrameNumber previousConfirmedFrame = m_session.roomState().lastConfirmedFrame;
    const uint8_t previousInputDelayFrames = m_session.roomState().inputDelayFrames;
    const uint8_t previousPredictFrames = m_session.roomState().predictFrames;
    const bool changed =
        status.currentFrame != previousCurrentFrame ||
        status.lastConfirmedFrame != previousConfirmedFrame ||
        status.inputDelayFrames != previousInputDelayFrames ||
        status.predictFrames != previousPredictFrames;

    m_session.roomState().currentFrame = status.currentFrame;
    m_session.roomState().lastConfirmedFrame = status.lastConfirmedFrame;

    if(!changed &&
       status.lastConfirmedFrame == m_lastBroadcastConfirmedFrame &&
       status.inputDelayFrames == m_lastBroadcastInputDelayFrames) {
        return;
    }

    m_lastBroadcastConfirmedFrame = status.lastConfirmedFrame;
    m_lastBroadcastInputDelayFrames = status.inputDelayFrames;
    m_transport.broadcastUnreliable(Channel::Diagnostics, buildFrameStatusPacket(status, m_session.roomState().sessionId));
}

bool NetplayCoordinator::allRequiredParticipantsRomCompatible() const
{
    if(m_session.roomState().selectedGameName.empty()) return false;

    for(const ParticipantInfo& participant : m_session.roomState().participants) {
        if(participantIsObserver(participant)) continue;
        if(!participant.connected || !participant.romLoaded || !participant.romCompatible) return false;
    }
    return true;
}

void NetplayCoordinator::refreshHostRoomState()
{
    if(!m_hosting) return;

    if(m_session.roomState().state == SessionState::Running ||
       m_session.roomState().state == SessionState::Resyncing ||
       m_session.roomState().state == SessionState::Paused ||
       m_session.roomState().state == SessionState::Ended) {
        return;
    }

    m_session.roomState().state =
        allRequiredParticipantsRomCompatible() ? SessionState::ReadyCheck : SessionState::ValidatingRom;
}

void NetplayCoordinator::updateReconnectReservations()
{
    if(!m_hosting) return;

    const auto now = std::chrono::steady_clock::now();
    std::vector<ParticipantId> expiredParticipants;
    std::vector<ParticipantId> changedParticipants;
    for(auto& participant : m_session.roomState().participants) {
        if(!participant.reconnectReserved) continue;

        const auto it = m_reconnectReservationDeadlines.find(participant.id);
        if(it == m_reconnectReservationDeadlines.end()) {
            participant.reconnectReserved = false;
            participant.reservationSecondsRemaining = 0;
            changedParticipants.push_back(participant.id);
            continue;
        }

        if(now >= it->second) {
            expiredParticipants.push_back(participant.id);
            continue;
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second - now);
        const uint16_t secondsRemaining = static_cast<uint16_t>(std::max<int64_t>(1, remaining.count() + 1));
        if(participant.reservationSecondsRemaining != secondsRemaining) {
            participant.reservationSecondsRemaining = secondsRemaining;
            changedParticipants.push_back(participant.id);
        }
    }

    for(ParticipantId participantId : changedParticipants) {
        if(const ParticipantInfo* participant = m_session.findParticipant(participantId)) {
            m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(*participant, 0));
        }
    }

    for(ParticipantId participantId : expiredParticipants) {
        const ParticipantInfo* participant = m_session.findParticipant(participantId);
        const std::string name =
            participant != nullptr && !participant->displayName.empty()
                ? participant->displayName
                : std::to_string(static_cast<int>(participantId));
        pushLog("Reconnect reservation expired for " + name);
        m_reconnectReservationDeadlines.erase(participantId);
        removeParticipant(participantId);
        m_transport.broadcastReliable(Channel::Control, buildParticipantLeftPacket(participantId));
        pushToast(name + " did not reconnect");
    }

    if(!expiredParticipants.empty()) {
        refreshHostRoomState();
    }
}

bool NetplayCoordinator::romValidationMatches(const RomValidationData& a, const RomValidationData& b)
{
    return a.romCrc32 == b.romCrc32 &&
           a.mapperId == b.mapperId &&
           a.subMapperId == b.subMapperId &&
           a.prgRomSize == b.prgRomSize &&
           a.chrRomSize == b.chrRomSize &&
           a.chrRamSize == b.chrRamSize &&
           a.fileSize == b.fileSize;
}

bool NetplayCoordinator::predictRemoteInputFrame(FrameNumber frame, ParticipantId participantId, PlayerSlot slot)
{
    if(participantId == kInvalidParticipantId || slot == kObserverPlayerSlot) return false;
    if(m_remoteInputs.find(frame, participantId, slot) != nullptr) return false;

    const TimelineInputEntry* lastKnown = m_remoteInputs.latestConfirmedFor(participantId, slot);
    if(lastKnown == nullptr) return false;

    TimelineInputEntry predicted = *lastKnown;
    predicted.frame = frame;
    predicted.inputFrame = InputFrame::repeatedFrom(lastKnown->inputFrame, frame);
    predicted.predicted = true;
    predicted.confirmed = false;
    m_remoteInputs.push(predicted);
    return true;
}

uint32_t NetplayCoordinator::unresolvedPredictedRemoteFrameCount() const
{
    uint32_t count = 0;
    for(const TimelineInputEntry& entry : m_remoteInputs.entries()) {
        if(!entry.predicted || entry.confirmed) continue;
        const ParticipantInfo* participant = m_session.findParticipant(entry.participantId);
        if(participant == nullptr) continue;
        if(!participant->connected || participantIsObserver(*participant)) continue;
        if(participant->inputSuspended || participant->inputResumeAwaitingResync) continue;
        ++count;
    }
    return count;
}

FrameNumber NetplayCoordinator::latestPredictedRemoteFrame() const
{
    FrameNumber latest = 0;
    for(const TimelineInputEntry& entry : m_remoteInputs.entries()) {
        if(entry.predicted && !entry.confirmed && entry.frame > latest) {
            latest = entry.frame;
        }
    }
    return latest;
}

void NetplayCoordinator::setRoomInputTopology(std::optional<Settings::Device> port1Device,
                                              std::optional<Settings::Device> port2Device,
                                              Settings::ExpansionDevice expansionDevice,
                                              Settings::NesMultitapDevice nesMultitapDevice,
                                              Settings::FamicomMultitapDevice famicomMultitapDevice,
                                              std::optional<ParticipantId> preservedParticipantId,
                                              PlayerSlot preservedAssignment)
{
    if(!m_hosting) return;

    RoomState& room = m_session.roomState();
    const bool changed =
        room.port1Device != port1Device ||
        room.port2Device != port2Device ||
        room.expansionDevice != expansionDevice ||
        room.nesMultitapDevice != nesMultitapDevice ||
        room.famicomMultitapDevice != famicomMultitapDevice;
    if(!changed) return;

    room.port1Device = port1Device;
    room.port2Device = port2Device;
    room.expansionDevice = expansionDevice;
    room.nesMultitapDevice = nesMultitapDevice;
    room.famicomMultitapDevice = famicomMultitapDevice;

    std::vector<ParticipantId> changedAssignments;
    for(ParticipantInfo& participant : room.participants) {
        if(participantIsObserver(participant)) continue;

        std::vector<PlayerSlot> preservedAssignments;
        const bool preserveParticipantAssignment = preservedParticipantId.has_value() && participant.id == *preservedParticipantId;
        for(PlayerSlot slot : participantAssignments(participant)) {
            const bool preserveSpecificAssignment =
                preserveParticipantAssignment &&
                slot == preservedAssignment &&
                preservedAssignment != kObserverPlayerSlot &&
                isAssignmentAvailable(preservedAssignment, room);
            if(preserveSpecificAssignment || isAssignmentAvailable(slot, room)) {
                preservedAssignments.push_back(slot);
            }
        }

        if(preservedAssignments == participant.controllerAssignments) continue;
        participant.controllerAssignments = std::move(preservedAssignments);
        syncParticipantRoleWithAssignments(participant, participant.id == m_localParticipantId);
        changedAssignments.push_back(participant.id);
        if(participantIsObserver(participant)) {
            pushLog(controllerAssignmentToast(kObserverPlayerSlot, room, participantLabel(participant)));
        }
    }

    for(ParticipantId changedId : changedAssignments) {
        ParticipantInfo* changed = m_session.findParticipant(changedId);
        if(changed == nullptr) continue;
        const AssignControllerData data = makeAssignControllerData(*changed);
        m_transport.broadcastReliable(Channel::Control, buildAssignControllerPacket(data, room.sessionId));
        m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(*changed, 0));
    }

    if(room.state == SessionState::Running || room.state == SessionState::Paused) {
        const FrameNumber resyncFrame = std::min(m_localSimulationFrame, room.lastConfirmedFrame);
        queuePendingHostResync(resyncFrame, ResyncReason::ConfirmedDesync);
        pushLog("Input topology changed; scheduling automatic resync");
    }
}

bool NetplayCoordinator::handleJoinRoom(NetTransport::PeerHandle peer, PacketReader& reader)
{
    if(!m_hosting) return false;

    JoinRoomData joinData;
    if(!reader.readPod(joinData)) return false;

    std::string displayName;
    if(!reader.readString(displayName)) return false;
    std::string emulatorVersion;
    if(reader.remaining() > 0 && !reader.readString(emulatorVersion)) return false;

    const bool hostRequiresSpecificRom = !m_session.roomState().selectedGameName.empty();
    const bool joinVersionCompatible = !emulatorVersion.empty() && emulatorVersion == m_localEmulatorVersion;
    if(!joinVersionCompatible) {
        pushLog("Rejected join from " + displayName + " due to emulator version mismatch");
        m_transport.sendReliable(
            peer,
            Channel::Control,
            buildJoinRejectedPacket(
                JoinRejectReason::EmulatorVersionMismatch,
                m_session.roomState().selectedGameName,
                m_session.roomState().romValidation,
                m_localEmulatorVersion
            )
        );
        m_transport.flush();
        m_transport.disconnectPeer(peer);
        return true;
    }

    const bool joinRomCompatible =
        !hostRequiresSpecificRom ||
        (joinData.romLoaded != 0 &&
         romValidationMatches(joinData.romValidation, m_session.roomState().romValidation));
    if(!joinRomCompatible) {
        pushLog("Rejected join from " + displayName + " due to ROM mismatch");
        m_transport.sendReliable(
            peer,
            Channel::Control,
            buildJoinRejectedPacket(
                JoinRejectReason::RomMismatch,
                m_session.roomState().selectedGameName,
                m_session.roomState().romValidation
            )
        );
        m_transport.flush();
        m_transport.disconnectPeer(peer);
        return true;
    }

    ParticipantInfo* reconnectParticipant = findParticipantByReconnectToken(joinData.reconnectToken);
    const bool reusesExistingIdentity = reconnectParticipant != nullptr;
    const bool reusedReconnectReservation =
        reconnectParticipant != nullptr &&
        reconnectParticipant->reconnectReserved &&
        !reconnectParticipant->connected;
    const bool replacingActivePeer =
        reconnectParticipant != nullptr &&
        reconnectParticipant->connected &&
        !reconnectParticipant->reconnectReserved;
    NetTransport::PeerHandle replacedPeer = NetTransport::kInvalidPeerHandle;
    if(replacingActivePeer) {
        replacedPeer = peerFromParticipantId(reconnectParticipant->id);
        if(replacedPeer != NetTransport::kInvalidPeerHandle && replacedPeer != peer) {
            m_transport.setPeerTag(replacedPeer, 0);
            clearPendingKickDisconnect(replacedPeer);
            m_transport.disconnectPeer(replacedPeer);
        }
    }

    ParticipantInfo& participant =
        reusesExistingIdentity
            ? *reconnectParticipant
            : ensureParticipant(m_nextAssignedParticipantId++, displayName);
    participant.displayName = displayName;
    participant.connected = true;
    participant.reconnectReserved = false;
    participant.reservationSecondsRemaining = 0;
    participant.inputSuspended = false;
    participant.inputResumeAwaitingResync = false;
    m_lastRemoteInputAt[participant.id] = std::chrono::steady_clock::now();
    m_reconnectReservationDeadlines.erase(participant.id);
    participant.reconnectToken = joinData.reconnectToken != 0 ? joinData.reconnectToken : generateReconnectToken();
    participant.romLoaded = joinData.romLoaded != 0;
    participant.romCompatible = joinRomCompatible;
    if(!reusesExistingIdentity) {
        participant.role = ParticipantRole::Observer;
        participant.controllerAssignments.clear();
        participant.normalizeControllerAssignments();
    } else if(std::find(
                  m_pendingSequenceResetParticipants.begin(),
                  m_pendingSequenceResetParticipants.end(),
                  participant.id
              ) == m_pendingSequenceResetParticipants.end()) {
        m_pendingSequenceResetParticipants.push_back(participant.id);
    }

    if(m_session.roomState().state == SessionState::Starting ||
       m_session.roomState().state == SessionState::Running ||
       m_session.roomState().state == SessionState::Paused ||
       m_session.roomState().state == SessionState::Resyncing) {
        m_pendingHostLateJoinResyncParticipant = participant.id;
        if(reusedReconnectReservation) {
            pushLog("Reconnect reservation claimed; waiting for ROM validation before automatic resync");
        } else if(replacingActivePeer) {
            pushLog("Reconnect token matched active participant; replacing peer before automatic resync");
        }
    }

    m_transport.setPeerTag(peer, static_cast<uintptr_t>(participant.id) + 1u);

    if(!m_transport.sendReliable(peer, Channel::Control, buildParticipantJoinedPacket(participant, participant.reconnectToken))) {
        m_lastError = "Failed to send ParticipantJoined";
        pushLog(m_lastError);
        return false;
    }

    for(const ParticipantInfo& existing : m_session.roomState().participants) {
        if(existing.id == participant.id) continue;
        if(!m_transport.sendReliable(peer, Channel::Control, buildParticipantJoinedPacket(existing, 0))) {
            m_lastError = "Failed to sync existing participant state";
            pushLog(m_lastError);
            return false;
        }
    }

    FrameStatusData status;
    status.timelineEpoch = m_session.roomState().timelineEpoch;
    status.currentFrame = m_session.roomState().currentFrame;
    status.lastConfirmedFrame = m_session.roomState().lastConfirmedFrame;
    status.inputDelayFrames = m_session.roomState().inputDelayFrames;
    status.predictFrames = m_session.roomState().predictFrames;
    status.topology = makeTopologyData(m_session.roomState());
    if(!m_transport.sendReliable(peer, Channel::Diagnostics, buildFrameStatusPacket(status, m_session.roomState().sessionId))) {
        m_lastError = "Failed to sync frame status";
        pushLog(m_lastError);
        return false;
    }

    if(m_session.roomState().state != SessionState::Lobby &&
       m_session.roomState().state != SessionState::ValidatingRom &&
       m_session.roomState().state != SessionState::ReadyCheck) {
        StartSessionData sessionData;
        sessionData.state = m_session.roomState().state;
        sessionData.inputDelayFrames = m_session.roomState().inputDelayFrames;
        sessionData.predictFrames = m_session.roomState().predictFrames;
        sessionData.topology = makeTopologyData(m_session.roomState());
        m_transport.sendReliable(peer, Channel::Control, buildStartSessionPacket(sessionData, m_session.roomState().sessionId));
    }

    m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(participant, 0), peer);

    if(!m_session.roomState().selectedGameName.empty()) {
        m_transport.sendReliable(peer, Channel::Control, buildSelectRomPacket(m_session.roomState().selectedGameName, m_session.roomState().romValidation));

        for(const ParticipantInfo& existing : m_session.roomState().participants) {
            RomValidationResultData validationResult;
            validationResult.participantId = existing.id;
            validationResult.romLoaded = existing.romLoaded ? 1 : 0;
            validationResult.romCompatible = existing.romCompatible ? 1 : 0;
            validationResult.romValidation = existing.id == m_localParticipantId ? m_session.roomState().romValidation : RomValidationData{};
            m_transport.sendReliable(peer, Channel::Control, buildRomValidationResultPacket(validationResult));
        }
    }

    std::ostringstream oss;
    oss << ((reusedReconnectReservation || replacingActivePeer) ? "Participant reconnected: " : "Participant joined as observer: ")
        << participant.displayName
        << " (id " << static_cast<int>(participant.id) << ")";
    pushLog(oss.str());
    if(reusedReconnectReservation || !replacingActivePeer) {
        pushToast(participantLabel(participant) + " joined");
    }
    return true;
}

bool NetplayCoordinator::handleParticipantJoined(PacketReader& reader)
{
    ParticipantId participantId = kInvalidParticipantId;
    uint64_t reconnectToken = 0;
    uint8_t connected = 0;
    uint8_t reconnectReserved = 0;
    uint16_t reservationSecondsRemaining = 0;
    ParticipantRole role = ParticipantRole::Observer;
    uint8_t assignmentCount = 0;
    std::string displayName;

    if(!reader.readPod(participantId)) return false;
    if(!reader.readPod(reconnectToken)) return false;
    if(!reader.readPod(connected)) return false;
    if(!reader.readPod(reconnectReserved)) return false;
    if(!reader.readPod(reservationSecondsRemaining)) return false;
    if(!reader.readPod(role)) return false;
    if(!reader.readPod(assignmentCount)) return false;
    std::vector<PlayerSlot> controllerAssignments;
    controllerAssignments.reserve(assignmentCount);
    for(uint8_t index = 0; index < assignmentCount; ++index) {
        PlayerSlot slot = kObserverPlayerSlot;
        if(!reader.readPod(slot)) return false;
        controllerAssignments.push_back(slot);
    }
    if(!reader.readString(displayName)) return false;

    ParticipantInfo* existingParticipant = m_session.findParticipant(participantId);
    const bool isNewParticipant = existingParticipant == nullptr;
    const bool wasConnected = existingParticipant != nullptr ? existingParticipant->connected : false;
    const bool wasReserved = existingParticipant != nullptr ? existingParticipant->reconnectReserved : false;
    const bool suppressPresenceToast =
        !m_hosting &&
        (m_reconnectPending ||
         m_reconnectAttemptInFlight ||
         m_session.roomState().activeResyncReason == ResyncReason::ObserverVisibilityRestore);
    ParticipantInfo& participant = ensureParticipant(participantId, displayName);
    if(reconnectToken != 0) {
        participant.reconnectToken = reconnectToken;
    }
    participant.connected = connected != 0;
    participant.reconnectReserved = reconnectReserved != 0;
    participant.reservationSecondsRemaining = reservationSecondsRemaining;
    participant.role = role;
    participant.controllerAssignments = std::move(controllerAssignments);
    participant.normalizeControllerAssignments();
    participant.inputSuspended = false;
    participant.inputResumeAwaitingResync = false;
    if(!m_hosting &&
       participantId != m_localParticipantId &&
       participant.connected &&
       !participant.reconnectReserved) {
        // Non-host peers can join while remote participants are already
        // simulating at high frame/sequence values. Arm one-time sequence
        // rebase so first received gameplay packet establishes baseline.
        participant.sequenceRebasePending = true;
    }
    m_lastRemoteInputAt[participant.id] = std::chrono::steady_clock::now();

    if(m_localParticipantId == kInvalidParticipantId && !m_hosting) {
        m_localParticipantId = participantId;
        m_connected = true;
        if(participant.reconnectToken != 0) {
            m_localReconnectToken = participant.reconnectToken;
        }
        m_lastError.clear();
        clearReconnectAttemptState();
    }

    if(isNewParticipant && participantId != m_localParticipantId) {
        if(!suppressPresenceToast) {
            pushToast(participantLabel(participant) + " joined");
        }
        pushLog("Participant active: " + participant.displayName + " (id " + std::to_string(static_cast<int>(participant.id)) + ")");
    } else if((participant.connected != wasConnected || participant.reconnectReserved != wasReserved) &&
              participantId != m_localParticipantId) {
        if(!participant.connected && participant.reconnectReserved) {
            pushToast(participantLabel(participant) + " left (reserved)");
        } else {
            std::ostringstream oss;
            oss << (participant.connected ? "Participant active: " : "Participant inactive: ")
                << participant.displayName << " (id " << static_cast<int>(participant.id) << ")";
            pushToast(oss.str());
        }
    }
    return true;
}

bool NetplayCoordinator::handleJoinRejected(PacketReader& reader)
{
    JoinRejectedData data;
    if(!reader.readPod(data)) return false;

    std::string gameName;
    if(!reader.readString(gameName)) return false;
    std::string expectedEmulatorVersion;
    if(reader.remaining() > 0 && !reader.readString(expectedEmulatorVersion)) return false;

    switch(data.reason) {
        case JoinRejectReason::EmulatorVersionMismatch: {
            std::ostringstream oss;
            oss << "Emulator version mismatch: host is "
                << (expectedEmulatorVersion.empty() ? "<unknown>" : expectedEmulatorVersion)
                << ", client is "
                << m_localEmulatorVersion;
            m_lastError = oss.str();
            pushLog("Join rejected: emulator version mismatch");
            break;
        }
        case JoinRejectReason::RomMismatch:
        default: {
            std::ostringstream oss;
            oss << "Owner requires ROM \"" << gameName << "\" (CRC "
                << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
                << data.romValidation.romCrc32 << ")";
            m_lastError = oss.str();
            pushLog("Join rejected: ROM mismatch");
            break;
        }
    }
    clearReconnectAttemptState();
    m_disconnectExpectedAfterJoinReject = false;
    const std::string preservedError = m_lastError;
    completeLocalDisconnect();
    m_lastError = preservedError;
    return true;
}

bool NetplayCoordinator::handleControlPacket(NetTransport::PeerHandle peer, const std::vector<uint8_t>& payload)
{
    PacketReader reader(payload.data(), payload.size());
    PacketHeader header;
    if(!reader.readPod(header)) return false;

    if(header.protocolVersion != kProtocolVersion) {
        m_lastError = "Protocol version mismatch";
        pushLog(m_lastError);
        return false;
    }

    if(m_hosting && header.sessionId != 0 && header.sessionId != m_session.roomState().sessionId) {
        pushLog("Ignored control packet for unknown session");
        return false;
    }

    if(!m_hosting && header.sessionId != 0) {
        m_session.roomState().sessionId = header.sessionId;
    }

    switch(header.type) {
        case MessageType::JoinRoom:
            return handleJoinRoom(peer, reader);

        case MessageType::JoinRejected:
            return handleJoinRejected(reader);

        case MessageType::ParticipantJoined:
            return handleParticipantJoined(reader);

        case MessageType::ParticipantLeft:
            return handleParticipantLeft(reader);

        case MessageType::LeaveRoom:
            return handleLeaveRoom(peer, reader);

        case MessageType::ResyncBegin:
            return handleResyncBegin(reader);

        case MessageType::ResyncChunk:
            return handleResyncChunk(reader);

        case MessageType::ResyncComplete:
            return handleResyncComplete(reader);

        case MessageType::ResyncAck:
            return handleResyncAck(reader);

        case MessageType::ResyncAbort:
            return handleResyncAbort(reader);

        case MessageType::ResyncRequest:
            return handleResyncRequest(peer, reader);

        case MessageType::ClockSyncRequest:
            return handleClockSyncRequest(peer, reader);

        case MessageType::ClockSyncResponse:
            return handleClockSyncResponse(peer, reader);

        case MessageType::PeerHealth:
            return handlePeerHealth(peer, reader);

        case MessageType::SelectRom:
            return handleSelectRom(reader);

        case MessageType::RomValidationResult:
            return handleRomValidationResult(peer, reader);

        case MessageType::InputFrame:
            return handleInputFrame(peer, reader);

        case MessageType::ConfirmedInputFrames:
            return handleConfirmedInputFrames(reader);

        case MessageType::InputAck:
            return handleInputAck(reader);

        case MessageType::FrameStatus:
            return handleFrameStatus(reader);

        case MessageType::CrcReport:
            return handleCrcReport(reader);

        case MessageType::AssignController:
            return handleAssignController(reader);

        case MessageType::StartSession:
        case MessageType::PauseSession:
        case MessageType::ResumeSession:
        case MessageType::EndSession:
            return handleStartSession(reader);

        default:
            pushLog("Unhandled control packet: " + messageTypeLabel(header.type));
            return false;
    }
}

bool NetplayCoordinator::host(uint16_t port, size_t maxPeers, const std::string& displayName)
{
    disconnect();

    m_localDisplayName = displayName.empty() ? defaultDisplayName() : displayName;

    if(!m_transport.hostSession(port, maxPeers)) {
        m_lastError = std::string("Failed to host ") + transportBackendLabel(m_transport.backend()) + " session";
        if(!m_transport.lastError().empty()) {
            m_lastError += ": " + m_transport.lastError();
        }
        pushLog(m_lastError);
        return false;
    }

    resetSessionState();
    m_hosting = true;
    m_connected = true;
    m_localParticipantId = 0;
    m_session.roomState().sessionId = generateSessionId();
    m_session.roomState().state = SessionState::Lobby;
    m_sharedClockSynchronized = true;
    m_sharedClockOffsetMicros = 0;
    m_sharedClockRttMicros = 0;
    m_bestClockSyncDelayMicros = 0;
    m_session.roomState().sharedClockSynchronized = true;
    m_session.roomState().sharedClockOffsetMicros = 0;
    m_session.roomState().sharedClockRttMicros = 0;
    m_session.roomState().sharedClockMicros = sharedClockNowMicros();

    ParticipantInfo& hostParticipant = ensureParticipant(m_localParticipantId, m_localDisplayName);
    hostParticipant.connected = true;
    hostParticipant.role = ParticipantRole::SessionOwner;
    hostParticipant.controllerAssignments.clear();
    hostParticipant.normalizeControllerAssignments();

    const auto& iceServers = m_transport.advertisedIceServers();
    if(!iceServers.empty()) {
        m_loggedAdvertisedIceServers = iceServers;
        pushLog("WebRTC signaling advertised " + std::to_string(iceServers.size()) + " ICE server(s)");
        for(const std::string& iceServer : iceServers) {
            pushLog("ICE server: " + iceServer);
        }
    }

    pushLog(describeHostTarget(m_transport.backend(), m_transport.options(), port, maxPeers));
    return true;
}

bool NetplayCoordinator::join(const std::string& hostName, uint16_t port, const std::string& displayName)
{
    const bool pendingJoinRomLoaded = m_pendingJoinRomLoaded;
    const RomValidationData pendingJoinRomValidation = m_pendingJoinRomValidation;
    const uint64_t pendingReconnectToken = m_localReconnectToken;
    disconnect();

    m_localDisplayName = displayName.empty() ? defaultDisplayName() : displayName;
    m_lastJoinHostName = hostName;
    m_lastJoinPort = port;

    if(!m_transport.connectToHost(hostName, port)) {
        m_lastError = std::string("Failed to connect to owner using ") + transportBackendLabel(m_transport.backend());
        if(!m_transport.lastError().empty()) {
            m_lastError += ": " + m_transport.lastError();
        }
        pushLog(m_lastError);
        return false;
    }

    resetSessionState();
    m_localReconnectToken = pendingReconnectToken;
    m_pendingJoinRomLoaded = pendingJoinRomLoaded;
    m_pendingJoinRomValidation = pendingJoinRomValidation;
    m_session.roomState().state = SessionState::Lobby;
    m_sharedClockSynchronized = false;
    m_sharedClockOffsetMicros = 0;
    m_sharedClockRttMicros = 0;
    m_bestClockSyncDelayMicros = std::numeric_limits<uint64_t>::max();
    m_pendingClockSyncRequests.clear();
    m_session.roomState().sharedClockSynchronized = false;
    m_session.roomState().sharedClockOffsetMicros = 0;
    m_session.roomState().sharedClockRttMicros = 0;
    m_session.roomState().sharedClockMicros = 0;
    const auto& iceServers = m_transport.advertisedIceServers();
    if(!iceServers.empty()) {
        m_loggedAdvertisedIceServers = iceServers;
        pushLog("WebRTC signaling advertised " + std::to_string(iceServers.size()) + " ICE server(s)");
        for(const std::string& iceServer : iceServers) {
            pushLog("ICE server: " + iceServer);
        }
    }
    pushLog("Connecting to " + describeConnectTarget(m_transport.backend(), m_transport.options(), hostName, port));
    return true;
}

void NetplayCoordinator::disconnect()
{
    if(m_transport.isActive()) {
        if(m_hosting) {
            endSession();
            m_transport.flush();
            m_transport.broadcastReliable(Channel::Control, buildParticipantLeftPacket(m_localParticipantId));
            m_transport.flush();
            m_transport.disconnectAll();
            completeLocalDisconnect(false);
            return;
        } else if(m_serverPeer != NetTransport::kInvalidPeerHandle && m_localParticipantId != kInvalidParticipantId) {
            m_transport.sendReliable(m_serverPeer, Channel::Control, buildLeaveRoomPacket(m_localParticipantId));
            m_transport.flush();
            m_transport.disconnectPeer(m_serverPeer);
            m_gracefulDisconnectPending = true;
            m_gracefulDisconnectDeadline = std::chrono::steady_clock::now() + kGracefulDisconnectTimeout;
            m_connected = false;
            m_session.roomState().state = SessionState::Ended;
            return;
        }
    }

    finalizeLocalTeardown(LocalTeardownMode::Immediate);
}

void NetplayCoordinator::disconnectImmediately()
{
    if(m_transport.isActive()) {
        if(!m_hosting &&
           m_serverPeer != NetTransport::kInvalidPeerHandle &&
           m_localParticipantId != kInvalidParticipantId) {
            m_transport.sendReliable(m_serverPeer, Channel::Control, buildLeaveRoomPacket(m_localParticipantId));
            m_transport.flush();
        } else if(m_hosting) {
            endSession();
            m_transport.flush();
            m_transport.broadcastReliable(Channel::Control, buildParticipantLeftPacket(m_localParticipantId));
            m_transport.flush();
        }

        m_transport.disconnectAll();
    }

    finalizeLocalTeardown(LocalTeardownMode::Graceful);
}

void NetplayCoordinator::shutdownForUnload()
{
    finalizeLocalTeardown(LocalTeardownMode::Unload);
}

void NetplayCoordinator::update(uint32_t timeoutMs)
{
    processPendingKickDisconnects();
    if(!m_transport.isActive()) {
        processPendingReconnect();
        return;
    }

    const auto handleEvent = [&](const NetTransport::Event& event) {
        switch(event.type) {
            case NetTransport::Event::Type::Connected:
                if(!m_hosting) {
                    // In WebRTC mesh mode clients can receive additional peer connections
                    // after bootstrap. Only the first connection is the host/session peer.
                    if(m_serverPeer == NetTransport::kInvalidPeerHandle) {
                        m_serverPeer = event.peer;
                        pushLog("Connected to host");
                        if(!m_transport.sendReliable(m_serverPeer, Channel::Control, buildJoinRoomPacket())) {
                            m_lastError = "Failed to send JoinRoom";
                            pushLog(m_lastError);
                        }
                    } else {
                        pushLog("Peer joined");
                    }
                }
                else {
                    pushLog("Peer joined");
                }
                break;

            case NetTransport::Event::Type::Disconnected:
                clearPendingKickDisconnect(event.peer);
                if(event.peer == m_serverPeer) {
                    pushLog("Disconnected from host");
                    pushToast("Disconnected from host");
                    m_serverPeer = NetTransport::kInvalidPeerHandle;
                    m_connected = false;
                    m_session.roomState().state = SessionState::Ended;
                    m_reconnectAttemptInFlight = false;
                    if(m_gracefulDisconnectPending) {
                        completeLocalDisconnect(false);
                    } else if(m_disconnectExpectedAfterJoinReject) {
                        const std::string preservedError = m_lastError;
                        m_disconnectExpectedAfterJoinReject = false;
                        completeLocalDisconnect(false);
                        m_lastError = preservedError;
                    } else if(m_disconnectExpectedAfterHostShutdown) {
                        const std::string preservedError = m_lastError.empty() ? std::string("Owner closed the room") : m_lastError;
                        m_disconnectExpectedAfterHostShutdown = false;
                        completeLocalDisconnect(false);
                        m_lastError = preservedError;
                    } else if(event.data == kDisconnectReasonKicked) {
                        const std::string preservedError = m_lastError.empty() ? std::string("Removed from room by host") : m_lastError;
                        completeLocalDisconnect(false);
                        m_lastError = preservedError;
                    } else if(m_localParticipantId != kInvalidParticipantId &&
                              m_localReconnectToken != 0 &&
                              hasReconnectTarget(m_transport.backend(), m_transport.options(), m_lastJoinHostName, m_lastJoinPort)) {
                        scheduleReconnectAttempt();
                    } else if(m_session.roomState().currentFrame > 0) {
                        m_lastError = "Owner disconnected during session";
                    }
                } else if(m_hosting) {
                    ParticipantId participantId = participantIdFromPeer(event.peer);
                    if(participantId == kInvalidParticipantId && event.data != 0) {
                        participantId = static_cast<ParticipantId>(event.data - 1u);
                    }
                    if(participantId != kInvalidParticipantId && m_session.findParticipant(participantId) != nullptr) {
                        ParticipantInfo* participant = m_session.findParticipant(participantId);
                        if(participant != nullptr && (!participant->connected || participant->reconnectReserved)) {
                            break;
                        }
                        const bool hasAssignedInput =
                            participant != nullptr &&
                            !participantIsObserver(*participant);
                        const bool reserveReconnect =
                            participant != nullptr &&
                            participant->reconnectToken != 0 &&
                            hasAssignedInput;
                        const std::string participantLeftLabel =
                            participant != nullptr
                                ? participantLabel(*participant)
                                : std::to_string(static_cast<int>(participantId));
                        pushLog(participantLeftLabel + " left");
                        const std::string participantLeftToast = participantLeftLabel + " left";
                        if(m_pendingHostLateJoinResyncParticipant.has_value() &&
                           *m_pendingHostLateJoinResyncParticipant == participantId) {
                            m_pendingHostLateJoinResyncParticipant.reset();
                        }
                        if(participant != nullptr && reserveReconnect) {
                            participant->connected = false;
                            participant->reconnectReserved = true;
                            const bool hadAssignedInput = hasAssignedInput;
                            participant->inputSuspended = hadAssignedInput;
                            participant->inputResumeAwaitingResync = false;
                            participant->reservationSecondsRemaining =
                                static_cast<uint16_t>(std::clamp<int64_t>(m_reconnectReservationDuration.count(), 1, 65535));
                            m_reconnectReservationDeadlines[participantId] =
                                std::chrono::steady_clock::now() + m_reconnectReservationDuration;
                            m_pendingResyncAcks.erase(
                                std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), participantId),
                                m_pendingResyncAcks.end()
                            );
                            m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
                            if(!m_pendingResyncAcks.empty()) {
                                m_activeResyncAckDeadline = std::chrono::steady_clock::now() + kResyncAckTimeout;
                            }
                            if(m_activeResyncTargetParticipantId == participantId) {
                                cancelTargetedResync(
                                    "Targeted resync participant disconnected before completion: " +
                                    std::to_string(static_cast<int>(participantId))
                                );
                            } else {
                                finalizeActiveResyncIfReady();
                            }
                            m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(*participant, 0), event.peer);
                            pushLog(participantLabel(*participant) + " left (reserved)");
                            pushToast(participantLabel(*participant) + " left (reserved)");
                            if(hadAssignedInput) {
                                // Keep the host authoritative timeline moving while this
                                // participant is temporarily disconnected by replaying
                                // neutral/repeated input from the latest confirmed frame.
                                synthesizeSuspendedRemoteInputsUpTo(m_localSimulationFrame);
                            }
                        } else {
                            removeParticipant(participantId);
                            m_transport.broadcastReliable(Channel::Control, buildParticipantLeftPacket(participantId), event.peer);
                            pushToast(participantLeftToast);
                        }
                        refreshHostRoomState();
                    } else {
                        ParticipantId participantId = participantIdFromPeer(event.peer);
                        if(participantId == kInvalidParticipantId && event.data != 0) {
                            participantId = static_cast<ParticipantId>(event.data - 1u);
                        }
                        if(participantId != kInvalidParticipantId) {
                            if(ParticipantInfo* participant = m_session.findParticipant(participantId)) {
                                const std::string participantLeftLabel = participantLabel(*participant);
                                pushLog(participantLeftLabel + " left");
                                pushToast(participantLeftLabel + " left");
                            } else {
                                const std::string participantLeftLabel = std::to_string(static_cast<int>(participantId));
                                pushLog(participantLeftLabel + " left");
                                pushToast(participantLeftLabel + " left");
                            }
                        } else {
                            pushLog("Participant left");
                            pushToast("Participant left");
                        }
                    }
                } else {
                    ParticipantId participantId = participantIdFromPeer(event.peer);
                    if(participantId != kInvalidParticipantId &&
                       participantId != m_localParticipantId) {
                        if(ParticipantInfo* participant = m_session.findParticipant(participantId)) {
                            const std::string participantLeftLabel = participantLabel(*participant);
                            pushLog(participantLeftLabel + " left");
                            pushToast(participantLeftLabel + " left");
                        } else {
                            const std::string participantLeftLabel = std::to_string(static_cast<int>(participantId));
                            pushLog(participantLeftLabel + " left");
                            pushToast(participantLeftLabel + " left");
                        }
                    } else {
                        pushLog("Participant left");
                        pushToast("Participant left");
                    }
                }
                break;

            case NetTransport::Event::Type::PacketReceived:
                if(event.channel == Channel::Control || event.channel == Channel::Gameplay || event.channel == Channel::Diagnostics) {
                    if(!handleControlPacket(event.peer, event.payload) && m_lastError.empty()) {
                        const char* channelLabel =
                            event.channel == Channel::Control ? "control" :
                            event.channel == Channel::Gameplay ? "gameplay" : "diagnostics";
                        pushLog(std::string("Failed to handle ") + channelLabel + " packet");
                    }
                } else {
                    pushLog("Received packet on channel " + std::to_string(static_cast<int>(event.channel)) + " (" + std::to_string(event.payload.size()) + " bytes)");
                }
                break;

            case NetTransport::Event::Type::None:
            default:
                break;
        }
    };

    const auto queueOrHandleEvent = [&](NetTransport::Event event) {
        if(event.type == NetTransport::Event::Type::PacketReceived && event.payload.size() >= sizeof(PacketHeader)) {
            PacketHeader header{};
            std::memcpy(&header, event.payload.data(), sizeof(PacketHeader));
            auto it = m_dropIncomingMessageCounts.find(static_cast<uint16_t>(header.type));
            if(it != m_dropIncomingMessageCounts.end() && it->second > 0u) {
                --it->second;
                if(it->second == 0u) {
                    m_dropIncomingMessageCounts.erase(it);
                }
                pushLog("Dropped incoming " + messageTypeLabel(header.type) + " packet via fault injection");
                return;
            }
        }
        const bool shouldDelayGameplay =
            m_gameplayReceiveDelayMs > 0 &&
            event.type == NetTransport::Event::Type::PacketReceived &&
            event.channel == Channel::Gameplay;
        if(shouldDelayGameplay) {
            DelayedPacketEvent delayed;
            delayed.releaseAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(m_gameplayReceiveDelayMs);
            delayed.event = std::move(event);
            m_delayedPacketEvents.push_back(std::move(delayed));
            return;
        }
        handleEvent(event);
    };

    std::vector<NetTransport::Event> events = m_transport.poll(timeoutMs);
    const auto& advertisedIceServers = m_transport.advertisedIceServers();
    if(!advertisedIceServers.empty() &&
       advertisedIceServers != m_loggedAdvertisedIceServers) {
        m_loggedAdvertisedIceServers = advertisedIceServers;
        pushLog("WebRTC signaling advertised " + std::to_string(advertisedIceServers.size()) + " ICE server(s)");
        for(const std::string& iceServer : advertisedIceServers) {
            pushLog("ICE server: " + iceServer);
        }
    }

    for(NetTransport::Event event : events) {
        queueOrHandleEvent(std::move(event));
    }

    const auto now = std::chrono::steady_clock::now();
    while(!m_delayedPacketEvents.empty() && m_delayedPacketEvents.front().releaseAt <= now) {
        DelayedPacketEvent delayed = std::move(m_delayedPacketEvents.front());
        m_delayedPacketEvents.pop_front();
        handleEvent(delayed.event);
    }

    const std::string transportError = m_transport.lastError();
    if(!transportError.empty() && transportError != m_lastTransportError) {
        m_lastTransportError = transportError;
        if(m_disconnectExpectedAfterJoinReject && !m_lastError.empty()) {
            pushLog("Transport closed after join rejection: " + transportError);
        } else {
            m_lastError = transportError;
            pushLog("Transport error: " + transportError);
            const std::string preservedError = m_lastError;
            completeLocalDisconnect();
            m_lastError = preservedError;
            return;
        }
    } else if(transportError.empty()) {
        m_lastTransportError.clear();
    }

    if(!m_hosting &&
       m_incomingResync.has_value() &&
       m_serverPeer != NetTransport::kInvalidPeerHandle &&
       m_incomingResync->lastActivityAt != std::chrono::steady_clock::time_point{} &&
       now - m_incomingResync->lastActivityAt >= kIncomingResyncTimeout) {
        pushLog("Incoming resync timed out; requesting retry");
        ResyncAbortData abort;
        abort.resyncId = m_incomingResync->resyncId;
        abort.participantId = m_localParticipantId;
        abort.reason = 3;
        m_transport.sendReliable(m_serverPeer, Channel::Control, buildResyncAbortPacket(abort));
        m_incomingResync.reset();
    }

    if(m_hosting &&
       !m_pendingResyncAcks.empty() &&
       m_activeResyncAckDeadline != std::chrono::steady_clock::time_point{} &&
       now >= m_activeResyncAckDeadline) {
        if(activeResyncIsTargeted()) {
            cancelTargetedResync(
                "Targeted resync ACK timed out waiting for participant " +
                std::to_string(static_cast<int>(m_activeResyncTargetParticipantId)) +
                "; cancelling targeted observer resync"
            );
        } else if(m_session.roomState().activeResyncId != 0u) {
            std::vector<ParticipantId> stalledParticipants;
            std::vector<ParticipantId> stalledObservers;
            stalledParticipants.reserve(m_pendingResyncAcks.size());
            stalledObservers.reserve(m_pendingResyncAcks.size());
            for(const ParticipantId participantId : m_pendingResyncAcks) {
                if(participantId == m_localParticipantId) continue;
                const ParticipantInfo* participant = m_session.findParticipant(participantId);
                if(participant == nullptr || !participant->connected || participant->reconnectReserved) {
                    continue;
                }
                if(participantIsObserver(*participant)) {
                    stalledObservers.push_back(participantId);
                } else {
                    stalledParticipants.push_back(participantId);
                }
            }

            if(!stalledObservers.empty()) {
                for(const ParticipantId participantId : stalledObservers) {
                    m_pendingResyncAcks.erase(
                        std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), participantId),
                        m_pendingResyncAcks.end()
                    );
                }
                m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
                m_activeResyncAckDeadline = std::chrono::steady_clock::now() + kResyncAckTimeout;
                std::ostringstream oss;
                oss << "Resync ACK timed out for observer participant(s); continuing without kick: ";
                for(size_t i = 0; i < stalledObservers.size(); ++i) {
                    if(i != 0) oss << ", ";
                    oss << static_cast<int>(stalledObservers[i]);
                }
                pushLog(oss.str());
            }

            if(!stalledParticipants.empty()) {
                std::ostringstream oss;
                oss << "Resync ACK timed out; removing stalled participant(s): ";
                for(size_t i = 0; i < stalledParticipants.size(); ++i) {
                    if(i != 0) oss << ", ";
                    oss << static_cast<int>(stalledParticipants[i]);
                }
                pushLog(oss.str());

                for(const ParticipantId participantId : stalledParticipants) {
                    (void)kickParticipant(participantId);
                }
            } else if(m_pendingResyncAcks.empty()) {
                finalizeActiveResyncIfReady();
            } else {
                scheduleResyncRetry(
                    m_session.roomState().resyncTargetFrame,
                    "Resync ACK timed out waiting for " +
                        std::to_string(static_cast<unsigned>(m_pendingResyncAcks.size())) +
                        " participant(s); retrying"
                );
            }
        }
    }

    updatePeerHealthFromTransport();
    processClockSyncIfNeeded(now);
    updateReconnectReservations();
    processRemoteInputSuspension(now);
    if(m_hosting && m_session.roomState().state == SessionState::Running) {
        synthesizeSuspendedRemoteInputsUpTo(m_localSimulationFrame);
        publishConfirmedFramesIfReady();
    }
    broadcastFrameStatusIfNeeded();
    broadcastPeerHealthIfNeeded();
    m_session.roomState().sharedClockMicros = sharedClockNowMicros();
    m_session.roomState().sharedClockRttMicros = m_sharedClockRttMicros;
    m_session.roomState().sharedClockOffsetMicros = m_hosting ? 0 : m_sharedClockOffsetMicros;
    m_session.roomState().sharedClockSynchronized = m_hosting || m_sharedClockSynchronized;
    if(m_gracefulDisconnectPending &&
       m_gracefulDisconnectDeadline != std::chrono::steady_clock::time_point{} &&
       std::chrono::steady_clock::now() >= m_gracefulDisconnectDeadline) {
        completeLocalDisconnect();
        return;
    }
    processPendingReconnect();
}

bool NetplayCoordinator::setTransportBackend(NetTransportBackend backend)
{
    return m_transport.setBackend(backend);
}

void NetplayCoordinator::setTransportOptions(const NetTransportOptions& options)
{
    m_transport.setOptions(options);
}

const NetTransportOptions& NetplayCoordinator::transportOptions() const
{
    return m_transport.options();
}

NetTransportBackend NetplayCoordinator::transportBackend() const
{
    return m_transport.backend();
}

bool NetplayCoordinator::isActive() const
{
    return m_transport.isActive();
}

bool NetplayCoordinator::isHosting() const
{
    return m_hosting;
}

bool NetplayCoordinator::isConnected() const
{
    return m_connected;
}

bool NetplayCoordinator::reconnectPending() const
{
    return m_reconnectPending;
}

uint16_t NetplayCoordinator::reconnectSecondsRemaining() const
{
    return m_reconnectSecondsRemaining;
}

void NetplayCoordinator::setPendingJoinRomValidation(bool romLoaded, const RomValidationData& romValidation)
{
    m_pendingJoinRomLoaded = romLoaded;
    m_pendingJoinRomValidation = romValidation;
}

uint32_t NetplayCoordinator::gameplayReceiveDelayMs() const
{
    return m_gameplayReceiveDelayMs;
}

void NetplayCoordinator::setGameplayReceiveDelayMs(uint32_t delayMs)
{
    m_gameplayReceiveDelayMs = delayMs;
}

void NetplayCoordinator::dropNextIncomingMessages(MessageType type, uint32_t count)
{
    if(count == 0u) return;
    m_dropIncomingMessageCounts[static_cast<uint16_t>(type)] += count;
}

void NetplayCoordinator::clearIncomingMessageDrops()
{
    m_dropIncomingMessageCounts.clear();
}

void NetplayCoordinator::setReconnectReservationDurationForTests(uint32_t seconds)
{
    m_reconnectReservationDuration = std::chrono::seconds(std::max<uint32_t>(1u, seconds));
    if(m_reconnectPending) {
        m_reconnectSecondsRemaining =
            static_cast<uint16_t>(std::clamp<int64_t>(m_reconnectReservationDuration.count(), 1, 65535));
    }
}

void NetplayCoordinator::setRemoteInputSuspendTimeoutForTests(uint32_t timeoutMs)
{
    m_remoteInputSuspendTimeout = std::chrono::milliseconds(std::max<uint32_t>(1u, timeoutMs));
}

void NetplayCoordinator::setLocalEmulatorVersionForTests(const std::string& version)
{
    m_localEmulatorVersion = version.empty() ? std::string(GERANES_VERSION) : version;
}

void NetplayCoordinator::simulateTransportFailureForTests()
{
    clearReconnectAttemptState();

    if(!m_transport.isActive()) {
        completeLocalDisconnect();
        return;
    }

    m_gracefulDisconnectPending = true;
    m_gracefulDisconnectDeadline = std::chrono::steady_clock::now() + kGracefulDisconnectTimeout;
    m_transport.disconnectAll();
    m_transport.flush();
}

bool NetplayCoordinator::injectFrameStatusForTests(const FrameStatusData& status)
{
    PacketWriter writer;
    writer.writePod(status);
    PacketReader reader(writer.data().data(), writer.data().size());
    return handleFrameStatus(reader);
}

bool NetplayCoordinator::injectInputFrameForTests(const InputFrameData& input, const InputFrame& contribution)
{
    PacketWriter writer;
    const std::vector<uint8_t> payload = serializeInputFrame(contribution);
    InputFrameData inputWithPayload = input;
    inputWithPayload.payloadSize = static_cast<uint16_t>(payload.size());
    writer.writePod(inputWithPayload);
    writer.writeBytes(std::span<const uint8_t>(payload.data(), payload.size()));
    PacketReader reader(writer.data().data(), writer.data().size());
    return handleInputFrame(NetTransport::kInvalidPeerHandle, reader);
}

bool NetplayCoordinator::injectConfirmedInputFramesForTests(const ConfirmedInputFramesData& data)
{
    PacketWriter writer;
    writer.writePod(data);
    PacketReader reader(writer.data().data(), writer.data().size());
    return handleConfirmedInputFrames(reader);
}

bool NetplayCoordinator::injectInputAckForTests(const InputAckData& ack)
{
    PacketWriter writer;
    writer.writePod(ack);
    PacketReader reader(writer.data().data(), writer.data().size());
    return handleInputAck(reader);
}

bool NetplayCoordinator::injectCrcReportForTests(const CrcReportData& report)
{
    PacketWriter writer;
    writer.writePod(report);
    PacketReader reader(writer.data().data(), writer.data().size());
    return handleCrcReport(reader);
}

bool NetplayCoordinator::injectResyncAckForTests(const ResyncAckData& ack)
{
    PacketWriter writer;
    writer.writePod(ack);
    PacketReader reader(writer.data().data(), writer.data().size());
    return handleResyncAck(reader);
}

ParticipantId NetplayCoordinator::localParticipantId() const
{
    return m_localParticipantId;
}

const std::string& NetplayCoordinator::localDisplayName() const
{
    return m_localDisplayName;
}

uint64_t NetplayCoordinator::localReconnectToken() const
{
    return m_localReconnectToken;
}

void NetplayCoordinator::setLocalReconnectToken(uint64_t token)
{
    m_localReconnectToken = token;
}

const std::string& NetplayCoordinator::lastError() const
{
    return m_lastError;
}

const NetSession& NetplayCoordinator::session() const
{
    return m_session;
}

const std::vector<std::string>& NetplayCoordinator::eventLog() const
{
    return m_eventLog;
}

void NetplayCoordinator::appendNetplayLog(const std::string& message)
{
    pushLog(message);
}

const RollbackStats& NetplayCoordinator::predictionStats() const
{
    return m_predictionStats;
}

void NetplayCoordinator::recordPlaybackStop(FrameNumber frame, bool predictionLimitReached)
{
    m_predictionStats.recordPlaybackStop(frame, predictionLimitReached);
}

void NetplayCoordinator::recordLocalAuthoritativeFrameStart(FrameNumber frame)
{
    if(!m_hosting || frame == 0u) {
        return;
    }
    if(m_authoritativeFrameStartClockMicros.find(frame) != m_authoritativeFrameStartClockMicros.end()) {
        return;
    }

    uint64_t clockMicros = sharedClockNowMicros();
    if(clockMicros == 0u) {
        const int64_t nowMicros = monotonicNowMicros();
        clockMicros = nowMicros > 0 ? static_cast<uint64_t>(nowMicros) : 0u;
    }
    m_authoritativeFrameStartClockMicros[frame] = clockMicros;
    m_authoritativeFrameStartClockOrder.push_back(frame);
    if(clockMicros != 0u) {
        m_session.roomState().lastAuthoritativeClockFrame = frame;
        m_session.roomState().lastAuthoritativeClockMicros = clockMicros;
    }

    constexpr size_t kAuthoritativeClockHistoryLimit = kConfirmedFrameHistoryCapacity * 2u;
    while(m_authoritativeFrameStartClockOrder.size() > kAuthoritativeClockHistoryLimit) {
        const FrameNumber dropFrame = m_authoritativeFrameStartClockOrder.front();
        m_authoritativeFrameStartClockOrder.pop_front();
        m_authoritativeFrameStartClockMicros.erase(dropFrame);
    }
}

void NetplayCoordinator::setLocalSimulationFrame(FrameNumber frame)
{
    m_localSimulationFrame = frame;
    if(m_hosting) {
        m_session.roomState().currentFrame = frame;
    }
    advanceRecoveryStabilization(frame);
}

void NetplayCoordinator::rescheduleRollbackFrame(FrameNumber frame)
{
    if(!m_pendingRollbackFrame.has_value() || frame < *m_pendingRollbackFrame) {
        m_pendingRollbackFrame = frame;
    }
}

std::optional<FrameNumber> NetplayCoordinator::consumePendingRollbackFrame()
{
    std::optional<FrameNumber> result = m_pendingRollbackFrame;
    m_pendingRollbackFrame.reset();
    return result;
}

std::optional<NetplayCoordinator::PendingHostResyncRequest> NetplayCoordinator::consumePendingHostResyncFrame()
{
    std::optional<PendingHostResyncRequest> result = m_pendingHostResyncFrame;
    m_pendingHostResyncFrame.reset();
    return result;
}

std::optional<ParticipantId> NetplayCoordinator::consumePendingHostLateJoinResyncParticipant()
{
    std::optional<ParticipantId> result = m_pendingHostLateJoinResyncParticipant;
    m_pendingHostLateJoinResyncParticipant.reset();
    return result;
}

const InputTimeline& NetplayCoordinator::localInputs() const
{
    return m_localInputs;
}

FrameNumber NetplayCoordinator::localSimulationFrame() const
{
    return m_localSimulationFrame;
}

void NetplayCoordinator::discardTimelineAfter(FrameNumber frame, bool preserveLocalInputs)
{
    if(!preserveLocalInputs) {
        m_localInputs.eraseFramesAfter(frame);
    }

    while(!m_confirmedFrames.empty() && m_confirmedFrames.back().frame > frame) {
        m_confirmedFrames.pop_back();
    }

    m_lastBroadcastConfirmedFrame = std::min(m_lastBroadcastConfirmedFrame, frame);
    m_session.roomState().lastConfirmedFrame = std::min(m_session.roomState().lastConfirmedFrame, frame);
    for(auto it = m_authoritativeFrameStartClockMicros.begin();
        it != m_authoritativeFrameStartClockMicros.end();) {
        if(it->first > frame) {
            it = m_authoritativeFrameStartClockMicros.erase(it);
        } else {
            ++it;
        }
    }
    while(!m_authoritativeFrameStartClockOrder.empty() &&
          m_authoritativeFrameStartClockOrder.back() > frame) {
        m_authoritativeFrameStartClockOrder.pop_back();
    }
    if(m_session.roomState().lastAuthoritativeClockFrame > frame) {
        m_session.roomState().lastAuthoritativeClockFrame = frame;
        m_session.roomState().lastAuthoritativeClockMicros =
            authoritativeFrameStartClockMicros(frame);
    }

    if(!preserveLocalInputs) {
        uint32_t latestLocalSequence = 0;
        FrameNumber latestLocalFrame = frame;
        for(auto it = m_localInputs.entries().rbegin(); it != m_localInputs.entries().rend(); ++it) {
            if(it->participantId != m_localParticipantId) continue;
            latestLocalSequence = it->sequence;
            latestLocalFrame = it->frame;
            break;
        }
        m_localInputSequence = latestLocalSequence;

        if(ParticipantInfo* localParticipant = m_session.findParticipant(m_localParticipantId)) {
            localParticipant->lastReceivedInputFrame = latestLocalFrame;
            localParticipant->lastContiguousInputFrame = latestLocalFrame;
            localParticipant->lastReceivedInputSequence = latestLocalSequence;
            localParticipant->pendingMissingInputFrom.reset();
        }
    }
}

const InputTimeline& NetplayCoordinator::remoteInputs() const
{
    return m_remoteInputs;
}

const NetplayCoordinator::ConfirmedFrameInputs* NetplayCoordinator::findConfirmedFrame(FrameNumber frame) const
{
    for(auto it = m_confirmedFrames.rbegin(); it != m_confirmedFrames.rend(); ++it) {
        if(it->frame == frame) {
            return &(*it);
        }
    }
    return nullptr;
}

FrameNumber NetplayCoordinator::latestPublishedConfirmedFrame() const
{
    return m_confirmedFrames.empty() ? 0u : m_confirmedFrames.back().frame;
}

FrameNumber NetplayCoordinator::latestConfirmedFrame() const
{
    return latestPublishedConfirmedFrame();
}

FrameNumber NetplayCoordinator::authoritativeResyncTargetFrame() const
{
    return m_session.roomState().resyncTargetFrame;
}

uint8_t NetplayCoordinator::predictFrames() const
{
    return m_session.roomState().predictFrames;
}

void NetplayCoordinator::storeConfirmedFrame(const ConfirmedFrameInputs& frame)
{
    if(frame.authoritativeFrameStartClockMicros != 0u) {
        m_session.roomState().lastAuthoritativeClockFrame = frame.frame;
        m_session.roomState().lastAuthoritativeClockMicros = frame.authoritativeFrameStartClockMicros;
    }

    for(auto& existing : m_confirmedFrames) {
        if(existing.frame == frame.frame) {
            existing = frame;
            return;
        }
    }

    if(m_confirmedFrames.size() >= kConfirmedFrameHistoryCapacity) {
        m_confirmedFrames.pop_front();
    }

    m_confirmedFrames.push_back(frame);
}

bool NetplayCoordinator::tryAssembleConfirmedFrame(FrameNumber frame, ConfirmedFrameInputs& outFrame) const
{
    outFrame = {};
    outFrame.frame = frame;
    outFrame.authoritativeFrameStartClockMicros = authoritativeFrameStartClockMicros(frame);
    outFrame.inputFrame = makeRoomTopologyBaseFrame(frame, m_session.roomState());
    bool haveAssignedParticipant = false;

    for(const auto& participant : m_session.roomState().participants) {
        for(PlayerSlot slot : participantAssignments(participant)) {
            if(!isPlayableSlot(slot)) {
                continue;
            }
            const TimelineInputEntry* entry =
                participant.id == m_localParticipantId
                    ? m_localInputs.find(frame, participant.id, slot)
                    : m_remoteInputs.find(frame, participant.id, slot);
            if(entry == nullptr || !entry->confirmed) {
                return false;
            }

            haveAssignedParticipant = true;
            outFrame.buttonMaskLo[slot] = entry->buttonMaskLo;
            outFrame.buttonMaskHi[slot] = entry->buttonMaskHi;
            applyAssignedContribution(outFrame.inputFrame, slot, entry->inputFrame);
        }
    }

    return haveAssignedParticipant || std::none_of(
        m_session.roomState().participants.begin(),
        m_session.roomState().participants.end(),
        [](const ParticipantInfo& participant) {
            return !participantIsObserver(participant);
        }
    );
}

bool NetplayCoordinator::tryBuildPlaybackFrameInternal(FrameNumber frame, bool allowPrediction, ConfirmedFrameInputs& outFrame)
{
    if(const ConfirmedFrameInputs* confirmed = findConfirmedFrame(frame)) {
        outFrame = *confirmed;
        outFrame.predicted = false;
        return true;
    }

    outFrame = {};
    outFrame.frame = frame;
    outFrame.authoritativeFrameStartClockMicros = authoritativeFrameStartClockMicros(frame);
    outFrame.inputFrame = makeRoomTopologyBaseFrame(frame, m_session.roomState());
    outFrame.predicted = false;
    bool haveAssignedParticipant = false;

    for(const auto& participant : m_session.roomState().participants) {
        for(PlayerSlot slot : participantAssignments(participant)) {
            if(!isPlayableSlot(slot)) {
                continue;
            }
            const bool isLocalParticipant = participant.id == m_localParticipantId;
            const InputTimeline& timeline = isLocalParticipant ? m_localInputs : m_remoteInputs;
            const TimelineInputEntry* entry = timeline.find(frame, participant.id, slot);
            if(entry == nullptr && allowPrediction && !isLocalParticipant) {
                if(!predictRemoteInputFrame(frame, participant.id, slot)) {
                    noteImplicitRemoteInputStall(participant.id, slot, frame);
                    return false;
                }
                entry = m_remoteInputs.find(frame, participant.id, slot);
            }

            if(entry == nullptr) {
                if(!isLocalParticipant) {
                    noteImplicitRemoteInputStall(participant.id, slot, frame);
                }
                return false;
            }

            haveAssignedParticipant = true;
            outFrame.buttonMaskLo[slot] = entry->buttonMaskLo;
            outFrame.buttonMaskHi[slot] = entry->buttonMaskHi;
            applyAssignedContribution(outFrame.inputFrame, slot, entry->inputFrame);
            if(entry->predicted) {
                m_predictionStats.recordPredictedFrameUse(frame, slot);
            }
            outFrame.predicted = outFrame.predicted || entry->predicted;
        }
    }

    return haveAssignedParticipant || std::none_of(
        m_session.roomState().participants.begin(),
        m_session.roomState().participants.end(),
        [](const ParticipantInfo& participant) {
            return !participantIsObserver(participant);
        }
    );
}

bool NetplayCoordinator::tryBuildPlaybackFrame(FrameNumber frame, bool allowPrediction, ConfirmedFrameInputs& outFrame)
{
    return tryBuildPlaybackFrameInternal(frame, allowPrediction, outFrame);
}

void NetplayCoordinator::publishConfirmedFramesIfReady()
{
    if(!m_hosting) return;
    if(m_session.roomState().state != SessionState::Running) return;
    synthesizeSuspendedRemoteInputsUpTo(m_localSimulationFrame);

    std::vector<ConfirmedFrameInputs> pendingFrames;
    FrameNumber nextFrame = m_lastBroadcastConfirmedFrame + 1u;
    const FrameNumber maxPublishableFrame = computeHostInputConfirmedFrame();
    while(pendingFrames.size() < kMaxConfirmedFramesPerPacket) {
        if(nextFrame > maxPublishableFrame) {
            break;
        }
        ConfirmedFrameInputs frame;
        if(!tryAssembleConfirmedFrame(nextFrame, frame)) {
            break;
        }
        pendingFrames.push_back(frame);
        ++nextFrame;
    }

    if(pendingFrames.empty()) {
        return;
    }

    constexpr uint64_t kNominalFrameStepMicros = 16667u;
    constexpr uint64_t kMinFrameStepMicros = 1000u;
    constexpr uint64_t kMaxFrameStepMicros = 50000u;
    constexpr size_t kAuthoritativeClockHistoryLimit = kConfirmedFrameHistoryCapacity * 2u;

    uint64_t batchBaseClockMicros = sharedClockNowMicros();
    if(batchBaseClockMicros == 0u) {
        const int64_t nowMicros = monotonicNowMicros();
        batchBaseClockMicros = nowMicros > 0 ? static_cast<uint64_t>(nowMicros) : 0u;
    }

    uint64_t frameStepMicros = kNominalFrameStepMicros;
    const FrameNumber firstPendingFrame = pendingFrames.front().frame;
    if(firstPendingFrame > 1u) {
        const uint64_t previousClockMicros = authoritativeFrameStartClockMicros(firstPendingFrame - 1u);
        if(previousClockMicros != 0u && batchBaseClockMicros > previousClockMicros) {
            const uint64_t gapMicros = batchBaseClockMicros - previousClockMicros;
            const FrameNumber gapFrames = firstPendingFrame - 1u;
            const uint64_t candidateStepMicros = gapFrames > 0u
                ? (gapMicros / static_cast<uint64_t>(gapFrames))
                : 0u;
            if(candidateStepMicros >= kMinFrameStepMicros &&
               candidateStepMicros <= kMaxFrameStepMicros) {
                frameStepMicros = candidateStepMicros;
            }
        }
    }

    uint64_t nextFrameClockMicros = 0u;
    if(firstPendingFrame > 1u) {
        const uint64_t previousClockMicros = authoritativeFrameStartClockMicros(firstPendingFrame - 1u);
        if(previousClockMicros != 0u) {
            nextFrameClockMicros = previousClockMicros + frameStepMicros;
        }
    }
    if(nextFrameClockMicros == 0u) {
        nextFrameClockMicros = batchBaseClockMicros;
    }

    for(auto& frame : pendingFrames) {
        frame.authoritativeFrameStartClockMicros = nextFrameClockMicros;
        if(m_authoritativeFrameStartClockMicros.find(frame.frame) == m_authoritativeFrameStartClockMicros.end()) {
            m_authoritativeFrameStartClockOrder.push_back(frame.frame);
        }
        m_authoritativeFrameStartClockMicros[frame.frame] = frame.authoritativeFrameStartClockMicros;
        if(frameStepMicros > 0u) {
            nextFrameClockMicros += frameStepMicros;
        }
    }

    while(m_authoritativeFrameStartClockOrder.size() > kAuthoritativeClockHistoryLimit) {
        const FrameNumber dropFrame = m_authoritativeFrameStartClockOrder.front();
        m_authoritativeFrameStartClockOrder.pop_front();
        m_authoritativeFrameStartClockMicros.erase(dropFrame);
    }

    m_session.roomState().lastAuthoritativeClockFrame = pendingFrames.back().frame;
    m_session.roomState().lastAuthoritativeClockMicros =
        pendingFrames.back().authoritativeFrameStartClockMicros;

    for(const auto& frame : pendingFrames) {
        storeConfirmedFrame(frame);
    }

    ConfirmedInputFramesData data;
    data.timelineEpoch = m_session.roomState().timelineEpoch;
    data.startFrame = pendingFrames.front().frame;
    data.frameCount = static_cast<uint16_t>(pendingFrames.size());

    m_transport.broadcastReliable(
        Channel::Gameplay,
        buildConfirmedInputFramesPacket(data, std::span<const ConfirmedFrameInputs>(pendingFrames.data(), pendingFrames.size()), m_session.roomState().sessionId)
    );

    m_lastBroadcastConfirmedFrame = pendingFrames.back().frame;
    m_session.roomState().lastConfirmedFrame = std::max(m_session.roomState().lastConfirmedFrame, m_lastBroadcastConfirmedFrame);

}

void NetplayCoordinator::recordLocalInputFrame(FrameNumber frame, PlayerSlot slot, const InputFrame& contribution)
{
    if(slot == kObserverPlayerSlot || m_localParticipantId == kInvalidParticipantId) return;
    if(m_session.roomState().recoveryInputMode == RecoveryInputMode::ResyncLocked) {
        noteDroppedGameplayInputDuringRecovery("local_input", frame, m_localParticipantId, slot);
        return;
    }

    const uint64_t buttonMaskLo = assignedContributionPrimaryMask(slot, contribution);
    const uint64_t buttonMaskHi = 0;

    const TimelineInputEntry* latest = m_localInputs.latestFor(m_localParticipantId, slot);
    if(latest != nullptr && frame != latest->frame + 1u) {
        std::ostringstream oss;
        oss << "Rejected non-sequential local input frame " << frame
            << " for slot " << static_cast<unsigned>(slot) + 1u
            << " expected " << (latest->frame + 1u);
        pushLog(oss.str());
        return;
    }

    const TimelineInputEntry* existing = m_localInputs.find(frame, m_localParticipantId, slot);
    if(existing != nullptr) {
        if(existing->buttonMaskLo != buttonMaskLo ||
           existing->buttonMaskHi != buttonMaskHi ||
           !inputFramesEqual(existing->inputFrame, contribution)) {
            std::ostringstream oss;
            oss << "Ignored attempt to overwrite committed local input on frame " << frame
                << " assignment " << inputAssignmentLabel(slot, m_session.roomState())
                << " oldMaskLo " << existing->buttonMaskLo
                << " newMaskLo " << buttonMaskLo;
            pushLog(oss.str());
        }
        return;
    }

    TimelineInputEntry entry;
    entry.frame = frame;
    entry.participantId = m_localParticipantId;
    entry.playerSlot = slot;
    entry.buttonMaskLo = buttonMaskLo;
    entry.buttonMaskHi = buttonMaskHi;
    entry.inputFrame = contribution;
    entry.sequence = ++m_localInputSequence;
    entry.confirmed = m_hosting;
    m_localInputs.push(entry);

    InputFrameData packetData;
    packetData.timelineEpoch = m_session.roomState().timelineEpoch;
    packetData.frame = frame;
    packetData.authoritativeFrameStartClockMicros =
        authoritativeFrameStartClockMicros(frame);
    packetData.participantId = m_localParticipantId;
    packetData.playerSlot = slot;
    packetData.buttonMaskLo = buttonMaskLo;
    packetData.buttonMaskHi = buttonMaskHi;
    packetData.sequence = entry.sequence;
    packetData.payloadSize = static_cast<uint16_t>(serializeInputFrame(contribution).size());
    const std::vector<uint8_t> payload = buildInputFramePacket(packetData, contribution);

    if(m_hosting) {
        synthesizeSuspendedRemoteInputsUpTo(frame);
        m_transport.broadcastReliable(Channel::Gameplay, payload);
        publishConfirmedFramesIfReady();
        return;
    }

    if(!m_connected || m_serverPeer == NetTransport::kInvalidPeerHandle) return;
    m_transport.sendReliable(m_serverPeer, Channel::Gameplay, payload);
}

void NetplayCoordinator::recordLocalInputFrame(FrameNumber frame,
                                               PlayerSlot slot,
                                               uint64_t buttonMaskLo,
                                               uint64_t /*buttonMaskHi*/)
{
    InputFrame contribution = makeContributionBase(makeRoomTopologyBaseFrame(frame, m_session.roomState()));
    auto bit = [buttonMaskLo](uint32_t index) {
        return (buttonMaskLo & (1ull << index)) != 0;
    };
    switch(slot) {
        case kPort1PlayerSlot:
        case kMultitapP1PlayerSlot:
            contribution.p1A = bit(0); contribution.p1B = bit(1); contribution.p1Select = bit(2); contribution.p1Start = bit(3);
            contribution.p1Up = bit(4); contribution.p1Down = bit(5); contribution.p1Left = bit(6); contribution.p1Right = bit(7);
            contribution.p1X = bit(8); contribution.p1Y = bit(9); contribution.p1L = bit(10); contribution.p1R = bit(11);
            break;
        case kPort2PlayerSlot:
        case kMultitapP2PlayerSlot:
            contribution.p2A = bit(0); contribution.p2B = bit(1); contribution.p2Select = bit(2); contribution.p2Start = bit(3);
            contribution.p2Up = bit(4); contribution.p2Down = bit(5); contribution.p2Left = bit(6); contribution.p2Right = bit(7);
            contribution.p2X = bit(8); contribution.p2Y = bit(9); contribution.p2L = bit(10); contribution.p2R = bit(11);
            break;
        case kExpansionPlayerSlot:
        case kMultitapP3PlayerSlot:
            contribution.p3A = bit(0); contribution.p3B = bit(1); contribution.p3Select = bit(2); contribution.p3Start = bit(3);
            contribution.p3Up = bit(4); contribution.p3Down = bit(5); contribution.p3Left = bit(6); contribution.p3Right = bit(7);
            break;
        case kMultitapP4PlayerSlot:
            contribution.p4A = bit(0); contribution.p4B = bit(1); contribution.p4Select = bit(2); contribution.p4Start = bit(3);
            contribution.p4Up = bit(4); contribution.p4Down = bit(5); contribution.p4Left = bit(6); contribution.p4Right = bit(7);
            break;
        default:
            break;
    }
    recordLocalInputFrame(frame, slot, contribution);
}

void NetplayCoordinator::predictRemoteInputsForFrame(FrameNumber frame)
{
    for(const ParticipantInfo& participant : m_session.roomState().participants) {
        if(participant.id == kInvalidParticipantId || participant.id == m_localParticipantId) continue;

        for(PlayerSlot slot : participantAssignments(participant)) {
            predictRemoteInputFrame(frame, participant.id, slot);
        }
    }
}

void NetplayCoordinator::submitLocalCrc(FrameNumber frame, uint32_t crc32)
{
    if(!kDesyncMonitorEnabled) return;
    if(m_session.roomState().state != SessionState::Running) return;
    if(m_session.roomState().recoveryInputMode == RecoveryInputMode::PostResyncStabilizing) {
        ++m_session.roomState().stabilizationCrcPassCount;
    }

    applyDesyncMonitorUpdate(m_desyncMonitor.submitLocalCrc(frame, crc32), "local CRC submission");

    if(!m_connected || !m_transport.isActive()) return;

    CrcReportData report;
    report.timelineEpoch = m_session.roomState().timelineEpoch;
    report.frame = frame;
    report.crc32 = crc32;
    report.severity = DesyncSeverity::NoIssue;

    const std::vector<uint8_t> payload = buildCrcReportPacket(report, m_session.roomState().sessionId);

    if(m_hosting) {
        m_transport.broadcastReliable(Channel::Diagnostics, payload);
    } else if(m_serverPeer != NetTransport::kInvalidPeerHandle) {
        m_transport.sendReliable(m_serverPeer, Channel::Diagnostics, payload);
    }
}

void NetplayCoordinator::invalidateLocalCrcHistoryAfter(FrameNumber frame)
{
    m_desyncMonitor.invalidateLocalHistoryAfter(frame);
}

bool NetplayCoordinator::beginResync(FrameNumber targetFrame,
                                     const std::vector<uint8_t>& payload,
                                     uint32_t payloadCrc32,
                                     uint32_t stateCrc32,
                                     ResyncReason reason,
                                     ParticipantId targetParticipantId)
{
    if(!m_hosting || payload.empty()) return false;

    const bool initialSessionSync = m_session.roomState().state == SessionState::Starting;
    const bool targetedResync = targetParticipantId != kInvalidParticipantId;
    const SessionState resumeState = m_session.roomState().state;
    NetTransport::PeerHandle targetPeer = NetTransport::kInvalidPeerHandle;
    if(targetedResync) {
        const ParticipantInfo* targetParticipant = m_session.findParticipant(targetParticipantId);
        if(targetParticipant == nullptr || !targetParticipant->connected || targetParticipant->reconnectReserved) {
            return false;
        }
        targetPeer = peerFromParticipantId(targetParticipantId);
        if(targetPeer == NetTransport::kInvalidPeerHandle) {
            return false;
        }
    }
    const uint32_t resyncId = m_nextResyncId++;
    const uint32_t previousEpoch = m_session.roomState().timelineEpoch;
    if(!targetedResync) {
        ++m_session.roomState().timelineEpoch;
        realignAuthoritativeState(
            targetFrame,
            true,
            0u,
            initialSessionSync || preserveConfirmedInputsAcrossRealignment(reason)
        );
        if(initialSessionSync) {
            m_localInputSequence = 0;
            for(ParticipantInfo& participant : m_session.roomState().participants) {
                participant.lastReceivedInputSequence = 0;
            }
        }
    }
    m_activeResyncTargetParticipantId = targetedResync ? targetParticipantId : kInvalidParticipantId;
    m_activeResyncResumeState = resumeState;
    if(!targetedResync) {
        m_session.roomState().state = SessionState::Resyncing;
    } else {
        m_activeTargetedResyncId = resyncId;
        m_activeTargetedResyncFrame = targetFrame;
        m_activeTargetedResyncExpectedStateCrc32 = stateCrc32;
    }
    if(!targetedResync) {
        m_session.roomState().activeResyncId = resyncId;
        m_session.roomState().resyncTargetFrame = targetFrame;
        m_session.roomState().resyncConfirmedFrame = targetFrame;
        m_session.roomState().resyncFrameReadyFrame = targetFrame;
        m_session.roomState().resyncPayloadSize = static_cast<uint32_t>(payload.size());
        m_session.roomState().resyncPayloadCrc32 = payloadCrc32;
        m_session.roomState().resyncFrameReadyCrc32 = stateCrc32;
        m_session.roomState().resyncInputSequenceBase = 0;
        m_session.roomState().activeResyncReason = reason;
    }
    m_activeResyncExpectedStateCrc32 = targetedResync ? 0 : stateCrc32;
    if(!targetedResync) {
        m_implicitRecoveryMonitor.reset();
    }
    m_pendingResyncAcks.clear();
    if(!targetedResync) {
        setRecoveryInputMode(RecoveryInputMode::ResyncLocked, "resync-begin", targetFrame);
    }
    m_activeResyncAckDeadline = std::chrono::steady_clock::now() + kResyncAckTimeout;

    {
        std::ostringstream oss;
        oss << "Beginning authoritative resync"
            << " reason " << resyncReasonLabel(reason)
            << " targetFrame " << targetFrame
            << " epoch " << previousEpoch
            << "->" << m_session.roomState().timelineEpoch
            << " stateCrc " << stateCrc32
            << " classification=hard_resync_request";
        pushLog(oss.str());
    }

    if(targetedResync) {
        m_pendingResyncAcks.push_back(targetParticipantId);
    } else {
        for(const ParticipantInfo& participant : m_session.roomState().participants) {
            if(participant.id != m_localParticipantId &&
               participant.connected &&
               !participant.reconnectReserved) {
                m_pendingResyncAcks.push_back(participant.id);
            }
        }
    }
    m_session.roomState().pendingResyncAckCount =
        targetedResync ? 0u : static_cast<uint32_t>(m_pendingResyncAcks.size());
    if(!initialSessionSync && !targetedResync) {
        m_predictionStats.recordHardResync();
    }

    if(m_pendingResyncAcks.empty()) {
        if(targetedResync) {
            clearTargetedResyncTracking();
        } else {
            clearActiveResyncTracking(SessionState::Running);
        }
        for(ParticipantInfo& participant : m_session.roomState().participants) {
            if(participant.id == m_localParticipantId) continue;
            participant.inputSuspended = false;
            participant.inputResumeAwaitingResync = false;
        }
        setRecoveryInputMode(RecoveryInputMode::Normal, "resync-skipped-no-peers", targetFrame);
        pushLog("Resync skipped: no remote peers");
        return true;
    }

    ResyncBeginData beginData;
    beginData.resyncId = resyncId;
    beginData.timelineEpoch = m_session.roomState().timelineEpoch;
    beginData.targetFrame = targetFrame;
    beginData.confirmedFrame = m_session.roomState().lastConfirmedFrame;
    beginData.frameReadyFrame = m_session.roomState().resyncFrameReadyFrame;
    beginData.payloadSize = static_cast<uint32_t>(payload.size());
    beginData.payloadCrc32 = payloadCrc32;
    beginData.stateCrc32 = m_activeResyncExpectedStateCrc32;
    beginData.frameReadyCrc32 = m_session.roomState().resyncFrameReadyCrc32;
    beginData.inputSequenceBase = m_session.roomState().resyncInputSequenceBase;
    beginData.reason = reason;
    const std::vector<uint8_t> beginPacket = buildResyncBeginPacket(beginData);
    if(targetedResync) {
        m_transport.sendReliable(targetPeer, Channel::Control, beginPacket);
    } else {
        m_transport.broadcastReliable(Channel::Control, beginPacket);
    }

    for(size_t offset = 0; offset < payload.size(); offset += kResyncChunkPayloadBytes) {
        const size_t chunkSize = std::min(kResyncChunkPayloadBytes, payload.size() - offset);
        ResyncChunkData chunkData;
        chunkData.resyncId = resyncId;
        chunkData.offset = static_cast<uint32_t>(offset);
        chunkData.size = static_cast<uint16_t>(chunkSize);
        const std::vector<uint8_t> chunkPacket =
            buildResyncChunkPacket(chunkData, std::span<const uint8_t>(payload.data() + offset, chunkSize));
        if(targetedResync) {
            m_transport.sendReliable(targetPeer, Channel::Control, chunkPacket);
        } else {
            m_transport.broadcastReliable(Channel::Control, chunkPacket);
        }
    }

    ResyncCompleteData completeData;
    completeData.resyncId = resyncId;
    const std::vector<uint8_t> completePacket = buildResyncCompletePacket(completeData);
    if(targetedResync) {
        m_transport.sendReliable(targetPeer, Channel::Control, completePacket);
    } else {
        m_transport.broadcastReliable(Channel::Control, completePacket);
    }
    {
        std::ostringstream oss;
        oss << "Owner forced resync"
            << " reason " << resyncReasonLabel(reason)
            << " targetFrame " << targetFrame
            << " resyncId " << resyncId;
        if(targetedResync) {
            oss << " targetParticipantId " << static_cast<int>(targetParticipantId);
        }
        pushLog(oss.str());
    }
    return true;
}

std::optional<NetplayCoordinator::PendingResyncApply> NetplayCoordinator::consumePendingResyncApply()
{
    if(m_pendingResyncApply.has_value() &&
       m_session.roomState().activeResyncId != 0u &&
       m_pendingResyncApply->resyncId != m_session.roomState().activeResyncId) {
        std::ostringstream oss;
        oss << "Discarded stale pending resync apply during consume"
            << " pendingResyncId " << m_pendingResyncApply->resyncId
            << " activeResyncId " << m_session.roomState().activeResyncId;
        pushLog(oss.str());
        m_pendingResyncApply.reset();
    }
    std::optional<PendingResyncApply> result = std::move(m_pendingResyncApply);
    m_pendingResyncApply.reset();
    return result;
}

bool NetplayCoordinator::acknowledgeResync(uint32_t resyncId, FrameNumber loadedFrame, uint32_t crc32, bool success)
{
    if(m_hosting || m_serverPeer == NetTransport::kInvalidPeerHandle) return false;

    ResyncAckData ack;
    ack.resyncId = resyncId;
    ack.participantId = m_localParticipantId;
    ack.loadedFrame = loadedFrame;
    ack.crc32 = crc32;
    ack.success = success ? 1 : 0;

    if(success) {
        {
            std::ostringstream oss;
            oss << "Acknowledging authoritative resync"
                << " reason " << resyncReasonLabel(m_session.roomState().activeResyncReason)
                << " loadedFrame " << loadedFrame
                << " crc32 " << crc32
                << " resyncId " << resyncId;
            pushLog(oss.str());
        }
        realignAuthoritativeState(
            loadedFrame,
            true,
            m_session.roomState().resyncInputSequenceBase,
            preserveConfirmedInputsAcrossRealignment(m_session.roomState().activeResyncReason)
        );
        m_session.roomState().activeResyncId = 0;
        m_session.roomState().resyncTargetFrame = 0;
        m_session.roomState().resyncConfirmedFrame = 0;
        m_session.roomState().resyncFrameReadyFrame = 0;
        m_session.roomState().resyncPayloadSize = 0;
        m_session.roomState().resyncPayloadCrc32 = 0;
        m_session.roomState().resyncFrameReadyCrc32 = 0;
        m_session.roomState().resyncInputSequenceBase = 0;
        m_session.roomState().pendingResyncAckCount = 0;
        m_session.roomState().state = SessionState::Resyncing;
        setRecoveryInputMode(
            RecoveryInputMode::PostResyncStabilizing,
            "resync-load-acknowledged",
            loadedFrame,
            kRecoveryStabilizationFrames
        );
    }

    return m_transport.sendReliable(m_serverPeer, Channel::Control, buildResyncAckPacket(ack));
}

bool NetplayCoordinator::requestHostResync(ResyncReason reason)
{
    if(m_hosting ||
       m_serverPeer == NetTransport::kInvalidPeerHandle ||
       !m_connected ||
       m_localParticipantId == kInvalidParticipantId) {
        return false;
    }

    ResyncRequestData request;
    request.participantId = m_localParticipantId;
    request.reason = reason;

    {
        std::ostringstream oss;
        oss << "Requesting host resync"
            << " reason " << resyncReasonLabel(reason)
            << " participant " << static_cast<unsigned>(m_localParticipantId);
        pushLog(oss.str());
    }

    return m_transport.sendReliable(m_serverPeer, Channel::Control, buildResyncRequestPacket(request));
}

bool NetplayCoordinator::assignController(ParticipantId participantId, PlayerSlot slot)
{
    std::string blockedReason;
    if(assignmentMutationBlocked(&blockedReason)) {
        pushLog(blockedReason);
        return false;
    }

    if(slot == kObserverPlayerSlot) {
        return clearControllerAssignments(participantId);
    }
    if(!clearControllerAssignments(participantId)) return false;
    return addControllerAssignment(participantId, slot);
}

bool NetplayCoordinator::addControllerAssignment(ParticipantId participantId, PlayerSlot slot)
{
    if(!m_hosting) return false;
    std::string blockedReason;
    if(assignmentMutationBlocked(&blockedReason)) {
        pushLog(blockedReason);
        return false;
    }
    if(!isAssignmentAvailable(slot, m_session.roomState())) return false;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr) return false;
    if(!participant->connected) {
        pushLog("Rejected assignment update for inactive participant " + participantLabel(*participant));
        return false;
    }
    pushLog("Applying assignment update for " + participantLabel(*participant));
    if(participantHasAssignment(*participant, slot)) return true;

    std::vector<ParticipantId> changedParticipants;
    std::vector<std::pair<PlayerSlot, std::string>> assignmentToasts;
    for(ParticipantInfo& other : m_session.roomState().participants) {
        if(other.id != participantId && participantHasAssignment(other, slot) && slot != kObserverPlayerSlot) {
            other.controllerAssignments.erase(
                std::remove(other.controllerAssignments.begin(), other.controllerAssignments.end(), slot),
                other.controllerAssignments.end()
            );
            syncParticipantRoleWithAssignments(other, other.id == m_localParticipantId);
            changedParticipants.push_back(other.id);
            assignmentToasts.emplace_back(participantIsObserver(other) ? kObserverPlayerSlot : other.controllerAssignment,
                                         participantLabel(other));
        }
    }

    const FrameNumber assignmentBaselineFrame =
        std::max({m_localSimulationFrame,
                  m_session.roomState().currentFrame,
                  m_session.roomState().lastConfirmedFrame});
    participant->controllerAssignments.push_back(slot);
    syncParticipantRoleWithAssignments(*participant, participantId == m_localParticipantId);
    discardTimelineStateAfter(assignmentBaselineFrame);
    participant->lastReceivedInputFrame = assignmentBaselineFrame;
    participant->lastContiguousInputFrame = assignmentBaselineFrame;
    participant->lastReceivedInputSequence = 0;
    participant->pendingMissingInputFrom.reset();
    if(participantId == m_localParticipantId) {
        m_localInputSequence = 0;
    }
    participant->lastReceivedInputFrame = assignmentBaselineFrame;
    participant->lastContiguousInputFrame = assignmentBaselineFrame;
    seedNeutralInputBaseline(participantId, slot, assignmentBaselineFrame);
    changedParticipants.push_back(participantId);
    assignmentToasts.emplace_back(slot, participantLabel(*participant));
    refreshHostRoomState();

    for(ParticipantId changedId : changedParticipants) {
        ParticipantInfo* changed = m_session.findParticipant(changedId);
        if(changed == nullptr) continue;
        const AssignControllerData data = makeAssignControllerData(*changed);
        m_transport.broadcastReliable(Channel::Control, buildAssignControllerPacket(data, m_session.roomState().sessionId));
        m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(*changed, 0));
    }

    const bool shouldAutoResyncAfterAssignment =
        (m_session.roomState().state == SessionState::Running ||
         m_session.roomState().state == SessionState::Paused) &&
        m_session.roomState().currentFrame > 0;
    if(shouldAutoResyncAfterAssignment) {
        queuePendingHostResync(assignmentBaselineFrame, ResyncReason::AssignmentChanged);
        pushLog("Controller assignment changed; scheduling automatic resync");
    }

    for(const auto& [assignedSlot, participantName] : assignmentToasts) {
        pushLog(controllerAssignmentToast(assignedSlot, m_session.roomState(), participantName));
    }

    return true;
}

bool NetplayCoordinator::removeControllerAssignment(ParticipantId participantId, PlayerSlot slot)
{
    if(!m_hosting) return false;
    std::string blockedReason;
    if(assignmentMutationBlocked(&blockedReason)) {
        pushLog(blockedReason);
        return false;
    }

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr) return false;
    if(!participant->connected) {
        pushLog("Rejected assignment update for inactive participant " + participantLabel(*participant));
        return false;
    }
    pushLog("Applying assignment update for " + participantLabel(*participant));
    if(!participantHasAssignment(*participant, slot)) return true;

    const FrameNumber assignmentBaselineFrame =
        std::max({m_localSimulationFrame,
                  m_session.roomState().currentFrame,
                  m_session.roomState().lastConfirmedFrame});
    participant->controllerAssignments.erase(
        std::remove(participant->controllerAssignments.begin(), participant->controllerAssignments.end(), slot),
        participant->controllerAssignments.end()
    );
    syncParticipantRoleWithAssignments(*participant, participantId == m_localParticipantId);
    discardTimelineStateAfter(assignmentBaselineFrame);
    participant->lastReceivedInputFrame = assignmentBaselineFrame;
    participant->lastContiguousInputFrame = assignmentBaselineFrame;
    participant->lastReceivedInputSequence = 0;
    participant->pendingMissingInputFrom.reset();
    if(participantId == m_localParticipantId) {
        m_localInputSequence = 0;
    }
    refreshHostRoomState();

    const AssignControllerData data = makeAssignControllerData(*participant);
    m_transport.broadcastReliable(Channel::Control, buildAssignControllerPacket(data, m_session.roomState().sessionId));
    m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(*participant, 0));

    const bool shouldAutoResyncAfterAssignment =
        (m_session.roomState().state == SessionState::Running ||
         m_session.roomState().state == SessionState::Paused) &&
        m_session.roomState().currentFrame > 0;
    if(shouldAutoResyncAfterAssignment) {
        queuePendingHostResync(assignmentBaselineFrame, ResyncReason::AssignmentChanged);
        pushLog("Controller assignment changed; scheduling automatic resync");
    }

    pushLog(controllerAssignmentToast(
        participantIsObserver(*participant) ? kObserverPlayerSlot : participant->controllerAssignment,
        m_session.roomState(),
        participantLabel(*participant)
    ));
    return true;
}

bool NetplayCoordinator::clearControllerAssignments(ParticipantId participantId)
{
    if(!m_hosting) return false;
    std::string blockedReason;
    if(assignmentMutationBlocked(&blockedReason)) {
        pushLog(blockedReason);
        return false;
    }

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr) return false;
    if(!participant->connected) {
        pushLog("Rejected assignment update for inactive participant " + participantLabel(*participant));
        return false;
    }
    if(participantIsObserver(*participant)) return true;

    std::vector<PlayerSlot> assignments = participant->controllerAssignments;
    for(PlayerSlot slot : assignments) {
        if(!removeControllerAssignment(participantId, slot)) {
            return false;
        }
    }
    return true;
}

bool NetplayCoordinator::setInputDelayFrames(uint8_t frames)
{
    if(!m_hosting) return false;

    if(m_session.roomState().inputDelayFrames == frames) return true;

    m_session.roomState().inputDelayFrames = frames;
    pushLog("Input delay set to " + std::to_string(static_cast<unsigned>(frames)) + " frame(s)");

    FrameStatusData status;
    status.timelineEpoch = m_session.roomState().timelineEpoch;
    status.currentFrame = m_session.roomState().currentFrame;
    status.lastConfirmedFrame = m_session.roomState().lastConfirmedFrame;
    status.inputDelayFrames = m_session.roomState().inputDelayFrames;
    status.predictFrames = m_session.roomState().predictFrames;
    status.topology = makeTopologyData(m_session.roomState());
    m_lastBroadcastInputDelayFrames = status.inputDelayFrames;
    m_transport.broadcastReliable(Channel::Diagnostics, buildFrameStatusPacket(status, m_session.roomState().sessionId));
    return true;
}

bool NetplayCoordinator::setPredictFrames(uint8_t frames)
{
    if(!m_hosting) return false;

    if(m_session.roomState().predictFrames == frames) return true;

    m_session.roomState().predictFrames = frames;
    pushLog("Predict window set to " + std::to_string(static_cast<unsigned>(frames)) + " frame(s)");

    FrameStatusData status;
    status.timelineEpoch = m_session.roomState().timelineEpoch;
    status.currentFrame = m_session.roomState().currentFrame;
    status.lastConfirmedFrame = m_session.roomState().lastConfirmedFrame;
    status.inputDelayFrames = m_session.roomState().inputDelayFrames;
    status.predictFrames = m_session.roomState().predictFrames;
    status.topology = makeTopologyData(m_session.roomState());
    m_transport.broadcastReliable(Channel::Diagnostics, buildFrameStatusPacket(status, m_session.roomState().sessionId));
    return true;
}

bool NetplayCoordinator::kickParticipant(ParticipantId participantId)
{
    if(!m_hosting || participantId == m_localParticipantId) return false;

    auto& participants = m_session.roomState().participants;
    if(std::none_of(participants.begin(), participants.end(), [participantId](const ParticipantInfo& participant) {
        return participant.id == participantId;
    })) {
        return false;
    }

    NetTransport::PeerHandle kickedPeer = NetTransport::kInvalidPeerHandle;
    for(NetTransport::PeerHandle peer : m_transport.connectedPeers()) {
        if(participantIdFromPeer(peer) == participantId) {
            kickedPeer = peer;
            break;
        }
    }

    removeParticipant(participantId);
    if(kickedPeer != NetTransport::kInvalidPeerHandle) {
        m_transport.sendReliable(kickedPeer, Channel::Control, buildParticipantLeftPacket(participantId));
        m_transport.flush();
        PendingKickDisconnect pending;
        pending.peer = kickedPeer;
        pending.participantId = participantId;
        pending.disconnectAt = std::chrono::steady_clock::now() + kKickDisconnectGrace;
        m_pendingKickDisconnects.push_back(pending);
    }
    m_transport.broadcastReliable(Channel::Control, buildParticipantLeftPacket(participantId), kickedPeer);
    refreshHostRoomState();
    pushLog("Participant kicked: " + std::to_string(static_cast<int>(participantId)));
    return true;
}

bool NetplayCoordinator::removeReconnectReservation(ParticipantId participantId)
{
    if(!m_hosting) return false;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr || !participant->reconnectReserved) return false;

    pushLog("Reconnect reservation removed for " + participant->displayName);
    m_reconnectReservationDeadlines.erase(participantId);
    removeParticipant(participantId);
    m_transport.broadcastReliable(Channel::Control, buildParticipantLeftPacket(participantId));
    refreshHostRoomState();
    return true;
}

bool NetplayCoordinator::selectRom(const std::string& gameName, const RomValidationData& romValidation)
{
    if(!m_hosting) return false;

    m_session.roomState().selectedGameName = gameName;
    m_session.roomState().romValidation = romValidation;
    m_session.roomState().state = SessionState::ValidatingRom;

    for(ParticipantInfo& participant : m_session.roomState().participants) {
        participant.romLoaded = participant.id == m_localParticipantId;
        participant.romCompatible = participant.id == m_localParticipantId;
    }

    m_transport.broadcastReliable(Channel::Control, buildSelectRomPacket(gameName, romValidation));

    RomValidationResultData hostResult;
    hostResult.participantId = m_localParticipantId;
    hostResult.romLoaded = 1;
    hostResult.romCompatible = 1;
    hostResult.romValidation = romValidation;
    m_transport.broadcastReliable(Channel::Control, buildRomValidationResultPacket(hostResult));

    pushLog("Owner selected ROM: " + gameName);
    return true;
}

bool NetplayCoordinator::submitLocalRomValidation(bool romLoaded, bool romCompatible, const RomValidationData& romValidation)
{
    if(m_localParticipantId == kInvalidParticipantId) return false;

    ParticipantInfo* participant = m_session.findParticipant(m_localParticipantId);
    if(participant == nullptr) return false;

    participant->romLoaded = romLoaded;
    participant->romCompatible = romCompatible;

    RomValidationResultData result;
    result.participantId = m_localParticipantId;
    result.romLoaded = romLoaded ? 1 : 0;
    result.romCompatible = romCompatible ? 1 : 0;
    result.romValidation = romValidation;

    if(m_hosting) {
        m_transport.broadcastReliable(Channel::Control, buildRomValidationResultPacket(result));
        refreshHostRoomState();
        return true;
    }

    if(m_serverPeer != NetTransport::kInvalidPeerHandle) {
        return m_transport.sendReliable(m_serverPeer, Channel::Control, buildRomValidationResultPacket(result));
    }

    return false;
}

bool NetplayCoordinator::startSession()
{
    if(!m_hosting) return false;
    if(!allRequiredParticipantsRomCompatible()) {
        pushLog("Cannot start session: ROM validation is incomplete or incompatible");
        return false;
    }

    const bool requiresInitialSync = m_localSimulationFrame > 0;
    resetRuntimeTimelineStateForSessionStart();
    m_session.roomState().state = requiresInitialSync ? SessionState::Starting : SessionState::Running;
    StartSessionData data;
    data.state = m_session.roomState().state;
    data.inputDelayFrames = m_session.roomState().inputDelayFrames;
    data.predictFrames = m_session.roomState().predictFrames;
    data.topology = makeTopologyData(m_session.roomState());
    m_transport.broadcastReliable(Channel::Control, buildStartSessionPacket(data, m_session.roomState().sessionId));
    pushLog(requiresInitialSync
        ? ("Owner started session and is awaiting initial sync at frame " + std::to_string(m_localSimulationFrame))
        : "Owner started session");
    return true;
}

bool NetplayCoordinator::pauseSession()
{
    if(!m_hosting || m_session.roomState().state != SessionState::Running) return false;

    m_session.roomState().state = SessionState::Paused;
    PacketWriter writer;
    PacketHeader header;
    header.type = MessageType::PauseSession;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    StartSessionData data;
    data.state = SessionState::Paused;
    data.inputDelayFrames = m_session.roomState().inputDelayFrames;
    data.predictFrames = m_session.roomState().predictFrames;
    data.topology = makeTopologyData(m_session.roomState());
    writer.writePod(data);
    m_transport.broadcastReliable(Channel::Control, writer.data());
    pushLog("Owner paused session");
    pushToast("Owner paused");
    return true;
}

bool NetplayCoordinator::resumeSession()
{
    if(!m_hosting || m_session.roomState().state != SessionState::Paused) return false;
    if(!allRequiredParticipantsRomCompatible()) {
        pushLog("Cannot resume session: assigned participants are not all connected and ROM-compatible");
        return false;
    }

    m_session.roomState().state = SessionState::Running;
    PacketWriter writer;
    PacketHeader header;
    header.type = MessageType::ResumeSession;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    StartSessionData data;
    data.state = SessionState::Running;
    data.inputDelayFrames = m_session.roomState().inputDelayFrames;
    data.predictFrames = m_session.roomState().predictFrames;
    data.topology = makeTopologyData(m_session.roomState());
    writer.writePod(data);
    m_transport.broadcastReliable(Channel::Control, writer.data());
    pushLog("Owner resumed session");
    pushToast("Owner resumed");
    return true;
}

bool NetplayCoordinator::endSession()
{
    if(!m_hosting || m_session.roomState().state == SessionState::Ended) return false;

    m_session.roomState().state = SessionState::Ended;
    PacketWriter writer;
    PacketHeader header;
    header.type = MessageType::EndSession;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    StartSessionData data;
    data.state = SessionState::Ended;
    data.inputDelayFrames = m_session.roomState().inputDelayFrames;
    data.predictFrames = m_session.roomState().predictFrames;
    data.topology = makeTopologyData(m_session.roomState());
    writer.writePod(data);
    m_transport.broadcastReliable(Channel::Control, writer.data());
    pushLog("Owner ended session");
    return true;
}

} // namespace Netplay
