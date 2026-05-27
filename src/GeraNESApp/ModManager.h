#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "GeraNES/GeraNESEmu.h"
#include "GeraNESApp/HdPackAudio.h"

class ModManager {
public:
    struct MemoryCondition {
        enum class MemorySource {
            Cpu,
            Ppu,
            Oam,
            PrimaryOam,
            SecondaryOam,
            Unknown
        };

        enum class CompareOp {
            Equal,
            NotEqual,
            Greater,
            GreaterOrEqual,
            Less,
            LessOrEqual,
            BitSet,
            BitClear,
            AnyOf
        };

        enum class Kind {
            MemoryCheck,
            FrameRange,
            TileAtPosition,
            TileNearby,
            SpriteAtPosition,
            SpriteNearby,
            SpritePaletteIndex
        };

        Kind kind = Kind::MemoryCheck;
        bool inverted = false;
        std::string memoryType = "cpu";
        MemorySource memorySource = MemorySource::Cpu;
        uint32_t address = 0;
        bool word = false;
        int scale = 1;
        uint64_t memoryCacheKey = 0;
        size_t readIndex = 0;
        bool compareAgainstMemory = false;
        std::string rhsMemoryType = "cpu";
        MemorySource rhsMemorySource = MemorySource::Cpu;
        uint32_t rhsAddress = 0;
        bool rhsWord = false;
        int rhsScale = 1;
        uint64_t rhsMemoryCacheKey = 0;
        size_t rhsReadIndex = 0;
        std::string op = "==";
        CompareOp compareOp = CompareOp::Equal;
        uint32_t value = 0;
        bool hasMask = false;
        uint32_t mask = 0;
        std::vector<uint32_t> values;
        int x = 0;
        int y = 0;
        bool hasExpectedTile = false;
        bool expectedTileByHash = false;
        int expectedTileIndex = -1;
        uint32_t expectedTileHash = 0;
        std::vector<uint8_t> expectedPalette;
        uint32_t expectedPaletteKey = 0;
        std::string debugName;
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
        bool assetAvailable = true;
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
        int hMirrorRequirement = 0;
        int vMirrorRequirement = 0;
        int bgPriorityRequirement = 0;
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
        bool assetAvailable = true;
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

    struct AdditionalSpriteRule {
        int originalTile = -1;
        uint32_t originalPaletteKey = 0;
        int offsetX = 0;
        int offsetY = 0;
        int additionalTile = -1;
        uint32_t additionalPaletteKey = 0;
        bool ignorePalette = false;
    };

    struct FallbackTileRule {
        int tile = -1;
        int fallbackTile = -1;
    };

    struct LoadRequest {
        std::filesystem::path romPath;
        std::filesystem::path effectiveRomPath;
        std::filesystem::path modPath;
        bool modLoaded = false;
        bool ipsApplied = false;
        std::string message;
    };

    struct OverscanConfig {
        bool enabled = false;
        int top = 0;
        int right = 0;
        int bottom = 0;
        int left = 0;
    };

    struct FrameConditionState {
        uint32_t frameCount = 0;
        std::vector<uint32_t> memoryValues;
    };

    struct ChrRenderSnapshot {
        int scrollX = 0;
        int scrollY = 0;
        std::array<int, PPU::SCREEN_HEIGHT> scrollXByLine = {};
        std::array<int, PPU::SCREEN_HEIGHT> scrollYByLine = {};
        uint8_t universalBgColor = 0;
        std::array<uint32_t, 64> paletteColors = {};
        std::array<uint32_t, 512> tileHashes = {};
        const PPU::DebugModBackgroundPixel* backgroundPixelsView = nullptr;
        size_t backgroundPixelsViewCount = 0;
        const PPU::DebugModSpritePixel* spritePixelsView = nullptr;
        size_t spritePixelsViewCount = 0;
        std::vector<PPU::DebugModBackgroundPixel> backgroundPixels;
        std::vector<PPU::DebugModSpritePixel> spritePixels;
        const FrameConditionState* frameConditionStateView = nullptr;
        FrameConditionState frameConditionState;
    };

    struct FrameConditionPlan {
        struct CachedMemoryRead {
            uint64_t key = 0;
            size_t readIndex = 0;
            MemoryCondition::MemorySource memorySource = MemoryCondition::MemorySource::Cpu;
            uint32_t address = 0;
            bool word = false;
            uint32_t scaleDivisor = 1;
        };

