#pragma once

#include "GeraNES/Settings.h"

namespace GeraNES {

struct InputTopology
{
    Settings::Device port1Device = Settings::Device::CONTROLLER;
    Settings::Device port2Device = Settings::Device::CONTROLLER;
    Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
    Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
    Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;

    bool operator==(const InputTopology&) const = default;
    bool operator!=(const InputTopology&) const = default;
};

} // namespace GeraNES
