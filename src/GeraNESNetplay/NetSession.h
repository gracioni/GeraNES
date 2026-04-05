#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "GeraNES/Settings.h"
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
    std::vector<PlayerSlot> controllerAssignments;
    bool romLoaded = false;
    bool romCompatible = false;
    uint16_t pingMs = 0;
    uint16_t jitterMs = 0;
    FrameNumber lastReportedCurrentFrame = 0;
    FrameNumber lastReportedConfirmedFrame = 0;
    uint32_t peerHealthSerial = 0;
    FrameNumber lastReceivedInputFrame = 0;
    FrameNumber lastContiguousInputFrame = 0;
    uint32_t lastReceivedInputSequence = 0;
    bool sequenceRebasePending = false;
    std::optional<FrameNumber> pendingMissingInputFrom;
    uint32_t missingInputGapCount = 0;
    uint32_t rollbackScheduledCount = 0;
    uint32_t futureFrameMismatchCount = 0;
    uint32_t confirmedFrameConflictCount = 0;
    FrameNumber lastDecisionFrame = 0;
    PlayerSlot lastDecisionSlot = kObserverPlayerSlot;
    std::string lastDecision;

    void normalizeControllerAssignments()
    {
        controllerAssignments.erase(
            std::remove(controllerAssignments.begin(), controllerAssignments.end(), kObserverPlayerSlot),
            controllerAssignments.end()
        );
        std::sort(controllerAssignments.begin(), controllerAssignments.end());
        controllerAssignments.erase(
            std::unique(controllerAssignments.begin(), controllerAssignments.end()),
            controllerAssignments.end()
        );
        controllerAssignment = controllerAssignments.empty() ? kObserverPlayerSlot : controllerAssignments.front();
    }

    bool hasControllerAssignment(PlayerSlot slot) const
    {
        return std::find(controllerAssignments.begin(), controllerAssignments.end(), slot) != controllerAssignments.end();
    }
};

struct RoomState
{
    uint32_t sessionId = 0;
    SessionState state = SessionState::Lobby;
    uint32_t timelineEpoch = 0;
    uint8_t inputDelayFrames = 2;
    uint8_t predictFrames = 0;
    // `currentFrame`: latest frame the host has reported as locally simulated.
    FrameNumber currentFrame = 0;
    // `lastConfirmedFrame`: highest frame the authoritative input timeline has
    // confirmed/published for the current epoch.
    FrameNumber lastConfirmedFrame = 0;
    FrameNumber lastRemoteCrcFrame = 0;
    uint32_t lastRemoteCrc32 = 0;
    uint32_t lastAcceptedRemoteEpoch = 0;
    uint32_t lastIgnoredStaleInputEpoch = 0;
    uint32_t lastIgnoredStaleFrameStatusEpoch = 0;
    uint32_t lastIgnoredStaleCrcEpoch = 0;
    uint32_t staleInputPacketCount = 0;
    uint32_t staleFrameStatusPacketCount = 0;
    uint32_t staleCrcPacketCount = 0;
    uint32_t activeResyncId = 0;
    FrameNumber resyncTargetFrame = 0;
    FrameNumber resyncConfirmedFrame = 0;
    FrameNumber resyncFrameReadyFrame = 0;
    uint32_t resyncPayloadSize = 0;
    uint32_t resyncPayloadCrc32 = 0;
    uint32_t resyncFrameReadyCrc32 = 0;
    uint32_t resyncInputSequenceBase = 0;
    ResyncReason activeResyncReason = ResyncReason::Unspecified;
    uint32_t pendingResyncAckCount = 0;
    std::string selectedGameName;
    RomValidationData romValidation;
    std::optional<Settings::Device> port1Device = Settings::Device::CONTROLLER;
    std::optional<Settings::Device> port2Device = Settings::Device::CONTROLLER;
    Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
    Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
    Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;
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