        struct CompiledMemoryCondition {
            size_t readIndex = 0;
            size_t rhsReadIndex = 0;
            MemoryCondition::CompareOp compareOp = MemoryCondition::CompareOp::Equal;
            uint32_t value = 0;
            uint32_t mask = 0;
            bool hasMask = false;
            bool inverted = false;
            bool compareAgainstMemory = false;
            std::vector<uint32_t> anyOfValues;
        };

        struct CompiledFrameRangeCondition {
            uint32_t range = 1;
            uint32_t start = 0;
            bool inverted = false;
        };

        struct CompiledRuleConditions {
            std::vector<CompiledMemoryCondition> memoryConditions;
            std::vector<CompiledFrameRangeCondition> frameRangeConditions;
        };

        std::vector<CachedMemoryRead> uniqueMemoryReads;
        std::vector<CompiledRuleConditions> uniqueGlobalConditionGroups;
        std::vector<size_t> chrOverrideGlobalConditionGroupIndices;
        std::vector<size_t> backgroundGlobalConditionGroupIndices;
    };

    struct DebugDecodedImage {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> rgba;
    };

    struct DebugComposeStage {
        bool valid = false;
        std::string stage;
        std::string assetPath;
        int priority = -1;
        int srcX = -1;
        int srcY = -1;
        uint32_t rawRgba = 0;
        int indexedPaletteIndex = -1;
        bool returnedBaseColor = false;
        std::string reason;
    };

    struct DebugComposePixel {
        bool valid = false;
        uint32_t baseColor = 0;
        uint32_t finalColor = 0;
        std::vector<DebugComposeStage> backgroundCandidates;
        std::vector<DebugComposeStage> backgroundStages;
        std::optional<DebugComposeStage> backgroundOverride;
        std::vector<DebugComposeStage> spriteStages;
    };

    void clear();
    bool selectModSource(const std::filesystem::path& modSourcePath, std::string& error);
    void clearModSource();
    LoadRequest prepareRomLoad(const std::filesystem::path& romPath);
    bool loadDefinitionForCurrentMod();
    void onFrame(GeraNESEmu& emu);
    void onEmulatorReset();
    void onStateLoaded(uint32_t frameCount);
    bool composeFrameOnEmuThread(GeraNESEmu& emu, ChrRenderSnapshot& snapshot, std::vector<uint32_t>& framebuffer, int activeTop, int activeBottom, bool captureDebugSnapshot);
    void composeChrFrame(std::vector<uint32_t>& framebuffer, int width, int height, int activeTop, int activeBottom, int scale, const uint32_t* sourceFramebuffer, const ChrRenderSnapshot& snapshot, const std::vector<const ChrOverride*>* activeOverrideFilter = nullptr, bool applyModLogic = true);

    bool active() const { return m_active; }
    bool hasSelectedSource() const { return !m_modPath.empty(); }
    const std::filesystem::path& modPath() const { return m_modPath; }
    int resolutionMultiplier() const { return m_resolutionMultiplier; }
    const OverscanConfig& overscanConfig() const { return m_overscanConfig; }
    const std::vector<ChrOverride>& chrOverrides() const { return m_chrOverrides; }
    const std::vector<BackgroundReplacement>& backgroundReplacements() const { return m_backgroundReplacements; }
    std::optional<std::vector<uint8_t>> readAsset(const std::string& assetPath) const;
    std::optional<DebugComposePixel> debugComposePixel(const uint32_t* sourceFramebuffer, const ChrRenderSnapshot& snapshot, int scale, int nesX, int nesY, const std::string& filterText = "");
    std::shared_ptr<IAudioOutput::ExternalAudioMixer> externalAudioMixer() const;
    bool handleHdAudioCpuWrite(uint16_t addr, uint8_t value);
    std::optional<uint8_t> handleHdAudioCpuRead(uint16_t addr) const;

private:
    mutable std::recursive_mutex m_runtimeMutex;
    std::filesystem::path m_originalRomPath;
    std::filesystem::path m_effectiveRomPath;
    std::filesystem::path m_modPath;
    std::string m_modArchiveRoot;
    bool m_active = false;
    bool m_scriptLoaded = false;
    int m_resolutionMultiplier = 1;
    bool m_disableOriginalTiles = false;
    bool m_automaticFallbackTiles = false;
    OverscanConfig m_overscanConfig;
    std::vector<ChrOverride> m_chrOverrides;
    std::vector<BackgroundReplacement> m_backgroundReplacements;
    std::vector<AdditionalSpriteRule> m_additionalSpriteRules;
    std::vector<FallbackTileRule> m_fallbackTileRules;
    std::vector<uint32_t> m_chrRomTileHashes;
    std::unordered_map<uint32_t, int> m_chrRomCanonicalTileByHash;
    std::string m_patchAssetPath;
    std::string m_patchExpectedRomHash;
    HdPackAudioConfig m_hdAudioConfig;
    std::shared_ptr<HdPackAudioRuntime> m_hdAudioRuntime;
    FrameConditionState m_frameConditionState;
    FrameConditionPlan m_frameConditionPlan;
    std::vector<uint8_t> m_frameConditionGroupMatchesScratch;
    uint32_t m_lastFrameConditionUpdate = UINT32_MAX;
    struct DecodedImage {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> rgba;
    };

