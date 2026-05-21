#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <sol/sol.hpp>

#include "GeraNES/GeraNESEmu.h"

class ModManager {
public:
    struct MemoryCondition {
        std::string memoryType = "cpu";
        uint32_t address = 0;
        bool word = false;
        int scale = 1;
        std::string op = "==";
        uint32_t value = 0;
        bool hasMask = false;
        uint32_t mask = 0;
        std::vector<uint32_t> values;
    };

    struct ChrOverride {
        enum class Target {
            Both,
            Background,
            Sprite
        };
        enum class SourceLayout {
            Auto,
            Linear,
            PatternTables
        };

        int tile = -1;
        std::string assetPath;
        bool ignorePalette = false;
        bool enabled = true;
        int priority = 0;
        Target target = Target::Both;
        int patternTable = -1;
        int columns = 16;
        SourceLayout sourceLayout = SourceLayout::Auto;
        int sourceX = -1;
        int sourceY = -1;
        int sourceTileOffset = 0;
        bool hasChrHash = false;
        uint32_t chrHash = 0;
        bool defaultTile = false;
        std::vector<uint8_t> paletteIndices;
        bool exactPaletteOrder = false;
        bool absoluteTile = false;
        std::vector<MemoryCondition> conditions;

        bool wholeChr() const { return tile < 0; }
        bool hasSourcePosition() const { return sourceX >= 0 && sourceY >= 0; }
    };

    struct BackgroundReplacement {
        struct PatternPixel {
            int x = 0;
            int y = 0;
            uint8_t paletteIndex = 0;
        };

        struct CameraSource {
            bool enabled = false;
            std::string memoryType = "cpu";
            uint32_t address = 0;
            bool word = false;
            int scale = 1;
        };

        struct ColorKey {
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            int tolerance = 0;
        };

        std::string id;
        std::string assetPath;
        int x = 0;
        int y = 0;
        int width = 256;
        int height = 240;
        int sourceX = 0;
        int sourceY = 0;
        float opacity = 1.0f;
        float parallaxX = 1.0f;
        float parallaxY = 1.0f;
        int scrollX = 0;
        int scrollY = 0;
        bool repeatX = true;
        bool repeatY = true;
        bool enabled = true;
        int priority = 0;
        bool preserveUnmatchedPixels = false;
        CameraSource cameraX;
        CameraSource cameraY;
        std::vector<MemoryCondition> conditions;
        std::vector<PatternPixel> detectPattern;
        std::vector<ColorKey> replaceOnlyColors;
        std::vector<uint8_t> replaceOnlyPaletteIndices;
    };

    struct LoadRequest {
        std::filesystem::path romPath;
        std::filesystem::path effectiveRomPath;
        std::filesystem::path modPath;
        bool modLoaded = false;
        bool ipsApplied = false;
        std::string message;
    };

    struct ChrRenderSnapshot {
        std::array<uint32_t, 64> paletteColors = {};
        std::array<uint32_t, 512> tileHashes = {};
        std::vector<PPU::DebugModBackgroundPixel> backgroundPixels;
        std::vector<PPU::DebugModSpritePixel> spritePixels;
        std::vector<PPU::DebugModRenderedPixel> renderedPixels;
    };
    struct DebugDecodedImage {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> rgba;
    };

    void clear();
    LoadRequest prepareRomLoad(const std::filesystem::path& romPath, bool useModIfAvailable);
    bool loadScriptForCurrentMod();
    void onFrame(GeraNESEmu& emu);
    void renderReplacements(std::vector<uint32_t>& framebuffer, int width, int height, int activeTop, int activeBottom, int scale, GeraNESEmu& emu);
    ChrRenderSnapshot captureChrRenderSnapshot(GeraNESEmu& emu) const;
    void composeChrFrame(std::vector<uint32_t>& framebuffer, int width, int height, int activeTop, int activeBottom, int scale, const uint32_t* sourceFramebuffer, const ChrRenderSnapshot& snapshot, const std::vector<const ChrOverride*>* activeOverrideFilter = nullptr);

    bool active() const { return m_active; }
    const std::filesystem::path& modPath() const { return m_modPath; }
    int resolutionMultiplier() const { return m_resolutionMultiplier; }
    const std::vector<ChrOverride>& chrOverrides() const { return m_chrOverrides; }
    const std::vector<BackgroundReplacement>& backgroundReplacements() const { return m_backgroundReplacements; }
    std::optional<std::vector<uint8_t>> readAsset(const std::string& assetPath) const;
    std::optional<uint32_t> debugReadDecodedImagePixel(const std::string& assetPath, int x, int y);
    std::optional<uint32_t> debugReadAssetPixelDirect(const std::string& assetPath, int x, int y);
    std::vector<std::string> debugListImageAssets() const;
    std::optional<DebugDecodedImage> debugCopyDecodedImage(const std::string& assetPath);

private:
    std::filesystem::path m_originalRomPath;
    std::filesystem::path m_effectiveRomPath;
    std::filesystem::path m_modPath;
    bool m_active = false;
    bool m_scriptLoaded = false;
    int m_resolutionMultiplier = 1;
    std::optional<std::array<uint32_t, 64>> m_customPalette;
    std::vector<ChrOverride> m_chrOverrides;
    std::vector<BackgroundReplacement> m_backgroundReplacements;
    sol::state m_lua;

    struct DecodedImage {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> rgba;
        std::vector<uint8_t> indexedPixels;
        bool indexedPng = false;
        bool indexedFourColor = false;
        int paletteColorCount = 0;
    };

    std::unordered_map<std::string, std::optional<DecodedImage>> m_imageCache;

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
    uint32_t readCameraValue(const BackgroundReplacement::CameraSource& source, GeraNESEmu& emu) const;
    uint32_t readMemoryValue(const MemoryCondition& source, GeraNESEmu& emu) const;
    bool conditionsMatch(const std::vector<MemoryCondition>& conditions, GeraNESEmu& emu) const;
    bool conditionMatches(const MemoryCondition& condition, GeraNESEmu& emu) const;
    bool patternMatches(const BackgroundReplacement& replacement, GeraNESEmu& emu) const;
    bool replacementPixelAllowed(const BackgroundReplacement& replacement, uint32_t pixel, uint8_t paletteIndex) const;
    const DecodedImage* decodedImage(const std::string& assetPath);
    static std::optional<DecodedImage> decodeImage(const std::vector<uint8_t>& data);
    static std::optional<MemoryCondition> parseMemoryConditionObject(const sol::object& object);
    static ChrOverride parseChrOverrideTable(const sol::table& table);
    static BackgroundReplacement parseBackgroundReplacementTable(const std::string& id, const sol::table& table);
    void renderBackgroundReplacements(std::vector<uint32_t>& framebuffer, int width, int height, int activeTop, int activeBottom, int scale, GeraNESEmu& emu);
    static uint32_t blendPixel(uint32_t dst, uint32_t src, int alphaScale);
    static uint32_t hashChrTile(PPU& ppu, int patternAddress);
    static uint8_t readBackgroundPaletteIndexAt(PPU& ppu, int screenX, int screenY);
};
