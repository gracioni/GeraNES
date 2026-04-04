#include "logger/logger.h"
#include "GeraNESNetplay/NetplayCoordinator.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>

#ifdef ERROR
#undef ERROR
#endif

#include "GeraNESNetplay/NetplayInputAssignment.h"
#include "GeraNES/defines.h"
#include "GeraNES/util/Crc32.h"
#include "GeraNES/Serialization.h"

namespace {

constexpr size_t kResyncChunkPayloadBytes = 1024;
constexpr size_t kRecentLocalCrcHistoryCapacity = 512;
constexpr size_t kConfirmedFrameHistoryCapacity = 16384;
constexpr uint16_t kMaxConfirmedFramesPerPacket = 64;
constexpr uint16_t kReconnectReservationSeconds = 300;
constexpr auto kReconnectRetryDelay = std::chrono::milliseconds(750);
constexpr auto kGracefulDisconnectTimeout = std::chrono::milliseconds(750);
constexpr auto kIncomingResyncTimeout = std::chrono::milliseconds(750);

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
    oss << " for up to " << maxPeers << " peers";
    return oss.str();
}

void notifySessionEvent(std::string_view message)
{
    Logger::instance().log(message, Logger::Type::USER);
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
    return "Player";
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
        case MessageType::ResyncBegin: return "ResyncBegin";
        case MessageType::ResyncChunk: return "ResyncChunk";
        case MessageType::ResyncComplete: return "ResyncComplete";
        case MessageType::ResyncAck: return "ResyncAck";
        case MessageType::ResyncAbort: return "ResyncAbort";
        default: return "Unknown";
    }
}

std::string NetplayCoordinator::resyncReasonToast(ResyncReason reason)
{
    switch(reason) {
        case ResyncReason::HostReset: return "Host reset the game";
        case ResyncReason::HostLoadedState: return "Host loaded a save state";
        default: return {};
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
    m_lastLocalCrcFrame = 0;
    m_lastLocalCrc32 = 0;
    m_recentLocalCrcHistory.clear();
    m_lastCrcMismatchFrame = 0;
    m_consecutiveCrcMismatchCount = 0;
    m_localSimulationFrame = 0;
    m_nextResyncId = 1;
    m_incomingResync.reset();
    m_pendingResyncApply.reset();
    m_pendingHostLateJoinResyncParticipant.reset();
    m_pendingImplicitRecovery.reset();
    m_pendingResyncAcks.clear();
    m_reconnectReservationDeadlines.clear();
    m_lastTransportError.clear();
    clearReconnectAttemptState();
    m_delayedPacketEvents.clear();
}

void NetplayCoordinator::pushLog(const std::string& message)
{
    m_eventLog.push_back(message);

    constexpr size_t MAX_LOG_LINES = 256;
    if(m_eventLog.size() > MAX_LOG_LINES) {
        m_eventLog.erase(m_eventLog.begin(), m_eventLog.begin() + (m_eventLog.size() - MAX_LOG_LINES));
    }
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
    m_session.roomState().participants.push_back(participant);
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
    m_pendingResyncAcks.erase(
        std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), participantId),
        m_pendingResyncAcks.end()
    );
    m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
    if(m_pendingImplicitRecovery.has_value() && m_pendingImplicitRecovery->participantId == participantId) {
        m_pendingImplicitRecovery.reset();
    }
}

void NetplayCoordinator::clearReconnectAttemptState()
{
    m_reconnectPending = false;
    m_reconnectAttemptInFlight = false;
    m_reconnectSecondsRemaining = 0;
    m_nextReconnectAttempt = {};
    m_reconnectDeadline = {};
}

