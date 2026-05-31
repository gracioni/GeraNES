#pragma once

#include <optional>

#include "GeraNES/Settings.h"

struct InputTopology
{
    std::optional<Settings::Device> port1Device = Settings::Device::CONTROLLER;
    std::optional<Settings::Device> port2Device = Settings::Device::CONTROLLER;
    Settings::ExpansionDevice expansionDevice = Settings::ExpansionDevice::NONE;
    Settings::NesMultitapDevice nesMultitapDevice = Settings::NesMultitapDevice::NONE;
    Settings::FamicomMultitapDevice famicomMultitapDevice = Settings::FamicomMultitapDevice::NONE;

    bool operator==(const InputTopology&) const = default;
    bool operator!=(const InputTopology&) const = default;
};
