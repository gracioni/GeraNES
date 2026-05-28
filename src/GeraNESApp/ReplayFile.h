#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "GeraNESApp/IEmulationHost.h"

namespace fs = std::filesystem;

class ReplayFile
{
public:
    struct Data {
        std::string romName;
        std::string romCrc;
        IEmulationHost::InputTopologySnapshot inputTopology = {};
        std::optional<InputFrame> bootstrapFrame;
        std::vector<InputFrame> frames;
    };

    static bool save(const fs::path& path, const Data& data, std::string& error);
    static bool load(const fs::path& path, Data& data, std::string& error);
};