    struct ImageCacheEntry {
        std::optional<DecodedImage> image;
        uint32_t lastUsedFrame = 0;
        bool pinned = false;
    };

    struct RenderPreparedOverride {
        const ChrOverride* override = nullptr;
        const DecodedImage* image = nullptr;
        const uint32_t* rgbaData = nullptr;
        int sourceScale = 1;
        int imageWidth = 0;
        int imageHeight = 0;
        int sourceTileOffset = 0;
        int sourceColumns = 0;
        int fixedTileSrcX = 0;
        int fixedTileSrcY = 0;
        ChrOverride::SourceLayout wholeChrLayout = ChrOverride::SourceLayout::PatternTables;
        bool hasDynamicConditions = false;
        bool tileOriginCacheable = true;
        bool ignorePalette = false;
        bool wholeChr = false;
        bool fixedTileSourceValid = false;
        uint8_t targetMask = 0;
        uint8_t patternTableMask = 0;
        uint8_t requirementMask = 0;
        std::vector<const MemoryCondition*> runtimeConditions;
        size_t sequence = 0;
    };

    struct RenderPreparedBackground {
        const BackgroundReplacement* replacement = nullptr;
        const DecodedImage* image = nullptr;
        int priority = 0;
        int backgroundScale = 1;
        int alphaScale = 255;
        bool opaqueCopy = false;
        std::vector<const MemoryCondition*> runtimeConditions;
    };

    struct RenderComposeCache {
        struct AdditionalSpriteRuleSpan {
            size_t offset = 0;
            size_t count = 0;
        };

        bool valid = false;
        int scale = 1;
        bool needsTileHashes = false;
        std::vector<const ChrOverride*> activeOverrides;
        std::vector<RenderPreparedOverride> preparedOverrides;
        std::array<std::vector<const RenderPreparedOverride*>, 512> overridesByFullTile;
        std::unordered_map<int, std::vector<const RenderPreparedOverride*>> overridesByOverflowFullTile;
        std::array<std::vector<const RenderPreparedOverride*>, 256> overridesByRelativeTile;
        std::array<std::vector<const RenderPreparedOverride*>, 512> dynamicOverridesByFullTile;
        std::unordered_map<int, std::vector<const RenderPreparedOverride*>> dynamicOverridesByOverflowFullTile;
        std::array<std::vector<const RenderPreparedOverride*>, 256> dynamicOverridesByRelativeTile;
        std::unordered_map<uint32_t, std::vector<const RenderPreparedOverride*>> overridesByChrHash;
        std::unordered_map<uint32_t, std::vector<const RenderPreparedOverride*>> dynamicOverridesByChrHash;
        std::vector<const RenderPreparedOverride*> wholeChrOverrides;
        std::vector<const RenderPreparedOverride*> dynamicWholeChrOverrides;
        std::array<bool, 512> hasStaticOverridesByFullTile = {};
        std::unordered_set<int> staticOverflowFullTiles;
        std::array<bool, 256> hasStaticOverridesByRelativeTile = {};
        std::unordered_set<uint32_t> staticOverrideHashes;
        bool hasWholeChrOverrides = false;
        std::array<bool, 512> hasDefaultOverridesByFullTile = {};
        std::unordered_set<int> defaultOverflowFullTiles;
        std::array<bool, 256> hasDefaultOverridesByRelativeTile = {};
        std::unordered_set<uint32_t> defaultOverrideHashes;
        bool hasWholeChrDefaultOverrides = false;
        std::array<bool, 512> hasDynamicOverridesByFullTile = {};
        std::unordered_set<int> dynamicOverflowFullTiles;
        std::array<bool, 256> hasDynamicOverridesByRelativeTile = {};
        std::unordered_set<uint32_t> dynamicOverrideHashes;
        std::vector<RenderPreparedBackground> preparedBackgrounds;
        std::unordered_map<uint64_t, AdditionalSpriteRuleSpan> additionalSpriteRulesByExactKey;
        std::unordered_map<int, AdditionalSpriteRuleSpan> additionalSpriteRulesByOriginalTile;
        std::vector<const AdditionalSpriteRule*> additionalSpriteRuleStorage;
        bool hasAdditionalSpriteRules = false;
        bool onlyWholeChrOverrides = false;
        const RenderPreparedOverride* fastBackgroundOverride = nullptr;
    };

