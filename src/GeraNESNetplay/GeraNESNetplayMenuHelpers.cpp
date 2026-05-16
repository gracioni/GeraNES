#include "GeraNESNetplay/GeraNESNetplayMenuHelpers.h"

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
    snapshot.port1Device = geraNESEffectivePortDeviceFromTopology(ui.room, kPort1PlayerSlot);
    snapshot.port2Device = geraNESEffectivePortDeviceFromTopology(ui.room, kPort2PlayerSlot);
    snapshot.expansionDevice = geraNESExpansionDeviceFromTopology(ui.room);
    snapshot.nesMultitapDevice = geraNESNesMultitapDeviceFromTopology(ui.room);
    snapshot.famicomMultitapDevice = geraNESFamicomMultitapDeviceFromTopology(ui.room);
    return snapshot;
}

} // namespace GeraNESNetplay
