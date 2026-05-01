#include "GeraNESNetplay/GeraNESNetplayUIHelpers.h"

#include "ConsoleNetplay/NetplayInputAssignment.h"
#include "GeraNESNetplay/GeraNESNetplayAdapters.h"

namespace GeraNESNetplay {

using namespace ConsoleNetplay;

MenuSnapshot menuSnapshot(const NetplayAppRuntime& runtime)
{
    const NetplayAppRuntime::UiSnapshot ui = runtime.uiSnapshot();
    MenuSnapshot snapshot;
    snapshot.hosting = ui.hosting;
    snapshot.inputManaged = ui.active && ui.connected;
    snapshot.transportBackend = ui.transportBackend;
    if(snapshot.inputManaged) {
        for(const auto& participant : ui.room.participants) {
            if(participant.id != ui.localParticipantId) continue;
            snapshot.localAssignments = participantAssignments(participant);
            break;
        }
    }
    snapshot.port1Device = geraNESPortDeviceFromTopology(ui.room, kPort1PlayerSlot);
    snapshot.port2Device = geraNESPortDeviceFromTopology(ui.room, kPort2PlayerSlot);
    snapshot.expansionDevice = geraNESExpansionDeviceFromTopology(ui.room);
    snapshot.nesMultitapDevice = geraNESNesMultitapDeviceFromTopology(ui.room);
    snapshot.famicomMultitapDevice = geraNESFamicomMultitapDeviceFromTopology(ui.room);
    return snapshot;
}

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
