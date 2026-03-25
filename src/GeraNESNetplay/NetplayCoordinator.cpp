#ifndef __EMSCRIPTEN__

#include "GeraNESNetplay/NetplayCoordinator.h"
#include "GeraNES/util/Crc32.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>

namespace {

constexpr size_t kResyncChunkPayloadBytes = 1024;
constexpr uint16_t kReconnectReservationSeconds = 30;
constexpr size_t kRecentLocalCrcHistoryCapacity = 512;

}

namespace Netplay {

NetplayCoordinator::NetplayCoordinator()
    : m_localDisplayName(defaultDisplayName())
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

std::string NetplayCoordinator::messageTypeLabel(MessageType type)
{
    switch(type) {
        case MessageType::CreateRoom: return "CreateRoom";
        case MessageType::JoinRoom: return "JoinRoom";
        case MessageType::ParticipantJoined: return "ParticipantJoined";
        case MessageType::ParticipantLeft: return "ParticipantLeft";
        case MessageType::ChatMessage: return "ChatMessage";
        case MessageType::SetRole: return "SetRole";
        case MessageType::AssignController: return "AssignController";
        case MessageType::SetReady: return "SetReady";
        case MessageType::RequestController: return "RequestController";
        case MessageType::SelectRom: return "SelectRom";
        case MessageType::RomValidationResult: return "RomValidationResult";
        case MessageType::StartSession: return "StartSession";
        case MessageType::PauseSession: return "PauseSession";
        case MessageType::ResumeSession: return "ResumeSession";
        case MessageType::EndSession: return "EndSession";
        case MessageType::InputFrame: return "InputFrame";
        case MessageType::InputAck: return "InputAck";
        case MessageType::FrameStatus: return "FrameStatus";
        case MessageType::PeerHealth: return "PeerHealth";
        case MessageType::CrcReport: return "CrcReport";
        case MessageType::ResyncBegin: return "ResyncBegin";
        case MessageType::ResyncChunk: return "ResyncChunk";
        case MessageType::ResyncComplete: return "ResyncComplete";
        case MessageType::ResyncAck: return "ResyncAck";
        case MessageType::ResyncAbort: return "ResyncAbort";
        case MessageType::SpectatorSyncBegin: return "SpectatorSyncBegin";
        case MessageType::SpectatorSyncChunk: return "SpectatorSyncChunk";
        case MessageType::SpectatorSyncComplete: return "SpectatorSyncComplete";
        case MessageType::SpectatorSyncAck: return "SpectatorSyncAck";
        default: return "Unknown";
    }
}

void NetplayCoordinator::resetSessionState()
{
    m_serverPeer = nullptr;
    m_localParticipantId = kInvalidParticipantId;
    m_localReconnectToken = 0;
    m_session.reset();
    m_localInputs.clear();
    m_remoteInputs.clear();
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
    m_nextResyncId = 1;
    m_incomingResync.reset();
    m_incomingSpectatorSync.reset();
    m_pendingResyncApply.reset();
    m_pendingSpectatorSyncApply.reset();
    m_pendingHostSpectatorSyncParticipant.reset();
    m_pendingResyncAcks.clear();
    m_reconnectReservationDeadlines.clear();
    m_requiresSpectatorSync = false;
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

ParticipantId NetplayCoordinator::participantIdFromPeer(ENetPeer* peer) const
{
    if(peer == nullptr || peer->data == nullptr) return kInvalidParticipantId;
    const uintptr_t stored = reinterpret_cast<uintptr_t>(peer->data);
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
    writer.writePod(joinData);
    writer.writeString(m_localDisplayName);

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
    writer.writePod(participant.controllerAssignment);
    writer.writePod(static_cast<uint8_t>(participant.controllerRequestPending ? 1 : 0));
    writer.writePod(participant.requestedControllerSlot);
    writer.writeString(participant.displayName);

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

std::vector<uint8_t> NetplayCoordinator::buildSetRolePacket(const SetRoleData& data) const
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::SetRole;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
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

std::vector<uint8_t> NetplayCoordinator::buildSpectatorSyncBeginPacket(const SpectatorSyncBeginData& data) const
{
    PacketWriter writer;
    PacketHeader header;
    header.type = MessageType::SpectatorSyncBegin;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(data);
    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildSpectatorSyncChunkPacket(const SpectatorSyncChunkData& data, std::span<const uint8_t> payloadChunk) const
{
    PacketWriter writer;
    PacketHeader header;
    header.type = MessageType::SpectatorSyncChunk;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(data);
    writer.writeBytes(payloadChunk);
    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildSpectatorSyncCompletePacket(const SpectatorSyncCompleteData& data) const
{
    PacketWriter writer;
    PacketHeader header;
    header.type = MessageType::SpectatorSyncComplete;
    header.sessionId = m_session.roomState().sessionId;
    writer.writePod(header);
    writer.writePod(data);
    return writer.data();
}

std::vector<uint8_t> NetplayCoordinator::buildSpectatorSyncAckPacket(const SpectatorSyncAckData& data) const
{
    PacketWriter writer;
    PacketHeader header;
    header.type = MessageType::SpectatorSyncAck;
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

static std::vector<uint8_t> buildInputFramePacket(const InputFrameData& input)
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::InputFrame;
    header.sessionId = 0;
    writer.writePod(header);
    writer.writePod(input);

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

static std::vector<uint8_t> buildSetReadyPacket(const SetReadyData& data, uint32_t sessionId)
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::SetReady;
    header.sessionId = sessionId;
    writer.writePod(header);
    writer.writePod(data);

    return writer.data();
}

static std::vector<uint8_t> buildRequestControllerPacket(const RequestControllerData& data, uint32_t sessionId)
{
    PacketWriter writer;

    PacketHeader header;
    header.type = MessageType::RequestController;
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
    writer.writePod(data);

    return writer.data();
}

bool NetplayCoordinator::handleInputFrame(ENetPeer* peer, PacketReader& reader)
{
    InputFrameData input;
    if(!reader.readPod(input)) return false;

    ParticipantInfo* participant = m_session.findParticipant(input.participantId);
    if(participant != nullptr) {
        if(participant->controllerAssignment != input.playerSlot) {
            std::ostringstream oss;
            oss << "Ignored input for unexpected slot from " << participant->displayName
                << ": got P" << static_cast<unsigned>(input.playerSlot) + 1u;
            if(participant->controllerAssignment == kObserverPlayerSlot) {
                oss << ", expected observer";
            } else {
                oss << ", expected P" << static_cast<unsigned>(participant->controllerAssignment) + 1u;
            }
            pushLog(oss.str());
            return false;
        }

        if(input.sequence <= participant->lastReceivedInputSequence &&
           participant->lastReceivedInputFrame >= input.frame) {
            pushLog("Ignored stale/duplicate input from " + participant->displayName);
            return true;
        }

        participant->lastReceivedInputFrame = std::max(participant->lastReceivedInputFrame, input.frame);
        participant->lastReceivedInputSequence = std::max(participant->lastReceivedInputSequence, input.sequence);
    }

    const TimelineInputEntry* existing = m_remoteInputs.find(input.frame, input.participantId, input.playerSlot);
    if(existing != nullptr && existing->predicted) {
        const bool predictionHit =
            existing->buttonMaskLo == input.buttonMaskLo &&
            existing->buttonMaskHi == input.buttonMaskHi;
        m_predictionStats.recordPrediction(predictionHit);

        if(!predictionHit) {
            handleConfirmedInputMismatch(input.participantId, input.frame, input.playerSlot);
        }
    }

    TimelineInputEntry entry;
    entry.frame = input.frame;
    entry.participantId = input.participantId;
    entry.playerSlot = input.playerSlot;
    entry.buttonMaskLo = input.buttonMaskLo;
    entry.buttonMaskHi = input.buttonMaskHi;
    entry.sequence = input.sequence;
    entry.confirmed = true;
    entry.predicted = false;
    m_remoteInputs.push(entry);

    if(participant != nullptr) {
        if(input.frame > participant->lastContiguousInputFrame + 1u) {
            const FrameNumber missingFrame = participant->lastContiguousInputFrame + 1u;
            if(!participant->pendingMissingInputFrom.has_value() ||
               *participant->pendingMissingInputFrom != missingFrame) {
                participant->pendingMissingInputFrom = missingFrame;
                recordMissingInputGap(*participant, missingFrame, input.playerSlot);
            }
        }
        advanceParticipantContiguousInputFrame(*participant, input.playerSlot);
        if(participant->pendingMissingInputFrom.has_value() &&
           participant->lastContiguousInputFrame >= *participant->pendingMissingInputFrom) {
            participant->pendingMissingInputFrom.reset();
        }
    }

    if(m_hosting) {
        m_transport.broadcastReliable(Channel::Gameplay, buildInputFramePacket(input), peer);
    }

    return true;
}

void NetplayCoordinator::recordMissingInputGap(ParticipantInfo& participant, FrameNumber missingFrame, PlayerSlot slot)
{
    ++participant.missingInputGapCount;
    participant.lastDecision = "Missing input gap";
    participant.lastDecisionFrame = missingFrame;
    participant.lastDecisionSlot = slot;
    m_predictionStats.recordMissingInputGap(missingFrame, slot);

    std::ostringstream oss;
    oss << "Missing input gap from " << participant.displayName
        << " starting at frame " << missingFrame
        << " slot " << static_cast<unsigned>(slot) + 1u;
    pushLog(oss.str());
}

void NetplayCoordinator::advanceParticipantContiguousInputFrame(ParticipantInfo& participant, PlayerSlot slot)
{
    while(true) {
        const FrameNumber nextFrame = participant.lastContiguousInputFrame + 1u;
        const TimelineInputEntry* entry = m_remoteInputs.find(nextFrame, participant.id, slot);
        if(entry == nullptr || !entry->confirmed) {
            break;
        }
        participant.lastContiguousInputFrame = nextFrame;
    }
}

void NetplayCoordinator::handleConfirmedInputMismatch(ParticipantId participantId, FrameNumber inputFrame, PlayerSlot slot)
{
    ParticipantInfo* participant = m_session.findParticipant(participantId);
    std::ostringstream oss;
    oss << "Prediction mismatch";
    if(participant != nullptr && !participant->displayName.empty()) {
        oss << " from " << participant->displayName;
    } else {
        oss << " from participant " << static_cast<unsigned>(participantId);
    }
    oss << " on frame " << inputFrame
        << " slot " << static_cast<unsigned>(slot) + 1u;

    auto recordParticipantDecision = [&](const std::string& label) {
        if(participant == nullptr) return;
        participant->lastDecision = label;
        participant->lastDecisionFrame = inputFrame;
        participant->lastDecisionSlot = slot;
    };

    const FrameNumber confirmedFrame = m_session.roomState().lastConfirmedFrame;
    const FrameNumber currentFrame = m_session.roomState().currentFrame;

    if(inputFrame <= confirmedFrame) {
        m_predictionStats.recordConfirmedFrameConflict(inputFrame, slot);
        if(participant != nullptr) {
            ++participant->confirmedFrameConflictCount;
        }
        recordParticipantDecision("Confirmed-frame conflict");
        pushLog(oss.str() + " after confirmed window");

        if(m_hosting &&
           m_session.roomState().state != SessionState::Resyncing &&
           (!m_pendingHostResyncFrame.has_value() || inputFrame < *m_pendingHostResyncFrame)) {
            m_pendingHostResyncFrame = inputFrame;
        }
        return;
    }

    if(inputFrame > currentFrame) {
        m_predictionStats.recordFutureFrameMismatch(inputFrame, slot);
        if(participant != nullptr) {
            ++participant->futureFrameMismatchCount;
        }
        recordParticipantDecision("Future-frame mismatch");
        pushLog(oss.str() + " before local simulation reached that frame");
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
    recordParticipantDecision("Rollback scheduled");
    pushLog(oss.str());
}

bool NetplayCoordinator::handleFrameStatus(PacketReader& reader)
{
    FrameStatusData status;
    if(!reader.readPod(status)) return false;

    m_session.roomState().currentFrame = status.currentFrame;
    m_session.roomState().lastConfirmedFrame = status.lastConfirmedFrame;
    m_session.roomState().inputDelayFrames = status.inputDelayFrames;
    return true;
}

bool NetplayCoordinator::handleCrcReport(PacketReader& reader)
{
    CrcReportData report;
    if(!reader.readPod(report)) return false;

    if(m_session.roomState().state != SessionState::Running) {
        return true;
    }

    m_session.roomState().lastRemoteCrcFrame = report.frame;
    m_session.roomState().lastRemoteCrc32 = report.crc32;

    const std::optional<uint32_t> matchingLocalCrc = findRecentLocalCrc(report.frame);
    if(matchingLocalCrc.has_value() && *matchingLocalCrc != 0 && *matchingLocalCrc != report.crc32) {
        pushLog("CRC mismatch detected on frame " + std::to_string(report.frame));

        if(m_hosting &&
           m_session.roomState().state != SessionState::Resyncing &&
           (!m_pendingHostResyncFrame.has_value() || report.frame < *m_pendingHostResyncFrame)) {
            m_pendingHostResyncFrame = report.frame;
        }
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

void NetplayCoordinator::realignAuthoritativeState(FrameNumber loadedFrame)
{
    m_localInputs.clear();
    m_remoteInputs.clear();
    m_pendingRollbackFrame.reset();
    m_predictionStats.lastDecision.clear();
    m_predictionStats.lastDecisionFrame = loadedFrame;
    m_predictionStats.lastDecisionSlot = kObserverPlayerSlot;
    m_session.roomState().currentFrame = loadedFrame;
    m_session.roomState().lastConfirmedFrame = loadedFrame;

    for(ParticipantInfo& participant : m_session.roomState().participants) {
        participant.lastReceivedInputFrame = loadedFrame;
        participant.lastContiguousInputFrame = loadedFrame;
        participant.lastReceivedInputSequence = 0;
        participant.pendingMissingInputFrom.reset();
        participant.lastDecisionFrame = loadedFrame;
        participant.lastDecisionSlot = kObserverPlayerSlot;
        participant.lastDecision.clear();
    }

    m_recentLocalCrcHistory.clear();
    m_lastLocalCrcFrame = loadedFrame;
    m_lastLocalCrc32 = 0;
}

bool NetplayCoordinator::handleAssignController(PacketReader& reader)
{
    AssignControllerData data;
    if(!reader.readPod(data)) return false;

    if(ParticipantInfo* participant = m_session.findParticipant(data.participantId)) {
        participant->controllerAssignment = data.controllerAssignment;
        participant->role = data.controllerAssignment == kObserverPlayerSlot ? ParticipantRole::Observer : ParticipantRole::Player;
        participant->controllerRequestPending = false;
        participant->requestedControllerSlot = kObserverPlayerSlot;
        if(data.controllerAssignment == kObserverPlayerSlot) {
            participant->ready = false;
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

    m_session.roomState().selectedGameName = gameName;
    m_session.roomState().romValidation = romValidation;
    m_session.roomState().state = SessionState::ValidatingRom;
    for(ParticipantInfo& participant : m_session.roomState().participants) {
        participant.ready = false;
        if(participant.id != m_localParticipantId) {
            participant.romLoaded = false;
            participant.romCompatible = false;
        }
    }

    pushLog("Selected ROM: " + gameName);
    return true;
}

bool NetplayCoordinator::handleRomValidationResult(ENetPeer* peer, PacketReader& reader)
{
    RomValidationResultData result;
    if(!reader.readPod(result)) return false;

    if(ParticipantInfo* participant = m_session.findParticipant(result.participantId)) {
        participant->romLoaded = result.romLoaded != 0;
        participant->romCompatible = result.romCompatible != 0;

        if(m_hosting) {
            m_transport.broadcastReliable(Channel::Control, buildRomValidationResultPacket(result), peer);
            refreshHostRoomState();
        }
        return true;
    }

    return false;
}

bool NetplayCoordinator::handleParticipantLeft(PacketReader& reader)
{
    ParticipantLeftData data;
    if(!reader.readPod(data)) return false;

    removeParticipant(data.participantId);
    pushLog("Participant left: " + std::to_string(static_cast<int>(data.participantId)));
    return true;
}

bool NetplayCoordinator::handleSetRole(PacketReader& reader)
{
    SetRoleData data;
    if(!reader.readPod(data)) return false;

    if(ParticipantInfo* participant = m_session.findParticipant(data.participantId)) {
        participant->role = data.role;
        participant->controllerRequestPending = false;
        participant->requestedControllerSlot = kObserverPlayerSlot;
        if(data.role == ParticipantRole::Observer) {
            participant->controllerAssignment = kObserverPlayerSlot;
            participant->ready = false;
        }
        return true;
    }

    return false;
}

bool NetplayCoordinator::handleResyncBegin(PacketReader& reader)
{
    ResyncBeginData data;
    if(!reader.readPod(data)) return false;

    m_incomingResync = IncomingResyncTransfer{};
    m_incomingResync->resyncId = data.resyncId;
    m_incomingResync->targetFrame = data.targetFrame;
    m_incomingResync->expectedPayloadCrc32 = data.payloadCrc32;
    m_incomingResync->payload.resize(data.payloadSize);
    m_incomingResync->receivedMask.assign(data.payloadSize, 0);

    m_session.roomState().activeResyncId = data.resyncId;
    m_session.roomState().resyncTargetFrame = data.targetFrame;
    m_session.roomState().resyncPayloadSize = data.payloadSize;
    m_session.roomState().resyncPayloadCrc32 = data.payloadCrc32;
    m_session.roomState().state = SessionState::Resyncing;

    pushLog("Resync begin: frame " + std::to_string(data.targetFrame));
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
        m_incomingResync.reset();
        return false;
    }

    const uint32_t payloadCrc32 = Crc32::calc(reinterpret_cast<const char*>(m_incomingResync->payload.data()), m_incomingResync->payload.size());
    if(payloadCrc32 != m_incomingResync->expectedPayloadCrc32) {
        pushLog("Resync payload CRC mismatch");
        m_incomingResync.reset();
        return false;
    }

    PendingResyncApply pending;
    pending.resyncId = m_incomingResync->resyncId;
    pending.targetFrame = m_incomingResync->targetFrame;
    pending.expectedPayloadCrc32 = m_incomingResync->expectedPayloadCrc32;
    pending.payload = std::move(m_incomingResync->payload);
    m_pendingResyncApply = std::move(pending);
    m_incomingResync.reset();
    pushLog("Resync payload received");
    return true;
}

bool NetplayCoordinator::handleResyncAck(PacketReader& reader)
{
    ResyncAckData data;
    if(!reader.readPod(data)) return false;
    if(!m_hosting || data.resyncId != m_session.roomState().activeResyncId) return true;

    m_pendingResyncAcks.erase(
        std::remove(m_pendingResyncAcks.begin(), m_pendingResyncAcks.end(), data.participantId),
        m_pendingResyncAcks.end()
    );

    pushLog(std::string("Resync ACK from participant ") + std::to_string(static_cast<int>(data.participantId)) +
            (data.success ? "" : " (failed)"));

    if(m_pendingResyncAcks.empty()) {
        m_session.roomState().state = SessionState::Running;
        m_session.roomState().pendingResyncAckCount = 0;
        m_session.roomState().activeResyncId = 0;
        m_session.roomState().resyncTargetFrame = 0;
        m_session.roomState().resyncPayloadSize = 0;
        m_session.roomState().resyncPayloadCrc32 = 0;
        pushLog("Resync complete");
    } else {
        m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
    }

    return true;
}

bool NetplayCoordinator::handleSpectatorSyncBegin(PacketReader& reader)
{
    SpectatorSyncBeginData data;
    if(!reader.readPod(data)) return false;

    m_incomingSpectatorSync = IncomingResyncTransfer{};
    m_incomingSpectatorSync->resyncId = data.resyncId;
    m_incomingSpectatorSync->targetFrame = data.targetFrame;
    m_incomingSpectatorSync->expectedPayloadCrc32 = data.payloadCrc32;
    m_incomingSpectatorSync->payload.resize(data.payloadSize);
    m_incomingSpectatorSync->receivedMask.assign(data.payloadSize, 0);
    m_requiresSpectatorSync = true;

    pushLog("Spectator sync begin: frame " + std::to_string(data.targetFrame));
    return true;
}

bool NetplayCoordinator::handleSpectatorSyncChunk(PacketReader& reader)
{
    SpectatorSyncChunkData data;
    if(!reader.readPod(data)) return false;
    if(!m_incomingSpectatorSync.has_value() || m_incomingSpectatorSync->resyncId != data.resyncId) return false;
    if(data.offset + data.size > m_incomingSpectatorSync->payload.size()) return false;

    std::vector<uint8_t> chunk;
    if(!reader.readBytes(chunk, data.size)) return false;

    if(data.size > 0) {
        std::memcpy(m_incomingSpectatorSync->payload.data() + data.offset, chunk.data(), data.size);
        std::fill_n(m_incomingSpectatorSync->receivedMask.begin() + data.offset, data.size, uint8_t{1});
    }
    return true;
}

bool NetplayCoordinator::handleSpectatorSyncComplete(PacketReader& reader)
{
    SpectatorSyncCompleteData data;
    if(!reader.readPod(data)) return false;
    if(!m_incomingSpectatorSync.has_value() || m_incomingSpectatorSync->resyncId != data.resyncId) return false;

    if(std::find(m_incomingSpectatorSync->receivedMask.begin(), m_incomingSpectatorSync->receivedMask.end(), uint8_t{0}) != m_incomingSpectatorSync->receivedMask.end()) {
        pushLog("Spectator sync payload incomplete");
        m_incomingSpectatorSync.reset();
        return false;
    }

    const uint32_t payloadCrc32 = Crc32::calc(reinterpret_cast<const char*>(m_incomingSpectatorSync->payload.data()), m_incomingSpectatorSync->payload.size());
    if(payloadCrc32 != m_incomingSpectatorSync->expectedPayloadCrc32) {
        pushLog("Spectator sync payload CRC mismatch");
        m_incomingSpectatorSync.reset();
        return false;
    }

    PendingResyncApply pending;
    pending.resyncId = m_incomingSpectatorSync->resyncId;
    pending.targetFrame = m_incomingSpectatorSync->targetFrame;
    pending.expectedPayloadCrc32 = m_incomingSpectatorSync->expectedPayloadCrc32;
    pending.payload = std::move(m_incomingSpectatorSync->payload);
    m_pendingSpectatorSyncApply = std::move(pending);
    m_incomingSpectatorSync.reset();
    pushLog("Spectator sync payload received");
    return true;
}

bool NetplayCoordinator::handleSpectatorSyncAck(PacketReader& reader)
{
    SpectatorSyncAckData data;
    if(!reader.readPod(data)) return false;

    pushLog(std::string("Spectator sync ACK from participant ") + std::to_string(static_cast<int>(data.participantId)) +
            (data.success ? "" : " (failed)"));
    return true;
}

bool NetplayCoordinator::handlePeerHealth(ENetPeer* peer, PacketReader& reader)
{
    PeerHealthData data;
    if(!reader.readPod(data)) return false;

    if(ParticipantInfo* participant = m_session.findParticipant(data.participantId)) {
        participant->pingMs = data.pingMs;
        participant->jitterMs = data.jitterMs;
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

bool NetplayCoordinator::handleSetReady(ENetPeer* peer, PacketReader& reader)
{
    SetReadyData data;
    if(!reader.readPod(data)) return false;

    if(ParticipantInfo* participant = m_session.findParticipant(data.participantId)) {
        participant->ready = data.ready != 0;

        if(m_hosting) {
            m_transport.broadcastReliable(Channel::Control, buildSetReadyPacket(data, m_session.roomState().sessionId), peer);
        }
        return true;
    }

    return false;
}

bool NetplayCoordinator::handleRequestController(ENetPeer* peer, PacketReader& reader)
{
    RequestControllerData data;
    if(!reader.readPod(data)) return false;
    if(!m_hosting) return false;

    const ParticipantId senderId = participantIdFromPeer(peer);
    if(senderId == kInvalidParticipantId || senderId != data.participantId) return false;

    ParticipantInfo* participant = m_session.findParticipant(data.participantId);
    if(participant == nullptr) return false;

    if(data.clearRequest != 0) {
        if(participant->controllerRequestPending) {
            pushLog(participant->displayName + " canceled controller request");
        }
        participant->controllerRequestPending = false;
        participant->requestedControllerSlot = kObserverPlayerSlot;
        m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(*participant, 0));
        return true;
    }

    if(participant->controllerAssignment != kObserverPlayerSlot) {
        pushLog("Ignored controller request from already-assigned participant " + participant->displayName);
        return false;
    }

    if(data.requestedSlot >= 4) return false;

    participant->controllerRequestPending = true;
    participant->requestedControllerSlot = data.requestedSlot;
    participant->ready = false;
    m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(*participant, 0));
    pushLog(participant->displayName + " requested P" + std::to_string(static_cast<unsigned>(data.requestedSlot) + 1u));
    return true;
}

bool NetplayCoordinator::handleStartSession(PacketReader& reader)
{
    StartSessionData data;
    if(!reader.readPod(data)) return false;

    m_session.roomState().inputDelayFrames = data.inputDelayFrames;
    m_session.roomState().state = data.state;
    pushLog(data.state == SessionState::Running ? "Session started" : "Session state updated");
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

    for(ENetPeer* peer : m_transport.connectedPeers()) {
        const ParticipantId participantId = participantIdFromPeer(peer);
        if(ParticipantInfo* participant = m_session.findParticipant(participantId)) {
            participant->pingMs = static_cast<uint16_t>(std::min<uint32_t>(peer->roundTripTime, 65535u));
            participant->jitterMs = static_cast<uint16_t>(std::min<uint32_t>(peer->roundTripTimeVariance, 65535u));
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
    } else if(m_serverPeer != nullptr) {
        data.pingMs = static_cast<uint16_t>(std::min<uint32_t>(m_serverPeer->roundTripTime, 65535u));
        data.jitterMs = static_cast<uint16_t>(std::min<uint32_t>(m_serverPeer->roundTripTimeVariance, 65535u));

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
        if(participant.controllerAssignment == kObserverPlayerSlot) continue;

        FrameNumber latestFrame = 0;
        if(participant.id == m_localParticipantId) {
            if(const TimelineInputEntry* latest = m_localInputs.latestFor(participant.id, participant.controllerAssignment)) {
                latestFrame = latest->frame;
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

    return anyAssigned ? confirmedFrame : 0;
}

void NetplayCoordinator::broadcastFrameStatusIfNeeded()
{
    if(!m_hosting) return;

    FrameStatusData status;
    status.currentFrame = 0;
    if(const TimelineInputEntry* latestLocal = m_localInputs.latest()) {
        status.currentFrame = latestLocal->frame;
    }
    status.lastConfirmedFrame = computeHostConfirmedFrame();
    status.inputDelayFrames = m_session.roomState().inputDelayFrames;

    const FrameNumber previousCurrentFrame = m_session.roomState().currentFrame;
    const FrameNumber previousConfirmedFrame = m_session.roomState().lastConfirmedFrame;
    const uint8_t previousInputDelayFrames = m_session.roomState().inputDelayFrames;
    const bool changed =
        status.currentFrame != previousCurrentFrame ||
        status.lastConfirmedFrame != previousConfirmedFrame ||
        status.inputDelayFrames != previousInputDelayFrames;

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

bool NetplayCoordinator::allRequiredParticipantsReady() const
{
    bool anyAssigned = false;
    for(const ParticipantInfo& participant : m_session.roomState().participants) {
        if(participant.controllerAssignment == kObserverPlayerSlot) continue;
        anyAssigned = true;
        if(!participant.connected || !participant.ready) return false;
    }
    return anyAssigned;
}

bool NetplayCoordinator::allRequiredParticipantsRomCompatible() const
{
    if(m_session.roomState().selectedGameName.empty()) return false;

    bool anyAssigned = false;
    for(const ParticipantInfo& participant : m_session.roomState().participants) {
        if(participant.controllerAssignment == kObserverPlayerSlot) continue;
        anyAssigned = true;
        if(!participant.connected || !participant.romLoaded || !participant.romCompatible) return false;
    }
    return anyAssigned;
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
    if(!m_hosting || m_reconnectReservationDeadlines.empty()) return;

    const auto now = std::chrono::steady_clock::now();
    std::vector<ParticipantId> expiredParticipants;

    for(auto it = m_reconnectReservationDeadlines.begin(); it != m_reconnectReservationDeadlines.end(); ++it) {
        ParticipantInfo* participant = m_session.findParticipant(it->first);
        if(participant == nullptr || !participant->reconnectReserved) {
            expiredParticipants.push_back(it->first);
            continue;
        }

        if(now >= it->second) {
            expiredParticipants.push_back(it->first);
            continue;
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second - now);
        const uint16_t secondsRemaining = static_cast<uint16_t>(std::clamp<long long>(remaining.count() + 1, 0, 65535));
        if(participant->reservationSecondsRemaining != secondsRemaining) {
            participant->reservationSecondsRemaining = secondsRemaining;
            m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(*participant, 0));
        }
    }

    for(ParticipantId participantId : expiredParticipants) {
        ParticipantInfo* participant = m_session.findParticipant(participantId);
        if(participant != nullptr && participant->reconnectReserved) {
            pushLog("Reconnect reservation expired for " + participant->displayName);
            removeParticipant(participantId);
            m_transport.broadcastReliable(Channel::Control, buildParticipantLeftPacket(participantId));
        }
        m_reconnectReservationDeadlines.erase(participantId);
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
    predicted.predicted = true;
    predicted.confirmed = false;
    m_remoteInputs.push(predicted);
    return true;
}

bool NetplayCoordinator::handleJoinRoom(ENetPeer* peer, PacketReader& reader)
{
    if(!m_hosting) return false;

    JoinRoomData joinData;
    if(!reader.readPod(joinData)) return false;

    std::string displayName;
    if(!reader.readString(displayName)) return false;

    ParticipantInfo* reconnectParticipant = nullptr;
    if(joinData.reconnectToken != 0) {
        for(ParticipantInfo& existing : m_session.roomState().participants) {
            if(existing.id == m_localParticipantId) continue;
            if(existing.connected) continue;
            if(existing.reconnectToken == joinData.reconnectToken) {
                reconnectParticipant = &existing;
                break;
            }
        }
    }

    ParticipantInfo& participant = reconnectParticipant != nullptr
        ? *reconnectParticipant
        : ensureParticipant(m_nextAssignedParticipantId++, displayName);

    participant.displayName = displayName;
    participant.connected = true;
    participant.reconnectReserved = false;
    participant.reservationSecondsRemaining = 0;
    m_reconnectReservationDeadlines.erase(participant.id);
    if(reconnectParticipant == nullptr) {
        participant.reconnectToken = joinData.reconnectToken != 0 ? joinData.reconnectToken : generateReconnectToken();
    }
    participant.controllerRequestPending = false;
    participant.requestedControllerSlot = kObserverPlayerSlot;
    if(reconnectParticipant == nullptr) {
        if(m_session.roomState().state == SessionState::Running ||
           m_session.roomState().state == SessionState::Paused) {
            participant.role = ParticipantRole::Observer;
            participant.controllerAssignment = kObserverPlayerSlot;
            participant.ready = false;
            m_pendingHostSpectatorSyncParticipant = participant.id;
        } else {
            participant.role = ParticipantRole::Player;
            participant.controllerAssignment = participant.id <= 3 ? static_cast<PlayerSlot>(participant.id) : kObserverPlayerSlot;
        }
    } else {
        participant.ready = false;
    }

    peer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(participant.id) + 1u);

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
    status.currentFrame = m_session.roomState().currentFrame;
    status.lastConfirmedFrame = m_session.roomState().lastConfirmedFrame;
    status.inputDelayFrames = m_session.roomState().inputDelayFrames;
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
    if(reconnectParticipant != nullptr) {
        oss << "Participant reconnected: " << participant.displayName
            << " (id " << static_cast<int>(participant.id) << "), waiting for ready";
    } else {
        oss << "Participant joined: " << participant.displayName << " (id " << static_cast<int>(participant.id) << ")";
    }
    pushLog(oss.str());
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
    PlayerSlot controllerAssignment = kObserverPlayerSlot;
    uint8_t controllerRequestPending = 0;
    PlayerSlot requestedControllerSlot = kObserverPlayerSlot;
    std::string displayName;

    if(!reader.readPod(participantId)) return false;
    if(!reader.readPod(reconnectToken)) return false;
    if(!reader.readPod(connected)) return false;
    if(!reader.readPod(reconnectReserved)) return false;
    if(!reader.readPod(reservationSecondsRemaining)) return false;
    if(!reader.readPod(role)) return false;
    if(!reader.readPod(controllerAssignment)) return false;
    if(!reader.readPod(controllerRequestPending)) return false;
    if(!reader.readPod(requestedControllerSlot)) return false;
    if(!reader.readString(displayName)) return false;

    ParticipantInfo& participant = ensureParticipant(participantId, displayName);
    if(reconnectToken != 0) {
        participant.reconnectToken = reconnectToken;
    }
    participant.connected = connected != 0;
    participant.reconnectReserved = reconnectReserved != 0;
    participant.reservationSecondsRemaining = reservationSecondsRemaining;
    participant.role = role;
    participant.controllerAssignment = controllerAssignment;
    participant.controllerRequestPending = controllerRequestPending != 0;
    participant.requestedControllerSlot = participant.controllerRequestPending ? requestedControllerSlot : kObserverPlayerSlot;

    if(m_localParticipantId == kInvalidParticipantId && !m_hosting) {
        m_localParticipantId = participantId;
        m_connected = true;
        participant.ready = false;
        if(participant.reconnectToken != 0) {
            m_localReconnectToken = participant.reconnectToken;
        }
    }

    std::ostringstream oss;
    oss << (participant.connected ? "Participant active: " : "Participant inactive: ")
        << participant.displayName << " (id " << static_cast<int>(participant.id) << ")";
    pushLog(oss.str());
    return true;
}

bool NetplayCoordinator::handleControlPacket(ENetPeer* peer, const std::vector<uint8_t>& payload)
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

        case MessageType::ParticipantJoined:
            return handleParticipantJoined(reader);

        case MessageType::ParticipantLeft:
            return handleParticipantLeft(reader);

        case MessageType::SetRole:
            return handleSetRole(reader);

        case MessageType::ResyncBegin:
            return handleResyncBegin(reader);

        case MessageType::ResyncChunk:
            return handleResyncChunk(reader);

        case MessageType::ResyncComplete:
            return handleResyncComplete(reader);

        case MessageType::ResyncAck:
            return handleResyncAck(reader);

        case MessageType::SpectatorSyncBegin:
            return handleSpectatorSyncBegin(reader);

        case MessageType::SpectatorSyncChunk:
            return handleSpectatorSyncChunk(reader);

        case MessageType::SpectatorSyncComplete:
            return handleSpectatorSyncComplete(reader);

        case MessageType::SpectatorSyncAck:
            return handleSpectatorSyncAck(reader);

        case MessageType::PeerHealth:
            return handlePeerHealth(peer, reader);

        case MessageType::SelectRom:
            return handleSelectRom(reader);

        case MessageType::RomValidationResult:
            return handleRomValidationResult(peer, reader);

        case MessageType::InputFrame:
            return handleInputFrame(peer, reader);

        case MessageType::FrameStatus:
            return handleFrameStatus(reader);

        case MessageType::CrcReport:
            return handleCrcReport(reader);

        case MessageType::AssignController:
            return handleAssignController(reader);

        case MessageType::SetReady:
            return handleSetReady(peer, reader);

        case MessageType::RequestController:
            return handleRequestController(peer, reader);

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
        m_lastError = "Failed to host ENet session";
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
    hostParticipant.controllerAssignment = 0;
    hostParticipant.ready = true;

    std::ostringstream oss;
    oss << "Hosting on port " << port << " for up to " << maxPeers << " peers";
    pushLog(oss.str());
    return true;
}

bool NetplayCoordinator::join(const std::string& hostName, uint16_t port, const std::string& displayName)
{
    disconnect();

    m_localDisplayName = displayName.empty() ? defaultDisplayName() : displayName;

    if(!m_transport.connectToHost(hostName, port)) {
        m_lastError = "Failed to connect to host";
        pushLog(m_lastError);
        return false;
    }

    resetSessionState();
    m_session.roomState().state = SessionState::Lobby;
    pushLog("Connecting to " + hostName + ":" + std::to_string(port));
    return true;
}

void NetplayCoordinator::disconnect()
{
    if(m_transport.isActive()) {
        m_transport.disconnectAll();
        m_transport.shutdown();
    }

    resetSessionState();
}

void NetplayCoordinator::update(uint32_t timeoutMs)
{
    if(!m_transport.isActive()) return;

    for(const NetTransport::Event& event : m_transport.poll(timeoutMs)) {
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
                    m_serverPeer = nullptr;
                    m_connected = false;
                    m_session.roomState().state = SessionState::Ended;
                    if(m_session.roomState().currentFrame > 0) {
                        m_lastError = "Host disconnected during session";
                    }
                } else if(m_hosting) {
                    const ParticipantId participantId = participantIdFromPeer(event.peer);
                    if(participantId != kInvalidParticipantId && m_session.findParticipant(participantId) != nullptr) {
                        ParticipantInfo* participant = m_session.findParticipant(participantId);
                        const bool hadActiveAssignment =
                            participant != nullptr &&
                            participant->controllerAssignment != kObserverPlayerSlot &&
                            (m_session.roomState().state == SessionState::Running ||
                             m_session.roomState().state == SessionState::Resyncing ||
                             m_session.roomState().state == SessionState::Paused);
                        const bool keepReservation = hadActiveAssignment;
                        pushLog("Peer disconnected: participant " + std::to_string(static_cast<int>(participantId)));

                        if(keepReservation && participant != nullptr) {
                            participant->connected = false;
                            participant->reconnectReserved = true;
                            participant->reservationSecondsRemaining = kReconnectReservationSeconds;
                            participant->ready = false;
                            participant->pingMs = 0;
                            participant->jitterMs = 0;
                            m_reconnectReservationDeadlines[participantId] =
                                std::chrono::steady_clock::now() + std::chrono::seconds(kReconnectReservationSeconds);
                            m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(*participant, 0), event.peer);
                            m_session.roomState().state = SessionState::Paused;
                            pushLog("Session paused because an active participant disconnected; slot reserved for reconnect");
                        } else {
                            removeParticipant(participantId);
                            m_transport.broadcastReliable(Channel::Control, buildParticipantLeftPacket(participantId), event.peer);
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
    }

    updatePeerHealthFromTransport();
    updateReconnectReservations();
    broadcastFrameStatusIfNeeded();
    broadcastPeerHealthIfNeeded();
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

std::optional<ParticipantId> NetplayCoordinator::consumePendingHostSpectatorSyncParticipant()
{
    std::optional<ParticipantId> result = m_pendingHostSpectatorSyncParticipant;
    m_pendingHostSpectatorSyncParticipant.reset();
    return result;
}

const InputTimeline& NetplayCoordinator::localInputs() const
{
    return m_localInputs;
}

const InputTimeline& NetplayCoordinator::remoteInputs() const
{
    return m_remoteInputs;
}

void NetplayCoordinator::recordLocalInputFrame(FrameNumber frame, PlayerSlot slot, uint64_t buttonMaskLo, uint64_t buttonMaskHi)
{
    if(slot >= 4 || m_localParticipantId == kInvalidParticipantId) return;

    TimelineInputEntry entry;
    entry.frame = frame;
    entry.participantId = m_localParticipantId;
    entry.playerSlot = slot;
    entry.buttonMaskLo = buttonMaskLo;
    entry.buttonMaskHi = buttonMaskHi;
    entry.sequence = ++m_localInputSequence;
    entry.confirmed = true;
    m_localInputs.push(entry);

    InputFrameData packetData;
    packetData.frame = frame;
    packetData.participantId = m_localParticipantId;
    packetData.playerSlot = slot;
    packetData.buttonMaskLo = buttonMaskLo;
    packetData.buttonMaskHi = buttonMaskHi;
    packetData.sequence = entry.sequence;
    const std::vector<uint8_t> payload = buildInputFramePacket(packetData);

    if(m_hosting) {
        m_transport.broadcastReliable(Channel::Gameplay, payload);
        return;
    }

    if(!m_connected || m_serverPeer == nullptr) return;
    m_transport.sendReliable(m_serverPeer, Channel::Gameplay, payload);
}

void NetplayCoordinator::predictRemoteInputsForFrame(FrameNumber frame)
{
    for(const ParticipantInfo& participant : m_session.roomState().participants) {
        if(participant.id == kInvalidParticipantId || participant.id == m_localParticipantId) continue;

        if(participant.controllerAssignment != kObserverPlayerSlot) {
            predictRemoteInputFrame(frame, participant.id, participant.controllerAssignment);
        }
    }
}

void NetplayCoordinator::submitLocalCrc(FrameNumber frame, uint32_t crc32)
{
    if(m_session.roomState().state != SessionState::Running) return;

    m_lastLocalCrcFrame = frame;
    m_lastLocalCrc32 = crc32;
    m_recentLocalCrcHistory.emplace_back(frame, crc32);
    while(m_recentLocalCrcHistory.size() > kRecentLocalCrcHistoryCapacity) {
        m_recentLocalCrcHistory.pop_front();
    }

    if(!m_connected || !m_transport.isActive()) return;

    CrcReportData report;
    report.frame = frame;
    report.crc32 = crc32;
    report.severity = DesyncSeverity::NoIssue;

    const std::vector<uint8_t> payload = buildCrcReportPacket(report, m_session.roomState().sessionId);

    if(m_hosting) {
        m_transport.broadcastReliable(Channel::Diagnostics, payload);
    } else if(m_serverPeer != nullptr) {
        m_transport.sendReliable(m_serverPeer, Channel::Diagnostics, payload);
    }
}

bool NetplayCoordinator::beginResync(FrameNumber targetFrame, const std::vector<uint8_t>& payload, uint32_t payloadCrc32)
{
    if(!m_hosting || payload.empty()) return false;

    const uint32_t resyncId = m_nextResyncId++;
    m_session.roomState().state = SessionState::Resyncing;
    m_session.roomState().activeResyncId = resyncId;
    m_session.roomState().resyncTargetFrame = targetFrame;
    m_session.roomState().resyncPayloadSize = static_cast<uint32_t>(payload.size());
    m_session.roomState().resyncPayloadCrc32 = payloadCrc32;
    m_pendingResyncAcks.clear();

    for(const ParticipantInfo& participant : m_session.roomState().participants) {
        if(participant.id != m_localParticipantId) {
            m_pendingResyncAcks.push_back(participant.id);
        }
    }
    m_session.roomState().pendingResyncAckCount = static_cast<uint32_t>(m_pendingResyncAcks.size());
    m_predictionStats.recordHardResync();

    if(m_pendingResyncAcks.empty()) {
        m_session.roomState().state = SessionState::Running;
        m_session.roomState().activeResyncId = 0;
        m_session.roomState().resyncTargetFrame = 0;
        m_session.roomState().resyncPayloadSize = 0;
        m_session.roomState().resyncPayloadCrc32 = 0;
        pushLog("Resync skipped: no remote peers");
        return true;
    }

    ResyncBeginData beginData;
    beginData.resyncId = resyncId;
    beginData.targetFrame = targetFrame;
    beginData.payloadSize = static_cast<uint32_t>(payload.size());
    beginData.payloadCrc32 = payloadCrc32;
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

bool NetplayCoordinator::beginSpectatorSync(ParticipantId participantId, FrameNumber targetFrame, const std::vector<uint8_t>& payload, uint32_t payloadCrc32)
{
    if(!m_hosting || payload.empty()) return false;

    ENetPeer* targetPeer = nullptr;
    for(ENetPeer* peer : m_transport.connectedPeers()) {
        if(participantIdFromPeer(peer) == participantId) {
            targetPeer = peer;
            break;
        }
    }
    if(targetPeer == nullptr) return false;

    const uint32_t resyncId = m_nextResyncId++;

    SpectatorSyncBeginData beginData;
    beginData.resyncId = resyncId;
    beginData.targetFrame = targetFrame;
    beginData.payloadSize = static_cast<uint32_t>(payload.size());
    beginData.payloadCrc32 = payloadCrc32;
    m_transport.sendReliable(targetPeer, Channel::Control, buildSpectatorSyncBeginPacket(beginData));

    for(size_t offset = 0; offset < payload.size(); offset += kResyncChunkPayloadBytes) {
        const size_t chunkSize = std::min(kResyncChunkPayloadBytes, payload.size() - offset);
        SpectatorSyncChunkData chunkData;
        chunkData.resyncId = resyncId;
        chunkData.offset = static_cast<uint32_t>(offset);
        chunkData.size = static_cast<uint16_t>(chunkSize);
        m_transport.sendReliable(
            targetPeer,
            Channel::Control,
            buildSpectatorSyncChunkPacket(chunkData, std::span<const uint8_t>(payload.data() + offset, chunkSize))
        );
    }

    SpectatorSyncCompleteData completeData;
    completeData.resyncId = resyncId;
    m_transport.sendReliable(targetPeer, Channel::Control, buildSpectatorSyncCompletePacket(completeData));
    pushLog("Sent spectator sync to participant " + std::to_string(static_cast<int>(participantId)));
    return true;
}

std::optional<NetplayCoordinator::PendingResyncApply> NetplayCoordinator::consumePendingResyncApply()
{
    std::optional<PendingResyncApply> result = std::move(m_pendingResyncApply);
    m_pendingResyncApply.reset();
    return result;
}

std::optional<NetplayCoordinator::PendingResyncApply> NetplayCoordinator::consumePendingSpectatorSyncApply()
{
    std::optional<PendingResyncApply> result = std::move(m_pendingSpectatorSyncApply);
    m_pendingSpectatorSyncApply.reset();
    return result;
}

bool NetplayCoordinator::acknowledgeResync(uint32_t resyncId, FrameNumber loadedFrame, uint32_t crc32, bool success)
{
    if(m_hosting || m_serverPeer == nullptr) return false;

    ResyncAckData ack;
    ack.resyncId = resyncId;
    ack.participantId = m_localParticipantId;
    ack.loadedFrame = loadedFrame;
    ack.crc32 = crc32;
    ack.success = success ? 1 : 0;

    if(success) {
        realignAuthoritativeState(loadedFrame);
        m_session.roomState().state = SessionState::Running;
        m_session.roomState().activeResyncId = 0;
        m_session.roomState().resyncTargetFrame = 0;
        m_session.roomState().resyncPayloadSize = 0;
        m_session.roomState().resyncPayloadCrc32 = 0;
        m_session.roomState().pendingResyncAckCount = 0;
    }

    return m_transport.sendReliable(m_serverPeer, Channel::Control, buildResyncAckPacket(ack));
}

bool NetplayCoordinator::acknowledgeSpectatorSync(uint32_t resyncId, FrameNumber loadedFrame, uint32_t crc32, bool success)
{
    if(m_hosting || m_serverPeer == nullptr) return false;

    SpectatorSyncAckData ack;
    ack.resyncId = resyncId;
    ack.participantId = m_localParticipantId;
    ack.loadedFrame = loadedFrame;
    ack.crc32 = crc32;
    ack.success = success ? 1 : 0;

    if(success) {
        realignAuthoritativeState(loadedFrame);
        m_requiresSpectatorSync = false;
    }

    return m_transport.sendReliable(m_serverPeer, Channel::Control, buildSpectatorSyncAckPacket(ack));
}

bool NetplayCoordinator::awaitingSpectatorSync() const
{
    return m_requiresSpectatorSync || m_incomingSpectatorSync.has_value() || m_pendingSpectatorSyncApply.has_value();
}

bool NetplayCoordinator::setLocalReady(bool ready)
{
    if(m_localParticipantId == kInvalidParticipantId) return false;

    ParticipantInfo* participant = m_session.findParticipant(m_localParticipantId);
    if(participant == nullptr) return false;
    participant->connected = true;

    if(!participant->romLoaded || !participant->romCompatible) {
        pushLog("Cannot mark ready: local ROM is not compatible with the room");
        return false;
    }

    participant->ready = ready;

    SetReadyData data;
    data.participantId = m_localParticipantId;
    data.ready = ready ? 1 : 0;

    if(m_hosting) {
        m_transport.broadcastReliable(Channel::Control, buildSetReadyPacket(data, m_session.roomState().sessionId));
        return true;
    }

    if(m_serverPeer != nullptr) {
        return m_transport.sendReliable(m_serverPeer, Channel::Control, buildSetReadyPacket(data, m_session.roomState().sessionId));
    }

    return false;
}

bool NetplayCoordinator::requestControllerSlot(PlayerSlot slot)
{
    if(m_hosting || m_serverPeer == nullptr || m_localParticipantId == kInvalidParticipantId) return false;
    if(slot >= 4) return false;

    ParticipantInfo* participant = m_session.findParticipant(m_localParticipantId);
    if(participant == nullptr || participant->controllerAssignment != kObserverPlayerSlot) return false;

    participant->controllerRequestPending = true;
    participant->requestedControllerSlot = slot;
    participant->ready = false;

    RequestControllerData data;
    data.participantId = m_localParticipantId;
    data.requestedSlot = slot;
    data.clearRequest = 0;
    return m_transport.sendReliable(m_serverPeer, Channel::Control, buildRequestControllerPacket(data, m_session.roomState().sessionId));
}

bool NetplayCoordinator::cancelControllerRequest()
{
    if(m_hosting || m_serverPeer == nullptr || m_localParticipantId == kInvalidParticipantId) return false;

    ParticipantInfo* participant = m_session.findParticipant(m_localParticipantId);
    if(participant == nullptr || !participant->controllerRequestPending) return false;

    participant->controllerRequestPending = false;
    participant->requestedControllerSlot = kObserverPlayerSlot;

    RequestControllerData data;
    data.participantId = m_localParticipantId;
    data.requestedSlot = kObserverPlayerSlot;
    data.clearRequest = 1;
    return m_transport.sendReliable(m_serverPeer, Channel::Control, buildRequestControllerPacket(data, m_session.roomState().sessionId));
}

bool NetplayCoordinator::approveControllerRequest(ParticipantId participantId)
{
    if(!m_hosting) return false;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr || !participant->controllerRequestPending) return false;

    if(m_session.roomState().state == SessionState::Running) {
        pauseSession();
    }

    const PlayerSlot requestedSlot = participant->requestedControllerSlot;
    participant->controllerRequestPending = false;
    participant->requestedControllerSlot = kObserverPlayerSlot;
    participant->ready = false;

    const bool assigned = assignController(participantId, requestedSlot);
    if(assigned) {
        pushLog("Approved controller request for " + participant->displayName + " -> P" + std::to_string(static_cast<unsigned>(requestedSlot) + 1u));
    }
    return assigned;
}

bool NetplayCoordinator::denyControllerRequest(ParticipantId participantId)
{
    if(!m_hosting) return false;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr || !participant->controllerRequestPending) return false;

    participant->controllerRequestPending = false;
    participant->requestedControllerSlot = kObserverPlayerSlot;
    participant->ready = false;
    m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(*participant, 0));
    pushLog("Denied controller request for " + participant->displayName);
    return true;
}

bool NetplayCoordinator::setParticipantRole(ParticipantId participantId, ParticipantRole role)
{
    if(!m_hosting || participantId == m_localParticipantId) return false;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr) return false;

    participant->role = role;
    participant->controllerRequestPending = false;
    participant->requestedControllerSlot = kObserverPlayerSlot;
    if(role == ParticipantRole::Observer) {
        participant->controllerAssignment = kObserverPlayerSlot;
        participant->ready = false;
    }

    SetRoleData data;
    data.participantId = participantId;
    data.role = role;
    m_transport.broadcastReliable(Channel::Control, buildSetRolePacket(data));
    refreshHostRoomState();
    return true;
}

bool NetplayCoordinator::assignController(ParticipantId participantId, PlayerSlot slot)
{
    if(!m_hosting) return false;

    ParticipantInfo* participant = m_session.findParticipant(participantId);
    if(participant == nullptr) return false;

    std::vector<ParticipantId> changedParticipants;
    for(ParticipantInfo& other : m_session.roomState().participants) {
        if(other.id != participantId && other.controllerAssignment == slot && slot != kObserverPlayerSlot) {
            other.controllerAssignment = kObserverPlayerSlot;
            other.role = ParticipantRole::Observer;
            other.ready = false;
            other.controllerRequestPending = false;
            other.requestedControllerSlot = kObserverPlayerSlot;
            changedParticipants.push_back(other.id);
        }
    }

    participant->controllerAssignment = slot;
    participant->role = slot == kObserverPlayerSlot ? ParticipantRole::Observer : ParticipantRole::Player;
    participant->ready = false;
    participant->controllerRequestPending = false;
    participant->requestedControllerSlot = kObserverPlayerSlot;
    changedParticipants.push_back(participantId);
    refreshHostRoomState();

    for(ParticipantId changedId : changedParticipants) {
        ParticipantInfo* changed = m_session.findParticipant(changedId);
        if(changed == nullptr) continue;
        AssignControllerData data;
        data.participantId = changedId;
        data.controllerAssignment = changed->controllerAssignment;
        m_transport.broadcastReliable(Channel::Control, buildAssignControllerPacket(data, m_session.roomState().sessionId));
        m_transport.broadcastReliable(Channel::Control, buildParticipantJoinedPacket(*changed, 0));
    }
    return true;
}

bool NetplayCoordinator::setInputDelayFrames(uint8_t frames)
{
    if(!m_hosting) return false;

    frames = static_cast<uint8_t>(std::min<unsigned>(frames, 8u));
    if(m_session.roomState().inputDelayFrames == frames) return true;

    m_session.roomState().inputDelayFrames = frames;
    pushLog("Input delay set to " + std::to_string(static_cast<unsigned>(frames)) + " frame(s)");

    FrameStatusData status;
    status.currentFrame = m_session.roomState().currentFrame;
    status.lastConfirmedFrame = m_session.roomState().lastConfirmedFrame;
    status.inputDelayFrames = m_session.roomState().inputDelayFrames;
    m_lastBroadcastInputDelayFrames = status.inputDelayFrames;
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

    for(ENetPeer* peer : m_transport.connectedPeers()) {
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
        participant.ready = false;
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
    if(!romCompatible) {
        participant->ready = false;
    }

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

    if(m_serverPeer != nullptr) {
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
    if(!allRequiredParticipantsReady()) {
        pushLog("Cannot start session: not all assigned participants are ready");
        return false;
    }

    m_session.roomState().state = SessionState::Starting;
    m_pendingHostResyncFrame = m_session.roomState().currentFrame;

    StartSessionData data;
    data.state = SessionState::Starting;
    data.inputDelayFrames = m_session.roomState().inputDelayFrames;
    m_transport.broadcastReliable(Channel::Control, buildStartSessionPacket(data, m_session.roomState().sessionId));
    pushLog("Host started session setup; waiting for initial sync");
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
    if(!allRequiredParticipantsReady()) {
        pushLog("Cannot resume session: waiting for assigned participants to mark ready");
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
    writer.writePod(data);
    m_transport.broadcastReliable(Channel::Control, writer.data());
    pushLog("Host ended session");
    return true;
}

} // namespace Netplay

#endif
