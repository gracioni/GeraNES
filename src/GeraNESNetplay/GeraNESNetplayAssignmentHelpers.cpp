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
            InputTopology inputTopology;
            inputTopology.port1Device = port1Device;
            inputTopology.port2Device = port2Device;
            inputTopology.expansionDevice = expansionDevice;
            inputTopology.nesMultitapDevice = nesMultitapDevice;
            inputTopology.famicomMultitapDevice = famicomMultitapDevice;
            setGeraNESRoomInputTopology(
                coordinator,
                inputTopology,
                preservedParticipantId,
                preservedSlot
            );
        }
    );
}

} // namespace GeraNESNetplay
