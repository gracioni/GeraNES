#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ConsoleNetplay/NetplayInputFrame.h"
#include "ConsoleNetplay/NetSession.h"

namespace ConsoleNetplay {

std::string inputAssignmentLabel(PlayerSlot slot, const RoomState& room);

std::string inputAssignmentGroupLabel(PlayerSlot slot, const RoomState& room);
std::string inputAssignmentLeafLabel(PlayerSlot slot, const RoomState& room);

std::vector<PlayerSlot> participantAssignments(const ParticipantInfo& participant);
bool participantHasAssignment(const ParticipantInfo& participant, PlayerSlot slot);
bool participantIsObserver(const ParticipantInfo& participant);
void syncParticipantRoleWithAssignments(ParticipantInfo& participant, bool keepHostRole = false);
std::string participantAssignmentsLabel(const ParticipantInfo& participant, const RoomState& room);

std::vector<PlayerSlot> availableInputAssignments(const RoomState& room);
bool isAssignmentAvailable(PlayerSlot slot, const RoomState& room);

RoomState roomWithInputTopology(RoomState room, std::vector<InputSlotDescriptor> inputTopology);

bool isInputAssignmentClaimedByOtherParticipant(const RoomState& room,
                                                ParticipantId participantId,
                                                PlayerSlot slot);

bool canAssignInputCandidate(const RoomState& room,
                             ParticipantId participantId,
                             const std::vector<InputSlotDescriptor>& inputTopology,
                             PlayerSlot slot);

NetplayInputFrame makeRoomTopologyBaseNetplayFrame(FrameNumber frame, const RoomState& room);
NetplayInputFrame makeContributionBase(const NetplayInputFrame& baseFrame);
uint64_t assignedContributionPrimaryMask(PlayerSlot slot, const NetplayInputFrame& contribution);
void applyAssignedContribution(NetplayInputFrame& target, PlayerSlot slot, const NetplayInputFrame& contribution);

} // namespace ConsoleNetplay
