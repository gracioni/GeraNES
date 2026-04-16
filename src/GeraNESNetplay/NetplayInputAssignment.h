#pragma once

#include <optional>
#include <string>
#include <vector>

#include "GeraNES/InputBuffer.h"
#include "GeraNESApp/EmulationHost.h"
#include "GeraNESNetplay/NetSession.h"

namespace Netplay {

std::string inputAssignmentLabel(PlayerSlot slot, const RoomState& room);

const char* portDeviceLabel(Settings::Device device);
const char* expansionDeviceLabel(Settings::ExpansionDevice device);
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
                           std::optional<Settings::Device> port1Device,
                           std::optional<Settings::Device> port2Device,
                           Settings::ExpansionDevice expansionDevice,
                           Settings::NesMultitapDevice nesMultitapDevice,
                           Settings::FamicomMultitapDevice famicomMultitapDevice);

bool isInputAssignmentClaimedByOtherParticipant(const RoomState& room,
                                                ParticipantId participantId,
                                                PlayerSlot slot);

bool canAssignInputCandidate(const RoomState& room,
                             ParticipantId participantId,
                             std::optional<Settings::Device> port1Device,
                             std::optional<Settings::Device> port2Device,
                             Settings::ExpansionDevice expansionDevice,
                             Settings::NesMultitapDevice nesMultitapDevice,
                             Settings::FamicomMultitapDevice famicomMultitapDevice,
                             PlayerSlot slot);

InputFrame makeRoomTopologyBaseFrame(FrameNumber frame, const RoomState& room);
InputFrame makeContributionBase(const InputFrame& baseFrame);
InputFrame buildAssignedContribution(PlayerSlot slot,
                                     const EmulationHost::InputState& state,
                                     const InputFrame& baseFrame);

uint64_t assignedContributionPrimaryMask(PlayerSlot slot, const InputFrame& contribution);
void applyAssignedContribution(InputFrame& target, PlayerSlot slot, const InputFrame& contribution);

} // namespace Netplay
