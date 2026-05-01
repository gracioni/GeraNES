#include "GeraNESNetplay/GeraNESNetplayAssignmentHelpers.h"

#include "GeraNESNetplay/GeraNESNetplayAdapters.h"

namespace GeraNESNetplay {

using namespace ConsoleNetplay;

void configureInputAssignments(NetplayAppRuntime& runtime,
                               ParticipantId participantId,
                               std::optional<Settings::Device> port1Device,
                               std::optional<Settings::Device> port2Device,
                               Settings::ExpansionDevice expansionDevice,
                               Settings::NesMultitapDevice nesMultitapDevice,
                               Settings::FamicomMultitapDevice famicomMultitapDevice,
                               const std::vector<PlayerSlot>& slots)
{
    runtime.configureInputAssignments(
        participantId,
        slots,
        [=](NetplayCoordinator& coordinator,
            std::optional<ParticipantId> preservedParticipantId,
            PlayerSlot preservedSlot) {
            setGeraNESRoomInputTopology(
                coordinator,
                port1Device,
                port2Device,
                expansionDevice,
                nesMultitapDevice,
                famicomMultitapDevice,
                preservedParticipantId,
                preservedSlot
            );
        }
    );
}

} // namespace GeraNESNetplay
