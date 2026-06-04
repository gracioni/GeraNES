#pragma once

#include <optional>
#include <vector>

#include "GeraNES/InputFrame.h"
#include "GeraNES/InputTopology.h"
#include "GeraNES/Settings.h"
#include "GeraNESNetplay/GeraNESInputState.h"
#include "GeraNESNetplay/GeraNESNetplaySlots.h"
#include "ConsoleNetplay/NetplayInputFrame.h"
#include "ConsoleNetplay/NetplayCoordinator.h"
#include "ConsoleNetplay/NetProtocol.h"
#include "ConsoleNetplay/NetSession.h"
#include "ConsoleNetplay/NetplayTopology.h"
using namespace GeraNES;

namespace GeraNESNetplay {

using ConsoleNetplay::FrameNumber;
using ConsoleNetplay::InputFrameData;
using ConsoleNetplay::kObserverPlayerSlot;
using ConsoleNetplay::NetplayCoordinator;
using ConsoleNetplay::NetplayInputFrame;
using ConsoleNetplay::ParticipantId;
using ConsoleNetplay::PlayerSlot;
using ConsoleNetplay::RoomState;

RoomState roomWithGeraNESInputTopology(RoomState room,
                                       const InputTopology& inputTopology);
bool canAssignGeraNESInputCandidate(const RoomState& room,
                                    ParticipantId participantId,
                                    std::optional<Settings::Device> port1Device,
                                    std::optional<Settings::Device> port2Device,
                                    Settings::ExpansionDevice expansionDevice,
                                    Settings::NesMultitapDevice nesMultitapDevice,
                                    Settings::FamicomMultitapDevice famicomMultitapDevice,
                                    PlayerSlot slot);
Settings::Device geraNESPortDeviceFromTopology(const RoomState& room, PlayerSlot slot);
Settings::ExpansionDevice geraNESExpansionDeviceFromTopology(const RoomState& room);
Settings::NesMultitapDevice geraNESNesMultitapDeviceFromTopology(const RoomState& room);
Settings::FamicomMultitapDevice geraNESFamicomMultitapDeviceFromTopology(const RoomState& room);
std::optional<PlayerSlot> remapGeraNESAssignmentForTopologyChange(PlayerSlot assignment,
                                                                  const RoomState& currentRoom,
                                                                  const std::vector<ConsoleNetplay::InputSlotDescriptor>& candidateTopology);
void setGeraNESRoomInputTopology(NetplayCoordinator& coordinator,
                                 const InputTopology& inputTopology,
                                 std::optional<ParticipantId> preservedParticipantId = std::nullopt,
                                 PlayerSlot preservedAssignment = kObserverPlayerSlot);

NetplayInputFrame toNetplayInputFrame(const InputFrame& inputFrame);

InputFrame toGeraNESInputFrame(const NetplayInputFrame& inputFrame);

bool injectInputFrameForTests(NetplayCoordinator& coordinator,
                              const InputFrameData& input,
                              const InputFrame& contribution);

InputFrame makeRoomTopologyBaseFrame(FrameNumber frame, const RoomState& room);
InputFrame buildAssignedContribution(PlayerSlot slot,
                                      const GeraNESInputState& state,
                                      const InputFrame& baseFrame);

void applyAssignedContribution(InputFrame& target, PlayerSlot slot, const InputFrame& contribution);

} // namespace GeraNESNetplay
