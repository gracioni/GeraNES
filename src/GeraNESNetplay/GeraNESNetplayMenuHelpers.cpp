#include "GeraNESNetplay/GeraNESNetplayMenuHelpers.h"

#include <algorithm>

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
            for(const PlayerSlot slot : participantAssignments(participant)) {
                if(std::find(snapshot.roomAssignments.begin(), snapshot.roomAssignments.end(), slot) ==
                   snapshot.roomAssignments.end()) {
                    snapshot.roomAssignments.push_back(slot);
                }
                snapshot.slotDisplays.push_back(MenuSnapshot::SlotParticipantDisplay{
                    slot,
                    participant.displayName,
                    participant.nameColorHue
                });
            }
            if(participant.id != ui.localParticipantId) continue;
            snapshot.localAssignments = participantAssignments(participant);
        }
    }
    snapshot.port1Device = geraNESPortDeviceFromTopology(ui.room, kPort1PlayerSlot);
    snapshot.port2Device = geraNESPortDeviceFromTopology(ui.room, kPort2PlayerSlot);
    snapshot.expansionDevice = geraNESExpansionDeviceFromTopology(ui.room);
    snapshot.nesMultitapDevice = geraNESNesMultitapDeviceFromTopology(ui.room);
    snapshot.famicomMultitapDevice = geraNESFamicomMultitapDeviceFromTopology(ui.room);
    return snapshot;
}

} // namespace GeraNESNetplay
