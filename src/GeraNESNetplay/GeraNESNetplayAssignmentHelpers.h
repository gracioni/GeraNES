#pragma once

#include <optional>
#include <vector>

#include "ConsoleNetplay/NetplayAppRuntime.h"
#include "GeraNES/Settings.h"
using namespace GeraNES;

namespace GeraNESNetplay {

void configureInputAssignments(ConsoleNetplay::NetplayAppRuntime& runtime,
                               ConsoleNetplay::ParticipantId participantId,
                               std::optional<Settings::Device> port1Device,
                               std::optional<Settings::Device> port2Device,
                               Settings::ExpansionDevice expansionDevice,
                               Settings::NesMultitapDevice nesMultitapDevice,
                               Settings::FamicomMultitapDevice famicomMultitapDevice,
                               const std::vector<ConsoleNetplay::PlayerSlot>& slots);

} // namespace GeraNESNetplay