void NetplayCoordinator::completeLocalDisconnect()
{
    if(m_transport.isActive()) {
        m_transport.shutdown();
    }
    resetSessionState();
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

    if(m_transport.isActive()) {
        m_transport.disconnectAll();
        m_transport.shutdown();
    }

    resetSessionState();
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
        m_lastError = "Failed to reconnect to host";
        pushLog(m_lastError);
        return;
    }

    pushLog("Attempting reconnect to " + describeConnectTarget(m_transport.backend(), m_transport.options(), hostName, port));
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
    if(input.timelineEpoch != m_session.roomState().timelineEpoch) {
        if(input.timelineEpoch < m_session.roomState().timelineEpoch) {
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

    ParticipantInfo* participant = m_session.findParticipant(input.participantId);
    uint32_t previousReceivedSequence = 0;
    if(participant != nullptr) {
        previousReceivedSequence = participant->lastReceivedInputSequence;
        if(!participantHasAssignment(*participant, input.playerSlot)) {
            std::ostringstream oss;
            oss << "Ignored input for unexpected assignment from " << participant->displayName
                << ": got " << inputAssignmentLabel(input.playerSlot, m_session.roomState());
            if(participantIsObserver(*participant)) {
                oss << ", expected observer";
            } else {
                oss << ", expected one of " << participantAssignmentsLabel(*participant, m_session.roomState());
            }
            pushLog(oss.str());
            return false;
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
            input.frame == expectedFrame &&
            input.sequence > participant->lastReceivedInputSequence;
        if(input.sequence != expectedSequence && !allowSequenceRebase) {
            std::ostringstream oss;
            oss << "Rejected non-sequential input sequence from " << participant->displayName
                << " seq " << input.sequence
                << " expectedSeq " << expectedSequence
                << " frame " << input.frame;
            pushLog(oss.str());
            return true;
        }

        if(input.frame != expectedFrame) {
            std::ostringstream oss;
            oss << "Rejected non-sequential input from " << participant->displayName
                << " frame " << input.frame
                << " expectedFrame " << expectedFrame
                << " seq " << input.sequence;
            pushLog(oss.str());
            return true;
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
        m_transport.broadcastReliable(Channel::Gameplay, buildInputFramePacket(input, inputFrame), peer);
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

    for(uint16_t i = 0; i < data.frameCount; ++i) {
        ConfirmedInputFrameEntry entry;
        if(!reader.readPod(entry)) return false;
        std::vector<uint8_t> payload;
        if(!reader.readBytes(payload, entry.payloadSize)) return false;

        ConfirmedFrameInputs frame;
        frame.frame = data.startFrame + static_cast<FrameNumber>(i);
        frame.buttonMaskLo = entry.buttonMaskLo;
        frame.buttonMaskHi = entry.buttonMaskHi;
        if(!deserializeInputFrame(payload.data(), payload.size(), frame.inputFrame)) return false;
        storeConfirmedFrame(frame);
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

    if(!m_pendingHostResyncFrame.has_value() || targetFrame < *m_pendingHostResyncFrame) {
        m_pendingHostResyncFrame = targetFrame;
    }

    m_pendingResyncAcks.clear();
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
    m_pendingImplicitRecovery.reset();
    m_session.roomState().state = SessionState::Paused;
    pushLog(reason);
}

void NetplayCoordinator::noteImplicitRemoteInputStall(ParticipantId participantId, PlayerSlot slot, FrameNumber frame)
{
    if(!m_hosting || m_session.roomState().state != SessionState::Running) return;
    if(participantId == kInvalidParticipantId || participantId == m_localParticipantId) return;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr || !participant->connected || participantIsObserver(*participant)) return;

    if(m_pendingImplicitRecovery.has_value() &&
       m_pendingImplicitRecovery->participantId == participantId &&
       m_pendingImplicitRecovery->playerSlot == slot &&
       m_pendingImplicitRecovery->stalledFrame == frame) {
        return;
    }

    PendingImplicitRecovery pending;
    pending.participantId = participantId;
    pending.playerSlot = slot;
    pending.stalledFrame = frame;
    pending.observedPeerHealthSerial = participant->peerHealthSerial;
    m_pendingImplicitRecovery = pending;

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
    if(!m_pendingImplicitRecovery.has_value()) return;
    if(m_pendingImplicitRecovery->participantId != participantId) return;
    if(recoveredThroughFrame < m_pendingImplicitRecovery->stalledFrame) return;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant != nullptr) {
        std::ostringstream oss;
        oss << "Implicit input stall recovered for " << participant->displayName
            << " by input frame " << recoveredThroughFrame;
        pushLog(oss.str());
    }

    m_pendingImplicitRecovery.reset();
}

void NetplayCoordinator::tryScheduleImplicitRecoveryResync(ParticipantInfo& participant)
{
    if(!m_hosting || !m_pendingImplicitRecovery.has_value()) return;
    if(m_pendingImplicitRecovery->participantId != participant.id) return;
    if(m_session.roomState().state != SessionState::Running) return;
    if(m_session.roomState().activeResyncId != 0 || m_session.roomState().pendingResyncAckCount != 0) return;
    if(participant.peerHealthSerial <= m_pendingImplicitRecovery->observedPeerHealthSerial) return;

    const FrameNumber resyncFrame =
        m_session.roomState().lastConfirmedFrame > 0
            ? std::min(m_localSimulationFrame, m_session.roomState().lastConfirmedFrame)
            : m_localSimulationFrame;

    if(!m_pendingHostResyncFrame.has_value() || resyncFrame < *m_pendingHostResyncFrame) {
        m_pendingHostResyncFrame = resyncFrame;
    }

    std::ostringstream oss;
    oss << "Fresh peer health received from " << participant.displayName
        << " after implicit input stall; scheduling recovery resync from frame "
        << resyncFrame;
    pushLog(oss.str());

    m_pendingImplicitRecovery.reset();
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
           m_session.roomState().state != SessionState::Resyncing &&
           (!m_pendingHostResyncFrame.has_value() || inputFrame < *m_pendingHostResyncFrame)) {
            m_pendingHostResyncFrame = inputFrame;
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
        pushLog(predictionMessage());
    }
}

bool NetplayCoordinator::handleFrameStatus(PacketReader& reader)
{
    FrameStatusData status;
    if(!reader.readPod(status)) return false;
    if(status.timelineEpoch != m_session.roomState().timelineEpoch) {
        if(status.timelineEpoch < m_session.roomState().timelineEpoch) {
            return true;
        }
        return true;
    }

    if(m_hosting) {
        return true;
    }

    m_session.roomState().currentFrame = status.currentFrame;
    m_session.roomState().lastConfirmedFrame = status.lastConfirmedFrame;
    m_session.roomState().inputDelayFrames = status.inputDelayFrames;
    m_session.roomState().predictFrames = status.predictFrames;
    applyTopologyData(m_session.roomState(), status.topology);
    return true;
}

bool NetplayCoordinator::handleCrcReport(PacketReader& reader)
{
    CrcReportData report;
    if(!reader.readPod(report)) return false;
    if(!kDesyncMonitorEnabled) return true;
    if(report.timelineEpoch != m_session.roomState().timelineEpoch) {
        if(report.timelineEpoch < m_session.roomState().timelineEpoch) {
            pushLog("Ignored stale CRC report from previous timeline epoch");
            return true;
        }
        return true;
    }

    if(m_session.roomState().state != SessionState::Running) {
        return true;
    }

    m_session.roomState().lastRemoteCrcFrame = report.frame;
    m_session.roomState().lastRemoteCrc32 = report.crc32;

    const std::optional<uint32_t> matchingLocalCrc = findRecentLocalCrc(report.frame);
    if(matchingLocalCrc.has_value() && *matchingLocalCrc != 0 && *matchingLocalCrc != report.crc32) {
        pushLog("CRC mismatch detected on frame " + std::to_string(report.frame));

        if(m_lastCrcMismatchFrame != 0 && report.frame >= m_lastCrcMismatchFrame) {
            m_consecutiveCrcMismatchCount = static_cast<uint8_t>(std::min<unsigned>(255u, m_consecutiveCrcMismatchCount + 1u));
        } else {
            m_consecutiveCrcMismatchCount = 1;
        }
        m_lastCrcMismatchFrame = report.frame;

        if(m_hosting &&
           m_session.roomState().state != SessionState::Resyncing &&
           m_consecutiveCrcMismatchCount >= 1 &&
           (!m_pendingHostResyncFrame.has_value() || report.frame < *m_pendingHostResyncFrame)) {
            m_pendingHostResyncFrame = report.frame;
        }
    } else if(matchingLocalCrc.has_value() && *matchingLocalCrc == report.crc32) {
        m_lastCrcMismatchFrame = 0;
        m_consecutiveCrcMismatchCount = 0;
    }

    return true;
}

std::optional<uint32_t> NetplayCoordinator::findRecentLocalCrc(FrameNumber frame) const
{
    for(auto it = m_recentLocalCrcHistory.rbegin(); it != m_recentLocalCrcHistory.rend(); ++it) {
        if(it->first == frame) {
            return it->second;
        }
    }
    return std::nullopt;
}

void NetplayCoordinator::realignAuthoritativeState(FrameNumber loadedFrame,
                                                   bool resetInputSequences,
                                                   uint32_t inputSequenceBase)
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
    m_lastCrcMismatchFrame = 0;
    m_consecutiveCrcMismatchCount = 0;
    m_session.roomState().currentFrame = loadedFrame;
    m_session.roomState().lastConfirmedFrame = loadedFrame;
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

    ConfirmedFrameInputs confirmedFrame;
    confirmedFrame.frame = loadedFrame;
    confirmedFrame.inputFrame = makeRoomTopologyBaseFrame(loadedFrame, m_session.roomState());
    bool haveConfirmedFrame = false;
    for(const ParticipantInfo& participant : m_session.roomState().participants) {
        for(PlayerSlot slot : participantAssignments(participant)) {
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

    for(ParticipantInfo& participant : m_session.roomState().participants) {
        participant.lastReceivedInputFrame = loadedFrame;
        participant.lastContiguousInputFrame = loadedFrame;
        participant.lastReceivedInputSequence = latestSequenceForParticipant(participant.id);
        participant.sequenceRebasePending = resetInputSequences;
        participant.pendingMissingInputFrom.reset();
        participant.lastDecisionFrame = loadedFrame;
        participant.lastDecisionSlot = kObserverPlayerSlot;
        participant.lastDecision.clear();
    }

    m_recentLocalCrcHistory.clear();
    m_lastLocalCrcFrame = loadedFrame;
    m_lastLocalCrc32 = 0;
    m_localSimulationFrame = loadedFrame;
    m_pendingSequenceResetParticipants.clear();
}

void NetplayCoordinator::resetRuntimeTimelineStateForSessionStart()
{
    m_localInputs.clear();
    m_remoteInputs.clear();
    m_confirmedFrames.clear();
    m_pendingRollbackFrame.reset();
    m_pendingHostResyncFrame.reset();
    m_pendingHostLateJoinResyncParticipant.reset();
    m_pendingImplicitRecovery.reset();
    m_pendingResyncApply.reset();
    m_pendingResyncAcks.clear();
    m_activeResyncExpectedStateCrc32 = 0;
    m_recentLocalCrcHistory.clear();
    m_lastBroadcastConfirmedFrame = 0;
    m_lastLocalCrcFrame = 0;
    m_lastLocalCrc32 = 0;
    m_lastCrcMismatchFrame = 0;
    m_consecutiveCrcMismatchCount = 0;
    m_localInputSequence = 0;
    m_localSimulationFrame = 0;
    m_predictionStats.lastDecision.clear();
    m_predictionStats.lastDecisionFrame = 0;
    m_predictionStats.lastDecisionSlot = kObserverPlayerSlot;
    m_session.roomState().currentFrame = 0;
    m_session.roomState().lastConfirmedFrame = 0;
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
    }
}

void NetplayCoordinator::seedNeutralInputBaseline(ParticipantId participantId, PlayerSlot slot, FrameNumber frame)
{
    if(participantId == kInvalidParticipantId || slot == kObserverPlayerSlot) return;

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
        const bool keepHostRole = participant->id == m_localParticipantId && m_hosting;
        syncParticipantRoleWithAssignments(*participant, keepHostRole);
        const bool assignmentChanged = previousAssignments != participant->controllerAssignments;
        if(assignmentChanged) {
            discardTimelineStateAfter(assignmentBaselineFrame);
            participant->lastReceivedInputFrame = assignmentBaselineFrame;
            participant->lastContiguousInputFrame = assignmentBaselineFrame;
            participant->lastReceivedInputSequence = 0;
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
            notifySessionEvent(controllerAssignmentToast(kObserverPlayerSlot, m_session.roomState(), participantLabel(*participant)));
        } else {
            for(PlayerSlot slot : participant->controllerAssignments) {
                notifySessionEvent(controllerAssignmentToast(slot, m_session.roomState(), participantLabel(*participant)));
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
                if(!m_pendingHostResyncFrame.has_value() || resyncFrame < *m_pendingHostResyncFrame) {
                    m_pendingHostResyncFrame = resyncFrame;
                }
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

    removeParticipant(data.participantId);
    if(hostLeft) {
        pushLog("Host left the room");
        m_serverPeer = NetTransport::kInvalidPeerHandle;
        m_connected = false;
        m_session.roomState().state = SessionState::Ended;
        m_lastError = "Host closed the room";
        clearReconnectAttemptState();
    } else {
        pushLog("Participant left: " + participantName);
        notifySessionEvent(participantName + " left");
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
    notifySessionEvent(name + " left");
    return true;
}

bool NetplayCoordinator::handleResyncBegin(PacketReader& reader)
{
    ResyncBeginData data;
    if(!reader.readPod(data)) return false;

    realignAuthoritativeState(data.targetFrame, true, data.inputSequenceBase);
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

    const std::string toast = resyncReasonToast(data.reason);
    if(!toast.empty()) {
        notifySessionEvent(toast);
        pushLog(toast);
    }

    return true;
}

bool NetplayCoordinator::handleResyncChunk(PacketReader& reader)
{
    ResyncChunkData data;
    if(!reader.readPod(data)) return false;
    if(!m_incomingResync.has_value() || m_incomingResync->resyncId != data.resyncId) return false;
    if(data.offset + data.size > m_incomingResync->payload.size()) return false;

    std::vector<uint8_t> chunk;
    if(!reader.readBytes(chunk, data.size)) return false;
    m_incomingResync->lastActivityAt = std::chrono::steady_clock::now();

    if(data.size > 0) {
        std::memcpy(m_incomingResync->payload.data() + data.offset, chunk.data(), data.size);
        std::fill_n(m_incomingResync->receivedMask.begin() + data.offset, data.size, uint8_t{1});
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
    m_incomingResync.reset();
    return true;
}

bool NetplayCoordinator::handleResyncAck(PacketReader& reader)
{
    ResyncAckData data;
    if(!reader.readPod(data)) return false;
    if(!m_hosting || data.resyncId != m_session.roomState().activeResyncId) return true;

    if(data.success == 0) {
        scheduleResyncRetry(
            m_session.roomState().resyncTargetFrame,
            "Resync ACK failure from participant " + std::to_string(static_cast<int>(data.participantId)) + "; retrying"
        );
        return true;
    }

    if(data.loadedFrame != m_session.roomState().resyncTargetFrame) {
        scheduleResyncRetry(
            m_session.roomState().resyncTargetFrame,
            "Resync ACK loaded unexpected frame from participant " +
                std::to_string(static_cast<int>(data.participantId)) + "; retrying"
        );
        return true;
    }

    if(m_activeResyncExpectedStateCrc32 != 0 &&
       data.crc32 != m_activeResyncExpectedStateCrc32) {
        scheduleResyncRetry(
            m_session.roomState().resyncTargetFrame,
            "Resync ACK state CRC mismatch from participant " +
                std::to_string(static_cast<int>(data.participantId)) + "; retrying"
        );
        return true;
    }

    m_pendingResyncAcks.erase(
        std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), data.participantId),
        m_pendingResyncAcks.end()
    );

    if(m_pendingResyncAcks.empty()) {
        m_session.roomState().state = SessionState::Running;
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
    } else {
        m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
    }

    return true;
}

bool NetplayCoordinator::handleResyncAbort(PacketReader& reader)
{
    ResyncAbortData data;
    if(!reader.readPod(data)) return false;
    if(!m_hosting || data.resyncId != m_session.roomState().activeResyncId) return true;

    scheduleResyncRetry(
        m_session.roomState().resyncTargetFrame,
        "Resync aborted by participant " + std::to_string(static_cast<int>(data.participantId)) + "; retrying"
    );
    return true;
}

bool NetplayCoordinator::handlePeerHealth(NetTransport::PeerHandle peer, PacketReader& reader)
{
    PeerHealthData data;
    if(!reader.readPod(data)) return false;

    if(ParticipantInfo* participant = m_session.findParticipant(data.participantId)) {
        participant->pingMs = data.pingMs;
        participant->jitterMs = data.jitterMs;
        participant->lastReportedCurrentFrame = data.currentFrame;
        participant->lastReportedConfirmedFrame = data.lastConfirmedFrame;
        ++participant->peerHealthSerial;
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
    } else if(data.state == SessionState::Ended) {
        clearReconnectAttemptState();
        m_disconnectExpectedAfterHostShutdown = true;
        m_lastError = "Host closed the room";
        pushLog("Host closed the room");
        notifySessionEvent("Host closed the room");
    } else {
        pushLog(data.state == SessionState::Running ? "Session started" : "Session state updated");
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
        }

        m_transport.sendUnreliable(
            m_serverPeer,
            Channel::Diagnostics,
            buildPeerHealthPacket(data, m_session.roomState().sessionId)
        );
    }
}

FrameNumber NetplayCoordinator::computeHostConfirmedFrame() const
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

void NetplayCoordinator::broadcastFrameStatusIfNeeded()
{
    if(!m_hosting) return;

    FrameStatusData status;
    status.timelineEpoch = m_session.roomState().timelineEpoch;
    status.currentFrame = m_localSimulationFrame;
    const FrameNumber inputConfirmedFrame = std::max(m_session.roomState().lastConfirmedFrame, computeHostConfirmedFrame());
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
        notifySessionEvent(name + " did not reconnect");
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
        if(entry.predicted && !entry.confirmed) {
            ++count;
        }
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
            notifySessionEvent(controllerAssignmentToast(kObserverPlayerSlot, room, participantLabel(participant)));
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
        if(!m_pendingHostResyncFrame.has_value() || resyncFrame < *m_pendingHostResyncFrame) {
            m_pendingHostResyncFrame = resyncFrame;
        }
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
    const bool reusedReconnectReservation =
        reconnectParticipant != nullptr &&
        reconnectParticipant->reconnectReserved &&
        !reconnectParticipant->connected;

    ParticipantInfo& participant =
        reusedReconnectReservation
            ? *reconnectParticipant
            : ensureParticipant(m_nextAssignedParticipantId++, displayName);
    participant.displayName = displayName;
    participant.connected = true;
    participant.reconnectReserved = false;
    participant.reservationSecondsRemaining = 0;
    m_reconnectReservationDeadlines.erase(participant.id);
    participant.reconnectToken = joinData.reconnectToken != 0 ? joinData.reconnectToken : generateReconnectToken();
    participant.romLoaded = joinData.romLoaded != 0;
    participant.romCompatible = joinRomCompatible;
    if(!reusedReconnectReservation) {
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
    oss << (reusedReconnectReservation ? "Participant reconnected: " : "Participant joined as observer: ")
        << participant.displayName
        << " (id " << static_cast<int>(participant.id) << ")";
    pushLog(oss.str());
    notifySessionEvent(participantLabel(participant) + (reusedReconnectReservation ? " reconnected" : " joined"));
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
        notifySessionEvent(participantLabel(participant) + " joined");
        pushLog("Participant active: " + participant.displayName + " (id " + std::to_string(static_cast<int>(participant.id)) + ")");
    } else if((participant.connected != wasConnected || participant.reconnectReserved != wasReserved) &&
              participantId != m_localParticipantId) {
        std::ostringstream oss;
        oss << (participant.connected ? "Participant active: " : "Participant inactive: ")
            << participant.displayName << " (id " << static_cast<int>(participant.id) << ")";
        pushLog(oss.str());
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
            oss << "Host requires ROM \"" << gameName << "\" (CRC "
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

    ParticipantInfo& hostParticipant = ensureParticipant(m_localParticipantId, m_localDisplayName);
    hostParticipant.connected = true;
    hostParticipant.role = ParticipantRole::Host;
    hostParticipant.controllerAssignments.clear();
    hostParticipant.normalizeControllerAssignments();

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
        m_lastError = std::string("Failed to connect to host using ") + transportBackendLabel(m_transport.backend());
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
    pushLog("Connecting to " + describeConnectTarget(m_transport.backend(), m_transport.options(), hostName, port));
    return true;
}

void NetplayCoordinator::disconnect()
{
    clearReconnectAttemptState();
    if(m_transport.isActive()) {
        if(m_hosting) {
            endSession();
            m_transport.flush();
            m_transport.broadcastReliable(Channel::Control, buildParticipantLeftPacket(m_localParticipantId));
            m_transport.flush();
            m_transport.disconnectAll();
            completeLocalDisconnect();
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

    completeLocalDisconnect();
}

void NetplayCoordinator::update(uint32_t timeoutMs)
{
    if(!m_transport.isActive()) {
        processPendingReconnect();
        return;
    }

    const auto handleEvent = [&](const NetTransport::Event& event) {
        switch(event.type) {
            case NetTransport::Event::Type::Connected:
                if(!m_hosting) {
                    m_serverPeer = event.peer;
                    pushLog("Connected to host");
                    if(!m_transport.sendReliable(m_serverPeer, Channel::Control, buildJoinRoomPacket())) {
                        m_lastError = "Failed to send JoinRoom";
                        pushLog(m_lastError);
                    }
                }
                else {
                    pushLog("Peer connected");
                }
                break;

            case NetTransport::Event::Type::Disconnected:
                if(event.peer == m_serverPeer) {
                    pushLog("Disconnected from host");
                    m_serverPeer = NetTransport::kInvalidPeerHandle;
                    m_connected = false;
                    m_session.roomState().state = SessionState::Ended;
                    m_reconnectAttemptInFlight = false;
                    if(m_gracefulDisconnectPending) {
                        completeLocalDisconnect();
                    } else if(m_disconnectExpectedAfterJoinReject) {
                        const std::string preservedError = m_lastError;
                        m_disconnectExpectedAfterJoinReject = false;
                        completeLocalDisconnect();
                        m_lastError = preservedError;
                    } else if(m_disconnectExpectedAfterHostShutdown) {
                        const std::string preservedError = m_lastError.empty() ? std::string("Host closed the room") : m_lastError;
                        m_disconnectExpectedAfterHostShutdown = false;
                        completeLocalDisconnect();
                        m_lastError = preservedError;
                    } else if(m_localParticipantId != kInvalidParticipantId &&
                              m_localReconnectToken != 0 &&
                              hasReconnectTarget(m_transport.backend(), m_transport.options(), m_lastJoinHostName, m_lastJoinPort)) {
                        scheduleReconnectAttempt();
                    } else if(m_session.roomState().currentFrame > 0) {
                        m_lastError = "Host disconnected during session";
                    }
                } else if(m_hosting) {
                    const ParticipantId participantId = participantIdFromPeer(event.peer);
                    if(participantId != kInvalidParticipantId && m_session.findParticipant(participantId) != nullptr) {
                        ParticipantInfo* participant = m_session.findParticipant(participantId);
                        const bool hadActiveAssignment =
                            participant != nullptr &&
                            !participantIsObserver(*participant) &&
                            (m_session.roomState().state == SessionState::Running ||
                             m_session.roomState().state == SessionState::Resyncing ||
                             m_session.roomState().state == SessionState::Paused);
                        const bool reserveReconnect =
                            participant != nullptr &&
                            participant->reconnectToken != 0 &&
                            (participant->romCompatible || !participantIsObserver(*participant));
                        pushLog("Peer disconnected: participant " + std::to_string(static_cast<int>(participantId)));
                        if(m_pendingHostLateJoinResyncParticipant.has_value() &&
                           *m_pendingHostLateJoinResyncParticipant == participantId) {
                            m_pendingHostLateJoinResyncParticipant.reset();
                        }
                        if(participant != nullptr && reserveReconnect) {
                            participant->connected = false;
                            participant->reconnectReserved = true;
                            participant->reservationSecondsRemaining =
                                static_cast<uint16_t>(std::clamp<int64_t>(m_reconnectReservationDuration.count(), 1, 65535));
                            m_reconnectReservationDeadlines[participantId] =
                                std::chrono::steady_clock::now() + m_reconnectReservationDuration;
                            m_pendingResyncAcks.erase(
                                std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), participantId),
                                m_pendingResyncAcks.end()
                            );
                            m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
                            m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(*participant, 0), event.peer);
                            notifySessionEvent(participantLabel(*participant) + " disconnected; reconnect reserved");
                        } else {
                            removeParticipant(participantId);
                            m_transport.broadcastReliable(Channel::Control, buildParticipantLeftPacket(participantId), event.peer);
                        }
                        if(hadActiveAssignment &&
                           (m_session.roomState().state == SessionState::Running ||
                            m_session.roomState().state == SessionState::Resyncing)) {
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
                            m_transport.broadcastReliable(Channel::Control, writer.data(), event.peer);
                            pushLog("Session paused because an assigned participant disconnected");
                        }
                        refreshHostRoomState();
                    } else {
                        pushLog("Peer disconnected");
                    }
                } else {
                    pushLog("Peer disconnected");
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

    for(NetTransport::Event event : m_transport.poll(timeoutMs)) {
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

    updatePeerHealthFromTransport();
    updateReconnectReservations();
    broadcastFrameStatusIfNeeded();
    broadcastPeerHealthIfNeeded();
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

const RollbackStats& NetplayCoordinator::predictionStats() const
{
    return m_predictionStats;
}

void NetplayCoordinator::recordPlaybackStop(FrameNumber frame, bool predictionLimitReached)
{
    m_predictionStats.recordPlaybackStop(frame, predictionLimitReached);
}

void NetplayCoordinator::setLocalSimulationFrame(FrameNumber frame)
{
    m_localSimulationFrame = frame;
    if(m_hosting) {
        m_session.roomState().currentFrame = frame;
    }
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

std::optional<FrameNumber> NetplayCoordinator::consumePendingHostResyncFrame()
{
    std::optional<FrameNumber> result = m_pendingHostResyncFrame;
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

void NetplayCoordinator::discardTimelineAfter(FrameNumber frame)
{
    m_localInputs.eraseFramesAfter(frame);

    while(!m_confirmedFrames.empty() && m_confirmedFrames.back().frame > frame) {
        m_confirmedFrames.pop_back();
    }

    m_lastBroadcastConfirmedFrame = std::min(m_lastBroadcastConfirmedFrame, frame);
    m_session.roomState().lastConfirmedFrame = std::min(m_session.roomState().lastConfirmedFrame, frame);

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

FrameNumber NetplayCoordinator::latestConfirmedFrame() const
{
    return m_confirmedFrames.empty() ? 0u : m_confirmedFrames.back().frame;
}

uint8_t NetplayCoordinator::predictFrames() const
{
    return m_session.roomState().predictFrames;
}

void NetplayCoordinator::storeConfirmedFrame(const ConfirmedFrameInputs& frame)
{
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
    outFrame.inputFrame = makeRoomTopologyBaseFrame(frame, m_session.roomState());
    bool haveAssignedParticipant = false;

    for(const auto& participant : m_session.roomState().participants) {
        for(PlayerSlot slot : participantAssignments(participant)) {
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
    outFrame.inputFrame = makeRoomTopologyBaseFrame(frame, m_session.roomState());
    outFrame.predicted = false;
    bool haveAssignedParticipant = false;

    for(const auto& participant : m_session.roomState().participants) {
        for(PlayerSlot slot : participantAssignments(participant)) {
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

    std::vector<ConfirmedFrameInputs> pendingFrames;
    FrameNumber nextFrame = m_lastBroadcastConfirmedFrame + 1u;
    const FrameNumber maxPublishableFrame = computeHostConfirmedFrame();
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
    packetData.participantId = m_localParticipantId;
    packetData.playerSlot = slot;
    packetData.buttonMaskLo = buttonMaskLo;
    packetData.buttonMaskHi = buttonMaskHi;
    packetData.sequence = entry.sequence;
    packetData.payloadSize = static_cast<uint16_t>(serializeInputFrame(contribution).size());
    const std::vector<uint8_t> payload = buildInputFramePacket(packetData, contribution);

    if(m_hosting) {
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

    m_lastLocalCrcFrame = frame;
    m_lastLocalCrc32 = crc32;
    m_recentLocalCrcHistory.emplace_back(frame, crc32);
    while(m_recentLocalCrcHistory.size() > kRecentLocalCrcHistoryCapacity) {
        m_recentLocalCrcHistory.pop_front();
    }

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
    while(!m_recentLocalCrcHistory.empty() && m_recentLocalCrcHistory.back().first > frame) {
        m_recentLocalCrcHistory.pop_back();
    }

    if(m_lastLocalCrcFrame > frame) {
        if(!m_recentLocalCrcHistory.empty()) {
            m_lastLocalCrcFrame = m_recentLocalCrcHistory.back().first;
            m_lastLocalCrc32 = m_recentLocalCrcHistory.back().second;
        } else {
            m_lastLocalCrcFrame = frame;
            m_lastLocalCrc32 = 0;
        }
    }
}

bool NetplayCoordinator::beginResync(FrameNumber targetFrame,
                                     const std::vector<uint8_t>& payload,
                                     uint32_t payloadCrc32,
                                     uint32_t stateCrc32,
                                     ResyncReason reason)
{
    if(!m_hosting || payload.empty()) return false;

    const bool initialSessionSync = m_session.roomState().state == SessionState::Starting;
    const uint32_t resyncId = m_nextResyncId++;
    ++m_session.roomState().timelineEpoch;
    realignAuthoritativeState(targetFrame, true, 0u);
    if(initialSessionSync) {
        m_localInputSequence = 0;
        for(ParticipantInfo& participant : m_session.roomState().participants) {
            participant.lastReceivedInputSequence = 0;
        }
    }
    m_session.roomState().state = SessionState::Resyncing;
    m_session.roomState().activeResyncId = resyncId;
    m_session.roomState().resyncTargetFrame = targetFrame;
    m_session.roomState().resyncConfirmedFrame = targetFrame;
    m_session.roomState().resyncFrameReadyFrame = targetFrame;
    m_session.roomState().resyncPayloadSize = static_cast<uint32_t>(payload.size());
    m_session.roomState().resyncPayloadCrc32 = payloadCrc32;
    m_session.roomState().resyncFrameReadyCrc32 = stateCrc32;
    m_session.roomState().resyncInputSequenceBase = 0;
    m_session.roomState().activeResyncReason = reason;
    m_activeResyncExpectedStateCrc32 = stateCrc32;
    m_pendingImplicitRecovery.reset();
    m_pendingResyncAcks.clear();

    for(const ParticipantInfo& participant : m_session.roomState().participants) {
        if(participant.id != m_localParticipantId &&
           participant.connected &&
           !participant.reconnectReserved) {
            m_pendingResyncAcks.push_back(participant.id);
        }
    }
    m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
    if(!initialSessionSync) {
        m_predictionStats.recordHardResync();
    }

    if(m_pendingResyncAcks.empty()) {
        m_session.roomState().state = SessionState::Running;
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
    m_transport.broadcastReliable(Channel::Control, buildResyncBeginPacket(beginData));

    for(size_t offset = 0; offset < payload.size(); offset += kResyncChunkPayloadBytes) {
        const size_t chunkSize = std::min(kResyncChunkPayloadBytes, payload.size() - offset);
        ResyncChunkData chunkData;
        chunkData.resyncId = resyncId;
        chunkData.offset = static_cast<uint32_t>(offset);
        chunkData.size = static_cast<uint16_t>(chunkSize);
        m_transport.broadcastReliable(
            Channel::Control,
            buildResyncChunkPacket(chunkData, std::span<const uint8_t>(payload.data() + offset, chunkSize))
        );
    }

    ResyncCompleteData completeData;
    completeData.resyncId = resyncId;
    m_transport.broadcastReliable(Channel::Control, buildResyncCompletePacket(completeData));
    pushLog("Host forced resync");
    return true;
}

std::optional<NetplayCoordinator::PendingResyncApply> NetplayCoordinator::consumePendingResyncApply()
{
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
        realignAuthoritativeState(
            loadedFrame,
            true,
            m_session.roomState().resyncInputSequenceBase
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
    }

    return m_transport.sendReliable(m_serverPeer, Channel::Control, buildResyncAckPacket(ack));
}

bool NetplayCoordinator::assignController(ParticipantId participantId, PlayerSlot slot)
{
    if(slot == kObserverPlayerSlot) {
        return clearControllerAssignments(participantId);
    }
    if(!clearControllerAssignments(participantId)) return false;
    return addControllerAssignment(participantId, slot);
}

bool NetplayCoordinator::addControllerAssignment(ParticipantId participantId, PlayerSlot slot)
{
    if(!m_hosting) return false;
    if(!isAssignmentAvailable(slot, m_session.roomState())) return false;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr) return false;
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
        if(!m_pendingHostResyncFrame.has_value() || assignmentBaselineFrame < *m_pendingHostResyncFrame) {
            m_pendingHostResyncFrame = assignmentBaselineFrame;
        }
        pushLog("Controller assignment changed; scheduling automatic resync");
    }

    for(const auto& [assignedSlot, participantName] : assignmentToasts) {
        notifySessionEvent(controllerAssignmentToast(assignedSlot, m_session.roomState(), participantName));
    }

    return true;
}

bool NetplayCoordinator::removeControllerAssignment(ParticipantId participantId, PlayerSlot slot)
{
    if(!m_hosting) return false;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr) return false;
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
        if(!m_pendingHostResyncFrame.has_value() || assignmentBaselineFrame < *m_pendingHostResyncFrame) {
            m_pendingHostResyncFrame = assignmentBaselineFrame;
        }
        pushLog("Controller assignment changed; scheduling automatic resync");
    }

    notifySessionEvent(controllerAssignmentToast(
        participantIsObserver(*participant) ? kObserverPlayerSlot : participant->controllerAssignment,
        m_session.roomState(),
        participantLabel(*participant)
    ));
    return true;
}

bool NetplayCoordinator::clearControllerAssignments(ParticipantId participantId)
{
    if(!m_hosting) return false;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr) return false;
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

    for(NetTransport::PeerHandle peer : m_transport.connectedPeers()) {
        if(participantIdFromPeer(peer) == participantId) {
            m_transport.disconnectPeer(peer);
            break;
        }
    }

    removeParticipant(participantId);
    m_transport.broadcastReliable(Channel::Control, buildParticipantLeftPacket(participantId));
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

    pushLog("Host selected ROM: " + gameName);
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
        ? ("Host started session and is awaiting initial sync at frame " + std::to_string(m_localSimulationFrame))
        : "Host started session");
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
    pushLog("Host paused session");
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
    pushLog("Host resumed session");
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
    pushLog("Host ended session");
    return true;
}

} // namespace Netplay
