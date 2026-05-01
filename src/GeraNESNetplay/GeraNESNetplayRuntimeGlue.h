#pragma once

#include <optional>
#include <vector>

#include "ConsoleNetplay/NetplayAppRuntime.h"
#include "GeraNES/GeraNESEmu.h"
#include "GeraNESApp/IEmulationHost.h"

namespace GeraNESNetplay {

struct MenuSnapshot
{
    bool hosting = false;
    bool inputManaged = false;
    ConsoleNetplay::NetTransportBackend transportBackend = ConsoleNetplay::defaultNetTransportBackend();
    std::vector<ConsoleNetplay::PlayerSlot> localAssignments;
    std::optional<Settings::Device> port1Device;
    std::optional<Settings::Device> port2Device;
    Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
    Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
    Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;
};

struct RuntimeLoopSettings
{
    ConsoleNetplay::RuntimeInputDelaySettings inputDelaySettings;
    ConsoleNetplay::NetplayAppRuntime::RuntimeFrameSettings frameSettings;
};

void configureRuntimeForGeraNES(ConsoleNetplay::NetplayAppRuntime& runtime, IEmulationHost& host);
MenuSnapshot menuSnapshot(const ConsoleNetplay::NetplayAppRuntime& runtime);
RuntimeLoopSettings buildRuntimeLoopSettings(IEmulationHost& host,
                                             bool autoGameplayTuning,
                                             bool showDebugLog,
                                             int gameplayReceiveDelayMs,
                                             int inputDelayFrames,
                                             int predictFrames);
void configureInputAssignments(ConsoleNetplay::NetplayAppRuntime& runtime,
                               ConsoleNetplay::ParticipantId participantId,
                               std::optional<Settings::Device> port1Device,
                               std::optional<Settings::Device> port2Device,
                               Settings::ExpansionDevice expansionDevice,
                               Settings::NesMultitapDevice nesMultitapDevice,
                               Settings::FamicomMultitapDevice famicomMultitapDevice,
                               const std::vector<ConsoleNetplay::PlayerSlot>& slots);
ConsoleNetplay::NetplayAppRuntime::UpdateResult runRuntimeOnEmulationThread(
    ConsoleNetplay::NetplayAppRuntime& runtime,
    IEmulationHost& host,
    GeraNESEmu& emu,
    const IEmulationHost::InputState& latestInputState,
    const RuntimeLoopSettings& settings);

} // namespace GeraNESNetplay
