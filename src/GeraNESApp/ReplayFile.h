#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "GeraNES/InputFrame.h"
#include "GeraNES/InputTopology.h"
using namespace GeraNES;

namespace fs = std::filesystem;

class ReplayFile
{
public:
    struct Data {
        std::string romName;
        std::string romCrc;
        InputTopology inputTopology = {};
        std::vector<InputFrame> frames;
    };

    static bool saveToBytes(const Data& data, std::vector<uint8_t>& bytes, std::string& error);
    static bool save(const fs::path& path, const Data& data, std::string& error);
    static bool load(const fs::path& path, Data& data, std::string& error);
};
