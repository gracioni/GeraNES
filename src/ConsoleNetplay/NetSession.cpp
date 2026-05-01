#include "ConsoleNetplay/NetSession.h"

#include <algorithm>

namespace ConsoleNetplay {

namespace {

bool isAssignableSlotInTopology(PlayerSlot slot, const std::vector<InputSlotDescriptor>& topology)
{
    const auto it = std::find_if(topology.begin(), topology.end(), [slot](const InputSlotDescriptor& descriptor) {
        return descriptor.slot == slot;
    });
    return it != topology.end() && it->assignable;
}

} // namespace

void ParticipantInfo::normalizeControllerAssignments(const std::vector<InputSlotDescriptor>* topology)
{
    if(controllerAssignments.empty() && controllerAssignment != kObserverPlayerSlot) {
        controllerAssignments.push_back(controllerAssignment);
    }
    controllerAssignments.erase(
        std::remove(controllerAssignments.begin(), controllerAssignments.end(), kObserverPlayerSlot),
        controllerAssignments.end()
    );
    if(topology != nullptr) {
        controllerAssignments.erase(
            std::remove_if(controllerAssignments.begin(),
                           controllerAssignments.end(),
                           [topology](PlayerSlot slot) {
                               return !isAssignableSlotInTopology(slot, *topology);
                           }),
            controllerAssignments.end()
        );
        if(controllerAssignment != kObserverPlayerSlot &&
           !isAssignableSlotInTopology(controllerAssignment, *topology)) {
            controllerAssignment = kObserverPlayerSlot;
        }
    }
    std::sort(controllerAssignments.begin(), controllerAssignments.end());
    controllerAssignments.erase(
        std::unique(controllerAssignments.begin(), controllerAssignments.end()),
        controllerAssignments.end()
    );
    controllerAssignment = controllerAssignments.empty() ? kObserverPlayerSlot : controllerAssignments.front();
}

bool ParticipantInfo::hasControllerAssignment(PlayerSlot slot) const
{
    return std::find(controllerAssignments.begin(), controllerAssignments.end(), slot) != controllerAssignments.end();
}

RoomState& NetSession::roomState()
{
    return m_roomState;
}

const RoomState& NetSession::roomState() const
{
    return m_roomState;
}

void NetSession::reset()
{
    m_roomState = {};
}

ParticipantInfo* NetSession::findParticipant(ParticipantId id)
{
    for(auto& participant : m_roomState.participants) {
        if(participant.id == id) return &participant;
    }
    return nullptr;
}

const ParticipantInfo* NetSession::findParticipant(ParticipantId id) const
{
    for(const auto& participant : m_roomState.participants) {
        if(participant.id == id) return &participant;
    }
    return nullptr;
}

} // namespace ConsoleNetplay