    std::unordered_map<std::string, ImageCacheEntry> m_imageCache;
    std::unordered_map<std::string, std::string> m_zipEntryLookup;
    std::vector<PPU::DebugModSpritePixel> m_augmentedSpritePixelsScratch;
    std::vector<const AdditionalSpriteRule*> m_resolvedAdditionalSpriteRulesScratch;
    std::unordered_map<uint64_t, RenderComposeCache::AdditionalSpriteRuleSpan> m_resolvedAdditionalSpriteRuleMemo;
    RenderComposeCache m_renderComposeCache;
    bool m_renderComposeCacheDirty = true;

    static std::string normalizeZipPath(std::string path);
    static std::optional<std::string> findZipEntryRootForFile(const std::filesystem::path& zipPath, const std::string& entryName);
    static std::optional<std::filesystem::path> resolveFolderEntryPath(const std::filesystem::path& rootPath, const std::string& entryName);
    static std::optional<std::vector<uint8_t>> readFileEntry(const std::filesystem::path& rootPath, const std::string& entryName);
    std::string resolveSourceEntryName(const std::string& entryName) const;
    std::string resolveZipEntryName(const std::string& entryName) const;
    bool rebuildZipEntryLookup();
    bool isFolderSource() const;
    bool sourceHasEntry(const std::string& entryName) const;
    std::optional<std::vector<uint8_t>> readSourceEntry(const std::string& entryName) const;
    static std::optional<std::vector<uint8_t>> readZipEntry(const std::filesystem::path& zipPath, const std::string& entryName);
    static bool writeBinaryFile(const std::filesystem::path& path, const std::vector<uint8_t>& data, std::string& error);
    static std::optional<std::vector<uint8_t>> applyIpsPatch(
        const std::vector<uint8_t>& romData,
        const std::vector<uint8_t>& patchData,
        std::string& error);
    static std::string sha1Hex(const std::vector<uint8_t>& data);
    static uint64_t makeMemoryCacheKey(MemoryCondition::MemorySource source, uint32_t address, bool word, int scale);
    void updateFrameConditionsForFrame(GeraNESEmu& emu);
    void preloadStartupAssets();
    void pinDecodedImage(const std::string& assetPath);
    void evictUnusedDynamicAssets(uint32_t frameCount);

    uint8_t readMemory(GeraNESEmu* emu, MemoryCondition::MemorySource source, uint32_t address) const;
    uint32_t readMemoryValue(const MemoryCondition& source, GeraNESEmu& emu) const;
    bool conditionsMatch(const std::vector<MemoryCondition>& conditions, GeraNESEmu& emu) const;
    bool conditionMatches(const MemoryCondition& condition, GeraNESEmu& emu) const;
    const DecodedImage* decodedImage(const std::string& assetPath);
    static std::optional<DecodedImage> decodeImage(const std::vector<uint8_t>& data);
    bool loadMesenHiresFile();
    void rebuildFrameConditionPlan();
    void invalidateRenderComposeCache();
    void populateOverrideLookupCache(RenderComposeCache& cache, const std::vector<const ChrOverride*>& activeOverrides, bool trackTileHashNeeds);
    void rebuildRenderComposeCache();
    RenderComposeCache buildFilteredRenderComposeCache(const std::vector<const ChrOverride*>& activeOverrideFilter);
    static uint32_t blendPixel(uint32_t dst, uint32_t src, int alphaScale);
    static uint32_t hashChrTile(PPU& ppu, int tileIndex);
};
