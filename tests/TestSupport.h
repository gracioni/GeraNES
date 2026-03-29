#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "TestConfig.h"

namespace GeraNESTestSupport
{
    inline std::filesystem::path romPath()
    {
        if(const char* env = std::getenv("GERANES_TEST_ROM")) {
            if(env[0] != '\0') {
                return std::filesystem::path(env);
            }
        }
        if(std::string_view(GERANES_DEFAULT_TEST_ROM).empty()) {
            return {};
        }
        return std::filesystem::path(GERANES_DEFAULT_TEST_ROM);
    }

    inline void requireRomFixture()
    {
        const auto rom = romPath();
        INFO("ROM fixture path: " << rom.string());
        INFO("Define GERANES_TEST_ROM or configure CMake with -DGERANES_DEFAULT_TEST_ROM=<path-to-rom> before running the tests.");
        REQUIRE_FALSE(rom.empty());
        REQUIRE(std::filesystem::exists(rom));
    }

    inline std::filesystem::path reportPath(const std::string& name)
    {
        const auto dir = std::filesystem::path(GERANES_BINARY_DIR) / "test_reports";
        std::filesystem::create_directories(dir);
        return dir / name;
    }

    inline nlohmann::json loadJson(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        INFO("Report file path: " << path.string());
        REQUIRE(static_cast<bool>(in));
        nlohmann::json report;
        in >> report;
        return report;
    }
}
