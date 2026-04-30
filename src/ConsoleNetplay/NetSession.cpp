#include "ConsoleNetplay/NetSession.h"

#include <algorithm>

namespace ConsoleNetplay {

void ParticipantInfo::normalizeControllerAssignments()
{
    controllerAssignments.erase(
        std::remove_if(
            controllerAssignments.begin(),
            controllerAssignments.end(),
            [](PlayerSlot slot) {
                return slot != kObserverPlayerSlot && slot > kMultitapP4PlayerSlot;
            }
        ),
        controllerAssignments.end()
    );
    controllerAssignments.erase(
        std::remove(controllerAssignments.begin(), controllerAssignments.end(), kObserverPlayerSlot),
        controllerAssignments.end()
    );
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
