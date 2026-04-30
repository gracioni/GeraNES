#pragma once

#include <optional>
#include <string>
#include <vector>

#include "GeraNESNetplay/NetplayInputFrame.h"
#include "GeraNESNetplay/NetplayInputState.h"
#include "GeraNESNetplay/NetSession.h"

namespace Netplay {

std::string inputAssignmentLabel(PlayerSlot slot, const RoomState& room);

const char* portDeviceLabel(PortDevice device);
const char* expansionDeviceLabel(ExpansionDevice device);
std::string inputAssignmentGroupLabel(PlayerSlot slot, const RoomState& room);
std::string inputAssignmentLeafLabel(PlayerSlot slot, const RoomState& room);

bool isMultitapActive(const RoomState& room);

std::vector<PlayerSlot> participantAssignments(const ParticipantInfo& participant);
bool participantHasAssignment(const ParticipantInfo& participant, PlayerSlot slot);
bool participantIsObserver(const ParticipantInfo& participant);
void syncParticipantRoleWithAssignments(ParticipantInfo& participant, bool keepHostRole = false);
std::string participantAssignmentsLabel(const ParticipantInfo& participant, const RoomState& room);

std::vector<PlayerSlot> availableInputAssignments(const RoomState& room);
bool isAssignmentAvailable(PlayerSlot slot, const RoomState& room);

RoomState roomWithTopology(RoomState room,
                           std::optional<PortDevice> port1Device,
                           std::optional<PortDevice> port2Device,
                           ExpansionDevice expansionDevice,
                           NesMultitapDevice nesMultitapDevice,
                           FamicomMultitapDevice famicomMultitapDevice);

bool isInputAssignmentClaimedByOtherParticipant(const RoomState& room,
                                                ParticipantId participantId,
                                                PlayerSlot slot);

bool canAssignInputCandidate(const RoomState& room,
                             ParticipantId participantId,
                             std::optional<PortDevice> port1Device,
                             std::optional<PortDevice> port2Device,
                             ExpansionDevice expansionDevice,
                             NesMultitapDevice nesMultitapDevice,
                             FamicomMultitapDevice famicomMultitapDevice,
                             PlayerSlot slot);

NetplayInputFrame makeRoomTopologyBaseNetplayFrame(FrameNumber frame, const RoomState& room);
NetplayInputFrame makeContributionBase(const NetplayInputFrame& baseFrame);
NetplayInputFrame buildAssignedContribution(PlayerSlot slot,
                                            const NetplayInputState& state,
                                            const NetplayInputFrame& baseFrame);
uint64_t assignedContributionPrimaryMask(PlayerSlot slot, const NetplayInputFrame& contribution);
void applyAssignedContribution(NetplayInputFrame& target, PlayerSlot slot, const NetplayInputFrame& contribution);

} // namespace Netplay
