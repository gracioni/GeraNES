#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <sol/sol.hpp>

#include "GeraNES/GeraNESEmu.h"

class ModManager {
public:
    struct ChrOverride {
        int tile = 0;
        std::string assetPath;
        bool ignorePalette = false;
    };

    struct BackgroundReplacement {
        std::string id;
        std::string assetPath;
        int x = 0;
        int y = 0;
        int width = 256;
        int height = 240;
    };

    struct LoadRequest {
        std::filesystem::path romPath;
        std::filesystem::path effectiveRomPath;
        std::filesystem::path modPath;
        bool modLoaded = false;
        bool ipsApplied = false;
        std::string message;
    };

    void clear();
    LoadRequest prepareRomLoad(const std::filesystem::path& romPath, bool useModIfAvailable);
    bool loadScriptForCurrentMod();
    void onFrame(GeraNESEmu& emu);

    bool active() const { return m_active; }
    const std::filesystem::path& modPath() const { return m_modPath; }
    int resolutionMultiplier() const { return m_resolutionMultiplier; }
    const std::vector<ChrOverride>& chrOverrides() const { return m_chrOverrides; }
    const std::vector<BackgroundReplacement>& backgroundReplacements() const { return m_backgroundReplacements; }
    std::optional<std::vector<uint8_t>> readAsset(const std::string& assetPath) const;

private:
    std::filesystem::path m_originalRomPath;
    std::filesystem::path m_effectiveRomPath;
    std::filesystem::path m_modPath;
    bool m_active = false;
    bool m_scriptLoaded = false;
    int m_resolutionMultiplier = 1;
    std::vector<ChrOverride> m_chrOverrides;
    std::vector<BackgroundReplacement> m_backgroundReplacements;
    sol::state m_lua;

    static std::filesystem::path findModPath(const std::filesystem::path& romPath);
    static std::string normalizeZipPath(std::string path);
    static std::optional<std::vector<uint8_t>> readZipEntry(const std::filesystem::path& zipPath, const std::string& entryName);
    static bool zipHasEntry(const std::filesystem::path& zipPath, const std::string& entryName);
    static bool writeBinaryFile(const std::filesystem::path& path, const std::vector<uint8_t>& data, std::string& error);
    static std::optional<std::vector<uint8_t>> applyIpsPatch(
        const std::vector<uint8_t>& romData,
        const std::vector<uint8_t>& patchData,
        std::string& error);

    void bindApi(GeraNESEmu* emu);
    uint8_t readMemory(GeraNESEmu* emu, const std::string& type, uint32_t address) const;
};
