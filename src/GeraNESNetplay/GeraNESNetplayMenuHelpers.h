#pragma once

#include <optional>
#include <vector>

#include "ConsoleNetplay/NetplayAppRuntime.h"
#include "GeraNES/Settings.h"
#include "GeraNESNetplay/GeraNESNetplaySlots.h"
using namespace GeraNES;

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

MenuSnapshot menuSnapshot(const ConsoleNetplay::NetplayAppRuntime& runtime);

} // namespace GeraNESNetplay
