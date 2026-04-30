#pragma once

#include "GeraNES/GeraNESEmu.h"
#include "GeraNESApp/EmulationHost.h"
#include "GeraNESNetplay/GeraNESInputState.h"
#include "ConsoleNetplay/NetplayInputFrame.h"
#include "ConsoleNetplay/NetSession.h"

namespace GeraNESNetplay
{

using ConsoleNetplay::FrameNumber;
using ConsoleNetplay::NetplayInputFrame;
using ConsoleNetplay::PlayerSlot;
using ConsoleNetplay::RoomState;

void applyPadMaskToInputState(EmulationHost::InputState& state, PlayerSlot slot, uint64_t mask);
void applyInputFrameToInputState(EmulationHost::InputState& state, const InputFrame& inputFrame);
NetplayInputFrame buildGeraNESLocalInputContribution(PlayerSlot slot,
                                                     FrameNumber frame,
                                                     const EmulationHost::InputState& localInputState,
                                                     const RoomState& room);

}
