#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "NetProtocol.h"

namespace Netplay {

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
    bool romLoaded = false;
    bool romCompatible = false;
    bool ready = false;
    uint16_t pingMs = 0;
    uint16_t jitterMs = 0;
    FrameNumber lastReceivedInputFrame = 0;
    uint32_t rollbackScheduledCount = 0;
    uint32_t futureFrameMismatchCount = 0;
    uint32_t confirmedFrameConflictCount = 0;
    FrameNumber lastDecisionFrame = 0;
    PlayerSlot lastDecisionSlot = kObserverPlayerSlot;
    std::string lastDecision;
};

struct RoomState
{
    uint32_t sessionId = 0;
    SessionState state = SessionState::Lobby;
    uint8_t inputDelayFrames = 2;
    FrameNumber currentFrame = 0;
    FrameNumber lastConfirmedFrame = 0;
    FrameNumber lastRemoteCrcFrame = 0;
    uint32_t lastRemoteCrc32 = 0;
    uint32_t activeResyncId = 0;
    FrameNumber resyncTargetFrame = 0;
    uint32_t resyncPayloadSize = 0;
    uint32_t resyncPayloadCrc32 = 0;
    uint32_t pendingResyncAckCount = 0;
    std::string selectedGameName;
    RomValidationData romValidation;
    std::vector<ParticipantInfo> participants;
};

class NetSession
{
private:
    RoomState m_roomState;

public:
    RoomState& roomState()
    {
        return m_roomState;
    }

    const RoomState& roomState() const
    {
        return m_roomState;
    }

    void reset()
    {
        m_roomState = {};
    }

    ParticipantInfo* findParticipant(ParticipantId id)
    {
        for(auto& participant : m_roomState.participants) {
            if(participant.id == id) return &participant;
        }
        return nullptr;
    }

    const ParticipantInfo* findParticipant(ParticipantId id) const
    {
        for(const auto& participant : m_roomState.participants) {
            if(participant.id == id) return &participant;
        }
        return nullptr;
    }
};

} // namespace Netplay
