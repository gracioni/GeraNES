#include "GeraNESApp/ModManager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sys/stat.h>
#include <sstream>
#include <unordered_set>

#include "GeraNES/RomFile.h"
#include "GeraNES/NesCartridgeData/_INesFormat.h"
#include "GeraNESApp/AppSettings.h"
#include "logger/logger.h"
#include "stb_image.h"
#include "zip/zip.h"

#define GERANES_MODMANAGER_PROFILE 0

namespace {
using mz_ulong = unsigned long;
extern "C" int mz_uncompress(unsigned char* pDest, mz_ulong* pDest_len, const unsigned char* pSource, mz_ulong source_len);
constexpr int MZ_OK = 0;
constexpr uint8_t kPngSignature[8] = { 0x89u, 'P', 'N', 'G', '\r', '\n', 0x1Au, '\n' };

void logModMessage(const std::string& message, Logger::Type type)
{
    Logger::instance().log("[MOD] " + message, type);
}

bool pathExists(const std::filesystem::path& path)
{
    struct stat st = {};
    return stat(path.string().c_str(), &st) == 0;
}

bool pathIsDirectory(const std::filesystem::path& path)
{
    struct stat st = {};
    if(stat(path.string().c_str(), &st) != 0) {
        return false;
    }
    return (st.st_mode & S_IFMT) == S_IFDIR;
}

bool pathIsRegularFile(const std::filesystem::path& path)
{
    struct stat st = {};
    if(stat(path.string().c_str(), &st) != 0) {
        return false;
    }
    return (st.st_mode & S_IFMT) == S_IFREG;
}

#if GERANES_MODMANAGER_PROFILE
enum class ModManagerProfileSection : size_t {
    ComposeFrameOnEmuThread,
    ComposeFrameSnapshotCapture,
    UpdateFrameConditions,
    RebuildRenderComposeCache,
    ComposeChrFrame,
    ComposeChrFrameSetup,
    ComposeChrFrameAdditionalSpriteAugment,
    ComposeChrFrameBackgroundPrep,
    ComposeChrFrameMainLoop,
    ComposeChrFrameBackgroundOverride,
    ComposeChrFrameBackgroundOverrideSetup,
    ComposeChrFrameBackgroundOverrideCachedApply,
    ComposeChrFrameBackgroundOverrideFallbackSample,
    ComposeChrFrameSpriteResolve,
    ComposeChrFrameSpriteLayers,
    ComposeChrFrameFinalWrite,
    ComposeChrFrameFindOverride,
    ComposeChrFrameConditionMatches,
    Count
};

struct ModManagerProfileStats {
    static constexpr uint64_t ReportEveryFrames = 180;

    std::array<uint64_t, static_cast<size_t>(ModManagerProfileSection::Count)> totalNs = {};
    std::array<uint64_t, static_cast<size_t>(ModManagerProfileSection::Count)> maxNs = {};
    std::array<uint64_t, static_cast<size_t>(ModManagerProfileSection::Count)> calls = {};

    uint64_t frames = 0;
    uint64_t renderCacheRebuilds = 0;
    uint64_t frameConditionCacheChanges = 0;
    uint64_t frameConditionMemoryReads = 0;
    uint64_t preparedOverrideCount = 0;
    uint64_t preparedBackgroundCount = 0;
    uint64_t compiledAdditionalSpriteRuleCount = 0;
    uint64_t overrideFindCalls = 0;
    uint64_t overrideLookupCacheHits = 0;
    uint64_t tileOriginOverrideCacheHits = 0;
    uint64_t overrideLookupStores = 0;
    uint64_t overrideFastRejects = 0;
    uint64_t conditionMatchesCalls = 0;
    uint64_t matchesOverrideCalls = 0;
    uint64_t scanCandidatesCalls = 0;
    uint64_t scanMergedCandidatesCalls = 0;
    uint64_t additionalSpriteMemoHits = 0;
    uint64_t additionalSpriteMemoMisses = 0;
    uint64_t additionalSpriteResolvedRules = 0;
    uint64_t additionalSpriteSourceCandidates = 0;
    uint64_t syntheticSpriteCandidatesAdded = 0;

    void addDuration(ModManagerProfileSection section, uint64_t ns)
    {
        const size_t index = static_cast<size_t>(section);
        totalNs[index] += ns;
        maxNs[index] = std::max(maxNs[index], ns);
        calls[index] += 1;
    }

    static const char* sectionName(ModManagerProfileSection section)
    {
        switch(section) {
        case ModManagerProfileSection::ComposeFrameOnEmuThread: return "composeFrameOnEmuThread";
        case ModManagerProfileSection::ComposeFrameSnapshotCapture: return "snapshot capture";
        case ModManagerProfileSection::UpdateFrameConditions: return "updateFrameConditionsForFrame";
        case ModManagerProfileSection::RebuildRenderComposeCache: return "rebuildRenderComposeCache";
        case ModManagerProfileSection::ComposeChrFrame: return "composeChrFrame";
        case ModManagerProfileSection::ComposeChrFrameSetup: return "composeChrFrame setup";
        case ModManagerProfileSection::ComposeChrFrameAdditionalSpriteAugment: return "additional sprite augment";
        case ModManagerProfileSection::ComposeChrFrameBackgroundPrep: return "background prep";
        case ModManagerProfileSection::ComposeChrFrameMainLoop: return "compose main loop";
        case ModManagerProfileSection::ComposeChrFrameBackgroundOverride: return "background override";
        case ModManagerProfileSection::ComposeChrFrameBackgroundOverrideSetup: return "background override setup";
        case ModManagerProfileSection::ComposeChrFrameBackgroundOverrideCachedApply: return "background override cached apply";
        case ModManagerProfileSection::ComposeChrFrameBackgroundOverrideFallbackSample: return "background override fallback sample";
        case ModManagerProfileSection::ComposeChrFrameSpriteResolve: return "sprite resolve";
        case ModManagerProfileSection::ComposeChrFrameSpriteLayers: return "sprite layers";
        case ModManagerProfileSection::ComposeChrFrameFinalWrite: return "final block write";
        case ModManagerProfileSection::ComposeChrFrameFindOverride: return "findOverride";
        case ModManagerProfileSection::ComposeChrFrameConditionMatches: return "conditionMatchesAt";
        case ModManagerProfileSection::Count: break;
        }
        return "unknown";
    }

    static double nsToMs(uint64_t ns)
    {
        return static_cast<double>(ns) / 1000000.0;
    }

    static double nsToUs(uint64_t ns)
    {
        return static_cast<double>(ns) / 1000.0;
    }

    void resetWindow()
    {
        *this = {};
    }

    void logReport() const
    {
        std::ostringstream report;
        report << std::fixed << std::setprecision(3);
        report << "ModManager profile window: " << frames << " frames";

        const auto appendSection = [&](ModManagerProfileSection section, bool perFrame) {
            const size_t index = static_cast<size_t>(section);
            const uint64_t callCount = calls[index];
            if(callCount == 0) {
                return;
            }
            const double totalMs = nsToMs(totalNs[index]);
            const double avgMs = totalMs / static_cast<double>(callCount);
            report << "\n  " << sectionName(section)
                   << ": total=" << totalMs << " ms"
                   << ", calls=" << callCount
                   << ", avg=" << avgMs << " ms"
                   << ", max=" << nsToMs(maxNs[index]) << " ms";
            if(perFrame && frames > 0) {
                report << ", per-frame=" << (totalMs / static_cast<double>(frames)) << " ms";
            }
        };

        appendSection(ModManagerProfileSection::ComposeFrameOnEmuThread, true);
        appendSection(ModManagerProfileSection::ComposeFrameSnapshotCapture, true);
        appendSection(ModManagerProfileSection::UpdateFrameConditions, true);
        appendSection(ModManagerProfileSection::RebuildRenderComposeCache, false);
        appendSection(ModManagerProfileSection::ComposeChrFrame, true);
        appendSection(ModManagerProfileSection::ComposeChrFrameSetup, true);
        appendSection(ModManagerProfileSection::ComposeChrFrameAdditionalSpriteAugment, true);
        appendSection(ModManagerProfileSection::ComposeChrFrameBackgroundPrep, true);
        appendSection(ModManagerProfileSection::ComposeChrFrameMainLoop, true);
        appendSection(ModManagerProfileSection::ComposeChrFrameBackgroundOverride, true);
        appendSection(ModManagerProfileSection::ComposeChrFrameBackgroundOverrideSetup, true);
        appendSection(ModManagerProfileSection::ComposeChrFrameBackgroundOverrideCachedApply, true);
        appendSection(ModManagerProfileSection::ComposeChrFrameBackgroundOverrideFallbackSample, true);
        appendSection(ModManagerProfileSection::ComposeChrFrameSpriteResolve, true);
        appendSection(ModManagerProfileSection::ComposeChrFrameSpriteLayers, true);
        appendSection(ModManagerProfileSection::ComposeChrFrameFinalWrite, true);
        appendSection(ModManagerProfileSection::ComposeChrFrameFindOverride, false);
        appendSection(ModManagerProfileSection::ComposeChrFrameConditionMatches, false);

        if(overrideFindCalls > 0) {
            report << "\n  override lookup counters:"
                   << " calls=" << overrideFindCalls
                   << ", tile-origin-hits=" << tileOriginOverrideCacheHits
                   << ", cache-hits=" << overrideLookupCacheHits
                   << ", stores=" << overrideLookupStores
                   << ", fast-rejects=" << overrideFastRejects;
        }
        if(conditionMatchesCalls > 0 || matchesOverrideCalls > 0) {
            report << "\n  rule evaluation counters:"
                   << " conditionMatches=" << conditionMatchesCalls
                   << ", matchesOverride=" << matchesOverrideCalls
                   << ", scanCandidates=" << scanCandidatesCalls
                   << ", scanMergedCandidates=" << scanMergedCandidatesCalls;
        }
        if(additionalSpriteSourceCandidates > 0 || syntheticSpriteCandidatesAdded > 0) {
            report << "\n  additional sprite counters:"
                   << " source-candidates=" << additionalSpriteSourceCandidates
                   << ", memo-hits=" << additionalSpriteMemoHits
                   << ", memo-misses=" << additionalSpriteMemoMisses
                   << ", resolved-rules=" << additionalSpriteResolvedRules
                   << ", synthetic-added=" << syntheticSpriteCandidatesAdded;
        }
        report << "\n  cache/build counters:"
               << " render-cache-rebuilds=" << renderCacheRebuilds
               << ", frame-condition-cache-changes=" << frameConditionCacheChanges
               << ", frame-condition-memory-reads=" << frameConditionMemoryReads
               << ", prepared-overrides=" << preparedOverrideCount
               << ", prepared-backgrounds=" << preparedBackgroundCount
               << ", compiled-additional-sprite-rules=" << compiledAdditionalSpriteRuleCount;

        logModMessage(report.str(), Logger::Type::INFO);
    }

    void onFrameComplete()
    {
        ++frames;
        if(frames < ReportEveryFrames) {
            return;
        }
        logReport();
        resetWindow();
    }
};

using ModManagerProfileClock = std::chrono::steady_clock;

static ModManagerProfileStats g_modManagerProfile;

class ModManagerProfileScope {
public:
    explicit ModManagerProfileScope(ModManagerProfileSection section)
        : m_section(section), m_startedAt(ModManagerProfileClock::now())
    {
    }

    ~ModManagerProfileScope()
    {
        const uint64_t elapsedNs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                ModManagerProfileClock::now() - m_startedAt)
                .count());
        g_modManagerProfile.addDuration(m_section, elapsedNs);
    }

private:
    ModManagerProfileSection m_section;
    ModManagerProfileClock::time_point m_startedAt;
};

#define MODMANAGER_PROFILE_SCOPE_JOIN_IMPL(a, b) a##b
#define MODMANAGER_PROFILE_SCOPE_JOIN(a, b) MODMANAGER_PROFILE_SCOPE_JOIN_IMPL(a, b)
#define MODMANAGER_PROFILE_SCOPE(section) ModManagerProfileScope MODMANAGER_PROFILE_SCOPE_JOIN(modManagerProfileScope, __LINE__)(ModManagerProfileSection::section)
#define MODMANAGER_PROFILE_COUNT(field, delta) do { g_modManagerProfile.field += static_cast<uint64_t>(delta); } while(false)
#else
#define MODMANAGER_PROFILE_SCOPE(section) do { } while(false)
#define MODMANAGER_PROFILE_COUNT(field, delta) do { } while(false)
#endif


bool evaluateMemoryCondition(const ModManager::MemoryCondition& condition, uint32_t actual, uint32_t expected)
{
    if(condition.hasMask) {
        actual &= condition.mask;
        expected &= condition.mask;
    }

    bool match = false;
    switch(condition.compareOp) {
    case ModManager::MemoryCondition::CompareOp::AnyOf:
        for(uint32_t value : condition.values) {
            if(condition.hasMask) {
                value &= condition.mask;
            }
            if(actual == value) {
                match = true;
                break;
            }
        }
        break;
    case ModManager::MemoryCondition::CompareOp::NotEqual:
        match = actual != expected;
        break;
    case ModManager::MemoryCondition::CompareOp::Greater:
        match = actual > expected;
        break;
    case ModManager::MemoryCondition::CompareOp::GreaterOrEqual:
        match = actual >= expected;
        break;
    case ModManager::MemoryCondition::CompareOp::Less:
        match = actual < expected;
        break;
    case ModManager::MemoryCondition::CompareOp::LessOrEqual:
        match = actual <= expected;
        break;
    case ModManager::MemoryCondition::CompareOp::BitSet:
        match = (actual & expected) == expected;
        break;
    case ModManager::MemoryCondition::CompareOp::BitClear:
        match = (actual & expected) == 0;
        break;
    case ModManager::MemoryCondition::CompareOp::Equal:
    default:
        match = actual == expected;
        break;
    }

    return condition.inverted ? !match : match;
}

uint32_t readBigEndian32(const uint8_t* data)
{
    return (static_cast<uint32_t>(data[0]) << 24u) |
           (static_cast<uint32_t>(data[1]) << 16u) |
           (static_cast<uint32_t>(data[2]) << 8u) |
           static_cast<uint32_t>(data[3]);
}

uint8_t paethPredictor(uint8_t a, uint8_t b, uint8_t c)
{
    const int p = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
    const int pa = std::abs(p - static_cast<int>(a));
    const int pb = std::abs(p - static_cast<int>(b));
    const int pc = std::abs(p - static_cast<int>(c));
    if(pa <= pb && pa <= pc) return a;
    if(pb <= pc) return b;
    return c;
}

struct IndexedPngDecode {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> rgba;
    std::vector<uint8_t> indices;
};

std::optional<IndexedPngDecode> decodeIndexedPng4(const std::vector<uint8_t>& data)
{
    if(data.size() < sizeof(kPngSignature) || std::memcmp(data.data(), kPngSignature, sizeof(kPngSignature)) != 0) {
        return std::nullopt;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t bitDepth = 0;
    uint8_t colorType = 0xFFu;
    uint8_t compressionMethod = 0xFFu;
    uint8_t filterMethod = 0xFFu;
    uint8_t interlaceMethod = 0xFFu;
    std::vector<uint8_t> paletteBytes;
    std::vector<uint8_t> alphaBytes;
    std::vector<uint8_t> idat;

    size_t offset = sizeof(kPngSignature);
    while(offset + 12u <= data.size()) {
        const uint32_t chunkLength = readBigEndian32(data.data() + offset);
        offset += 4u;
        if(offset + 4u > data.size()) return std::nullopt;
        const char* type = reinterpret_cast<const char*>(data.data() + offset);
        offset += 4u;
        if(offset + static_cast<size_t>(chunkLength) + 4u > data.size()) return std::nullopt;
        const uint8_t* chunkData = data.data() + offset;
        offset += static_cast<size_t>(chunkLength);
        offset += 4u; // CRC

        if(std::memcmp(type, "IHDR", 4) == 0) {
            if(chunkLength != 13u) return std::nullopt;
            width = readBigEndian32(chunkData + 0);
            height = readBigEndian32(chunkData + 4);
            bitDepth = chunkData[8];
            colorType = chunkData[9];
            compressionMethod = chunkData[10];
            filterMethod = chunkData[11];
            interlaceMethod = chunkData[12];
        } else if(std::memcmp(type, "PLTE", 4) == 0) {
            paletteBytes.assign(chunkData, chunkData + chunkLength);
        } else if(std::memcmp(type, "tRNS", 4) == 0) {
            alphaBytes.assign(chunkData, chunkData + chunkLength);
        } else if(std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), chunkData, chunkData + chunkLength);
        } else if(std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
    }

    if(width == 0 || height == 0 || colorType != 3u || compressionMethod != 0u || filterMethod != 0u || interlaceMethod != 0u) {
        return std::nullopt;
    }
    if(bitDepth != 1u && bitDepth != 2u && bitDepth != 4u && bitDepth != 8u) {
        return std::nullopt;
    }
    if(paletteBytes.empty() || (paletteBytes.size() % 3u) != 0u || idat.empty()) {
        return std::nullopt;
    }

    const size_t paletteColorCount = paletteBytes.size() / 3u;
    if(paletteColorCount != 4u) {
        return std::nullopt;
    }

    const size_t packedRowBytes = (static_cast<size_t>(width) * static_cast<size_t>(bitDepth) + 7u) / 8u;
    const size_t expectedInflatedSize = static_cast<size_t>(height) * (1u + packedRowBytes);
    std::vector<uint8_t> inflated(expectedInflatedSize);
    mz_ulong inflatedSize = static_cast<mz_ulong>(inflated.size());
    if(mz_uncompress(inflated.data(), &inflatedSize, idat.data(), static_cast<mz_ulong>(idat.size())) != MZ_OK ||
       inflatedSize != static_cast<mz_ulong>(expectedInflatedSize)) {
        return std::nullopt;
    }

    IndexedPngDecode decoded;
    decoded.width = static_cast<int>(width);
    decoded.height = static_cast<int>(height);
    decoded.rgba.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
    decoded.indices.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

    std::vector<uint8_t> previousRow(packedRowBytes, 0u);
    std::vector<uint8_t> currentRow(packedRowBytes, 0u);
    size_t srcOffset = 0;
    for(uint32_t y = 0; y < height; ++y) {
        const uint8_t filterType = inflated[srcOffset++];
        if(srcOffset + packedRowBytes > inflated.size()) {
            return std::nullopt;
        }
        std::copy_n(inflated.data() + srcOffset, packedRowBytes, currentRow.begin());
        srcOffset += packedRowBytes;

        for(size_t x = 0; x < packedRowBytes; ++x) {
            const uint8_t left = x > 0 ? currentRow[x - 1] : 0u;
            const uint8_t up = previousRow[x];
            const uint8_t upLeft = x > 0 ? previousRow[x - 1] : 0u;
            switch(filterType) {
            case 0: break;
            case 1: currentRow[x] = static_cast<uint8_t>(currentRow[x] + left); break;
            case 2: currentRow[x] = static_cast<uint8_t>(currentRow[x] + up); break;
            case 3: currentRow[x] = static_cast<uint8_t>(currentRow[x] + static_cast<uint8_t>((static_cast<int>(left) + static_cast<int>(up)) / 2)); break;
            case 4: currentRow[x] = static_cast<uint8_t>(currentRow[x] + paethPredictor(left, up, upLeft)); break;
            default: return std::nullopt;
            }
        }

        for(uint32_t x = 0; x < width; ++x) {
            uint8_t paletteIndex = 0;
            switch(bitDepth) {
            case 1: {
                const uint8_t byte = currentRow[x >> 3u];
                paletteIndex = static_cast<uint8_t>((byte >> (7u - (x & 7u))) & 0x01u);
                break;
            }
            case 2: {
                const uint8_t byte = currentRow[x >> 2u];
                const uint8_t shift = static_cast<uint8_t>((3u - (x & 3u)) * 2u);
                paletteIndex = static_cast<uint8_t>((byte >> shift) & 0x03u);
                break;
            }
            case 4: {
                const uint8_t byte = currentRow[x >> 1u];
                paletteIndex = static_cast<uint8_t>(((x & 1u) == 0u) ? ((byte >> 4u) & 0x0Fu) : (byte & 0x0Fu));
                break;
            }
            case 8:
                paletteIndex = currentRow[x];
                break;
            default:
                return std::nullopt;
            }
            if(paletteIndex >= paletteColorCount) {
                return std::nullopt;
            }
            const size_t dstIndex = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
            const size_t paletteOffset = static_cast<size_t>(paletteIndex) * 3u;
            const uint8_t alpha = paletteIndex < alphaBytes.size() ? alphaBytes[paletteIndex] : 0xFFu;
            decoded.indices[dstIndex] = paletteIndex;
            decoded.rgba[dstIndex] =
                (static_cast<uint32_t>(alpha) << 24u) |
                static_cast<uint32_t>(paletteBytes[paletteOffset + 0u]) |
                (static_cast<uint32_t>(paletteBytes[paletteOffset + 1u]) << 8u) |
                (static_cast<uint32_t>(paletteBytes[paletteOffset + 2u]) << 16u);
        }

        previousRow.swap(currentRow);
    }

    return decoded;
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

ModManager::MemoryCondition::MemorySource parseMemorySourceName(const std::string& type)
{
    const std::string normalizedType = toLower(type);
    if(normalizedType == "cpu") return ModManager::MemoryCondition::MemorySource::Cpu;
    if(normalizedType == "ppu") return ModManager::MemoryCondition::MemorySource::Ppu;
    if(normalizedType == "oam") return ModManager::MemoryCondition::MemorySource::Oam;
    if(normalizedType == "primary_oam") return ModManager::MemoryCondition::MemorySource::PrimaryOam;
    if(normalizedType == "secondary_oam") return ModManager::MemoryCondition::MemorySource::SecondaryOam;
    return ModManager::MemoryCondition::MemorySource::Unknown;
}

ModManager::MemoryCondition::CompareOp parseCompareOpName(const std::string& op)
{
    const std::string normalizedOp = toLower(op);
    if(normalizedOp == "in" || normalizedOp == "any_of" || normalizedOp == "anyof") {
        return ModManager::MemoryCondition::CompareOp::AnyOf;
    }
    if(normalizedOp == "!=" || normalizedOp == "~=" || normalizedOp == "not_equal" || normalizedOp == "not_equals") {
        return ModManager::MemoryCondition::CompareOp::NotEqual;
    }
    if(normalizedOp == ">" || normalizedOp == "greater_than" || normalizedOp == "greater") {
        return ModManager::MemoryCondition::CompareOp::Greater;
    }
    if(normalizedOp == ">=" || normalizedOp == "greater_or_equal" || normalizedOp == "greater_equals") {
        return ModManager::MemoryCondition::CompareOp::GreaterOrEqual;
    }
    if(normalizedOp == "<" || normalizedOp == "less_than" || normalizedOp == "less") {
        return ModManager::MemoryCondition::CompareOp::Less;
    }
    if(normalizedOp == "<=" || normalizedOp == "less_or_equal" || normalizedOp == "less_equals") {
        return ModManager::MemoryCondition::CompareOp::LessOrEqual;
    }
    if(normalizedOp == "bit_set" || normalizedOp == "bits_set") {
        return ModManager::MemoryCondition::CompareOp::BitSet;
    }
    if(normalizedOp == "bit_clear" || normalizedOp == "bits_clear") {
        return ModManager::MemoryCondition::CompareOp::BitClear;
    }
    return ModManager::MemoryCondition::CompareOp::Equal;
}

std::string trimMesenToken(std::string value)
{
    auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) { return !isSpace(static_cast<unsigned char>(ch)); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) { return !isSpace(static_cast<unsigned char>(ch)); }).base(), value.end());
    return value;
}

std::vector<std::string> splitComma(const std::string& value)
{
    std::vector<std::string> result;
    std::string token;
    std::stringstream stream(value);
    while(std::getline(stream, token, ',')) {
        result.push_back(trimMesenToken(token));
    }
    return result;
}

uint32_t parseHexValue(const std::string& text, uint32_t fallback = 0)
{
    if(text.empty()) return fallback;
    errno = 0;
    char* end = nullptr;
    const unsigned long value = std::strtoul(text.c_str(), &end, 16);
    if(errno != 0 || end != text.c_str() + text.size()) {
        return fallback;
    }
    return static_cast<uint32_t>(value);
}

uint32_t parseDecOrHexValue(const std::string& text, uint32_t fallback = 0)
{
    if(text.empty()) return fallback;
    const bool looksHex = text.find_first_of("ABCDEFabcdef") != std::string::npos;
    errno = 0;
    char* end = nullptr;
    const unsigned long value = std::strtoul(text.c_str(), &end, looksHex ? 16 : 10);
    if(errno != 0 || end != text.c_str() + text.size()) {
        return fallback;
    }
    return static_cast<uint32_t>(value);
}

std::optional<int> parseIntStrict(const std::string& text, int base = 10)
{
    if(text.empty()) {
        return std::nullopt;
    }

    errno = 0;
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, base);
    if(errno != 0 || end != text.c_str() + text.size()) {
        return std::nullopt;
    }
    if(value < static_cast<long>(std::numeric_limits<int>::min()) ||
       value > static_cast<long>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

bool parseMesenBool(const std::string& text)
{
    const std::string value = toLower(trimMesenToken(text));
    return value == "y" || value == "yes" || value == "true" || value == "1";
}

uint32_t hashTileBytes(const std::string& tileData)
{
    uint8_t bytes[16] = {};
    for(size_t i = 0; i + 1 < tileData.size() && i < 32; i += 2) {
        bytes[i / 2] = static_cast<uint8_t>(parseHexValue(tileData.substr(i, 2)));
    }

    uint32_t hash = 0;
    for(size_t i = 0; i < std::size(bytes); i += sizeof(uint32_t)) {
        uint32_t chunk = 0;
        std::memcpy(&chunk, bytes + i, sizeof(uint32_t));
        hash += chunk;
        hash = (hash << 2) | (hash >> 30);
    }
    return hash;
}

uint32_t hashTileBytes(const uint8_t* bytes)
{
    uint32_t hash = 0;
    for(size_t i = 0; i < 16; i += sizeof(uint32_t)) {
        uint32_t chunk = 0;
        std::memcpy(&chunk, bytes + i, sizeof(uint32_t));
        hash += chunk;
        hash = (hash << 2) | (hash >> 30);
    }
    return hash;
}

std::vector<uint32_t> buildChrRomTileHashes(const std::filesystem::path& romPath)
{
    std::vector<uint32_t> hashes;
    if(romPath.empty()) {
        return hashes;
    }

    RomFile rom;
    if(!rom.open(romPath.string())) {
        return hashes;
    }

    _INesFormat iNes(rom);
    if(!iNes.valid() || iNes.chrSize() <= 0) {
        return hashes;
    }

    const int tileCount = iNes.chrSize() / 16;
    hashes.resize(static_cast<size_t>(tileCount), 0u);
    uint8_t bytes[16] = {};
    for(int tileIndex = 0; tileIndex < tileCount; ++tileIndex) {
        for(int offset = 0; offset < 16; ++offset) {
            bytes[offset] = iNes.readChr(tileIndex * 16 + offset);
        }
        hashes[static_cast<size_t>(tileIndex)] = hashTileBytes(bytes);
    }
    return hashes;
}

std::vector<uint8_t> parseMesenPalette(const std::string& paletteText)
{
    std::vector<uint8_t> palette;
    for(size_t i = 0; i + 1 < paletteText.size() && i < 8; i += 2) {
        const uint32_t value = parseHexValue(paletteText.substr(i, 2), 0xFF);
        if(value != 0xFF) {
            palette.push_back(static_cast<uint8_t>(value & 0x3F));
        }
    }
    if(palette.size() > 3) {
        palette.erase(palette.begin());
    }
    return palette;
}

std::optional<int> parseDecimalIntStrict(const std::string& text)
{
    return parseIntStrict(text, 10);
}

std::optional<int> parseSignedDecimalIntStrict(const std::string& text)
{
    return parseIntStrict(text, 10);
}

std::string safeCacheStem(const std::filesystem::path& path)
{
    std::string stem = path.stem().string();
    if(stem.empty()) stem = "modded-rom";
    for(char& ch : stem) {
        const bool keep =
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' ||
            ch == '_';
        if(!keep) ch = '_';
    }
    return stem;
}

struct Sha1State {
    uint32_t h0 = 0x67452301u;
    uint32_t h1 = 0xEFCDAB89u;
    uint32_t h2 = 0x98BADCFEu;
    uint32_t h3 = 0x10325476u;
    uint32_t h4 = 0xC3D2E1F0u;
};

uint32_t rotl32(uint32_t value, uint32_t amount)
{
    return (value << amount) | (value >> (32u - amount));
}

void sha1ProcessBlock(Sha1State& state, const uint8_t* block)
{
    uint32_t w[80] = {};
    for(int i = 0; i < 16; ++i) {
        w[i] =
            (static_cast<uint32_t>(block[i * 4 + 0]) << 24u) |
            (static_cast<uint32_t>(block[i * 4 + 1]) << 16u) |
            (static_cast<uint32_t>(block[i * 4 + 2]) << 8u) |
            static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for(int i = 16; i < 80; ++i) {
        w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1u);
    }

    uint32_t a = state.h0;
    uint32_t b = state.h1;
    uint32_t c = state.h2;
    uint32_t d = state.h3;
    uint32_t e = state.h4;
    for(int i = 0; i < 80; ++i) {
        uint32_t f = 0;
        uint32_t k = 0;
        if(i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999u;
        } else if(i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if(i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        const uint32_t temp = rotl32(a, 5u) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotl32(b, 30u);
        b = a;
        a = temp;
    }

    state.h0 += a;
    state.h1 += b;
    state.h2 += c;
    state.h3 += d;
    state.h4 += e;
}

}

void ModManager::clear()
{
    if(!m_hdAudioRuntime) {
        m_hdAudioRuntime = std::make_shared<HdPackAudioRuntime>(
            [this](const std::string& assetPath) { return readSourceEntry(assetPath); });
    }

    m_originalRomPath.clear();
    m_effectiveRomPath.clear();
    m_modPath.clear();
    m_modArchiveRoot.clear();
    m_zipEntryLookup.clear();
    m_active = false;
    m_scriptLoaded = false;
    m_resolutionMultiplier = 1;
    m_disableOriginalTiles = false;
    m_automaticFallbackTiles = false;
    m_overscanConfig = {};
    m_chrOverrides.clear();
    m_backgroundReplacements.clear();
    m_additionalSpriteRules.clear();
    m_fallbackTileRules.clear();
    m_chrRomTileHashes.clear();
    m_chrRomCanonicalTileByHash.clear();
    m_patchAssetPath.clear();
    m_patchExpectedRomHash.clear();
    m_hdAudioConfig = {};
    m_hdAudioRuntime->clearConfig();
    m_frameConditionState = {};
    m_frameConditionPlan = {};
    m_frameConditionGroupMatchesScratch.clear();
    m_lastFrameConditionUpdate = UINT32_MAX;
    m_imageCache.clear();
    m_augmentedSpritePixelsScratch.clear();
    m_resolvedAdditionalSpriteRulesScratch.clear();
    m_resolvedAdditionalSpriteRuleMemo.clear();
    invalidateRenderComposeCache();
}

void ModManager::invalidateRenderComposeCache()
{
    m_renderComposeCache = {};
    m_renderComposeCacheDirty = true;
}

bool ModManager::selectModSource(const std::filesystem::path& modSourcePath, std::string& error)
{
    clear();

    if(modSourcePath.empty()) {
        error = "Mod source path is empty.";
        return false;
    }

    const bool exists = pathExists(modSourcePath);
    if(!exists) {
        error = "Mod source does not exist.";
        return false;
    }

    const bool isDirectory = pathIsDirectory(modSourcePath);
    const bool isFile = pathIsRegularFile(modSourcePath);
    if(!isDirectory && !isFile) {
        error = "Mod source must be a folder or zip file.";
        return false;
    }
    if(isFile) {
        std::string extension = toLower(modSourcePath.extension().string());
        if(extension != ".zip") {
            error = "Mod file must be a .zip archive.";
            return false;
        }

        if(const auto archiveRoot = findZipEntryRootForFile(modSourcePath, "hires.txt"); archiveRoot.has_value()) {
            m_modArchiveRoot = *archiveRoot;
        }
    }

    m_modPath = modSourcePath;
    if(isFile && !rebuildZipEntryLookup()) {
        error = "Failed to index mod archive.";
        clear();
        return false;
    }
    m_active = true;
    return true;
}

void ModManager::clearModSource()
{
    clear();
}

void ModManager::onEmulatorReset()
{
    m_lastFrameConditionUpdate = UINT32_MAX;
    m_frameConditionState = {};
    m_frameConditionGroupMatchesScratch.clear();
    if(m_hdAudioRuntime) {
        m_hdAudioRuntime->resetRuntime();
    }
}

void ModManager::onStateLoaded(uint32_t frameCount)
{
    m_lastFrameConditionUpdate = UINT32_MAX;
    m_frameConditionState = {};
    m_frameConditionGroupMatchesScratch.clear();
    for(auto& [_, entry] : m_imageCache) {
        entry.lastUsedFrame = frameCount;
    }
    if(m_hdAudioRuntime) {
        m_hdAudioRuntime->resetRuntime();
        m_hdAudioRuntime->rebaseCacheFrame(frameCount);
    }
}

bool ModManager::composeFrameOnEmuThread(
    GeraNESEmu& emu,
    ChrRenderSnapshot& snapshot,
    std::vector<uint32_t>& framebuffer,
    int activeTop,
    int activeBottom,
    bool captureDebugSnapshot)
{
    MODMANAGER_PROFILE_SCOPE(ComposeFrameOnEmuThread);
    std::scoped_lock runtimeLock(m_runtimeMutex);
    auto resetSnapshotForReuse = [&](bool releaseViews) {
        snapshot.scrollX = 0;
        snapshot.scrollY = 0;
        snapshot.scrollXByLine.fill(0);
        snapshot.scrollYByLine.fill(0);
        snapshot.universalBgColor = 0;
        snapshot.paletteColors.fill(0);
        snapshot.tileHashes.fill(0);
        if(releaseViews) {
            snapshot.backgroundPixelsView = nullptr;
            snapshot.backgroundPixelsViewCount = 0;
            snapshot.spritePixelsView = nullptr;
            snapshot.spritePixelsViewCount = 0;
            snapshot.frameConditionStateView = nullptr;
        }
        snapshot.backgroundPixels.clear();
        snapshot.spritePixels.clear();
        snapshot.frameConditionState = {};
    };

    if(!m_active || !emu.valid()) {
        resetSnapshotForReuse(true);
        framebuffer.clear();
        return false;
    }

    updateFrameConditionsForFrame(emu);
    const int scale = std::clamp(m_resolutionMultiplier, 1, 8);
    const bool renderCacheNeedsRebuild =
        m_renderComposeCacheDirty ||
        !m_renderComposeCache.valid ||
        m_renderComposeCache.scale != scale;
    if(renderCacheNeedsRebuild) {
        rebuildRenderComposeCache();
    }

    const bool needsTileHashesForCompose = m_renderComposeCache.needsTileHashes;
    PPU& ppu = emu.getConsole().ppu();
    {
        MODMANAGER_PROFILE_SCOPE(ComposeFrameSnapshotCapture);
        resetSnapshotForReuse(false);
        for(int y = 0; y < PPU::SCREEN_HEIGHT; ++y) {
            snapshot.scrollXByLine[static_cast<size_t>(y)] = ppu.debugPresentedScanlineScrollX(y);
            snapshot.scrollYByLine[static_cast<size_t>(y)] = ppu.debugPresentedScanlineScrollY(y);
        }
        snapshot.scrollX = snapshot.scrollXByLine[0];
        snapshot.scrollY = snapshot.scrollYByLine[0];
        snapshot.universalBgColor = static_cast<uint8_t>(ppu.debugPeekPpuMemory(0x3F00) & 0x3F);
        if(captureDebugSnapshot) {
            ppu.debugCopyPresentedBackgroundPixels(snapshot.backgroundPixels);
            ppu.debugCopyPresentedSpritePixels(snapshot.spritePixels);
            snapshot.backgroundPixelsView = snapshot.backgroundPixels.data();
            snapshot.backgroundPixelsViewCount = snapshot.backgroundPixels.size();
            snapshot.spritePixelsView = snapshot.spritePixels.data();
            snapshot.spritePixelsViewCount = snapshot.spritePixels.size();
        } else {
            snapshot.backgroundPixels.clear();
            snapshot.spritePixels.clear();
            snapshot.backgroundPixelsView = ppu.debugPresentedBackgroundPixelsData();
            snapshot.backgroundPixelsViewCount = ppu.debugPresentedBackgroundPixelsCount();
            snapshot.spritePixelsView = ppu.debugPresentedSpritePixelsData();
            snapshot.spritePixelsViewCount = ppu.debugPresentedSpritePixelsCount();
        }
        snapshot.frameConditionStateView = &m_frameConditionState;
        for(size_t i = 0; i < snapshot.paletteColors.size(); ++i) {
            snapshot.paletteColors[i] = ppu.NESToRGBAColor(static_cast<uint8_t>(i));
        }
        if(needsTileHashesForCompose) {
            for(size_t i = 0; i < snapshot.tileHashes.size(); ++i) {
                snapshot.tileHashes[i] = ppu.debugHashChrTile(static_cast<int>(i));
            }
        }
    }

    const int width = PPU::SCREEN_WIDTH * scale;
    const int height = 256 * scale;
    const int activeTopScaled = std::clamp(activeTop, 0, PPU::SCREEN_HEIGHT) * scale;
    const int activeBottomScaled = std::clamp(activeBottom, 0, PPU::SCREEN_HEIGHT) * scale;
    if(framebuffer.size() != static_cast<size_t>(width * height)) {
        framebuffer.assign(static_cast<size_t>(width * height), 0u);
    }

    composeChrFrame(
        framebuffer,
        width,
        height,
        activeTopScaled,
        activeBottomScaled,
        scale,
        emu.getFramebuffer(),
        snapshot
    );
    if(captureDebugSnapshot) {
        if(!needsTileHashesForCompose) {
            PPU& ppu = emu.getConsole().ppu();
            for(size_t i = 0; i < snapshot.tileHashes.size(); ++i) {
                snapshot.tileHashes[i] = ppu.debugHashChrTile(static_cast<int>(i));
            }
        }
        snapshot.frameConditionState = m_frameConditionState;
    }
#if GERANES_MODMANAGER_PROFILE
    g_modManagerProfile.onFrameComplete();
#endif
    return true;
}

std::shared_ptr<IAudioOutput::ExternalAudioMixer> ModManager::externalAudioMixer() const
{
    if(!m_hdAudioRuntime) {
        auto* self = const_cast<ModManager*>(this);
        self->m_hdAudioRuntime = std::make_shared<HdPackAudioRuntime>(
            [self](const std::string& assetPath) { return self->readSourceEntry(assetPath); });
    }
    return m_hdAudioRuntime;
}

bool ModManager::handleHdAudioCpuWrite(uint16_t addr, uint8_t value)
{
    return m_hdAudioRuntime ? m_hdAudioRuntime->handleCpuWrite(addr, value) : false;
}

std::optional<uint8_t> ModManager::handleHdAudioCpuRead(uint16_t addr) const
{
    return m_hdAudioRuntime ? m_hdAudioRuntime->handleCpuRead(addr) : std::nullopt;
}

ModManager::LoadRequest ModManager::prepareRomLoad(const std::filesystem::path& romPath)
{
    LoadRequest request;
    request.romPath = romPath;
    request.effectiveRomPath = romPath;

    m_originalRomPath = romPath;
    m_effectiveRomPath = romPath;

    if(m_modPath.empty()) {
        m_active = false;
        request.message = "No mod selected.";
        return request;
    }

    m_active = true;
    request.modPath = m_modPath;
    request.modLoaded = true;

    m_patchAssetPath.clear();
    m_patchExpectedRomHash.clear();
    const auto hiresData = readSourceEntry("hires.txt");
    if(!hiresData.has_value()) {
        request.message = "Mod selected.";
        return request;
    }

    const std::string hiresText(reinterpret_cast<const char*>(hiresData->data()), hiresData->size());
    std::stringstream hiresLines(hiresText);
    std::string hiresLine;
    while(std::getline(hiresLines, hiresLine)) {
        hiresLine = trimMesenToken(hiresLine);
        if(hiresLine.rfind("<patch>", 0) != 0) {
            continue;
        }
        const std::vector<std::string> tokens = splitComma(hiresLine.substr(7));
        if(tokens.empty()) {
            continue;
        }
        m_patchAssetPath = normalizeZipPath(tokens[0]);
        if(tokens.size() >= 2) {
            m_patchExpectedRomHash = toLower(tokens[1]);
        }
        break;
    }

    if(m_patchAssetPath.empty()) {
        request.message = "Mod selected.";
        return request;
    }

    const auto ipsData = readSourceEntry(m_patchAssetPath);
    if(!ipsData.has_value()) {
        request.message = "Mod selected, but patch file is missing: " + m_patchAssetPath;
        logModMessage(request.message, Logger::Type::ERROR);
        return request;
    }

    RomFile baseRom;
    if(!baseRom.open(romPath.string()) || !baseRom.error().empty()) {
        request.message = "Mod selected, but base ROM could not be read for Mesen patch.";
        logModMessage(request.message, Logger::Type::ERROR);
        return request;
    }

    if(!m_patchExpectedRomHash.empty()) {
        const std::string actualHash = toLower(sha1Hex(baseRom.dataBytes()));
        if(actualHash != m_patchExpectedRomHash) {
            request.message = "Mod selected, but ROM hash does not match hires.txt patch.";
            logModMessage(request.message, Logger::Type::ERROR);
            return request;
        }
    }

    std::string patchError;
    auto patchedRom = applyIpsPatch(baseRom.dataBytes(), *ipsData, patchError);
    if(!patchedRom.has_value()) {
        request.message = "Mod selected, but hires.txt patch failed: " + patchError;
        logModMessage(request.message, Logger::Type::ERROR);
        return request;
    }

    const std::filesystem::path cacheDir = AppSettings::storageDirectory() / "mod-cache";
    const std::filesystem::path patchedPath = cacheDir / (safeCacheStem(romPath) + ".modded.nes");
    std::string writeError;
    if(!writeBinaryFile(patchedPath, *patchedRom, writeError)) {
        request.message = "Mod found, but patched ROM could not be cached: " + writeError;
        logModMessage(request.message, Logger::Type::ERROR);
        return request;
    }

    request.effectiveRomPath = patchedPath;
    request.ipsApplied = true;
    request.message = "Mod selected with hires.txt patch.";
    m_effectiveRomPath = patchedPath;
    return request;
}

bool ModManager::loadDefinitionForCurrentMod()
{
    if(!m_active || m_modPath.empty()) return false;
    m_scriptLoaded = false;
    m_chrOverrides.clear();
    m_backgroundReplacements.clear();
    m_frameConditionPlan = {};
    m_lastFrameConditionUpdate = UINT32_MAX;
    m_imageCache.clear();
    invalidateRenderComposeCache();

    if(!loadMesenHiresFile()) {
        return false;
    }

    std::unordered_map<std::string, bool> chrAssetValidity;
    for(ChrOverride& override : m_chrOverrides) {
        if(!override.assetAvailable || override.assetPath.empty()) {
            continue;
        }
        if(override.ignorePalette) {
            if(decodedImage(override.assetPath) == nullptr) {
                logModMessage(
                    "Failed to load CHR image asset: " + override.assetPath,
                    Logger::Type::WARNING);
                override.assetAvailable = false;
            }
            continue;
        }
        const std::string normalizedPath = normalizeZipPath(override.assetPath);
        auto it = chrAssetValidity.find(normalizedPath);
        if(it == chrAssetValidity.end()) {
            const DecodedImage* image = decodedImage(normalizedPath);
            const bool valid = image != nullptr && image->indexedFourColor;
            if(!valid) {
                logModMessage(
                    "CHR sheet must be an indexed PNG with exactly 4 colors: " + normalizedPath,
                    Logger::Type::WARNING);
            }
            it = chrAssetValidity.emplace(normalizedPath, valid).first;
        }
        if(!it->second) {
            override.assetAvailable = false;
        }
    }

    rebuildFrameConditionPlan();
    preloadStartupAssets();
    m_scriptLoaded = true;
    logModMessage("Mesen hires.txt loaded.", Logger::Type::INFO);
    return true;
}

void ModManager::preloadStartupAssets()
{
    constexpr uint32_t startupFrame = 0;
    std::unordered_set<std::string> imageAssets;

    auto addImageAsset = [&](const std::string& assetPath) {
        const std::string normalizedPath = normalizeZipPath(assetPath);
        if(!normalizedPath.empty()) {
            imageAssets.insert(normalizedPath);
        }
    };

    for(const ChrOverride& override : m_chrOverrides) {
        if(!override.assetAvailable || override.assetPath.empty()) {
            continue;
        }
        addImageAsset(override.assetPath);
    }

    for(const BackgroundReplacement& replacement : m_backgroundReplacements) {
        if(!replacement.assetAvailable || replacement.assetPath.empty()) {
            continue;
        }
        addImageAsset(replacement.assetPath);
    }

    for(const std::string& assetPath : imageAssets) {
        decodedImage(assetPath);
        pinDecodedImage(assetPath);
    }

    if(!m_hdAudioRuntime) {
        m_hdAudioRuntime = std::make_shared<HdPackAudioRuntime>(
            [this](const std::string& assetPath) { return readSourceEntry(assetPath); });
    }
    m_hdAudioRuntime->setConfig(m_hdAudioConfig);
    m_hdAudioRuntime->setCacheFrame(startupFrame);

    std::vector<std::string> audioAssets;
    audioAssets.reserve(m_hdAudioConfig.bgmFilesById.size() + m_hdAudioConfig.sfxFilesById.size());

    for(const auto& [_, track] : m_hdAudioConfig.bgmFilesById) {
        if(!track.assetPath.empty()) {
            audioAssets.push_back(normalizeZipPath(track.assetPath));
        }
    }

    for(const auto& [_, assetPath] : m_hdAudioConfig.sfxFilesById) {
        const std::string normalizedPath = normalizeZipPath(assetPath);
        if(!normalizedPath.empty()) {
            audioAssets.push_back(normalizedPath);
        }
    }

    std::sort(audioAssets.begin(), audioAssets.end());
    audioAssets.erase(std::unique(audioAssets.begin(), audioAssets.end()), audioAssets.end());
    for(const std::string& assetPath : audioAssets) {
        m_hdAudioRuntime->preloadClip(assetPath);
    }
}

bool ModManager::loadMesenHiresFile()
{
    const auto hiresData = readSourceEntry("hires.txt");
    if(!hiresData.has_value()) {
        logModMessage("Selected mod does not contain hires.txt.", Logger::Type::ERROR);
        return false;
    }

    const std::string text(reinterpret_cast<const char*>(hiresData->data()), hiresData->size());
    std::vector<std::string> images;
    std::unordered_map<std::string, std::vector<MemoryCondition>> namedConditions;
    int nextPriority = 1000000;
    int lineNumber = 0;
    m_disableOriginalTiles = false;
    m_overscanConfig = {};
    m_patchAssetPath.clear();
    m_patchExpectedRomHash.clear();
    m_hdAudioConfig = {};

    auto makeConditions = [&](const std::string& conditionText) {
        std::vector<MemoryCondition> conditions;
        std::stringstream stream(conditionText);
        std::string name;
        while(std::getline(stream, name, '&')) {
            name = trimMesenToken(name);
            if(name.empty()) continue;
            const bool inverted = !name.empty() && name.front() == '!';
            const std::string lookupName = inverted ? name.substr(1) : name;
            const std::string normalizedLookupName = toLower(lookupName);
            if(normalizedLookupName.rfind("sppalette", 0) == 0 && normalizedLookupName.size() == 10) {
                const char paletteChar = normalizedLookupName.back();
                if(paletteChar >= '0' && paletteChar <= '3') {
                    MemoryCondition condition;
                    condition.kind = MemoryCondition::Kind::SpritePaletteIndex;
                    condition.value = static_cast<uint32_t>(paletteChar - '0');
                    condition.inverted = inverted;
                    condition.debugName = lookupName;
                    conditions.push_back(std::move(condition));
                    continue;
                }
            }
            const auto it = namedConditions.find(lookupName);
            if(it != namedConditions.end()) {
                for(const MemoryCondition& source : it->second) {
                    MemoryCondition condition = source;
                    condition.inverted = condition.inverted ^ inverted;
                    if(condition.debugName.empty()) {
                        condition.debugName = lookupName;
                    }
                    conditions.push_back(std::move(condition));
                }
            }
        }
        return conditions;
    };

    std::stringstream lines(text);
    std::string line;
    while(std::getline(lines, line)) {
        ++lineNumber;
        line = trimMesenToken(line);
        if(line.empty() || line[0] == '#') continue;

        std::vector<MemoryCondition> ruleConditions;
        std::vector<std::string> ruleConditionNames;
        if(line.front() == '[') {
            const size_t end = line.find(']');
            if(end == std::string::npos) {
                logModMessage("Invalid Mesen condition prefix on line " + std::to_string(lineNumber), Logger::Type::ERROR);
                continue;
            }
            std::stringstream conditionNames(line.substr(1, end - 1));
            std::string conditionName;
            while(std::getline(conditionNames, conditionName, '&')) {
                ruleConditionNames.push_back(trimMesenToken(conditionName));
            }
            ruleConditions = makeConditions(line.substr(1, end - 1));
            line = trimMesenToken(line.substr(end + 1));
        }

        if(line.rfind("<ver>", 0) == 0) {
            continue;
        }
        if(line.rfind("<options>", 0) == 0) {
            const std::vector<std::string> options = splitComma(line.substr(9));
            for(const std::string& rawOption : options) {
                const std::string option = toLower(trimMesenToken(rawOption));
                if(option == "disableoriginaltiles") {
                    m_disableOriginalTiles = true;
                } else if(option == "automaticfallbacktiles") {
                    m_automaticFallbackTiles = true;
                } else if(option == "alternateregisterrange") {
                    m_hdAudioConfig.alternateRegisterRange = true;
                }
            }
            continue;
        }
        if(line.rfind("<overscan>", 0) == 0) {
            const std::vector<std::string> tokens = splitComma(line.substr(10));
            if(tokens.size() != 4) {
                logModMessage("Invalid Mesen <overscan> on line " + std::to_string(lineNumber), Logger::Type::ERROR);
                continue;
            }
            const std::optional<int> top = parseIntStrict(trimMesenToken(tokens[0]));
            const std::optional<int> right = parseIntStrict(trimMesenToken(tokens[1]));
            const std::optional<int> bottom = parseIntStrict(trimMesenToken(tokens[2]));
            const std::optional<int> left = parseIntStrict(trimMesenToken(tokens[3]));
            if(!top.has_value() || !right.has_value() || !bottom.has_value() || !left.has_value()) {
                logModMessage("Invalid Mesen <overscan> on line " + std::to_string(lineNumber), Logger::Type::ERROR);
                continue;
            }
            OverscanConfig overscan;
            overscan.enabled = true;
            overscan.top = std::clamp(*top, 0, PPU::SCREEN_HEIGHT);
            overscan.right = std::clamp(*right, 0, PPU::SCREEN_WIDTH);
            overscan.bottom = std::clamp(*bottom, 0, PPU::SCREEN_HEIGHT);
            overscan.left = std::clamp(*left, 0, PPU::SCREEN_WIDTH);
            if(overscan.top + overscan.bottom >= PPU::SCREEN_HEIGHT ||
               overscan.left + overscan.right >= PPU::SCREEN_WIDTH) {
                logModMessage("Invalid Mesen <overscan> dimensions on line " + std::to_string(lineNumber), Logger::Type::ERROR);
                continue;
            }
            m_overscanConfig = overscan;
            continue;
        }
        if(line.rfind("<patch>", 0) == 0) {
            const std::vector<std::string> tokens = splitComma(line.substr(7));
            if(!tokens.empty()) {
                m_patchAssetPath = normalizeZipPath(tokens[0]);
                if(tokens.size() >= 2) {
                    m_patchExpectedRomHash = toLower(tokens[1]);
                }
            }
            continue;
        }
        if(line.rfind("<scale>", 0) == 0) {
            const std::optional<int> scale = parseIntStrict(trimMesenToken(line.substr(7)));
            if(!scale.has_value()) {
                logModMessage("Invalid Mesen <scale> on line " + std::to_string(lineNumber), Logger::Type::ERROR);
            } else {
                m_resolutionMultiplier = std::clamp(*scale, 1, 8);
            }
            continue;
        }
        if(line.rfind("<img>", 0) == 0) {
            const std::string imagePath = normalizeZipPath(trimMesenToken(line.substr(5)));
            if(!sourceHasEntry(imagePath)) {
                logModMessage("Mesen image not found: " + imagePath, Logger::Type::WARNING);
            }
            images.push_back(imagePath);
            continue;
        }
        if(line.rfind("<bgm>", 0) == 0) {
            const std::vector<std::string> tokens = splitComma(line.substr(5));
            if(tokens.size() >= 3) {
                const int album = static_cast<int>(parseDecOrHexValue(tokens[0], 0) & 0xFFu);
                const int track = static_cast<int>(parseDecOrHexValue(tokens[1], 0) & 0xFFu);
                HdPackAudioBgmTrack bgmTrack;
                bgmTrack.assetPath = normalizeZipPath(tokens[2]);
                if(!sourceHasEntry(bgmTrack.assetPath)) {
                    logModMessage("Mesen BGM not found: " + bgmTrack.assetPath, Logger::Type::WARNING);
                }
                if(tokens.size() >= 4) {
                    bgmTrack.loopPosition = parseDecOrHexValue(tokens[3], 0);
                }
                m_hdAudioConfig.bgmFilesById[album * 256 + track] = std::move(bgmTrack);
            }
            continue;
        }
        if(line.rfind("<sfx>", 0) == 0) {
            const std::vector<std::string> tokens = splitComma(line.substr(5));
            if(tokens.size() >= 3) {
                const int album = static_cast<int>(parseDecOrHexValue(tokens[0], 0) & 0xFFu);
                const int track = static_cast<int>(parseDecOrHexValue(tokens[1], 0) & 0xFFu);
                const std::string assetPath = normalizeZipPath(tokens[2]);
                if(!sourceHasEntry(assetPath)) {
                    logModMessage("Mesen SFX not found: " + assetPath, Logger::Type::WARNING);
                }
                m_hdAudioConfig.sfxFilesById[album * 256 + track] = assetPath;
            }
            continue;
        }
        if(line.rfind("<condition>", 0) == 0) {
            const std::vector<std::string> tokens = splitComma(line.substr(11));
            if(tokens.size() < 2) continue;
            const std::string type = tokens[1];
            const std::string normalizedType = toLower(type);
            if((normalizedType == "memorycheck" || normalizedType == "ppumemorycheck") && tokens.size() >= 5) {
                MemoryCondition condition;
                condition.kind = MemoryCondition::Kind::MemoryCheck;
                condition.memoryType = normalizedType == "ppumemorycheck" ? "ppu" : "cpu";
                condition.memorySource = parseMemorySourceName(condition.memoryType);
                condition.address = parseHexValue(tokens[2]);
                condition.memoryCacheKey = makeMemoryCacheKey(condition.memorySource, condition.address, condition.word, condition.scale);
                condition.compareAgainstMemory = true;
                condition.rhsMemoryType = condition.memoryType;
                condition.rhsMemorySource = condition.memorySource;
                condition.rhsAddress = parseHexValue(tokens[4]);
                condition.rhsMemoryCacheKey = makeMemoryCacheKey(condition.rhsMemorySource, condition.rhsAddress, condition.rhsWord, condition.rhsScale);
                condition.op = tokens[3];
                condition.compareOp = parseCompareOpName(condition.op);
                condition.debugName = tokens[0];
                if(tokens.size() >= 6) {
                    condition.hasMask = true;
                    condition.mask = parseHexValue(tokens[5]);
                }
                namedConditions[tokens[0]] = { condition };
            } else if((normalizedType == "memorycheckconstant" || normalizedType == "ppumemorycheckconstant") && tokens.size() >= 5) {
                MemoryCondition condition;
                condition.kind = MemoryCondition::Kind::MemoryCheck;
                condition.memoryType = normalizedType == "ppumemorycheckconstant" ? "ppu" : "cpu";
                condition.memorySource = parseMemorySourceName(condition.memoryType);
                condition.address = parseHexValue(tokens[2]);
                condition.memoryCacheKey = makeMemoryCacheKey(condition.memorySource, condition.address, condition.word, condition.scale);
                condition.op = tokens[3];
                condition.compareOp = parseCompareOpName(condition.op);
                condition.value = parseHexValue(tokens[4]);
                condition.debugName = tokens[0];
                if(tokens.size() >= 6) {
                    condition.hasMask = true;
                    condition.mask = parseHexValue(tokens[5]);
                }
                namedConditions[tokens[0]] = { condition };
            } else if(normalizedType == "framerange" && tokens.size() >= 4) {
                MemoryCondition condition;
                condition.kind = MemoryCondition::Kind::FrameRange;
                condition.value = parseDecOrHexValue(tokens[2], 1);
                condition.address = parseDecOrHexValue(tokens[3], 0);
                condition.debugName = tokens[0];
                namedConditions[tokens[0]] = { condition };
            } else if((normalizedType == "tileatposition" || normalizedType == "tilenearby" ||
                        normalizedType == "spriteatposition" || normalizedType == "spritenearby") && tokens.size() >= 6) {
                MemoryCondition condition;
                condition.kind =
                    normalizedType == "tileatposition" ? MemoryCondition::Kind::TileAtPosition :
                    normalizedType == "tilenearby" ? MemoryCondition::Kind::TileNearby :
                    normalizedType == "spriteatposition" ? MemoryCondition::Kind::SpriteAtPosition :
                    MemoryCondition::Kind::SpriteNearby;
                condition.x = parseSignedDecimalIntStrict(tokens[2]).value_or(0);
                condition.y = parseSignedDecimalIntStrict(tokens[3]).value_or(0);
                condition.debugName = tokens[0];
                condition.hasExpectedTile = true;
                if(tokens[4].size() >= 32) {
                    condition.expectedTileByHash = true;
                    condition.expectedTileHash = hashTileBytes(tokens[4]);
                } else {
                    condition.expectedTileIndex = static_cast<int>(parseHexValue(tokens[4]));
                }
                condition.expectedPalette = parseMesenPalette(tokens[5]);
                condition.expectedPaletteKey = parseHexValue(tokens[5]);
                namedConditions[tokens[0]] = { condition };
            }
            continue;
        }
        if(line.rfind("<tile>", 0) == 0) {
            const std::vector<std::string> tokens = splitComma(line.substr(6));
            if(tokens.size() < 7) {
                logModMessage("Invalid Mesen <tile> on line " + std::to_string(lineNumber), Logger::Type::ERROR);
                continue;
            }

            ChrOverride override;
            override.ignorePalette = true;
            override.priority = nextPriority--;
            override.conditions = ruleConditions;
            for(const std::string& conditionName : ruleConditionNames) {
                const bool inverted = !conditionName.empty() && conditionName.front() == '!';
                const std::string normalizedName = inverted ? conditionName.substr(1) : conditionName;
                if(normalizedName == "hmirror") override.hMirrorRequirement = inverted ? -1 : 1;
                else if(normalizedName == "vmirror") override.vMirrorRequirement = inverted ? -1 : 1;
                else if(normalizedName == "bgpriority") override.bgPriorityRequirement = inverted ? -1 : 1;
            }

            bool parsed = false;
            if(tokens.size() >= 8) {
                const std::optional<int> legacyTile = parseDecimalIntStrict(tokens[0]);
                const std::optional<int> legacyImageIndex = parseDecimalIntStrict(tokens[1]);
                const std::optional<int> legacyPalette0 = parseDecimalIntStrict(tokens[2]);
                const std::optional<int> legacyPalette1 = parseDecimalIntStrict(tokens[3]);
                const std::optional<int> legacyPalette2 = parseDecimalIntStrict(tokens[4]);
                const std::optional<int> legacySourceX = parseDecimalIntStrict(tokens[5]);
                const std::optional<int> legacySourceY = parseDecimalIntStrict(tokens[6]);

                if(legacyTile.has_value() &&
                   legacyImageIndex.has_value() &&
                   legacyPalette0.has_value() &&
                   legacyPalette1.has_value() &&
                   legacyPalette2.has_value() &&
                   legacySourceX.has_value() &&
                   legacySourceY.has_value() &&
                   *legacyImageIndex >= 0 &&
                   *legacyImageIndex < static_cast<int>(images.size())) {
                    override.assetPath = images[static_cast<size_t>(*legacyImageIndex)];
                    override.tile = *legacyTile;
                    override.absoluteTile = true;
                    override.sourceX = std::max(0, *legacySourceX);
                    override.sourceY = std::max(0, *legacySourceY);
                    override.defaultTile = parseMesenBool(tokens[7]);
                    override.target = ChrOverride::Target::Both;
                    override.exactPaletteOrder = true;
                    override.paletteIndices = {
                        static_cast<uint8_t>(std::clamp(*legacyPalette0, 0, 255)),
                        static_cast<uint8_t>(std::clamp(*legacyPalette1, 0, 255)),
                        static_cast<uint8_t>(std::clamp(*legacyPalette2, 0, 255))
                    };
                    parsed = true;
                }
            }

            if(!parsed) {
                const int imageIndex = parseIntStrict(tokens[0]).value_or(-1);

                if(imageIndex >= 0 && imageIndex < static_cast<int>(images.size())) {
                    override.assetPath = images[static_cast<size_t>(imageIndex)];
                    override.sourceX = std::max(0, static_cast<int>(std::strtol(tokens[3].c_str(), nullptr, 10)));
                    override.sourceY = std::max(0, static_cast<int>(std::strtol(tokens[4].c_str(), nullptr, 10)));
                    override.defaultTile = parseMesenBool(tokens[6]);
                    override.paletteIndices = parseMesenPalette(tokens[2]);
                    override.exactPaletteOrder = true;
                    override.target = tokens[2].size() >= 2 && toLower(tokens[2].substr(0, 2)) == "ff"
                        ? ChrOverride::Target::Sprite
                        : ChrOverride::Target::Background;

                    const std::string tileData = tokens[1];
                    if(tileData.size() >= 32) {
                        override.tile = -1;
                        override.hasChrHash = true;
                        override.chrHash = hashTileBytes(tileData);
                    } else {
                        override.tile = static_cast<int>(parseHexValue(tileData));
                        override.absoluteTile = true;
                    }
                    parsed = true;
                }
            }

            if(!parsed) {
                logModMessage("Mesen <tile> references invalid image index on line " + std::to_string(lineNumber), Logger::Type::ERROR);
                continue;
            }
            m_chrOverrides.push_back(std::move(override));
            continue;
        }
        if(line.rfind("<addition>", 0) == 0) {
            const std::vector<std::string> tokens = splitComma(line.substr(10));
            if(tokens.size() < 6) {
                logModMessage("Invalid Mesen <addition> on line " + std::to_string(lineNumber), Logger::Type::ERROR);
                continue;
            }

            AdditionalSpriteRule rule;
            rule.originalTile = static_cast<int>(parseHexValue(tokens[0], 0xFFFFFFFFu));
            rule.originalPaletteKey = parseHexValue(tokens[1]);
            rule.offsetX = parseSignedDecimalIntStrict(tokens[2]).value_or(0);
            rule.offsetY = parseSignedDecimalIntStrict(tokens[3]).value_or(0);
            rule.additionalTile = static_cast<int>(parseHexValue(tokens[4], 0xFFFFFFFFu));
            rule.additionalPaletteKey = parseHexValue(tokens[5]);
            if(tokens.size() >= 7) {
                rule.ignorePalette = parseMesenBool(tokens[6]);
            }
            m_additionalSpriteRules.push_back(std::move(rule));
            continue;
        }
        if(line.rfind("<fallback>", 0) == 0) {
            const std::vector<std::string> tokens = splitComma(line.substr(10));
            if(tokens.size() < 2) {
                logModMessage("Invalid Mesen <fallback> on line " + std::to_string(lineNumber), Logger::Type::ERROR);
                continue;
            }

            FallbackTileRule rule;
            rule.tile = static_cast<int>(parseHexValue(tokens[0], 0xFFFFFFFFu));
            rule.fallbackTile = static_cast<int>(parseHexValue(tokens[1], 0xFFFFFFFFu));
            m_fallbackTileRules.push_back(std::move(rule));
            continue;
        }
        if(line.rfind("<background>", 0) == 0) {
            const std::vector<std::string> tokens = splitComma(line.substr(12));
            if(tokens.empty()) continue;
            BackgroundReplacement replacement;
            replacement.id = tokens[0] + "#" + std::to_string(lineNumber);
            replacement.assetPath = normalizeZipPath(tokens[0]);
            replacement.opacity = tokens.size() >= 2 ? std::strtof(tokens[1].c_str(), nullptr) : 1.0f;
            replacement.parallaxX = tokens.size() >= 3 ? std::strtof(tokens[2].c_str(), nullptr) : 0.0f;
            replacement.parallaxY = tokens.size() >= 4 ? std::strtof(tokens[3].c_str(), nullptr) : 0.0f;
            replacement.priority = tokens.size() >= 5 ? std::atoi(tokens[4].c_str()) : 10;
            replacement.sourceX = tokens.size() >= 6 ? std::atoi(tokens[5].c_str()) : 0;
            replacement.sourceY = tokens.size() >= 7 ? std::atoi(tokens[6].c_str()) : 0;
            replacement.repeatX = false;
            replacement.repeatY = false;
            replacement.conditions = ruleConditions;
            m_backgroundReplacements.push_back(std::move(replacement));
            continue;
        }
    }

    const bool hasAudioEntries = !m_hdAudioConfig.bgmFilesById.empty() || !m_hdAudioConfig.sfxFilesById.empty();
    if(m_chrOverrides.empty() && m_backgroundReplacements.empty() && !hasAudioEntries) {
        logModMessage("Mesen hires.txt did not define any supported graphics replacements.", Logger::Type::ERROR);
        return false;
    }
    if(m_hdAudioRuntime) {
        m_hdAudioRuntime->setConfig(m_hdAudioConfig);
    }

    m_chrRomTileHashes = buildChrRomTileHashes(m_effectiveRomPath.empty() ? m_originalRomPath : m_effectiveRomPath);
    m_chrRomCanonicalTileByHash.clear();
    if(!m_chrRomTileHashes.empty()) {
        auto tileHashForIndex = [&](int tileIndex) -> uint32_t {
            return tileIndex >= 0 && tileIndex < static_cast<int>(m_chrRomTileHashes.size())
                ? m_chrRomTileHashes[static_cast<size_t>(tileIndex)]
                : 0u;
        };

        std::unordered_map<uint32_t, int> firstOverrideTileByHash;
        for(const ChrOverride& override : m_chrOverrides) {
            if(!override.assetAvailable || !override.absoluteTile || override.tile < 0) {
                continue;
            }
            const uint32_t hash = tileHashForIndex(override.tile);
            if(hash != 0 && firstOverrideTileByHash.find(hash) == firstOverrideTileByHash.end()) {
                firstOverrideTileByHash.emplace(hash, override.tile);
            }
        }

        for(const auto& [hash, tileIndex] : firstOverrideTileByHash) {
            m_chrRomCanonicalTileByHash.emplace(hash, tileIndex);
        }
        for(const FallbackTileRule& rule : m_fallbackTileRules) {
            const uint32_t hash = tileHashForIndex(rule.tile);
            if(hash != 0 && rule.fallbackTile >= 0) {
                m_chrRomCanonicalTileByHash[hash] = rule.fallbackTile;
            }
        }
        if(m_automaticFallbackTiles) {
            for(size_t tileIndex = 0; tileIndex < m_chrRomTileHashes.size(); ++tileIndex) {
                const uint32_t hash = m_chrRomTileHashes[tileIndex];
                if(hash == 0) {
                    continue;
                }
                if(const auto it = firstOverrideTileByHash.find(hash); it != firstOverrideTileByHash.end()) {
                    m_chrRomCanonicalTileByHash[hash] = it->second;
                }
            }
        }
    }

    return true;
}

void ModManager::onFrame(GeraNESEmu& emu)
{
    if(!m_active || !m_scriptLoaded) return;
    const bool needsGraphicsCapture =
        (!m_chrOverrides.empty() || !m_backgroundReplacements.empty());
    emu.getConsole().ppu().debugSetModRenderCaptureEnabled(needsGraphicsCapture);
    updateFrameConditionsForFrame(emu);
}

void ModManager::updateFrameConditionsForFrame(GeraNESEmu& emu)
{
    MODMANAGER_PROFILE_SCOPE(UpdateFrameConditions);
    constexpr uint32_t DynamicAssetEvictionFrames = 3600;
    const uint32_t frameCount = emu.frameCount();
    if(m_lastFrameConditionUpdate == frameCount) {
        return;
    }

    evictUnusedDynamicAssets(frameCount);
    if(m_hdAudioRuntime) {
        m_hdAudioRuntime->setCacheFrame(frameCount);
        m_hdAudioRuntime->evictUnusedDynamicClips(DynamicAssetEvictionFrames);
    }

    m_frameConditionState.frameCount = frameCount;
    m_frameConditionState.memoryValues.resize(m_frameConditionPlan.uniqueMemoryReads.size());
    m_frameConditionGroupMatchesScratch.resize(m_frameConditionPlan.uniqueGlobalConditionGroups.size());
    MODMANAGER_PROFILE_COUNT(frameConditionMemoryReads, m_frameConditionPlan.uniqueMemoryReads.size());

    auto memoryConditionMatchesCached = [&](const FrameConditionPlan::CompiledMemoryCondition& compiledCondition) {
        const uint32_t actual =
            compiledCondition.readIndex < m_frameConditionState.memoryValues.size()
                ? m_frameConditionState.memoryValues[compiledCondition.readIndex]
                : 0u;
        const uint32_t expected =
            compiledCondition.compareAgainstMemory && compiledCondition.rhsReadIndex < m_frameConditionState.memoryValues.size()
                ? m_frameConditionState.memoryValues[compiledCondition.rhsReadIndex]
                : compiledCondition.value;

        uint32_t maskedActual = actual;
        uint32_t maskedExpected = expected;
        if(compiledCondition.hasMask) {
            maskedActual &= compiledCondition.mask;
            maskedExpected &= compiledCondition.mask;
        }

        bool match = false;
        switch(compiledCondition.compareOp) {
        case MemoryCondition::CompareOp::AnyOf:
            for(uint32_t value : compiledCondition.anyOfValues) {
                if(compiledCondition.hasMask) {
                    value &= compiledCondition.mask;
                }
                if(maskedActual == value) {
                    match = true;
                    break;
                }
            }
            break;
        case MemoryCondition::CompareOp::NotEqual:
            match = maskedActual != maskedExpected;
            break;
        case MemoryCondition::CompareOp::Greater:
            match = maskedActual > maskedExpected;
            break;
        case MemoryCondition::CompareOp::GreaterOrEqual:
            match = maskedActual >= maskedExpected;
            break;
        case MemoryCondition::CompareOp::Less:
            match = maskedActual < maskedExpected;
            break;
        case MemoryCondition::CompareOp::LessOrEqual:
            match = maskedActual <= maskedExpected;
            break;
        case MemoryCondition::CompareOp::BitSet:
            match = (maskedActual & maskedExpected) == maskedExpected;
            break;
        case MemoryCondition::CompareOp::BitClear:
            match = (maskedActual & maskedExpected) == 0;
            break;
        case MemoryCondition::CompareOp::Equal:
        default:
            match = maskedActual == maskedExpected;
            break;
        }
        return compiledCondition.inverted ? !match : match;
    };

    auto frameRangeConditionMatches = [&](const FrameConditionPlan::CompiledFrameRangeCondition& condition) {
        const uint32_t range = std::max(1u, condition.range);
        const bool match = (m_frameConditionState.frameCount % range) >= condition.start;
        return condition.inverted ? !match : match;
    };

    auto globalConditionsMatch = [&](const FrameConditionPlan::CompiledRuleConditions& conditions) {
        for(const FrameConditionPlan::CompiledMemoryCondition& condition : conditions.memoryConditions) {
            if(!memoryConditionMatchesCached(condition)) {
                return false;
            }
        }
        for(const FrameConditionPlan::CompiledFrameRangeCondition& condition : conditions.frameRangeConditions) {
            if(!frameRangeConditionMatches(condition)) {
                return false;
            }
        }
        return true;
    };

    for(const FrameConditionPlan::CachedMemoryRead& cachedRead : m_frameConditionPlan.uniqueMemoryReads) {
        if(cachedRead.readIndex < m_frameConditionState.memoryValues.size()) {
            uint32_t low = readMemory(&emu, cachedRead.memorySource, cachedRead.address);
            if(cachedRead.word) {
                const uint32_t high = readMemory(&emu, cachedRead.memorySource, cachedRead.address + 1u);
                low |= high << 8u;
            }
            m_frameConditionState.memoryValues[cachedRead.readIndex] =
                low / std::max(1u, cachedRead.scaleDivisor);
        }
    }

    for(size_t i = 0; i < m_frameConditionPlan.uniqueGlobalConditionGroups.size(); ++i) {
        m_frameConditionGroupMatchesScratch[i] =
            globalConditionsMatch(m_frameConditionPlan.uniqueGlobalConditionGroups[i]) ? 1u : 0u;
    }

    const size_t chrCount = std::min(m_chrOverrides.size(), m_frameConditionPlan.chrOverrideGlobalConditionGroupIndices.size());
    for(size_t i = 0; i < chrCount; ++i) {
        const size_t groupIndex = m_frameConditionPlan.chrOverrideGlobalConditionGroupIndices[i];
        const bool enabled =
            groupIndex < m_frameConditionGroupMatchesScratch.size()
                ? (m_frameConditionGroupMatchesScratch[groupIndex] != 0)
                : true;
        m_chrOverrides[i].enabled = enabled;
    }
    for(size_t i = chrCount; i < m_chrOverrides.size(); ++i) {
        m_chrOverrides[i].enabled = true;
    }

    const size_t bgCount = std::min(m_backgroundReplacements.size(), m_frameConditionPlan.backgroundGlobalConditionGroupIndices.size());
    for(size_t i = 0; i < bgCount; ++i) {
        const size_t groupIndex = m_frameConditionPlan.backgroundGlobalConditionGroupIndices[i];
        const bool enabled =
            groupIndex < m_frameConditionGroupMatchesScratch.size()
                ? (m_frameConditionGroupMatchesScratch[groupIndex] != 0)
                : true;
        m_backgroundReplacements[i].enabled = enabled;
    }
    for(size_t i = bgCount; i < m_backgroundReplacements.size(); ++i) {
        m_backgroundReplacements[i].enabled = true;
    }
    m_lastFrameConditionUpdate = frameCount;
}

void ModManager::rebuildFrameConditionPlan()
{
    m_frameConditionPlan = {};
    std::unordered_map<uint64_t, size_t> memoryReadIndexByKey;

    auto ensureMemoryRead = [&](MemoryCondition::MemorySource source, uint32_t address, bool word, int scale, uint64_t& cacheKey) {
        if(cacheKey == 0) {
            cacheKey = makeMemoryCacheKey(source, address, word, scale);
        }
        if(const auto it = memoryReadIndexByKey.find(cacheKey); it != memoryReadIndexByKey.end()) {
            return it->second;
        }

        FrameConditionPlan::CachedMemoryRead cachedRead;
        cachedRead.key = cacheKey;
        cachedRead.readIndex = m_frameConditionPlan.uniqueMemoryReads.size();
        cachedRead.memorySource = source;
        cachedRead.address = address;
        cachedRead.word = word;
        cachedRead.scaleDivisor = static_cast<uint32_t>(std::max(1, scale));

        const size_t readIndex = cachedRead.readIndex;
        m_frameConditionPlan.uniqueMemoryReads.push_back(std::move(cachedRead));
        memoryReadIndexByKey.emplace(cacheKey, readIndex);
        return readIndex;
    };

    auto compileGlobalConditions = [&](const std::vector<MemoryCondition>& source) {
        FrameConditionPlan::CompiledRuleConditions globals;
        globals.memoryConditions.reserve(source.size());
        globals.frameRangeConditions.reserve(source.size());
        for(const MemoryCondition& condition : source) {
            if(condition.kind == MemoryCondition::Kind::FrameRange) {
                globals.frameRangeConditions.push_back(FrameConditionPlan::CompiledFrameRangeCondition {
                    std::max(1u, condition.value),
                    condition.address,
                    condition.inverted
                });
                continue;
            }
            if(condition.kind != MemoryCondition::Kind::MemoryCheck) {
                continue;
            }
            uint64_t cacheKey = condition.memoryCacheKey;
            const size_t readIndex = ensureMemoryRead(
                condition.memorySource,
                condition.address,
                condition.word,
                condition.scale,
                cacheKey);
            size_t rhsReadIndex = 0;
            if(condition.compareAgainstMemory) {
                uint64_t rhsCacheKey = condition.rhsMemoryCacheKey;
                rhsReadIndex = ensureMemoryRead(
                    condition.rhsMemorySource,
                    condition.rhsAddress,
                    condition.rhsWord,
                    condition.rhsScale,
                    rhsCacheKey);
            }
            globals.memoryConditions.push_back(FrameConditionPlan::CompiledMemoryCondition {
                readIndex,
                rhsReadIndex
                ,
                condition.compareOp,
                condition.value,
                condition.mask,
                condition.hasMask,
                condition.inverted,
                condition.compareAgainstMemory,
                condition.values
            });
        }
        return globals;
    };

    auto compiledMemoryConditionsEqual = [](const FrameConditionPlan::CompiledMemoryCondition& lhs,
                                            const FrameConditionPlan::CompiledMemoryCondition& rhs) {
        if(lhs.readIndex != rhs.readIndex || lhs.rhsReadIndex != rhs.rhsReadIndex) {
            return false;
        }
        return lhs.compareOp == rhs.compareOp &&
               lhs.value == rhs.value &&
               lhs.mask == rhs.mask &&
               lhs.hasMask == rhs.hasMask &&
               lhs.inverted == rhs.inverted &&
               lhs.compareAgainstMemory == rhs.compareAgainstMemory &&
               lhs.anyOfValues == rhs.anyOfValues;
    };

    auto frameRangeConditionsEqual = [](const FrameConditionPlan::CompiledFrameRangeCondition& lhs,
                                        const FrameConditionPlan::CompiledFrameRangeCondition& rhs) {
        return lhs.range == rhs.range &&
               lhs.start == rhs.start &&
               lhs.inverted == rhs.inverted;
    };

    auto compiledGlobalConditionsEqual = [&](const FrameConditionPlan::CompiledRuleConditions& lhs,
                                             const FrameConditionPlan::CompiledRuleConditions& rhs) {
        if(lhs.memoryConditions.size() != rhs.memoryConditions.size() ||
           lhs.frameRangeConditions.size() != rhs.frameRangeConditions.size()) {
            return false;
        }
        for(size_t i = 0; i < lhs.memoryConditions.size(); ++i) {
            if(!compiledMemoryConditionsEqual(lhs.memoryConditions[i], rhs.memoryConditions[i])) {
                return false;
            }
        }
        for(size_t i = 0; i < lhs.frameRangeConditions.size(); ++i) {
            if(!frameRangeConditionsEqual(lhs.frameRangeConditions[i], rhs.frameRangeConditions[i])) {
                return false;
            }
        }
        return true;
    };

    auto findOrAppendGlobalConditionGroup = [&](FrameConditionPlan::CompiledRuleConditions&& globals) {
        for(size_t i = 0; i < m_frameConditionPlan.uniqueGlobalConditionGroups.size(); ++i) {
            if(compiledGlobalConditionsEqual(m_frameConditionPlan.uniqueGlobalConditionGroups[i], globals)) {
                return i;
            }
        }
        const size_t index = m_frameConditionPlan.uniqueGlobalConditionGroups.size();
        m_frameConditionPlan.uniqueGlobalConditionGroups.push_back(std::move(globals));
        return index;
    };

    m_frameConditionPlan.chrOverrideGlobalConditionGroupIndices.resize(m_chrOverrides.size());
    for(size_t i = 0; i < m_chrOverrides.size(); ++i) {
        m_frameConditionPlan.chrOverrideGlobalConditionGroupIndices[i] =
            findOrAppendGlobalConditionGroup(compileGlobalConditions(m_chrOverrides[i].conditions));
    }

    m_frameConditionPlan.backgroundGlobalConditionGroupIndices.resize(m_backgroundReplacements.size());
    for(size_t i = 0; i < m_backgroundReplacements.size(); ++i) {
        m_frameConditionPlan.backgroundGlobalConditionGroupIndices[i] =
            findOrAppendGlobalConditionGroup(compileGlobalConditions(m_backgroundReplacements[i].conditions));
    }
}

void ModManager::populateOverrideLookupCache(RenderComposeCache& cache, const std::vector<const ChrOverride*>& activeOverrides, bool trackTileHashNeeds)
{
    auto appendRuntimeConditions = [](const std::vector<MemoryCondition>& conditions, auto& runtimeConditions) {
        runtimeConditions.clear();
        runtimeConditions.reserve(conditions.size());
        for(const MemoryCondition& condition : conditions) {
            if(condition.kind != MemoryCondition::Kind::MemoryCheck &&
               condition.kind != MemoryCondition::Kind::FrameRange) {
                runtimeConditions.push_back(&condition);
            }
        }
    };
    auto conditionsAllowTileOriginCache = [](const auto& runtimeConditions) {
        return std::all_of(runtimeConditions.begin(), runtimeConditions.end(), [](const MemoryCondition* condition) {
            return condition == nullptr ||
                   (condition->kind != MemoryCondition::Kind::TileNearby &&
                    condition->kind != MemoryCondition::Kind::SpriteNearby);
        });
    };
    auto requirementMatchesValue = [](int requirement, bool value) {
        if(requirement == 0) {
            return true;
        }
        return requirement > 0 ? value : !value;
    };

    cache.activeOverrides = activeOverrides;
    if(!cache.activeOverrides.empty()) {
        std::stable_sort(cache.activeOverrides.begin(), cache.activeOverrides.end(), [](const ChrOverride* a, const ChrOverride* b) {
            return a->priority > b->priority;
        });
    }

    cache.preparedOverrides.reserve(cache.activeOverrides.size());
    for(const ChrOverride* override : cache.activeOverrides) {
        RenderPreparedOverride prepared;
        prepared.override = override;
        prepared.sequence = cache.preparedOverrides.size();
        prepared.image = decodedImage(override->assetPath);
        if(prepared.image == nullptr || prepared.image->rgba.empty()) {
            continue;
        }
        prepared.rgbaData = prepared.image->rgba.data();
        prepared.indexedPixelsData = prepared.image->indexedPixels.data();
        prepared.sourceScale = std::max(1, m_resolutionMultiplier);
        prepared.imageWidth = prepared.image->width;
        prepared.imageHeight = prepared.image->height;
        prepared.sourceTileOffset = override->sourceTileOffset;
        prepared.sourceColumns = std::max(1, override->columns);
        prepared.ignorePalette = override->ignorePalette;
        if(!override->wholeChr() && !override->hasSourcePosition() && override->sourceLayout != ChrOverride::SourceLayout::PatternTables) {
            const int scaleX = prepared.image->width / std::max(1, override->columns * 8);
            if(scaleX > 0) {
                prepared.sourceScale = scaleX;
            }
        }
        prepared.wholeChr = override->wholeChr();
        prepared.wholeChrLayout =
            override->sourceLayout == ChrOverride::SourceLayout::Auto
                ? ChrOverride::SourceLayout::PatternTables
                : override->sourceLayout;
        prepared.usesIndexedPalette =
            !prepared.ignorePalette &&
            prepared.image->indexedFourColor &&
            prepared.image->indexedPixels.size() == prepared.image->rgba.size();
        if(override->hasSourcePosition()) {
            prepared.fixedTileSrcX = override->sourceX;
            prepared.fixedTileSrcY = override->sourceY;
            prepared.fixedTileSourceValid = true;
        } else if(!prepared.wholeChr) {
            const int tilePixelSize = 8 * prepared.sourceScale;
            const bool atlasImage =
                override->sourceTileOffset > 0 ||
                (prepared.imageWidth >= prepared.sourceColumns * tilePixelSize &&
                 prepared.imageHeight > tilePixelSize);
            if(atlasImage) {
                int sourceColumn = 0;
                int sourceRow = 0;
                if(override->sourceLayout == ChrOverride::SourceLayout::PatternTables) {
                    const int sourceTile = override->sourceTileOffset;
                    const int table = sourceTile / 256;
                    const int tileInTable = sourceTile & 0xFF;
                    sourceColumn = (table * 16) + (tileInTable & 0x0F);
                    sourceRow = tileInTable >> 4;
                } else {
                    const int sourceTile = override->sourceTileOffset;
                    sourceColumn = sourceTile % prepared.sourceColumns;
                    sourceRow = sourceTile / prepared.sourceColumns;
                }
                prepared.fixedTileSrcX = sourceColumn * tilePixelSize;
                prepared.fixedTileSrcY = sourceRow * tilePixelSize;
            } else {
                prepared.fixedTileSrcX = 0;
                prepared.fixedTileSrcY = 0;
            }
            prepared.fixedTileSourceValid = true;
        }
        appendRuntimeConditions(override->conditions, prepared.runtimeConditions);
        if(trackTileHashNeeds && override->hasChrHash) {
            cache.needsTileHashes = true;
        }
        for(const MemoryCondition* condition : prepared.runtimeConditions) {
            if(trackTileHashNeeds && condition != nullptr && condition->hasExpectedTile && condition->expectedTileByHash) {
                cache.needsTileHashes = true;
            }
        }
        prepared.hasDynamicConditions = std::any_of(
            prepared.runtimeConditions.begin(),
            prepared.runtimeConditions.end(),
            [](const MemoryCondition* condition) {
                return condition != nullptr &&
                       (condition->kind == MemoryCondition::Kind::TileAtPosition ||
                        condition->kind == MemoryCondition::Kind::TileNearby ||
                        condition->kind == MemoryCondition::Kind::SpriteAtPosition ||
                        condition->kind == MemoryCondition::Kind::SpriteNearby);
            });
        prepared.tileOriginCacheable = conditionsAllowTileOriginCache(prepared.runtimeConditions);
        prepared.targetMask =
            override->target == ChrOverride::Target::Both
                ? static_cast<uint8_t>(0x3u)
                : static_cast<uint8_t>(override->target == ChrOverride::Target::Background ? 0x1u : 0x2u);
        prepared.patternTableMask =
            override->patternTable >= 0
                ? static_cast<uint8_t>(1u << (override->patternTable & 0x03))
                : static_cast<uint8_t>(0x0Fu);
        prepared.requirementMask = 0;
        for(int combo = 0; combo < 8; ++combo) {
            const bool hMirror = (combo & 0x1) != 0;
            const bool vMirror = (combo & 0x2) != 0;
            const bool bgPriority = (combo & 0x4) != 0;
            if(requirementMatchesValue(override->hMirrorRequirement, hMirror) &&
               requirementMatchesValue(override->vMirrorRequirement, vMirror) &&
               requirementMatchesValue(override->bgPriorityRequirement, bgPriority)) {
                prepared.requirementMask |= static_cast<uint8_t>(1u << combo);
            }
        }
        cache.preparedOverrides.push_back(std::move(prepared));
        const RenderPreparedOverride* preparedPtr = &cache.preparedOverrides.back();
        if(override->wholeChr()) {
            if(override->hasChrHash) {
                cache.staticOverrideHashes.insert(override->chrHash);
                if(override->defaultTile) {
                    cache.defaultOverrideHashes.insert(override->chrHash);
                }
                if(preparedPtr->hasDynamicConditions) {
                    cache.dynamicOverridesByChrHash[override->chrHash].push_back(preparedPtr);
                } else {
                    cache.overridesByChrHash[override->chrHash].push_back(preparedPtr);
                }
            } else if(preparedPtr->hasDynamicConditions) {
                if(override->defaultTile) {
                    cache.hasWholeChrDefaultOverrides = true;
                }
                cache.dynamicWholeChrOverrides.push_back(preparedPtr);
            } else {
                cache.hasWholeChrOverrides = true;
                if(override->defaultTile) {
                    cache.hasWholeChrDefaultOverrides = true;
                }
                cache.wholeChrOverrides.push_back(preparedPtr);
            }
        } else if(override->absoluteTile) {
            if(override->tile >= 0) {
                if(override->tile < static_cast<int>(cache.overridesByFullTile.size())) {
                    cache.hasStaticOverridesByFullTile[static_cast<size_t>(override->tile)] = true;
                    if(override->defaultTile) {
                        cache.hasDefaultOverridesByFullTile[static_cast<size_t>(override->tile)] = true;
                    }
                    if(preparedPtr->hasDynamicConditions) {
                        cache.dynamicOverridesByFullTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    } else {
                        cache.overridesByFullTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    }
                } else {
                    cache.staticOverflowFullTiles.insert(override->tile);
                    if(override->defaultTile) {
                        cache.defaultOverflowFullTiles.insert(override->tile);
                    }
                    if(preparedPtr->hasDynamicConditions) {
                        cache.dynamicOverflowFullTiles.insert(override->tile);
                        cache.dynamicOverridesByOverflowFullTile[override->tile].push_back(preparedPtr);
                    } else {
                        cache.overridesByOverflowFullTile[override->tile].push_back(preparedPtr);
                    }
                }
            }
        } else if(override->tile >= 0) {
            if(override->tile < static_cast<int>(cache.overridesByRelativeTile.size())) {
                cache.hasStaticOverridesByRelativeTile[static_cast<size_t>(override->tile)] = true;
                if(override->defaultTile) {
                    cache.hasDefaultOverridesByRelativeTile[static_cast<size_t>(override->tile)] = true;
                }
                if(preparedPtr->hasDynamicConditions) {
                    cache.dynamicOverridesByRelativeTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                } else {
                    cache.overridesByRelativeTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                }
            } else if(override->tile < static_cast<int>(cache.overridesByFullTile.size())) {
                cache.hasStaticOverridesByFullTile[static_cast<size_t>(override->tile)] = true;
                if(override->defaultTile) {
                    cache.hasDefaultOverridesByFullTile[static_cast<size_t>(override->tile)] = true;
                }
                if(preparedPtr->hasDynamicConditions) {
                    cache.dynamicOverridesByFullTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                } else {
                    cache.overridesByFullTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                }
            }
        }
    }

    for(size_t i = 0; i < cache.dynamicOverridesByFullTile.size(); ++i) {
        cache.hasDynamicOverridesByFullTile[i] = !cache.dynamicOverridesByFullTile[i].empty();
    }
    for(size_t i = 0; i < cache.dynamicOverridesByRelativeTile.size(); ++i) {
        cache.hasDynamicOverridesByRelativeTile[i] = !cache.dynamicOverridesByRelativeTile[i].empty();
    }
    cache.dynamicOverrideHashes.reserve(cache.dynamicOverridesByChrHash.size());
    for(const auto& [hash, _] : cache.dynamicOverridesByChrHash) {
        cache.dynamicOverrideHashes.insert(hash);
    }

    cache.onlyWholeChrOverrides = std::all_of(
        cache.activeOverrides.begin(),
        cache.activeOverrides.end(),
        [](const ChrOverride* override) {
            return override != nullptr &&
                   override->wholeChr() &&
                   !override->hasChrHash &&
                   override->paletteIndices.empty() &&
                   override->patternTable < 0 &&
                   !override->defaultTile &&
                   !override->absoluteTile;
        });
    if(cache.onlyWholeChrOverrides) {
        for(const RenderPreparedOverride& prepared : cache.preparedOverrides) {
            const ChrOverride* override = prepared.override;
            if(override != nullptr &&
               !prepared.hasDynamicConditions &&
               (override->target == ChrOverride::Target::Both || override->target == ChrOverride::Target::Background)) {
                cache.fastBackgroundOverride = &prepared;
                break;
            }
        }
    }
}

void ModManager::rebuildRenderComposeCache()
{
    MODMANAGER_PROFILE_SCOPE(RebuildRenderComposeCache);
    MODMANAGER_PROFILE_COUNT(renderCacheRebuilds, 1);
    m_renderComposeCache = {};
    m_renderComposeCache.scale = std::max(1, m_resolutionMultiplier);
    m_renderComposeCache.needsTileHashes = false;

    auto makeAdditionalSpriteRuleKey = [](int tile, uint32_t paletteKey) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(tile)) << 32u) | static_cast<uint64_t>(paletteKey);
    };
    auto appendRuntimeConditions = [](const std::vector<MemoryCondition>& conditions, auto& runtimeConditions) {
        runtimeConditions.clear();
        runtimeConditions.reserve(conditions.size());
        for(const MemoryCondition& condition : conditions) {
            if(condition.kind != MemoryCondition::Kind::MemoryCheck &&
               condition.kind != MemoryCondition::Kind::FrameRange) {
                runtimeConditions.push_back(&condition);
            }
        }
    };

    std::vector<const ChrOverride*> activeOverrides;
    activeOverrides.reserve(m_chrOverrides.size());
    for(const ChrOverride& override : m_chrOverrides) {
        if(override.assetAvailable && !override.assetPath.empty()) {
            activeOverrides.push_back(&override);
        }
    }
    populateOverrideLookupCache(m_renderComposeCache, activeOverrides, true);

    m_renderComposeCache.preparedBackgrounds.reserve(m_backgroundReplacements.size());
    for(const BackgroundReplacement& replacement : m_backgroundReplacements) {
        if(!replacement.assetAvailable || replacement.assetPath.empty()) {
            continue;
        }
        const DecodedImage* image = decodedImage(replacement.assetPath);
        if(image == nullptr || image->rgba.empty()) {
            continue;
        }
        RenderPreparedBackground prepared;
        prepared.replacement = &replacement;
        prepared.image = image;
        prepared.priority = std::clamp(replacement.priority, 0, 39);
        prepared.backgroundScale = std::max(1, m_resolutionMultiplier);
        prepared.alphaScale = std::clamp(static_cast<int>(std::round(replacement.opacity * 255.0f)), 0, 255);
        prepared.opaqueCopy = prepared.alphaScale >= 255;
        appendRuntimeConditions(replacement.conditions, prepared.runtimeConditions);
        for(const MemoryCondition* condition : prepared.runtimeConditions) {
            if(condition != nullptr && condition->hasExpectedTile && condition->expectedTileByHash) {
                m_renderComposeCache.needsTileHashes = true;
            }
        }
        m_renderComposeCache.preparedBackgrounds.push_back(std::move(prepared));
    }

    auto appendAdditionalSpriteRuleSpan = [&](auto& index, auto key, const AdditionalSpriteRule* rulePtr) {
        if(rulePtr == nullptr) {
            return;
        }
        auto [it, inserted] = index.emplace(key, RenderComposeCache::AdditionalSpriteRuleSpan {
            m_renderComposeCache.additionalSpriteRuleStorage.size(),
            0u
        });
        if(inserted) {
            it->second.offset = m_renderComposeCache.additionalSpriteRuleStorage.size();
        }
        m_renderComposeCache.additionalSpriteRuleStorage.push_back(rulePtr);
        ++it->second.count;
        m_renderComposeCache.hasAdditionalSpriteRules = true;
    };

    m_renderComposeCache.additionalSpriteRulesByExactKey.clear();
    m_renderComposeCache.additionalSpriteRulesByOriginalTile.clear();
    m_renderComposeCache.additionalSpriteRuleStorage.clear();
    m_renderComposeCache.additionalSpriteRuleStorage.reserve(m_additionalSpriteRules.size());
    for(const AdditionalSpriteRule& rule : m_additionalSpriteRules) {
        if(rule.originalTile < 0) {
            continue;
        }
        if(rule.ignorePalette) {
            appendAdditionalSpriteRuleSpan(m_renderComposeCache.additionalSpriteRulesByOriginalTile, rule.originalTile, &rule);
        } else {
            appendAdditionalSpriteRuleSpan(
                m_renderComposeCache.additionalSpriteRulesByExactKey,
                makeAdditionalSpriteRuleKey(rule.originalTile, rule.originalPaletteKey),
                &rule);
        }
    }

    m_renderComposeCache.valid = true;
    m_renderComposeCacheDirty = false;
    MODMANAGER_PROFILE_COUNT(preparedOverrideCount, m_renderComposeCache.preparedOverrides.size());
    MODMANAGER_PROFILE_COUNT(preparedBackgroundCount, m_renderComposeCache.preparedBackgrounds.size());
    MODMANAGER_PROFILE_COUNT(compiledAdditionalSpriteRuleCount, m_renderComposeCache.additionalSpriteRuleStorage.size());
}

ModManager::RenderComposeCache ModManager::buildFilteredRenderComposeCache(const std::vector<const ChrOverride*>& activeOverrideFilter)
{
    RenderComposeCache filteredCache = {};
    filteredCache.scale = std::max(1, m_resolutionMultiplier);
    populateOverrideLookupCache(filteredCache, activeOverrideFilter, false);
    filteredCache.valid = true;
    return filteredCache;
}

std::optional<std::vector<uint8_t>> ModManager::readAsset(const std::string& assetPath) const
{
    if(!m_active) return std::nullopt;
    return readSourceEntry(assetPath);
}

bool ModManager::isFolderSource() const
{
    if(m_modPath.empty()) {
        return false;
    }
    return pathIsDirectory(m_modPath);
}

std::string ModManager::normalizeZipPath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    while(!path.empty() && path.front() == '/') {
        path.erase(path.begin());
    }
    return path;
}

std::optional<std::string> ModManager::findZipEntryRootForFile(const std::filesystem::path& zipPath, const std::string& entryName)
{
    const std::string normalizedTarget = normalizeZipPath(entryName);
    if(normalizedTarget.empty()) {
        return std::nullopt;
    }

    const std::string lowerTarget = toLower(normalizedTarget);
    zip_t* zip = zip_open(zipPath.string().c_str(), 0, 'r');
    if(zip == nullptr) {
        return std::nullopt;
    }

    std::optional<std::string> bestRoot;
    const ssize_t totalEntries = zip_entries_total(zip);
    for(ssize_t i = 0; i < totalEntries; ++i) {
        if(zip_entry_openbyindex(zip, static_cast<size_t>(i)) != 0) {
            continue;
        }

        const char* name = zip_entry_name(zip);
        const bool isDirectory = zip_entry_isdir(zip) != 0;
        std::string normalizedName = name != nullptr ? normalizeZipPath(name) : "";
        zip_entry_close(zip);

        if(isDirectory || normalizedName.empty()) {
            continue;
        }

        const std::string lowerName = toLower(normalizedName);
        if(lowerName != lowerTarget) {
            const std::string suffix = "/" + lowerTarget;
            if(lowerName.size() <= suffix.size() || lowerName.compare(lowerName.size() - suffix.size(), suffix.size(), suffix) != 0) {
                continue;
            }
        }

        const size_t lastSlash = normalizedName.find_last_of('/');
        const std::string root = lastSlash == std::string::npos
            ? std::string()
            : normalizedName.substr(0, lastSlash + 1);
        if(!bestRoot.has_value() ||
           root.size() < bestRoot->size() ||
           (root.size() == bestRoot->size() && root < *bestRoot)) {
            bestRoot = root;
        }
    }

    zip_close(zip);
    return bestRoot;
}

std::optional<std::filesystem::path> ModManager::resolveFolderEntryPath(const std::filesystem::path& rootPath, const std::string& entryName)
{
    if(rootPath.empty()) {
        return std::nullopt;
    }

    const std::string normalized = normalizeZipPath(entryName);
    if(normalized.empty()) {
        return std::nullopt;
    }

    std::filesystem::path relativePath = std::filesystem::path(normalized).lexically_normal();
    if(relativePath.is_absolute()) {
        return std::nullopt;
    }
    for(const auto& component : relativePath) {
        if(component == "..") {
            return std::nullopt;
        }
    }
    return rootPath / relativePath;
}

std::string ModManager::resolveSourceEntryName(const std::string& entryName) const
{
    const std::string normalizedEntry = normalizeZipPath(entryName);
    if(normalizedEntry.empty() || m_modArchiveRoot.empty()) {
        return normalizedEntry;
    }
    if(normalizedEntry.rfind(m_modArchiveRoot, 0) == 0) {
        return normalizedEntry;
    }
    return m_modArchiveRoot + normalizedEntry;
}

std::string ModManager::resolveZipEntryName(const std::string& entryName) const
{
    const std::string resolvedEntryName = resolveSourceEntryName(entryName);
    if(resolvedEntryName.empty()) {
        return resolvedEntryName;
    }
    if(const auto it = m_zipEntryLookup.find(toLower(resolvedEntryName)); it != m_zipEntryLookup.end()) {
        return it->second;
    }
    return resolvedEntryName;
}

bool ModManager::rebuildZipEntryLookup()
{
    m_zipEntryLookup.clear();
    if(m_modPath.empty() || isFolderSource()) {
        return true;
    }

    zip_t* zip = zip_open(m_modPath.string().c_str(), 0, 'r');
    if(zip == nullptr) {
        return false;
    }

    const ssize_t totalEntries = zip_entries_total(zip);
    if(totalEntries > 0) {
        m_zipEntryLookup.reserve(static_cast<size_t>(totalEntries));
    }

    for(ssize_t i = 0; i < totalEntries; ++i) {
        if(zip_entry_openbyindex(zip, static_cast<size_t>(i)) != 0) {
            continue;
        }

        const char* name = zip_entry_name(zip);
        const bool isDirectory = zip_entry_isdir(zip) != 0;
        const std::string normalizedName = name != nullptr ? normalizeZipPath(name) : "";
        zip_entry_close(zip);
        if(isDirectory || normalizedName.empty()) {
            continue;
        }

        const std::string lookupKey = toLower(normalizedName);
        auto it = m_zipEntryLookup.find(lookupKey);
        if(it == m_zipEntryLookup.end() || normalizedName < it->second) {
            m_zipEntryLookup[lookupKey] = normalizedName;
        }
    }

    zip_close(zip);
    return true;
}

std::optional<std::vector<uint8_t>> ModManager::readFileEntry(const std::filesystem::path& rootPath, const std::string& entryName)
{
    const auto resolvedPath = resolveFolderEntryPath(rootPath, entryName);
    if(!resolvedPath.has_value()) {
        return std::nullopt;
    }

    if(!pathExists(*resolvedPath) || !pathIsRegularFile(*resolvedPath)) {
        return std::nullopt;
    }

    std::ifstream file(*resolvedPath, std::ios::binary);
    if(!file) {
        return std::nullopt;
    }
    return std::vector<uint8_t>(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
}

bool ModManager::sourceHasEntry(const std::string& entryName) const
{
    if(m_modPath.empty()) {
        return false;
    }
    const bool folderSource = isFolderSource();
    const std::string resolvedEntryName = folderSource ? resolveSourceEntryName(entryName) : resolveZipEntryName(entryName);
    if(resolvedEntryName.empty()) {
        return false;
    }
    if(folderSource) {
        const auto resolvedPath = resolveFolderEntryPath(m_modPath, resolvedEntryName);
        if(!resolvedPath.has_value()) {
            return false;
        }
        return pathExists(*resolvedPath) && pathIsRegularFile(*resolvedPath);
    }
    return m_zipEntryLookup.find(toLower(resolvedEntryName)) != m_zipEntryLookup.end();
}

std::optional<std::vector<uint8_t>> ModManager::readSourceEntry(const std::string& entryName) const
{
    if(m_modPath.empty()) {
        return std::nullopt;
    }
    const bool folderSource = isFolderSource();
    const std::string resolvedEntryName = folderSource ? resolveSourceEntryName(entryName) : resolveZipEntryName(entryName);
    if(resolvedEntryName.empty()) {
        return std::nullopt;
    }
    if(folderSource) {
        return readFileEntry(m_modPath, resolvedEntryName);
    }
    return readZipEntry(m_modPath, resolvedEntryName);
}

std::optional<std::vector<uint8_t>> ModManager::readZipEntry(const std::filesystem::path& zipPath, const std::string& entryName)
{
    char* buffer = nullptr;
    size_t bufferSize = 0;

    zip_t* zip = zip_open(zipPath.string().c_str(), 0, 'r');
    if(zip == nullptr) return std::nullopt;

    const std::string normalizedEntry = normalizeZipPath(entryName);
    int openResult = zip_entry_open(zip, normalizedEntry.c_str());
    if(openResult != 0) {
        const ssize_t totalEntries = zip_entries_total(zip);
        for(ssize_t i = 0; i < totalEntries; ++i) {
            if(zip_entry_openbyindex(zip, static_cast<size_t>(i)) != 0) {
                continue;
            }
            const char* name = zip_entry_name(zip);
            const bool matches = name != nullptr && normalizeZipPath(name) == normalizedEntry;
            if(!matches) {
                zip_entry_close(zip);
                continue;
            }
            openResult = 0;
            break;
        }
    }
    if(openResult != 0) {
        zip_close(zip);
        return std::nullopt;
    }

    const ssize_t readSize = zip_entry_read(zip, reinterpret_cast<void**>(&buffer), &bufferSize);
    zip_entry_close(zip);
    zip_close(zip);

    if(readSize < 0 || buffer == nullptr) {
        std::free(buffer);
        return std::nullopt;
    }

    std::vector<uint8_t> data(buffer, buffer + bufferSize);
    std::free(buffer);
    return data;
}

bool ModManager::writeBinaryFile(const std::filesystem::path& path, const std::vector<uint8_t>& data, std::string& error)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if(ec) {
        error = ec.message();
        return false;
    }

    std::ofstream out(path, std::ios::binary);
    if(!out) {
        error = "open failed";
        return false;
    }
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if(!out.good()) {
        error = "write failed";
        return false;
    }
    return true;
}

std::optional<std::vector<uint8_t>> ModManager::applyIpsPatch(
    const std::vector<uint8_t>& romData,
    const std::vector<uint8_t>& patchData,
    std::string& error)
{
    if(romData.empty() || patchData.empty()) {
        error = "empty ROM or patch data";
        return std::nullopt;
    }

    mem original = {const_cast<uint8_t*>(romData.data()), romData.size()};
    mem patch = {const_cast<uint8_t*>(patchData.data()), patchData.size()};
    mem out = {};

    const ipserror result = ips_apply(patch, original, &out);
    if(result != ips_ok) {
        ips_free(out);
        error = "invalid IPS patch";
        return std::nullopt;
    }

    std::vector<uint8_t> patched(out.ptr, out.ptr + out.len);
    ips_free(out);
    return patched;
}

std::string ModManager::sha1Hex(const std::vector<uint8_t>& data)
{
    Sha1State state;
    std::vector<uint8_t> padded = data;
    const uint64_t bitLength = static_cast<uint64_t>(padded.size()) * 8u;
    padded.push_back(0x80u);
    while((padded.size() % 64u) != 56u) {
        padded.push_back(0u);
    }
    for(int i = 7; i >= 0; --i) {
        padded.push_back(static_cast<uint8_t>((bitLength >> (i * 8)) & 0xFFu));
    }
    for(size_t offset = 0; offset < padded.size(); offset += 64u) {
        sha1ProcessBlock(state, padded.data() + offset);
    }

    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');
    out << std::setw(8) << state.h0;
    out << std::setw(8) << state.h1;
    out << std::setw(8) << state.h2;
    out << std::setw(8) << state.h3;
    out << std::setw(8) << state.h4;
    return out.str();
}

uint64_t ModManager::makeMemoryCacheKey(MemoryCondition::MemorySource source, uint32_t address, bool word, int scale)
{
    uint64_t hash = 1469598103934665603ull;
    hash ^= static_cast<uint64_t>(source);
    hash *= 1099511628211ull;
    hash ^= static_cast<uint64_t>(address);
    hash *= 1099511628211ull;
    hash ^= static_cast<uint64_t>(word ? 1u : 0u);
    hash *= 1099511628211ull;
    hash ^= static_cast<uint64_t>(std::max(1, scale));
    hash *= 1099511628211ull;
    return hash;
}

uint8_t ModManager::readMemory(GeraNESEmu* emu, MemoryCondition::MemorySource source, uint32_t address) const
{
    if(emu == nullptr) return 0;
    if(source == MemoryCondition::MemorySource::Cpu) {
        return emu->debugPeekCpuMemory(static_cast<uint16_t>(address));
    }
    if(source == MemoryCondition::MemorySource::Ppu) {
        return emu->getConsole().ppu().debugPeekPpuMemory(static_cast<uint16_t>(address));
    }
    if(source == MemoryCondition::MemorySource::Oam || source == MemoryCondition::MemorySource::PrimaryOam) {
        return emu->getConsole().ppu().debugPeekPrimaryOam(static_cast<uint8_t>(address));
    }
    if(source == MemoryCondition::MemorySource::SecondaryOam) {
        return emu->getConsole().ppu().debugPeekSecondaryOam(static_cast<uint8_t>(address));
    }
    return 0;
}

uint32_t ModManager::readMemoryValue(const MemoryCondition& source, GeraNESEmu& emu) const
{
    uint32_t low = readMemory(&emu, source.memorySource, source.address);
    if(source.word) {
        const uint32_t high = readMemory(&emu, source.memorySource, source.address + 1u);
        low |= high << 8u;
    }
    const int scale = std::max(1, source.scale);
    return low / static_cast<uint32_t>(scale);
}

bool ModManager::conditionsMatch(const std::vector<MemoryCondition>& conditions, GeraNESEmu& emu) const
{
    for(const auto& condition : conditions) {
        if(!conditionMatches(condition, emu)) {
            return false;
        }
    }
    return true;
}

bool ModManager::conditionMatches(const MemoryCondition& condition, GeraNESEmu& emu) const
{
    if(condition.kind == MemoryCondition::Kind::FrameRange) {
        const uint32_t range = std::max(1u, condition.value);
        const bool match = (emu.frameCount() % range) >= condition.address;
        return condition.inverted ? !match : match;
    }
    if(condition.kind != MemoryCondition::Kind::MemoryCheck) {
        return !condition.inverted;
    }

    const uint32_t actual = readMemoryValue(condition, emu);
    uint32_t expected = condition.value;
    if(condition.compareAgainstMemory) {
        MemoryCondition rhsCondition;
        rhsCondition.memorySource = condition.rhsMemorySource;
        rhsCondition.address = condition.rhsAddress;
        rhsCondition.word = condition.rhsWord;
        rhsCondition.scale = condition.rhsScale;
        expected = readMemoryValue(rhsCondition, emu);
    }

    const bool match = evaluateMemoryCondition(condition, actual, expected);
    return condition.inverted ? !match : match;
}

const ModManager::DecodedImage* ModManager::decodedImage(const std::string& assetPath)
{
    const std::string normalizedPath = normalizeZipPath(assetPath);
    auto cacheIt = m_imageCache.find(normalizedPath);
    if(cacheIt == m_imageCache.end()) {
        std::optional<DecodedImage> decoded;
        if(const auto data = readAsset(normalizedPath); data.has_value()) {
            decoded = decodeImage(*data);
        }
        ImageCacheEntry entry;
        entry.image = std::move(decoded);
        entry.lastUsedFrame = m_lastFrameConditionUpdate == UINT32_MAX ? 0u : m_lastFrameConditionUpdate;
        cacheIt = m_imageCache.emplace(normalizedPath, std::move(entry)).first;
        if(!cacheIt->second.image.has_value()) {
            logModMessage("Failed to load mod image asset: " + normalizedPath, Logger::Type::WARNING);
        }
    }
    cacheIt->second.lastUsedFrame = m_lastFrameConditionUpdate == UINT32_MAX ? 0u : m_lastFrameConditionUpdate;
    return cacheIt->second.image.has_value() ? &*cacheIt->second.image : nullptr;
}

void ModManager::pinDecodedImage(const std::string& assetPath)
{
    const std::string normalizedPath = normalizeZipPath(assetPath);
    auto it = m_imageCache.find(normalizedPath);
    if(it == m_imageCache.end()) {
        const DecodedImage* image = decodedImage(normalizedPath);
        if(image == nullptr) {
            return;
        }
        it = m_imageCache.find(normalizedPath);
        if(it == m_imageCache.end()) {
            return;
        }
    }
    it->second.pinned = true;
    it->second.lastUsedFrame = m_lastFrameConditionUpdate == UINT32_MAX ? 0u : m_lastFrameConditionUpdate;
}

void ModManager::evictUnusedDynamicAssets(uint32_t frameCount)
{
    constexpr uint32_t DynamicAssetEvictionFrames = 3600;
    for(auto it = m_imageCache.begin(); it != m_imageCache.end(); ) {
        const ImageCacheEntry& entry = it->second;
        if(entry.pinned || frameCount - entry.lastUsedFrame <= DynamicAssetEvictionFrames) {
            ++it;
        } else {
            it = m_imageCache.erase(it);
        }
    }
}

std::optional<ModManager::DebugComposePixel> ModManager::debugComposePixel(const uint32_t* sourceFramebuffer, const ChrRenderSnapshot& snapshot, int scale, int nesX, int nesY, const std::string& filterText)
{
    std::scoped_lock runtimeLock(m_runtimeMutex);

    if(sourceFramebuffer == nullptr || scale <= 0 ||
       nesX < 0 || nesX >= PPU::SCREEN_WIDTH ||
       nesY < 0 || nesY >= PPU::SCREEN_HEIGHT) {
        return std::nullopt;
    }

    const FrameConditionState& frameConditionState =
        snapshot.frameConditionStateView != nullptr ? *snapshot.frameConditionStateView : snapshot.frameConditionState;

    struct PreparedOverride {
        const ChrOverride* override = nullptr;
        const DecodedImage* image = nullptr;
        int sourceScale = 1;
        ChrOverride::SourceLayout wholeChrLayout = ChrOverride::SourceLayout::PatternTables;
        bool hasDynamicConditions = false;
        std::vector<const MemoryCondition*> runtimeConditions;
        size_t sequence = 0;
    };

    struct PreparedBackground {
        const BackgroundReplacement* replacement = nullptr;
        const DecodedImage* image = nullptr;
        int priority = 0;
        int backgroundScale = 1;
        int alphaScale = 255;
        bool opaqueCopy = false;
        int bgScrollX = 0;
        int bgScrollY = 0;
        std::vector<const MemoryCondition*> runtimeConditions;
    };

    auto appendRuntimeConditions = [](const std::vector<MemoryCondition>& conditions, auto& runtimeConditions) {
        runtimeConditions.clear();
        runtimeConditions.reserve(conditions.size());
        for(const MemoryCondition& condition : conditions) {
            if(condition.kind != MemoryCondition::Kind::MemoryCheck &&
               condition.kind != MemoryCondition::Kind::FrameRange) {
                runtimeConditions.push_back(&condition);
            }
        }
    };

    auto scrollXForLine = [&](int y) {
        return (y >= 0 && y < PPU::SCREEN_HEIGHT) ? snapshot.scrollXByLine[static_cast<size_t>(y)] : snapshot.scrollX;
    };
    auto scrollYForLine = [&](int y) {
        return (y >= 0 && y < PPU::SCREEN_HEIGHT) ? snapshot.scrollYByLine[static_cast<size_t>(y)] : snapshot.scrollY;
    };

    std::vector<const ChrOverride*> activeOverrides;
    activeOverrides.reserve(m_chrOverrides.size());
    for(const ChrOverride& override : m_chrOverrides) {
        if(override.assetAvailable && !override.assetPath.empty()) {
            activeOverrides.push_back(&override);
        }
    }
    if(!activeOverrides.empty()) {
        std::stable_sort(activeOverrides.begin(), activeOverrides.end(), [](const ChrOverride* a, const ChrOverride* b) {
            return a->priority > b->priority;
        });
    }

    std::vector<PreparedOverride> preparedOverrides;
    preparedOverrides.reserve(activeOverrides.size());
    std::array<std::vector<const PreparedOverride*>, 512> overridesByFullTile;
    std::array<std::vector<const PreparedOverride*>, 256> overridesByRelativeTile;
    std::array<std::vector<const PreparedOverride*>, 512> dynamicOverridesByFullTile;
    std::array<std::vector<const PreparedOverride*>, 256> dynamicOverridesByRelativeTile;
    std::unordered_map<uint32_t, std::vector<const PreparedOverride*>> overridesByChrHash;
    std::unordered_map<uint32_t, std::vector<const PreparedOverride*>> dynamicOverridesByChrHash;
    std::vector<const PreparedOverride*> wholeChrOverrides;
    std::vector<const PreparedOverride*> dynamicWholeChrOverrides;
    for(const ChrOverride* override : activeOverrides) {
        PreparedOverride prepared;
        prepared.override = override;
        prepared.sequence = preparedOverrides.size();
        prepared.image = decodedImage(override->assetPath);
        if(prepared.image == nullptr || prepared.image->rgba.empty()) {
            continue;
        }
        prepared.sourceScale = std::max(1, m_resolutionMultiplier);
        if(!override->wholeChr() && !override->hasSourcePosition() && override->sourceLayout != ChrOverride::SourceLayout::PatternTables) {
            const int scaleX = prepared.image->width / std::max(1, override->columns * 8);
            if(scaleX > 0) {
                prepared.sourceScale = scaleX;
            }
        }
        prepared.wholeChrLayout =
            override->sourceLayout == ChrOverride::SourceLayout::Auto
                ? ChrOverride::SourceLayout::PatternTables
                : override->sourceLayout;
        appendRuntimeConditions(override->conditions, prepared.runtimeConditions);
        prepared.hasDynamicConditions = std::any_of(
            prepared.runtimeConditions.begin(),
            prepared.runtimeConditions.end(),
            [](const MemoryCondition* condition) {
                return condition != nullptr &&
                       (condition->kind == MemoryCondition::Kind::TileAtPosition ||
                        condition->kind == MemoryCondition::Kind::TileNearby ||
                        condition->kind == MemoryCondition::Kind::SpriteAtPosition ||
                        condition->kind == MemoryCondition::Kind::SpriteNearby);
            });
        preparedOverrides.push_back(prepared);
        const PreparedOverride* preparedPtr = &preparedOverrides.back();
        if(override->wholeChr()) {
            if(override->hasChrHash) {
                if(preparedPtr->hasDynamicConditions) {
                    dynamicOverridesByChrHash[override->chrHash].push_back(preparedPtr);
                } else {
                    overridesByChrHash[override->chrHash].push_back(preparedPtr);
                }
            } else if(preparedPtr->hasDynamicConditions) {
                dynamicWholeChrOverrides.push_back(preparedPtr);
            } else {
                wholeChrOverrides.push_back(preparedPtr);
            }
        } else {
            if(override->absoluteTile) {
                if(override->tile >= 0 && override->tile < static_cast<int>(overridesByFullTile.size())) {
                    if(preparedPtr->hasDynamicConditions) {
                        dynamicOverridesByFullTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    } else {
                        overridesByFullTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    }
                }
            } else if(override->tile >= 0) {
                if(override->tile < static_cast<int>(overridesByRelativeTile.size())) {
                    if(preparedPtr->hasDynamicConditions) {
                        dynamicOverridesByRelativeTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    } else {
                        overridesByRelativeTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    }
                } else if(override->tile < static_cast<int>(overridesByFullTile.size())) {
                    if(preparedPtr->hasDynamicConditions) {
                        dynamicOverridesByFullTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    } else {
                        overridesByFullTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    }
                }
            }
        }
    }

    std::vector<PreparedBackground> preparedBackgrounds;
    preparedBackgrounds.reserve(m_backgroundReplacements.size());
    for(const BackgroundReplacement& replacement : m_backgroundReplacements) {
        if(!replacement.assetAvailable || replacement.assetPath.empty()) {
            continue;
        }
        const DecodedImage* image = decodedImage(replacement.assetPath);
        if(image == nullptr || image->rgba.empty()) {
            continue;
        }
        PreparedBackground prepared;
        prepared.replacement = &replacement;
        prepared.image = image;
        prepared.priority = std::clamp(replacement.priority, 0, 39);
        prepared.backgroundScale = std::max(1, m_resolutionMultiplier);
        prepared.alphaScale = std::clamp(static_cast<int>(std::round(replacement.opacity * 255.0f)), 0, 255);
        prepared.opaqueCopy = prepared.alphaScale >= 255;
        prepared.bgScrollX = static_cast<int>(scrollXForLine(0) * replacement.parallaxX) + replacement.scrollX;
        prepared.bgScrollY = static_cast<int>(scrollYForLine(0) * replacement.parallaxY) + replacement.scrollY;
        appendRuntimeConditions(replacement.conditions, prepared.runtimeConditions);
        preparedBackgrounds.push_back(prepared);
    }

    auto tileHash = [&](int tileIndex) {
        if(tileIndex < 0) {
            return 0u;
        }
        if(tileIndex <= 0x01FF) {
            return snapshot.tileHashes[static_cast<size_t>(tileIndex)];
        }
        if(tileIndex < static_cast<int>(m_chrRomTileHashes.size())) {
            return m_chrRomTileHashes[static_cast<size_t>(tileIndex)];
        }
        return 0u;
    };
    auto canonicalTileIndex = [&](int tileIndex, uint32_t currentHash) {
        if(tileIndex > 0x01FF) {
            return tileIndex;
        }
        if(currentHash != 0) {
            if(const auto it = m_chrRomCanonicalTileByHash.find(currentHash); it != m_chrRomCanonicalTileByHash.end()) {
                return it->second;
            }
        }
        return tileIndex;
    };

    const std::string normalizedFilter = toLower(filterText);
    auto debugAssetMatchesFilter = [&](const std::string& assetPath) {
        return normalizedFilter.empty() || toLower(assetPath).find(normalizedFilter) != std::string::npos;
    };

    auto backgroundPixelAt = [&](int x, int y) -> const PPU::DebugModBackgroundPixel* {
        if(x < 0 || x >= PPU::SCREEN_WIDTH || y < 0 || y >= PPU::SCREEN_HEIGHT) {
            return nullptr;
        }
        const size_t index = static_cast<size_t>(y) * PPU::SCREEN_WIDTH + static_cast<size_t>(x);
        if(snapshot.backgroundPixelsView != nullptr) {
            if(index >= snapshot.backgroundPixelsViewCount) {
                return nullptr;
            }
            return &snapshot.backgroundPixelsView[index];
        }
        if(index >= snapshot.backgroundPixels.size()) {
            return nullptr;
        }
        return &snapshot.backgroundPixels[index];
    };

    auto spritePixelAt = [&](int x, int y) -> const PPU::DebugModSpritePixel* {
        if(x < 0 || x >= PPU::SCREEN_WIDTH || y < 0 || y >= PPU::SCREEN_HEIGHT) {
            return nullptr;
        }
        const size_t index = static_cast<size_t>(y) * PPU::SCREEN_WIDTH + static_cast<size_t>(x);
        if(snapshot.spritePixelsView != nullptr) {
            if(index >= snapshot.spritePixelsViewCount) {
                return nullptr;
            }
            return &snapshot.spritePixelsView[index];
        }
        if(index >= snapshot.spritePixels.size()) {
            return nullptr;
        }
        return &snapshot.spritePixels[index];
    };

    struct ConditionContext {
        int nesX = 0;
        int nesY = 0;
        const PPU::DebugModBackgroundPixel* backgroundPixel = nullptr;
        const PPU::DebugModSpriteCandidate* spriteCandidate = nullptr;
    };

    auto paletteMatches = [](const ChrOverride& override, const std::array<uint8_t, 3>& palette, bool allowDefaultTileFallback) {
        if(override.paletteIndices.empty()) {
            return true;
        }
        if(override.defaultTile && allowDefaultTileFallback) {
            return true;
        }
        if(override.exactPaletteOrder) {
            if(palette.size() < override.paletteIndices.size()) {
                return false;
            }
            for(size_t i = 0; i < override.paletteIndices.size(); ++i) {
                if(palette[i] != override.paletteIndices[i]) {
                    return false;
                }
            }
            return true;
        }
        for(uint8_t expected : override.paletteIndices) {
            if(std::find(palette.begin(), palette.end(), expected) == palette.end()) {
                return false;
            }
        }
        return true;
    };

    auto paletteVectorMatches = [](const std::vector<uint8_t>& expectedPalette, const uint8_t palette[3]) {
        if(expectedPalette.empty()) {
            return true;
        }
        if(expectedPalette.size() > 3) {
            return false;
        }
        for(size_t i = 0; i < expectedPalette.size(); ++i) {
            if(expectedPalette[i] != palette[i]) {
                return false;
            }
        }
        return true;
    };

    auto backgroundPaletteKey = [&](const PPU::DebugModBackgroundPixel& pixel) -> uint32_t {
        return (static_cast<uint32_t>(snapshot.universalBgColor & 0x3F) << 24u) |
               (static_cast<uint32_t>(pixel.palette[0] & 0x3F) << 16u) |
               (static_cast<uint32_t>(pixel.palette[1] & 0x3F) << 8u) |
               static_cast<uint32_t>(pixel.palette[2] & 0x3F);
    };

    auto spritePaletteKey = [](const PPU::DebugModSpriteCandidate& candidate) -> uint32_t {
        return 0xFF000000u |
               (static_cast<uint32_t>(candidate.palette[0] & 0x3F) << 16u) |
               (static_cast<uint32_t>(candidate.palette[1] & 0x3F) << 8u) |
               static_cast<uint32_t>(candidate.palette[2] & 0x3F);
    };

    auto tileMatchesCondition = [&](const MemoryCondition& condition, const PPU::DebugModBackgroundPixel& pixel) {
        if(!pixel.valid) {
            return false;
        }
        if(condition.expectedPaletteKey != 0 && backgroundPaletteKey(pixel) != condition.expectedPaletteKey) {
            return false;
        }
        if(condition.hasExpectedTile) {
            const uint32_t pixelHash = pixel.tileHash != 0 ? pixel.tileHash : tileHash(pixel.tileIndex);
            if(condition.expectedTileByHash) {
                if(pixelHash != condition.expectedTileHash) {
                    return false;
                }
            } else if(condition.expectedTileIndex >= 0 &&
                      canonicalTileIndex(static_cast<int>(pixel.tileIndex), pixelHash) != condition.expectedTileIndex) {
                return false;
            }
        }
        return condition.expectedPaletteKey != 0 ? true : paletteVectorMatches(condition.expectedPalette, pixel.palette);
    };

    auto spriteCandidateMatchesCondition = [&](const MemoryCondition& condition, const PPU::DebugModSpriteCandidate& candidate) {
        if(!candidate.valid) {
            return false;
        }
        if(condition.expectedPaletteKey != 0 && spritePaletteKey(candidate) != condition.expectedPaletteKey) {
            return false;
        }
        if(condition.hasExpectedTile) {
            const uint32_t candidateHash = candidate.tileHash != 0 ? candidate.tileHash : tileHash(candidate.tileIndex);
            if(condition.expectedTileByHash) {
                if(candidateHash != condition.expectedTileHash) {
                    return false;
                }
            } else if(condition.expectedTileIndex >= 0 &&
                      canonicalTileIndex(static_cast<int>(candidate.tileIndex), candidateHash) != condition.expectedTileIndex) {
                return false;
            }
        }
        return condition.expectedPaletteKey != 0 ? true : paletteVectorMatches(condition.expectedPalette, candidate.palette);
    };

    auto conditionMatchesAt = [&](const MemoryCondition& condition, const ConditionContext& ctx) {
        MODMANAGER_PROFILE_SCOPE(ComposeChrFrameConditionMatches);
        MODMANAGER_PROFILE_COUNT(conditionMatchesCalls, 1);
        bool match = false;
        switch(condition.kind) {
        case MemoryCondition::Kind::MemoryCheck: {
            const uint32_t actual =
                condition.readIndex < frameConditionState.memoryValues.size()
                    ? frameConditionState.memoryValues[condition.readIndex]
                    : 0u;
            const uint32_t expected =
                condition.compareAgainstMemory && condition.rhsReadIndex < frameConditionState.memoryValues.size()
                    ? frameConditionState.memoryValues[condition.rhsReadIndex]
                    : condition.value;
            match = evaluateMemoryCondition(condition, actual, expected);
            break;
        }
        case MemoryCondition::Kind::FrameRange: {
            const uint32_t range = std::max(1u, condition.value);
            match = (frameConditionState.frameCount % range) >= condition.address;
            break;
        }
        case MemoryCondition::Kind::TileAtPosition:
        case MemoryCondition::Kind::TileNearby: {
            int targetX = condition.x;
            int targetY = condition.y;
            if(condition.kind == MemoryCondition::Kind::TileNearby) {
                targetX = ctx.nesX + condition.x;
                targetY = ctx.nesY + condition.y;
            }
            const PPU::DebugModBackgroundPixel* pixel = backgroundPixelAt(targetX, targetY);
            match = pixel != nullptr && tileMatchesCondition(condition, *pixel);
            break;
        }
        case MemoryCondition::Kind::SpriteAtPosition:
        case MemoryCondition::Kind::SpriteNearby: {
            int xSign = 1;
            int ySign = 1;
            if(ctx.spriteCandidate != nullptr) {
                xSign = ctx.spriteCandidate->horizontalMirror ? -1 : 1;
                ySign = ctx.spriteCandidate->verticalMirror ? -1 : 1;
            }
            int targetX = condition.x;
            int targetY = condition.y;
            if(condition.kind == MemoryCondition::Kind::SpriteNearby) {
                targetX = ctx.nesX + condition.x * xSign;
                targetY = ctx.nesY + condition.y * ySign;
            }
            const PPU::DebugModSpritePixel* pixel = spritePixelAt(targetX, targetY);
            if(pixel != nullptr) {
                for(uint8_t i = 0; i < pixel->count; ++i) {
                    if(spriteCandidateMatchesCondition(condition, pixel->candidates[static_cast<size_t>(i)])) {
                        match = true;
                        break;
                    }
                }
            }
            break;
        }
        case MemoryCondition::Kind::SpritePaletteIndex:
            match = ctx.spriteCandidate != nullptr && ctx.spriteCandidate->paletteSlot == static_cast<uint8_t>(condition.value & 0x03u);
            break;
        }
        return condition.inverted ? !match : match;
    };

    auto describeCondition = [&](const MemoryCondition& condition, bool match, const ConditionContext& ctx) -> std::string {
        std::ostringstream out;
        const std::string name = condition.debugName.empty() ? "unnamed condition" : condition.debugName;
        out << name;

        switch(condition.kind) {
        case MemoryCondition::Kind::MemoryCheck: {
            const uint32_t actual =
                condition.readIndex < frameConditionState.memoryValues.size()
                    ? frameConditionState.memoryValues[condition.readIndex]
                    : 0u;
            const uint32_t expected =
                condition.compareAgainstMemory && condition.rhsReadIndex < frameConditionState.memoryValues.size()
                    ? frameConditionState.memoryValues[condition.rhsReadIndex]
                    : condition.value;
            out << " (" << condition.memoryType << "[$"
                << std::uppercase << std::hex << condition.address
                << "]=" << actual
                << " " << condition.op << " ";
            if(condition.compareAgainstMemory) {
                out << condition.rhsMemoryType << "[$" << condition.rhsAddress << "]=" << expected;
            } else {
                out << expected;
            }
            if(condition.hasMask) {
                out << " mask " << condition.mask;
            }
            out << std::dec << ")";
            break;
        }
        case MemoryCondition::Kind::FrameRange:
            out << " (frame " << frameConditionState.frameCount
                << " in range " << condition.address << ".." << std::max(1u, condition.value) << ")";
            break;
        case MemoryCondition::Kind::TileAtPosition:
        case MemoryCondition::Kind::TileNearby: {
            int targetX = condition.x;
            int targetY = condition.y;
            if(condition.kind == MemoryCondition::Kind::TileNearby) {
                targetX = ctx.nesX + condition.x;
                targetY = ctx.nesY + condition.y;
            }
            out << " (tile @" << targetX << "," << targetY << ")";
            break;
        }
        case MemoryCondition::Kind::SpriteAtPosition:
        case MemoryCondition::Kind::SpriteNearby: {
            int xSign = 1;
            int ySign = 1;
            if(ctx.spriteCandidate != nullptr) {
                xSign = ctx.spriteCandidate->horizontalMirror ? -1 : 1;
                ySign = ctx.spriteCandidate->verticalMirror ? -1 : 1;
            }
            int targetX = condition.x;
            int targetY = condition.y;
            if(condition.kind == MemoryCondition::Kind::SpriteNearby) {
                targetX = ctx.nesX + condition.x * xSign;
                targetY = ctx.nesY + condition.y * ySign;
            }
            out << " (sprite @" << targetX << "," << targetY << ")";
            break;
        }
        case MemoryCondition::Kind::SpritePaletteIndex:
            out << " (sprite palette slot " << condition.value << ")";
            break;
        }

        out << (match ? " matched" : " failed");
        return out.str();
    };

    auto firstFailedGlobalCondition = [&](const std::vector<MemoryCondition>& conditions) -> std::optional<std::string> {
        const ConditionContext emptyContext = {};
        for(const MemoryCondition& condition : conditions) {
            if(condition.kind != MemoryCondition::Kind::MemoryCheck &&
               condition.kind != MemoryCondition::Kind::FrameRange) {
                continue;
            }
            const bool match = conditionMatchesAt(condition, emptyContext);
            if(!match) {
                return describeCondition(condition, false, emptyContext);
            }
        }
        return std::nullopt;
    };

    auto firstFailedConditionName = [&](const std::vector<MemoryCondition>& conditions, const ConditionContext& ctx) -> std::optional<std::string> {
        for(const MemoryCondition& condition : conditions) {
            const bool match = conditionMatchesAt(condition, ctx);
            if(!match) {
                return describeCondition(condition, false, ctx);
            }
        }
        return std::nullopt;
    };

    auto firstFailedRuntimeConditionName = [&](const std::vector<const MemoryCondition*>& conditions, const ConditionContext& ctx) -> std::optional<std::string> {
        for(const MemoryCondition* condition : conditions) {
            if(condition != nullptr && !conditionMatchesAt(*condition, ctx)) {
                return describeCondition(*condition, false, ctx);
            }
        }
        return std::nullopt;
    };

    auto matchesRequirement = [](int requirement, bool value) {
        return requirement == 0 || (requirement > 0 && value) || (requirement < 0 && !value);
    };

    auto matchesOverride = [&](const PreparedOverride& preparedOverride, ChrOverride::Target target, bool allowDefaultTileFallback, int tileIndex, int fullTileIndex, int currentPatternTable, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority, const ConditionContext& ctx) {
        MODMANAGER_PROFILE_COUNT(matchesOverrideCalls, 1);
        const ChrOverride& override = *preparedOverride.override;
        if(!override.enabled) {
            return false;
        }
        if(override.target != ChrOverride::Target::Both && override.target != target) {
            return false;
        }
        if(!matchesRequirement(override.hMirrorRequirement, hMirror) ||
           !matchesRequirement(override.vMirrorRequirement, vMirror) ||
           !matchesRequirement(override.bgPriorityRequirement, bgPriority)) {
            return false;
        }
        if(override.patternTable >= 0 && override.patternTable != currentPatternTable) {
            return false;
        }
        if(!override.wholeChr()) {
            if(override.absoluteTile) {
                if(override.tile != fullTileIndex) {
                    return false;
                }
            } else if(override.tile != tileIndex && override.tile != fullTileIndex) {
                return false;
            }
        }
        const uint32_t currentTileHash =
            target == ChrOverride::Target::Sprite
                ? ((ctx.spriteCandidate != nullptr && ctx.spriteCandidate->tileHash != 0) ? ctx.spriteCandidate->tileHash : tileHash(fullTileIndex))
                : ((ctx.backgroundPixel != nullptr && ctx.backgroundPixel->tileHash != 0) ? ctx.backgroundPixel->tileHash : tileHash(fullTileIndex));
        if(override.hasChrHash && currentTileHash != override.chrHash) {
            return false;
        }
        if(!paletteMatches(override, palette, allowDefaultTileFallback)) {
            return false;
        }
        for(const MemoryCondition* condition : preparedOverride.runtimeConditions) {
            if(condition != nullptr && !conditionMatchesAt(*condition, ctx)) {
                return false;
            }
        }
        return true;
    };

    auto scanMergedCandidates = [&](const std::vector<const PreparedOverride*>& staticCandidates, const std::vector<const PreparedOverride*>& dynamicCandidates, ChrOverride::Target target, bool allowDefaultTileFallback, int tileIndex, int fullTileIndex, int currentPatternTable, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority, const ConditionContext& ctx) -> const PreparedOverride* {
        size_t staticIndex = 0;
        size_t dynamicIndex = 0;
        while(staticIndex < staticCandidates.size() || dynamicIndex < dynamicCandidates.size()) {
            const PreparedOverride* candidate = nullptr;
            if(dynamicIndex >= dynamicCandidates.size()) {
                candidate = staticCandidates[staticIndex++];
            } else if(staticIndex >= staticCandidates.size()) {
                candidate = dynamicCandidates[dynamicIndex++];
            } else if(staticCandidates[staticIndex]->sequence <= dynamicCandidates[dynamicIndex]->sequence) {
                candidate = staticCandidates[staticIndex++];
            } else {
                candidate = dynamicCandidates[dynamicIndex++];
            }

            if(matchesOverride(*candidate, target, allowDefaultTileFallback, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx)) {
                return candidate;
            }
        }
        return nullptr;
    };

    auto findOverride = [&](ChrOverride::Target target, int tileIndex, int fullTileIndex, int currentPatternTable, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority, const ConditionContext& ctx) -> const PreparedOverride* {
        const uint32_t lookupHash = tileHash(fullTileIndex);

        auto scanFullTile = [&](int lookupTile, bool allowDefaultTileFallback) -> const PreparedOverride* {
            if(lookupTile >= 0 && lookupTile < static_cast<int>(overridesByFullTile.size())) {
                return scanMergedCandidates(
                    overridesByFullTile[static_cast<size_t>(lookupTile)],
                    dynamicOverridesByFullTile[static_cast<size_t>(lookupTile)],
                    target, allowDefaultTileFallback, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx);
            }
            return nullptr;
        };

        auto scanRelativeTile = [&](int lookupTile, bool allowDefaultTileFallback) -> const PreparedOverride* {
            if(lookupTile >= 0 && lookupTile < static_cast<int>(overridesByRelativeTile.size())) {
                return scanMergedCandidates(
                    overridesByRelativeTile[static_cast<size_t>(lookupTile)],
                    dynamicOverridesByRelativeTile[static_cast<size_t>(lookupTile)],
                    target, allowDefaultTileFallback, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx);
            }
            return nullptr;
        };

        auto scanHash = [&](bool allowDefaultTileFallback) -> const PreparedOverride* {
            static const std::vector<const PreparedOverride*> emptyCandidates;
            const auto staticIt = overridesByChrHash.find(lookupHash);
            const auto dynamicIt = dynamicOverridesByChrHash.find(lookupHash);
            const auto& staticCandidates = staticIt != overridesByChrHash.end() ? staticIt->second : emptyCandidates;
            const auto& dynamicCandidates = dynamicIt != dynamicOverridesByChrHash.end() ? dynamicIt->second : emptyCandidates;
            return scanMergedCandidates(staticCandidates, dynamicCandidates, target, allowDefaultTileFallback, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx);
        };

        auto scanWhole = [&](bool allowDefaultTileFallback) -> const PreparedOverride* {
            return scanMergedCandidates(wholeChrOverrides, dynamicWholeChrOverrides, target, allowDefaultTileFallback, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx);
        };

        if(const PreparedOverride* found = scanFullTile(fullTileIndex, false)) return found;
        if(fullTileIndex != tileIndex) {
            if(const PreparedOverride* found = scanRelativeTile(tileIndex, false)) return found;
        }
        if(const PreparedOverride* found = scanHash(false)) return found;
        if(const PreparedOverride* found = scanWhole(false)) return found;

        if(const PreparedOverride* found = scanFullTile(fullTileIndex, true)) return found;
        if(const PreparedOverride* found = scanHash(true)) return found;
        if(fullTileIndex == tileIndex) {
            if(const PreparedOverride* found = scanRelativeTile(tileIndex, true)) return found;
        }
        return scanWhole(true);
    };

    auto explainOverrideMismatch = [&](const PreparedOverride& preparedOverride, ChrOverride::Target target, bool allowDefaultTileFallback, int tileIndex, int fullTileIndex, int currentPatternTable, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority, const ConditionContext& ctx) -> std::string {
        const ChrOverride& override = *preparedOverride.override;
        if(override.target != ChrOverride::Target::Both && override.target != target) {
            return "target mismatch";
        }
        if(!matchesRequirement(override.hMirrorRequirement, hMirror)) {
            return "hmirror mismatch";
        }
        if(!matchesRequirement(override.vMirrorRequirement, vMirror)) {
            return "vmirror mismatch";
        }
        if(!matchesRequirement(override.bgPriorityRequirement, bgPriority)) {
            return "bgpriority mismatch";
        }
        if(override.patternTable >= 0 && override.patternTable != currentPatternTable) {
            return "pattern table mismatch";
        }
        if(!override.wholeChr()) {
            if(override.absoluteTile) {
                if(override.tile != fullTileIndex) {
                    return "absolute tile mismatch";
                }
            } else if(override.tile != tileIndex && override.tile != fullTileIndex) {
                return "relative tile mismatch";
            }
        }
        const uint32_t currentTileHash =
            target == ChrOverride::Target::Sprite
                ? ((ctx.spriteCandidate != nullptr && ctx.spriteCandidate->tileHash != 0) ? ctx.spriteCandidate->tileHash : tileHash(fullTileIndex))
                : ((ctx.backgroundPixel != nullptr && ctx.backgroundPixel->tileHash != 0) ? ctx.backgroundPixel->tileHash : tileHash(fullTileIndex));
        if(override.hasChrHash && currentTileHash != override.chrHash) {
            return "tile hash mismatch";
        }
        if(!paletteMatches(override, palette, allowDefaultTileFallback)) {
            return "palette mismatch";
        }
        if(const auto failed = firstFailedConditionName(override.conditions, ctx); failed.has_value()) {
            return "condition failed: " + *failed;
        }
        return {};
    };

    DebugComposePixel result;
    result.valid = true;
    result.baseColor = sourceFramebuffer[static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH + static_cast<size_t>(nesX)];
    result.finalColor = m_disableOriginalTiles ? 0x00000000u : result.baseColor;

    std::array<int, 40> backgroundPrunedCounts = {};
    for(const BackgroundReplacement& replacement : m_backgroundReplacements) {
        if(replacement.assetPath.empty()) {
            continue;
        }
        const int priority = std::clamp(replacement.priority, 0, 39);
        if(const auto failed = firstFailedGlobalCondition(replacement.conditions); failed.has_value()) {
            if(backgroundPrunedCounts[static_cast<size_t>(priority)] >= 12 && !debugAssetMatchesFilter(replacement.assetPath)) {
                continue;
            }
            DebugComposeStage stage;
            stage.valid = true;
            stage.stage = "bg pruned";
            stage.assetPath = replacement.assetPath;
            stage.priority = priority;
            stage.returnedBaseColor = true;
            stage.reason = "pruned in onFrame: " + *failed;
            result.backgroundCandidates.push_back(std::move(stage));
            backgroundPrunedCounts[static_cast<size_t>(priority)]++;
        }
    }

    std::array<const PreparedBackground*, 40> activeBackgroundsByPriority = {};
    std::array<int, 40> backgroundDebugCounts = {};
    const ConditionContext backgroundConditionContext = {};
    for(const PreparedBackground& prepared : preparedBackgrounds) {
        const BackgroundReplacement* replacement = prepared.replacement;
        if(replacement == nullptr) {
            continue;
        }
        const int priority = std::clamp(replacement->priority, 0, 39);
        DebugComposeStage debugStage;
        debugStage.valid = true;
        debugStage.stage = "bg select";
        debugStage.assetPath = replacement->assetPath;
        debugStage.priority = priority;

        if(!replacement->enabled) {
            debugStage.returnedBaseColor = true;
            debugStage.reason = "disabled by frame conditions";
            if(backgroundDebugCounts[static_cast<size_t>(priority)] < 6 || debugAssetMatchesFilter(replacement->assetPath)) {
                result.backgroundCandidates.push_back(std::move(debugStage));
                if(backgroundDebugCounts[static_cast<size_t>(priority)] < 6) {
                    backgroundDebugCounts[static_cast<size_t>(priority)]++;
                }
            }
            continue;
        }

        if(activeBackgroundsByPriority[static_cast<size_t>(priority)] != nullptr) {
            debugStage.returnedBaseColor = true;
            debugStage.reason = "skipped after earlier selected background";
            if(backgroundDebugCounts[static_cast<size_t>(priority)] < 6 || debugAssetMatchesFilter(replacement->assetPath)) {
                result.backgroundCandidates.push_back(std::move(debugStage));
                if(backgroundDebugCounts[static_cast<size_t>(priority)] < 6) {
                    backgroundDebugCounts[static_cast<size_t>(priority)]++;
                }
            }
            continue;
        }

        if(const auto failed = firstFailedRuntimeConditionName(prepared.runtimeConditions, backgroundConditionContext); failed.has_value()) {
            debugStage.returnedBaseColor = true;
            debugStage.reason = "condition failed: " + *failed;
            if(backgroundDebugCounts[static_cast<size_t>(priority)] < 6 || debugAssetMatchesFilter(prepared.replacement->assetPath)) {
                result.backgroundCandidates.push_back(std::move(debugStage));
                if(backgroundDebugCounts[static_cast<size_t>(priority)] < 6) {
                    backgroundDebugCounts[static_cast<size_t>(priority)]++;
                }
            }
        } else {
            debugStage.returnedBaseColor = false;
            debugStage.reason = "selected for priority";
            result.backgroundCandidates.push_back(debugStage);
            backgroundDebugCounts[static_cast<size_t>(priority)]++;
            activeBackgroundsByPriority[static_cast<size_t>(priority)] = &prepared;
        }
    }

    const size_t pixelIndex = static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH + static_cast<size_t>(nesX);
    const PPU::DebugModBackgroundPixel* bgPixel =
        snapshot.backgroundPixelsView != nullptr
            ? (pixelIndex < snapshot.backgroundPixelsViewCount ? &snapshot.backgroundPixelsView[pixelIndex] : nullptr)
            : (pixelIndex < snapshot.backgroundPixels.size() ? &snapshot.backgroundPixels[pixelIndex] : nullptr);
    const PPU::DebugModSpritePixel* spritePixel =
        snapshot.spritePixelsView != nullptr
            ? (pixelIndex < snapshot.spritePixelsViewCount ? &snapshot.spritePixelsView[pixelIndex] : nullptr)
            : (pixelIndex < snapshot.spritePixels.size() ? &snapshot.spritePixels[pixelIndex] : nullptr);
    bool hasAnyValidSpriteCandidate = false;
    if(spritePixel != nullptr) {
        for(int i = 0; i < static_cast<int>(spritePixel->count); ++i) {
            if(spritePixel->candidates[static_cast<size_t>(i)].valid) {
                hasAnyValidSpriteCandidate = true;
                break;
            }
        }
    }
    uint32_t color =
        m_disableOriginalTiles
            ? 0x00000000u
            : (hasAnyValidSpriteCandidate ? snapshot.paletteColors[snapshot.universalBgColor & 0x3Fu] : result.baseColor);
    result.finalColor = color;

    const int subX = 0;
    const int subY = 0;

    auto sampleBackgroundStage = [&](const PreparedBackground& prepared) -> DebugComposeStage {
        DebugComposeStage stage;
        stage.valid = true;
        stage.stage = "background";
        stage.assetPath = prepared.replacement->assetPath;
        stage.priority = prepared.replacement->priority;

        const BackgroundReplacement& replacement = *prepared.replacement;
        const DecodedImage& image = *prepared.image;
        if(image.width <= 0 || image.height <= 0) {
            stage.returnedBaseColor = true;
            stage.reason = "image empty";
            return stage;
        }

        const int backgroundScale = std::max(1, m_resolutionMultiplier);
        const int bgScrollX = static_cast<int>(scrollXForLine(nesY) * replacement.parallaxX) + replacement.scrollX;
        const int bgScrollY = static_cast<int>(scrollYForLine(nesY) * replacement.parallaxY) + replacement.scrollY;
        const int srcNesX = replacement.sourceX + nesX + bgScrollX;
        const int srcNesY = replacement.sourceY + nesY + bgScrollY;
        if(srcNesX < 0 || srcNesY < 0) {
            stage.returnedBaseColor = true;
            stage.reason = "out of bounds: negative source";
            return stage;
        }
        if((srcNesX + 1) * backgroundScale > image.width || (srcNesY + 1) * backgroundScale > image.height) {
            stage.returnedBaseColor = true;
            stage.reason = "out of bounds: exceeds image";
            return stage;
        }

        stage.srcX = srcNesX * backgroundScale + std::clamp(subX, 0, backgroundScale - 1);
        stage.srcY = srcNesY * backgroundScale + std::clamp(subY, 0, backgroundScale - 1);
        const size_t srcIndex = static_cast<size_t>(stage.srcY) * static_cast<size_t>(image.width) + static_cast<size_t>(stage.srcX);
        stage.rawRgba = image.rgba[srcIndex];
        const uint8_t srcAlpha = static_cast<uint8_t>((stage.rawRgba >> 24u) & 0xFFu);
        stage.returnedBaseColor = srcAlpha == 0;
        stage.reason = srcAlpha == 0 ? "alpha zero" : "blended background";
        return stage;
    };

    auto sampleOverrideStage = [&](uint32_t baseColor, uint32_t /*fallbackLayerColor*/, const PreparedOverride* prepared, int tileIndex, int offsetX, int offsetY, const std::array<uint8_t, 3>& palette, bool horizontalMirror, bool verticalMirror, bool preserveSourceAlpha, const char* stageName) -> std::pair<uint32_t, DebugComposeStage> {
        DebugComposeStage stage;
        stage.valid = true;
        stage.stage = stageName;
        if(prepared == nullptr) {
            stage.returnedBaseColor = true;
            stage.reason = "no matching override";
            return { baseColor, stage };
        }

        const ChrOverride* override = prepared->override;
        const DecodedImage* image = prepared->image;
        stage.assetPath = override->assetPath;
        stage.priority = override->priority;

        const int localOffsetX = offsetX & 0x07;
        const int localOffsetY = offsetY & 0x07;
        const int sourceScale = prepared->sourceScale;
        const int sourceSubX = std::clamp((subX * sourceScale) / scale, 0, sourceScale - 1);
        const int sourceSubY = std::clamp((subY * sourceScale) / scale, 0, sourceScale - 1);
        const int tileSubX = horizontalMirror ? (7 - localOffsetX) * sourceScale + (sourceScale - 1 - sourceSubX) : localOffsetX * sourceScale + sourceSubX;
        const int tileSubY = verticalMirror ? (7 - localOffsetY) * sourceScale + (sourceScale - 1 - sourceSubY) : localOffsetY * sourceScale + sourceSubY;
        int srcX = override->hasSourcePosition() ? override->sourceX + tileSubX : tileSubX;
        int srcY = override->hasSourcePosition() ? override->sourceY + tileSubY : tileSubY;
        const int tilePixelSize = 8 * sourceScale;
        const bool atlasImage =
            !override->hasSourcePosition() &&
            (override->wholeChr() ||
             override->sourceTileOffset > 0 ||
             (image->width >= override->columns * tilePixelSize && image->height > tilePixelSize));
        if(atlasImage) {
            int sourceColumn = 0;
            int sourceRow = 0;
            if(override->wholeChr()) {
                if(tileIndex < 0) {
                    stage.returnedBaseColor = true;
                    stage.reason = "whole-chr source but tile index invalid";
                    return { baseColor, stage };
                }
                const int sourceTile = tileIndex + override->sourceTileOffset;
                if(prepared->wholeChrLayout == ChrOverride::SourceLayout::PatternTables) {
                    const int table = sourceTile / 256;
                    const int tileInTable = sourceTile & 0xFF;
                    sourceColumn = (table * 16) + (tileInTable & 0x0F);
                    sourceRow = tileInTable >> 4;
                } else {
                    sourceColumn = sourceTile % override->columns;
                    sourceRow = sourceTile / override->columns;
                }
            } else if(override->sourceLayout == ChrOverride::SourceLayout::PatternTables) {
                const int sourceTile = override->sourceTileOffset;
                const int table = sourceTile / 256;
                const int tileInTable = sourceTile & 0xFF;
                sourceColumn = (table * 16) + (tileInTable & 0x0F);
                sourceRow = tileInTable >> 4;
            } else {
                const int sourceTile = override->sourceTileOffset;
                sourceColumn = sourceTile % override->columns;
                sourceRow = sourceTile / override->columns;
            }
            srcX += sourceColumn * tilePixelSize;
            srcY += sourceRow * tilePixelSize;
        }

        stage.srcX = srcX;
        stage.srcY = srcY;
        if(srcX < 0 || srcY < 0 || srcX >= image->width || srcY >= image->height) {
            stage.returnedBaseColor = true;
            stage.reason = "source coordinate out of bounds";
            return { baseColor, stage };
        }

        const size_t sourcePixelIndex = static_cast<size_t>(srcY) * static_cast<size_t>(image->width) + static_cast<size_t>(srcX);
        stage.rawRgba = image->rgba[sourcePixelIndex];
        const uint8_t sourceAlpha = static_cast<uint8_t>((stage.rawRgba >> 24u) & 0xFFu);
        if(override->ignorePalette) {
            if(sourceAlpha == 0) {
                stage.returnedBaseColor = true;
                stage.reason = preserveSourceAlpha ? "raw rgba alpha zero -> no sprite draw" : "raw rgba alpha zero";
                return { baseColor, stage };
            }
            const uint32_t opaqueSource = (stage.rawRgba & 0x00FFFFFFu) | 0xFF000000u;
            if(!preserveSourceAlpha) {
                stage.reason = "raw rgba opaque replace";
                return { opaqueSource, stage };
            }
            if(sourceAlpha == 0xFF) {
                stage.reason = "raw rgba opaque replace";
                return { opaqueSource, stage };
            }
            stage.reason = "raw rgba alpha blend";
            return { blendPixel(baseColor, opaqueSource, sourceAlpha), stage };
        }

        if(sourceAlpha == 0) {
            stage.returnedBaseColor = true;
            stage.reason = preserveSourceAlpha ? "indexed source alpha zero -> no sprite draw" : "indexed source alpha zero";
            return { baseColor, stage };
        }
        if(!image->indexedFourColor || image->indexedPixels.size() != image->rgba.size()) {
            stage.returnedBaseColor = true;
            stage.reason = preserveSourceAlpha ? "not indexed four-color png -> no sprite draw" : "not indexed four-color png";
            return { baseColor, stage };
        }

        const uint8_t sourcePaletteIndex = image->indexedPixels[sourcePixelIndex];
        stage.indexedPaletteIndex = static_cast<int>(sourcePaletteIndex);
        if(sourcePaletteIndex == 0) {
            if(preserveSourceAlpha) {
                stage.returnedBaseColor = true;
                stage.reason = "indexed color 0 -> no sprite draw";
                return { baseColor, stage };
            }
            const uint32_t mappedColor =
                (snapshot.paletteColors[snapshot.universalBgColor & 0x3F] & 0x00FFFFFFu) |
                (static_cast<uint32_t>(sourceAlpha) << 24u);
            stage.reason = "indexed color 0 -> universal background";
            return { blendPixel(baseColor, mappedColor, 255), stage };
        }

        const int paletteIndex = static_cast<int>(sourcePaletteIndex) - 1;
        if(paletteIndex < 0 || paletteIndex >= static_cast<int>(palette.size())) {
            stage.returnedBaseColor = true;
            stage.reason = "indexed color outside palette mapping";
            return { baseColor, stage };
        }

        const uint32_t mappedColor =
            (snapshot.paletteColors[palette[static_cast<size_t>(paletteIndex)] & 0x3F] & 0x00FFFFFFu) |
            (static_cast<uint32_t>(sourceAlpha) << 24u);
        stage.reason = "indexed color mapped to NES palette";
        return { blendPixel(baseColor, mappedColor, 255), stage };
    };

    for(int priority = 0; priority < 10; ++priority) {
        if(const PreparedBackground* preparedBackground = activeBackgroundsByPriority[static_cast<size_t>(priority)]; preparedBackground != nullptr) {
            DebugComposeStage stage = sampleBackgroundStage(*preparedBackground);
            if(!stage.returnedBaseColor && stage.srcX >= 0 && stage.srcY >= 0) {
                color = blendPixel(color, stage.rawRgba, std::clamp(static_cast<int>(std::round(preparedBackground->replacement->opacity * 255.0f)), 0, 255));
            }
            result.backgroundStages.push_back(stage);
        }
    }

    const PreparedOverride* backgroundOverride = nullptr;
    uint32_t backgroundFallbackColor = result.baseColor;
    bool backgroundFallbackVisible = false;
    if(bgPixel != nullptr && bgPixel->valid) {
        const int bgFullTileIndex = bgPixel->tileIndex != 0xFFFF ? static_cast<int>(bgPixel->tileIndex) : -1;
        const std::array<uint8_t, 3> bgPalette = { bgPixel->palette[0], bgPixel->palette[1], bgPixel->palette[2] };
        if(bgFullTileIndex >= 0) {
            const ConditionContext context = { nesX, nesY, bgPixel, nullptr };
            if(!normalizedFilter.empty()) {
                for(const PreparedOverride& prepared : preparedOverrides) {
                    if(!debugAssetMatchesFilter(prepared.override->assetPath)) {
                        continue;
                    }
                    DebugComposeStage candidateStage;
                    candidateStage.valid = true;
                    candidateStage.stage = "override candidate";
                    candidateStage.assetPath = prepared.override->assetPath;
                    candidateStage.priority = prepared.override->priority;
                    if(prepared.override->hasSourcePosition()) {
                        candidateStage.srcX = prepared.override->sourceX;
                        candidateStage.srcY = prepared.override->sourceY;
                    }
                    const std::string mismatch = explainOverrideMismatch(
                        prepared,
                        ChrOverride::Target::Background,
                        false,
                        bgFullTileIndex & 0xFF,
                        bgFullTileIndex,
                        bgFullTileIndex / 256,
                        bgPalette,
                        false,
                        false,
                        false,
                        context);
                    candidateStage.returnedBaseColor = !mismatch.empty();
                    candidateStage.reason = mismatch.empty() ? "matched exact rule" : mismatch;
                    result.backgroundStages.push_back(std::move(candidateStage));
                }
            }
            backgroundOverride = findOverride(ChrOverride::Target::Background, bgFullTileIndex & 0xFF, bgFullTileIndex, bgFullTileIndex / 256, bgPalette, false, false, false, context);
        }
        backgroundFallbackColor = snapshot.paletteColors[bgPixel->paletteIndex & 0x3F];
        backgroundFallbackVisible = bgPixel->colorLowBits != 0;
    }
    const auto spriteFallbackColorFor = [&](const PPU::DebugModSpriteCandidate& candidate) {
        if(candidate.colorLowBits == 0) {
            return color;
        }
        const std::array<uint8_t, 3> spritePalette = { candidate.palette[0], candidate.palette[1], candidate.palette[2] };
        const int spritePaletteIndex = std::clamp(static_cast<int>(candidate.colorLowBits), 1, 3) - 1;
        if(spritePaletteIndex >= 0 && spritePaletteIndex < static_cast<int>(spritePalette.size())) {
            return snapshot.paletteColors[spritePalette[static_cast<size_t>(spritePaletteIndex)] & 0x3F];
        }
        return result.baseColor;
    };

    auto applySpriteCandidate = [&](const PPU::DebugModSpriteCandidate& candidate, int candidateIndex, const char* bucket) {
        if(!candidate.valid) {
            return;
        }
        DebugComposeStage header;
        header.valid = true;
        header.stage = bucket;
        header.priority = candidateIndex;

        const uint32_t spriteFallbackColor = spriteFallbackColorFor(candidate);
        const int spriteFullTileIndex = candidate.tileIndex != 0xFFFF ? static_cast<int>(candidate.tileIndex) : -1;
        const std::array<uint8_t, 3> spritePalette = { candidate.palette[0], candidate.palette[1], candidate.palette[2] };
        const ConditionContext context = { nesX, nesY, bgPixel, &candidate };
        const PreparedOverride* spriteOverride =
            spriteFullTileIndex >= 0
                ? findOverride(
                    ChrOverride::Target::Sprite,
                    spriteFullTileIndex & 0xFF,
                    spriteFullTileIndex,
                    spriteFullTileIndex / 256,
                    spritePalette,
                    candidate.horizontalMirror,
                    candidate.verticalMirror,
                    candidate.behindBackground,
                    context)
                : nullptr;
        auto [newColor, stage] = sampleOverrideStage(color, color, spriteOverride, candidate.tileIndex, candidate.offsetX, candidate.offsetY, spritePalette, candidate.horizontalMirror, candidate.verticalMirror, true, "sprite override");
        stage.priority = candidateIndex;
        if(spriteOverride == nullptr && candidate.colorLowBits != 0) {
            newColor = spriteFallbackColor;
            stage.reason = "no override, using NES sprite fallback";
            stage.returnedBaseColor = false;
        } else if(spriteOverride == nullptr) {
            return;
        }
        color = newColor;
        result.spriteStages.push_back(stage);
    };

    auto higherPriorityBehindSpriteCoversPixel = [&](int frontCandidateIndex) {
        if(spritePixel == nullptr) {
            return false;
        }
        for(int i = 0; i < frontCandidateIndex; ++i) {
            const PPU::DebugModSpriteCandidate& candidate = spritePixel->candidates[static_cast<size_t>(i)];
            if(!candidate.valid || !candidate.behindBackground) {
                continue;
            }

            const int spriteFullTileIndex = candidate.tileIndex != 0xFFFF ? static_cast<int>(candidate.tileIndex) : -1;
            const std::array<uint8_t, 3> spritePalette = { candidate.palette[0], candidate.palette[1], candidate.palette[2] };
            const ConditionContext context = { nesX, nesY, bgPixel, &candidate };
            const PreparedOverride* spriteOverride =
                spriteFullTileIndex >= 0
                    ? findOverride(
                        ChrOverride::Target::Sprite,
                        spriteFullTileIndex & 0xFF,
                        spriteFullTileIndex,
                        spriteFullTileIndex / 256,
                        spritePalette,
                        candidate.horizontalMirror,
                        candidate.verticalMirror,
                        candidate.behindBackground,
                        context)
                    : nullptr;
            if(spriteOverride != nullptr) {
                auto [unusedColor, stage] = sampleOverrideStage(
                    0u,
                    0u,
                    spriteOverride,
                    candidate.tileIndex,
                    candidate.offsetX,
                    candidate.offsetY,
                    spritePalette,
                    candidate.horizontalMirror,
                    candidate.verticalMirror,
                    true,
                    "sprite coverage");
                static_cast<void>(unusedColor);
                if(!stage.returnedBaseColor) {
                    return true;
                }
            } else if(candidate.colorLowBits != 0) {
                return true;
            }
        }
        return false;
    };

    if(spritePixel != nullptr && spritePixel->count > 0) {
        for(int i = static_cast<int>(spritePixel->count) - 1; i >= 0; --i) {
            const PPU::DebugModSpriteCandidate& candidate = spritePixel->candidates[static_cast<size_t>(i)];
            if(!candidate.valid || !candidate.behindBackground) {
                continue;
            }
            applySpriteCandidate(candidate, i, "sprite behind-bg");
        }
    }

    if(bgPixel != nullptr && bgPixel->valid) {
        for(int priority = 10; priority < 20; ++priority) {
            if(const PreparedBackground* preparedBackground = activeBackgroundsByPriority[static_cast<size_t>(priority)]; preparedBackground != nullptr) {
                DebugComposeStage stage = sampleBackgroundStage(*preparedBackground);
                if(!stage.returnedBaseColor) {
                    color = blendPixel(color, stage.rawRgba, std::clamp(static_cast<int>(std::round(preparedBackground->replacement->opacity * 255.0f)), 0, 255));
                }
                result.backgroundStages.push_back(stage);
            }
        }

        const std::array<uint8_t, 3> bgPalette = { bgPixel->palette[0], bgPixel->palette[1], bgPixel->palette[2] };
        if(backgroundOverride != nullptr) {
            auto [newColor, stage] = sampleOverrideStage(color, backgroundFallbackColor, backgroundOverride, bgPixel->tileIndex, bgPixel->offsetX, bgPixel->offsetY, bgPalette, false, false, false, "bg override");
            result.backgroundOverride = stage;
            color = newColor;
        } else {
            DebugComposeStage stage;
            stage.valid = true;
            stage.stage = "bg override";
            stage.returnedBaseColor = true;
            stage.reason = "no matching override";
            result.backgroundOverride = stage;
            if(!m_disableOriginalTiles && backgroundFallbackVisible) {
                color = backgroundFallbackColor;
            }
        }
    }

    for(int priority = 20; priority < 30; ++priority) {
        if(const PreparedBackground* preparedBackground = activeBackgroundsByPriority[static_cast<size_t>(priority)]; preparedBackground != nullptr) {
            DebugComposeStage stage = sampleBackgroundStage(*preparedBackground);
            if(!stage.returnedBaseColor) {
                color = blendPixel(color, stage.rawRgba, std::clamp(static_cast<int>(std::round(preparedBackground->replacement->opacity * 255.0f)), 0, 255));
            }
            result.backgroundStages.push_back(stage);
        }
    }

    if(spritePixel != nullptr && spritePixel->count > 0) {
        for(int i = static_cast<int>(spritePixel->count) - 1; i >= 0; --i) {
            const PPU::DebugModSpriteCandidate& candidate = spritePixel->candidates[static_cast<size_t>(i)];
            if(!candidate.valid || candidate.behindBackground) {
                continue;
            }
            if(higherPriorityBehindSpriteCoversPixel(i)) {
                continue;
            }
            applySpriteCandidate(candidate, i, "sprite front");
        }
    }

    for(int priority = 30; priority < 40; ++priority) {
        if(const PreparedBackground* preparedBackground = activeBackgroundsByPriority[static_cast<size_t>(priority)]; preparedBackground != nullptr) {
            DebugComposeStage stage = sampleBackgroundStage(*preparedBackground);
            if(!stage.returnedBaseColor) {
                color = blendPixel(color, stage.rawRgba, std::clamp(static_cast<int>(std::round(preparedBackground->replacement->opacity * 255.0f)), 0, 255));
            }
            result.backgroundStages.push_back(stage);
        }
    }

    result.finalColor = color;
    return result;
}

std::optional<ModManager::DecodedImage> ModManager::decodeImage(const std::vector<uint8_t>& data)
{
    stbi_set_flip_vertically_on_load_thread(0);
    int width = 0;
    int height = 0;
    int channels = 0;
    if(!stbi_info_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels) ||
       width <= 0 || height <= 0) {
        return std::nullopt;
    }

    static constexpr int kMaxDecodedImageDimension = 8192;
    static constexpr size_t kMaxDecodedImagePixels = 16u * 1024u * 1024u;
    if(width > kMaxDecodedImageDimension || height > kMaxDecodedImageDimension) {
        return std::nullopt;
    }
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    if(pixelCount == 0 || pixelCount > kMaxDecodedImagePixels) {
        return std::nullopt;
    }

    width = 0;
    height = 0;
    channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels, 4);
    if(pixels == nullptr || width <= 0 || height <= 0) {
        stbi_image_free(pixels);
        return std::nullopt;
    }

    DecodedImage image;
    image.width = width;
    image.height = height;
    image.rgba.resize(pixelCount);
    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            const size_t srcIndex = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
            const size_t dstIndex = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
            image.rgba[dstIndex] =
                (static_cast<uint32_t>(pixels[srcIndex + 3u]) << 24u) |
                static_cast<uint32_t>(pixels[srcIndex + 0u]) |
                (static_cast<uint32_t>(pixels[srcIndex + 1u]) << 8u) |
                (static_cast<uint32_t>(pixels[srcIndex + 2u]) << 16u);
        }
    }
    stbi_image_free(pixels);

    if(const auto indexed = decodeIndexedPng4(data); indexed.has_value() &&
       indexed->width == image.width && indexed->height == image.height &&
       indexed->indices.size() == image.rgba.size()) {
        image.indexedPixels = indexed->indices;
        image.indexedFourColor = true;
    }

    return image;
}

void ModManager::composeChrFrame(std::vector<uint32_t>& framebuffer, int width, int height, int activeTop, int activeBottom, int scale, const uint32_t* sourceFramebuffer, const ChrRenderSnapshot& snapshot, const std::vector<const ChrOverride*>* activeOverrideFilter, bool applyModLogic)
{
    MODMANAGER_PROFILE_SCOPE(ComposeChrFrame);
    std::scoped_lock runtimeLock(m_runtimeMutex);

    if(sourceFramebuffer == nullptr || framebuffer.empty() || width <= 0 || height <= 0 || scale <= 0) {
        return;
    }

    const FrameConditionState& frameConditionState =
        snapshot.frameConditionStateView != nullptr ? *snapshot.frameConditionStateView : snapshot.frameConditionState;
    const auto blitSourceFramebuffer = [&]() {
        const int nesYBegin = std::max(0, activeTop / scale);
        const int nesYEnd = std::min(PPU::SCREEN_HEIGHT, (activeBottom + scale - 1) / scale);

        if(scale == 1 && width == PPU::SCREEN_WIDTH) {
            for(int nesY = nesYBegin; nesY < nesYEnd; ++nesY) {
                if(nesY < activeTop || nesY >= activeBottom || nesY < 0 || nesY >= height) {
                    continue;
                }
                const uint32_t* srcRow = sourceFramebuffer + static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH;
                uint32_t* dstRow = framebuffer.data() + static_cast<size_t>(nesY) * static_cast<size_t>(width);
                std::memcpy(dstRow, srcRow, static_cast<size_t>(PPU::SCREEN_WIDTH) * sizeof(uint32_t));
            }
            return;
        }

        for(int nesY = nesYBegin; nesY < nesYEnd; ++nesY) {
            const uint32_t* srcRow = sourceFramebuffer + static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH;
            for(int sy = 0; sy < scale; ++sy) {
                const int outY = nesY * scale + sy;
                if(outY < activeTop || outY >= activeBottom || outY < 0 || outY >= height) {
                    continue;
                }
                uint32_t* dstRow = framebuffer.data() + static_cast<size_t>(outY) * static_cast<size_t>(width);
                for(int nesX = 0; nesX < PPU::SCREEN_WIDTH; ++nesX) {
                    std::fill_n(
                        dstRow + static_cast<size_t>(nesX * scale),
                        static_cast<size_t>(scale),
                        srcRow[nesX]);
                }
            }
        }
    };

    if(applyModLogic && m_chrOverrides.empty()) {
        blitSourceFramebuffer();
        return;
    }

    auto makeAdditionalSpriteRuleKey = [](int tile, uint32_t paletteKey) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(tile)) << 32u) | static_cast<uint64_t>(paletteKey);
    };
    RenderComposeCache filteredOverrideCache;
    const RenderComposeCache* selectedOverrideCache = nullptr;

    const std::vector<RenderPreparedBackground>* preparedBackgroundsPtr = nullptr;
    const std::unordered_map<uint64_t, RenderComposeCache::AdditionalSpriteRuleSpan>* additionalSpriteRulesByExactKeyPtr = nullptr;
    const std::unordered_map<int, RenderComposeCache::AdditionalSpriteRuleSpan>* additionalSpriteRulesByOriginalTilePtr = nullptr;
    const std::vector<const AdditionalSpriteRule*>* additionalSpriteRuleStoragePtr = nullptr;
    bool hasAdditionalSpriteRules = false;

    RenderComposeCache emptyOverrideCache;
    if(applyModLogic) {
        if(m_renderComposeCacheDirty || !m_renderComposeCache.valid || m_renderComposeCache.scale != scale) {
            rebuildRenderComposeCache();
        }

        preparedBackgroundsPtr = &m_renderComposeCache.preparedBackgrounds;
        additionalSpriteRulesByExactKeyPtr = &m_renderComposeCache.additionalSpriteRulesByExactKey;
        additionalSpriteRulesByOriginalTilePtr = &m_renderComposeCache.additionalSpriteRulesByOriginalTile;
        additionalSpriteRuleStoragePtr = &m_renderComposeCache.additionalSpriteRuleStorage;
        hasAdditionalSpriteRules = m_renderComposeCache.hasAdditionalSpriteRules;

        if(activeOverrideFilter == nullptr) {
            selectedOverrideCache = &m_renderComposeCache;
        } else {
            filteredOverrideCache = buildFilteredRenderComposeCache(*activeOverrideFilter);
            selectedOverrideCache = &filteredOverrideCache;
        }
    } else {
        static const std::vector<RenderPreparedBackground> emptyPreparedBackgroundStorage;
        static const std::unordered_map<uint64_t, RenderComposeCache::AdditionalSpriteRuleSpan> emptyAdditionalSpriteRulesByExactKey;
        static const std::unordered_map<int, RenderComposeCache::AdditionalSpriteRuleSpan> emptyAdditionalSpriteRulesByOriginalTile;
        static const std::vector<const AdditionalSpriteRule*> emptyAdditionalSpriteRuleStorage;
        preparedBackgroundsPtr = &emptyPreparedBackgroundStorage;
        additionalSpriteRulesByExactKeyPtr = &emptyAdditionalSpriteRulesByExactKey;
        additionalSpriteRulesByOriginalTilePtr = &emptyAdditionalSpriteRulesByOriginalTile;
        additionalSpriteRuleStoragePtr = &emptyAdditionalSpriteRuleStorage;
        emptyOverrideCache.scale = scale;
        emptyOverrideCache.valid = true;
        selectedOverrideCache = &emptyOverrideCache;
    }

    const auto& preparedOverrides = selectedOverrideCache->preparedOverrides;
    const auto& overridesByFullTile = selectedOverrideCache->overridesByFullTile;
    const auto& overridesByOverflowFullTile = selectedOverrideCache->overridesByOverflowFullTile;
    const auto& overridesByRelativeTile = selectedOverrideCache->overridesByRelativeTile;
    const auto& dynamicOverridesByFullTile = selectedOverrideCache->dynamicOverridesByFullTile;
    const auto& dynamicOverridesByOverflowFullTile = selectedOverrideCache->dynamicOverridesByOverflowFullTile;
    const auto& dynamicOverridesByRelativeTile = selectedOverrideCache->dynamicOverridesByRelativeTile;
    const auto& overridesByChrHash = selectedOverrideCache->overridesByChrHash;
    const auto& dynamicOverridesByChrHash = selectedOverrideCache->dynamicOverridesByChrHash;
    const auto& wholeChrOverrides = selectedOverrideCache->wholeChrOverrides;
    const auto& dynamicWholeChrOverrides = selectedOverrideCache->dynamicWholeChrOverrides;
    const auto& hasStaticOverridesByFullTile = selectedOverrideCache->hasStaticOverridesByFullTile;
    const auto& staticOverflowFullTiles = selectedOverrideCache->staticOverflowFullTiles;
    const auto& hasStaticOverridesByRelativeTile = selectedOverrideCache->hasStaticOverridesByRelativeTile;
    const auto& staticOverrideHashes = selectedOverrideCache->staticOverrideHashes;
    const bool hasWholeChrOverrides = selectedOverrideCache->hasWholeChrOverrides;
    const auto& hasDefaultOverridesByFullTile = selectedOverrideCache->hasDefaultOverridesByFullTile;
    const auto& defaultOverflowFullTiles = selectedOverrideCache->defaultOverflowFullTiles;
    const auto& hasDefaultOverridesByRelativeTile = selectedOverrideCache->hasDefaultOverridesByRelativeTile;
    const auto& defaultOverrideHashes = selectedOverrideCache->defaultOverrideHashes;
    const bool hasWholeChrDefaultOverrides = selectedOverrideCache->hasWholeChrDefaultOverrides;
    const auto& hasDynamicOverridesByFullTile = selectedOverrideCache->hasDynamicOverridesByFullTile;
    const auto& dynamicOverflowFullTiles = selectedOverrideCache->dynamicOverflowFullTiles;
    const auto& hasDynamicOverridesByRelativeTile = selectedOverrideCache->hasDynamicOverridesByRelativeTile;
    const PPU::DebugModBackgroundPixel* const backgroundPixelsData =
        snapshot.backgroundPixelsView != nullptr ? snapshot.backgroundPixelsView : snapshot.backgroundPixels.data();
    const size_t backgroundPixelsCount =
        snapshot.backgroundPixelsView != nullptr ? snapshot.backgroundPixelsViewCount : snapshot.backgroundPixels.size();
    const PPU::DebugModSpritePixel* const rawSpritePixelsData =
        snapshot.spritePixelsView != nullptr ? snapshot.spritePixelsView : snapshot.spritePixels.data();
    const size_t rawSpritePixelsCount =
        snapshot.spritePixelsView != nullptr ? snapshot.spritePixelsViewCount : snapshot.spritePixels.size();
    const bool hasBackgroundData = backgroundPixelsData != nullptr && backgroundPixelsCount != 0;
    const bool hasSpriteData = rawSpritePixelsData != nullptr && rawSpritePixelsCount != 0;

    static const std::vector<RenderPreparedBackground> emptyPreparedBackgrounds;
    const auto& preparedBackgrounds = hasBackgroundData ? *preparedBackgroundsPtr : emptyPreparedBackgrounds;
    const bool disableOriginalBackgroundTiles = applyModLogic && m_disableOriginalTiles && hasBackgroundData;
    const uint32_t universalBackgroundColor = snapshot.paletteColors[snapshot.universalBgColor & 0x3Fu];
    const auto& additionalSpriteRulesByExactKey = *additionalSpriteRulesByExactKeyPtr;
    const auto& additionalSpriteRulesByOriginalTile = *additionalSpriteRulesByOriginalTilePtr;
    const auto& additionalSpriteRuleStorage = *additionalSpriteRuleStoragePtr;
    const bool onlyWholeChrOverrides = selectedOverrideCache->onlyWholeChrOverrides;
    const RenderPreparedOverride* fastBackgroundOverride = selectedOverrideCache->fastBackgroundOverride;
    const bool canUseOverrideLookupCache = !preparedOverrides.empty();
    using PreparedOverride = RenderPreparedOverride;
    using PreparedBackground = RenderPreparedBackground;

    if(applyModLogic && preparedOverrides.empty() && preparedBackgrounds.empty()) {
        blitSourceFramebuffer();
        return;
    }

    MODMANAGER_PROFILE_COUNT(preparedOverrideCount, preparedOverrides.size());
    MODMANAGER_PROFILE_COUNT(preparedBackgroundCount, preparedBackgrounds.size());
#if GERANES_MODMANAGER_PROFILE
    const auto composeChrFrameSetupStartedAt = ModManagerProfileClock::now();
#endif

    auto tileHash = [&](int tileIndex) {
        if(tileIndex < 0) {
            return 0u;
        }
        if(tileIndex <= 0x01FF) {
            return snapshot.tileHashes[static_cast<size_t>(tileIndex)];
        }
        if(tileIndex < static_cast<int>(m_chrRomTileHashes.size())) {
            return m_chrRomTileHashes[static_cast<size_t>(tileIndex)];
        }
        return 0u;
    };

    auto canonicalTileIndex = [&](int tileIndex, uint32_t currentHash) {
        if(tileIndex > 0x01FF) {
            return tileIndex;
        }
        if(currentHash != 0) {
            if(const auto it = m_chrRomCanonicalTileByHash.find(currentHash); it != m_chrRomCanonicalTileByHash.end()) {
                return it->second;
            }
        }
        return tileIndex;
    };

    const bool hasCanonicalTileRemap = applyModLogic && !m_chrRomCanonicalTileByHash.empty();
    std::array<int, 512> canonicalSnapshotTileIndices = {};
    if(hasCanonicalTileRemap) {
        for(size_t i = 0; i < canonicalSnapshotTileIndices.size(); ++i) {
            canonicalSnapshotTileIndices[i] = static_cast<int>(i);
        }
        for(size_t i = 0; i < canonicalSnapshotTileIndices.size(); ++i) {
            const uint32_t currentHash = snapshot.tileHashes[i];
            if(currentHash != 0) {
                if(const auto it = m_chrRomCanonicalTileByHash.find(currentHash); it != m_chrRomCanonicalTileByHash.end()) {
                    canonicalSnapshotTileIndices[i] = it->second;
                }
            }
        }
    }

    auto canonicalSnapshotTileIndex = [&](int tileIndex, uint32_t currentHash) {
        if(!hasCanonicalTileRemap) {
            return tileIndex;
        }
        if(tileIndex >= 0 && tileIndex <= 0x01FF) {
            return canonicalSnapshotTileIndices[static_cast<size_t>(tileIndex)];
        }
        return canonicalTileIndex(tileIndex, currentHash);
    };
#if GERANES_MODMANAGER_PROFILE
    g_modManagerProfile.addDuration(
        ModManagerProfileSection::ComposeChrFrameSetup,
        static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                ModManagerProfileClock::now() - composeChrFrameSetupStartedAt)
                .count()));
#endif
    auto backgroundPixelAt = [&](int x, int y) -> const PPU::DebugModBackgroundPixel* {
        if(x < 0 || x >= PPU::SCREEN_WIDTH || y < 0 || y >= PPU::SCREEN_HEIGHT) {
            return nullptr;
        }
        const size_t index = static_cast<size_t>(y) * PPU::SCREEN_WIDTH + static_cast<size_t>(x);
        return index < backgroundPixelsCount ? &backgroundPixelsData[index] : nullptr;
    };

    auto rawSpritePixelAt = [&](int x, int y) -> const PPU::DebugModSpritePixel* {
        if(x < 0 || x >= PPU::SCREEN_WIDTH || y < 0 || y >= PPU::SCREEN_HEIGHT) {
            return nullptr;
        }
        const size_t index = static_cast<size_t>(y) * PPU::SCREEN_WIDTH + static_cast<size_t>(x);
        return index < rawSpritePixelsCount ? &rawSpritePixelsData[index] : nullptr;
    };

    auto candidatePaletteKey = [](const PPU::DebugModSpriteCandidate& candidate) -> uint32_t {
        return 0xFF000000u |
               (static_cast<uint32_t>(candidate.palette[0] & 0x3F) << 16u) |
               (static_cast<uint32_t>(candidate.palette[1] & 0x3F) << 8u) |
               static_cast<uint32_t>(candidate.palette[2] & 0x3F);
    };

    auto decodeSpritePaletteKey = [](uint32_t paletteKey, uint8_t palette[3]) {
        palette[0] = static_cast<uint8_t>((paletteKey >> 16u) & 0x3Fu);
        palette[1] = static_cast<uint8_t>((paletteKey >> 8u) & 0x3Fu);
        palette[2] = static_cast<uint8_t>(paletteKey & 0x3Fu);
    };

    auto appendSyntheticSpriteCandidate = [&](PPU::DebugModSpritePixel& targetPixel,
                                              const PPU::DebugModSpriteCandidate& source,
                                              const AdditionalSpriteRule& rule,
                                              int targetX,
                                              int targetY,
                                              int originX,
                                              int originY) {
        if(targetPixel.count >= targetPixel.candidates.size()) {
            return;
        }

        PPU::DebugModSpriteCandidate added = source;
        added.tileIndex = static_cast<uint16_t>(std::clamp(rule.additionalTile, 0, 0xFFFF));
        added.tileHash = tileHash(rule.additionalTile);
        decodeSpritePaletteKey(rule.additionalPaletteKey, added.palette);
        added.paletteSlot = source.paletteSlot;
        added.offsetX = static_cast<uint8_t>(std::clamp(targetX - originX, 0, 255));
        added.offsetY = static_cast<uint8_t>(std::clamp(targetY - originY, 0, 255));
        added.colorLowBits = 0;
        added.synthetic = true;
        added.valid = true;

        targetPixel.candidates[targetPixel.count++] = added;
        MODMANAGER_PROFILE_COUNT(syntheticSpriteCandidatesAdded, 1);
        if(!targetPixel.valid) {
            targetPixel.tileIndex = added.tileIndex;
            targetPixel.tileHash = added.tileHash;
            targetPixel.palette[0] = added.palette[0];
            targetPixel.palette[1] = added.palette[1];
            targetPixel.palette[2] = added.palette[2];
            targetPixel.paletteSlot = added.paletteSlot;
            targetPixel.colorLowBits = added.colorLowBits;
            targetPixel.offsetX = added.offsetX;
            targetPixel.offsetY = added.offsetY;
            targetPixel.behindBackground = added.behindBackground;
            targetPixel.horizontalMirror = added.horizontalMirror;
            targetPixel.verticalMirror = added.verticalMirror;
            targetPixel.synthetic = added.synthetic;
            targetPixel.valid = true;
        }
    };

    auto appendResolvedAdditionalSpriteRules = [&](std::vector<const AdditionalSpriteRule*>& output, uint64_t ruleKey, int resolvedTileIndex) {
        if(const auto exactIt = additionalSpriteRulesByExactKey.find(ruleKey); exactIt != additionalSpriteRulesByExactKey.end()) {
            const auto& span = exactIt->second;
            output.insert(
                output.end(),
                additionalSpriteRuleStorage.begin() + static_cast<std::ptrdiff_t>(span.offset),
                additionalSpriteRuleStorage.begin() + static_cast<std::ptrdiff_t>(span.offset + span.count));
        }
        if(const auto wildcardIt = additionalSpriteRulesByOriginalTile.find(resolvedTileIndex); wildcardIt != additionalSpriteRulesByOriginalTile.end()) {
            const auto& span = wildcardIt->second;
            output.insert(
                output.end(),
                additionalSpriteRuleStorage.begin() + static_cast<std::ptrdiff_t>(span.offset),
                additionalSpriteRuleStorage.begin() + static_cast<std::ptrdiff_t>(span.offset + span.count));
        }
    };

    const std::vector<PPU::DebugModSpritePixel>* augmentedSpritePixels = nullptr;
    if(hasAdditionalSpriteRules) {
        MODMANAGER_PROFILE_SCOPE(ComposeChrFrameAdditionalSpriteAugment);
        const size_t totalSpritePixels = static_cast<size_t>(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT);
        const size_t copySpritePixelCount = std::min(totalSpritePixels, rawSpritePixelsCount);
        bool augmentedSpritePixelsInitialized = false;
        m_resolvedAdditionalSpriteRulesScratch.clear();
        m_resolvedAdditionalSpriteRuleMemo.clear();
        m_resolvedAdditionalSpriteRulesScratch.reserve(std::min<size_t>(m_additionalSpriteRules.size() * 4u, 4096u));
        m_resolvedAdditionalSpriteRuleMemo.reserve(std::min<size_t>(m_additionalSpriteRules.size() * 4u, 4096u));

        for(size_t index = 0; index < copySpritePixelCount; ++index) {
            const PPU::DebugModSpritePixel& pixel = rawSpritePixelsData[index];
            const int y = static_cast<int>(index / PPU::SCREEN_WIDTH);
            const int x = static_cast<int>(index % PPU::SCREEN_WIDTH);
            for(uint8_t i = 0; i < pixel.count; ++i) {
                MODMANAGER_PROFILE_COUNT(additionalSpriteSourceCandidates, 1);
                const PPU::DebugModSpriteCandidate& source = pixel.candidates[static_cast<size_t>(i)];
                const uint32_t sourceHash = source.tileHash != 0 ? source.tileHash : tileHash(source.tileIndex);
                const int resolvedTileIndex = canonicalSnapshotTileIndex(source.tileIndex, sourceHash);
                const uint32_t sourcePaletteKey = candidatePaletteKey(source);
                const int originX = x - static_cast<int>(source.offsetX);
                const int originY = y - static_cast<int>(source.offsetY);
                const uint64_t sourceRuleKey = makeAdditionalSpriteRuleKey(resolvedTileIndex, sourcePaletteKey);
                auto resolvedRuleIt = m_resolvedAdditionalSpriteRuleMemo.find(sourceRuleKey);
                if(resolvedRuleIt == m_resolvedAdditionalSpriteRuleMemo.end()) {
                    MODMANAGER_PROFILE_COUNT(additionalSpriteMemoMisses, 1);
                    const size_t rulesOffset = m_resolvedAdditionalSpriteRulesScratch.size();
                    appendResolvedAdditionalSpriteRules(m_resolvedAdditionalSpriteRulesScratch, sourceRuleKey, resolvedTileIndex);
                    const size_t rulesCount = m_resolvedAdditionalSpriteRulesScratch.size() - rulesOffset;
                    MODMANAGER_PROFILE_COUNT(additionalSpriteResolvedRules, rulesCount);
                    const auto inserted = m_resolvedAdditionalSpriteRuleMemo.emplace(
                        sourceRuleKey,
                        RenderComposeCache::AdditionalSpriteRuleSpan { rulesOffset, rulesCount });
                    resolvedRuleIt = inserted.first;
                    if(rulesCount == 0) {
                        continue;
                    }
                } else {
                    MODMANAGER_PROFILE_COUNT(additionalSpriteMemoHits, 1);
                }
                const RenderComposeCache::AdditionalSpriteRuleSpan& resolvedSpan = resolvedRuleIt->second;
                if(resolvedSpan.count == 0) {
                    continue;
                }
                for(size_t ruleIndex = 0; ruleIndex < resolvedSpan.count; ++ruleIndex) {
                    const AdditionalSpriteRule* rulePtr = m_resolvedAdditionalSpriteRulesScratch[resolvedSpan.offset + ruleIndex];
                    if(rulePtr == nullptr) {
                        continue;
                    }
                    const AdditionalSpriteRule& rule = *rulePtr;
                    const int targetX = x + rule.offsetX;
                    const int targetY = y + rule.offsetY;
                    if(targetX < 0 || targetX >= PPU::SCREEN_WIDTH || targetY < 0 || targetY >= PPU::SCREEN_HEIGHT) {
                        continue;
                    }
                    if(!augmentedSpritePixelsInitialized) {
                        m_augmentedSpritePixelsScratch.resize(totalSpritePixels);
                        if(copySpritePixelCount > 0) {
                            std::copy_n(rawSpritePixelsData, copySpritePixelCount, m_augmentedSpritePixelsScratch.begin());
                        }
                        augmentedSpritePixelsInitialized = true;
                    }

                    PPU::DebugModSpritePixel& targetPixel = m_augmentedSpritePixelsScratch[static_cast<size_t>(targetY) * PPU::SCREEN_WIDTH + static_cast<size_t>(targetX)];
                    appendSyntheticSpriteCandidate(targetPixel, source, rule, targetX, targetY, originX, originY);
                }
            }
        }
        if(augmentedSpritePixelsInitialized) {
            augmentedSpritePixels = &m_augmentedSpritePixelsScratch;
        }
    }

    auto spritePixelAt = [&](int x, int y) -> const PPU::DebugModSpritePixel* {
        if(x < 0 || x >= PPU::SCREEN_WIDTH || y < 0 || y >= PPU::SCREEN_HEIGHT) {
            return nullptr;
        }
        if(augmentedSpritePixels != nullptr) {
            return &(*augmentedSpritePixels)[static_cast<size_t>(y) * PPU::SCREEN_WIDTH + static_cast<size_t>(x)];
        }
        return rawSpritePixelAt(x, y);
    };

    auto scrollXForLine = [&](int y) {
        return (y >= 0 && y < PPU::SCREEN_HEIGHT) ? snapshot.scrollXByLine[static_cast<size_t>(y)] : snapshot.scrollX;
    };
    auto scrollYForLine = [&](int y) {
        return (y >= 0 && y < PPU::SCREEN_HEIGHT) ? snapshot.scrollYByLine[static_cast<size_t>(y)] : snapshot.scrollY;
    };

    struct ConditionContext {
        int nesX = 0;
        int nesY = 0;
        const PPU::DebugModBackgroundPixel* backgroundPixel = nullptr;
        const PPU::DebugModSpriteCandidate* spriteCandidate = nullptr;
    };

    auto paletteMatches = [](const ChrOverride& override, const std::array<uint8_t, 3>& palette, bool allowDefaultTileFallback) {
        if(override.paletteIndices.empty()) {
            return true;
        }
        if(override.defaultTile && allowDefaultTileFallback) {
            return true;
        }
        if(override.exactPaletteOrder) {
            if(palette.size() < override.paletteIndices.size()) {
                return false;
            }
            for(size_t i = 0; i < override.paletteIndices.size(); ++i) {
                if(palette[i] != override.paletteIndices[i]) {
                    return false;
                }
            }
            return true;
        }
        for(uint8_t expected : override.paletteIndices) {
            if(std::find(palette.begin(), palette.end(), expected) == palette.end()) {
                return false;
            }
        }
        return true;
    };

    auto paletteVectorMatches = [](const std::vector<uint8_t>& expectedPalette, const uint8_t palette[3]) {
        if(expectedPalette.empty()) {
            return true;
        }
        if(expectedPalette.size() > 3) {
            return false;
        }
        for(size_t i = 0; i < expectedPalette.size(); ++i) {
            if(expectedPalette[i] != palette[i]) {
                return false;
            }
        }
        return true;
    };

    auto backgroundPaletteKey = [&](const PPU::DebugModBackgroundPixel& pixel) -> uint32_t {
        return (static_cast<uint32_t>(snapshot.universalBgColor & 0x3F) << 24u) |
               (static_cast<uint32_t>(pixel.palette[0] & 0x3F) << 16u) |
               (static_cast<uint32_t>(pixel.palette[1] & 0x3F) << 8u) |
               static_cast<uint32_t>(pixel.palette[2] & 0x3F);
    };

    auto spritePaletteKey = [](const PPU::DebugModSpriteCandidate& candidate) -> uint32_t {
        return 0xFF000000u |
               (static_cast<uint32_t>(candidate.palette[0] & 0x3F) << 16u) |
               (static_cast<uint32_t>(candidate.palette[1] & 0x3F) << 8u) |
               static_cast<uint32_t>(candidate.palette[2] & 0x3F);
    };

    auto tileMatchesCondition = [&](const MemoryCondition& condition, const PPU::DebugModBackgroundPixel& pixel) {
        if(!pixel.valid) {
            return false;
        }
        if(condition.expectedPaletteKey != 0 && backgroundPaletteKey(pixel) != condition.expectedPaletteKey) {
            return false;
        }
        if(condition.hasExpectedTile) {
            const uint32_t pixelHash = pixel.tileHash != 0 ? pixel.tileHash : tileHash(pixel.tileIndex);
            if(condition.expectedTileByHash) {
                if(pixelHash != condition.expectedTileHash) {
                    return false;
                }
            } else if(condition.expectedTileIndex >= 0 &&
                      canonicalSnapshotTileIndex(static_cast<int>(pixel.tileIndex), pixelHash) != condition.expectedTileIndex) {
                return false;
            }
        }
        return condition.expectedPaletteKey != 0 ? true : paletteVectorMatches(condition.expectedPalette, pixel.palette);
    };

    auto spriteCandidateMatchesCondition = [&](const MemoryCondition& condition, const PPU::DebugModSpriteCandidate& candidate) {
        if(!candidate.valid) {
            return false;
        }
        if(condition.expectedPaletteKey != 0 && spritePaletteKey(candidate) != condition.expectedPaletteKey) {
            return false;
        }
        if(condition.hasExpectedTile) {
            const uint32_t candidateHash = candidate.tileHash != 0 ? candidate.tileHash : tileHash(candidate.tileIndex);
            if(condition.expectedTileByHash) {
                if(candidateHash != condition.expectedTileHash) {
                    return false;
                }
            } else if(condition.expectedTileIndex >= 0 &&
                      canonicalSnapshotTileIndex(static_cast<int>(candidate.tileIndex), candidateHash) != condition.expectedTileIndex) {
                return false;
            }
        }
        return condition.expectedPaletteKey != 0 ? true : paletteVectorMatches(condition.expectedPalette, candidate.palette);
    };

    auto conditionMatchesAt = [&](const MemoryCondition& condition, const ConditionContext& ctx) {
        bool match = false;
        switch(condition.kind) {
        case MemoryCondition::Kind::MemoryCheck: {
            const uint32_t actual =
                condition.readIndex < frameConditionState.memoryValues.size()
                    ? frameConditionState.memoryValues[condition.readIndex]
                    : 0u;
            const uint32_t expected =
                condition.compareAgainstMemory && condition.rhsReadIndex < frameConditionState.memoryValues.size()
                    ? frameConditionState.memoryValues[condition.rhsReadIndex]
                    : condition.value;
            match = evaluateMemoryCondition(condition, actual, expected);
            break;
        }
        case MemoryCondition::Kind::FrameRange: {
            const uint32_t range = std::max(1u, condition.value);
            match = (frameConditionState.frameCount % range) >= condition.address;
            break;
        }
        case MemoryCondition::Kind::TileAtPosition:
        case MemoryCondition::Kind::TileNearby: {
            int targetX = condition.x;
            int targetY = condition.y;
            if(condition.kind == MemoryCondition::Kind::TileNearby) {
                targetX = ctx.nesX + condition.x;
                targetY = ctx.nesY + condition.y;
            }
            const PPU::DebugModBackgroundPixel* pixel = backgroundPixelAt(targetX, targetY);
            match = pixel != nullptr && tileMatchesCondition(condition, *pixel);
            break;
        }
        case MemoryCondition::Kind::SpriteAtPosition:
        case MemoryCondition::Kind::SpriteNearby: {
            int xSign = 1;
            int ySign = 1;
            if(ctx.spriteCandidate != nullptr) {
                xSign = ctx.spriteCandidate->horizontalMirror ? -1 : 1;
                ySign = ctx.spriteCandidate->verticalMirror ? -1 : 1;
            }
            int targetX = condition.x;
            int targetY = condition.y;
            if(condition.kind == MemoryCondition::Kind::SpriteNearby) {
                targetX = ctx.nesX + condition.x * xSign;
                targetY = ctx.nesY + condition.y * ySign;
            }
            const PPU::DebugModSpritePixel* pixel = spritePixelAt(targetX, targetY);
            if(pixel != nullptr) {
                for(uint8_t i = 0; i < pixel->count; ++i) {
                    if(spriteCandidateMatchesCondition(condition, pixel->candidates[static_cast<size_t>(i)])) {
                        match = true;
                        break;
                    }
                }
            }
            break;
        }
        case MemoryCondition::Kind::SpritePaletteIndex:
            match = ctx.spriteCandidate != nullptr && ctx.spriteCandidate->paletteSlot == static_cast<uint8_t>(condition.value & 0x03u);
            break;
        }
        return condition.inverted ? !match : match;
    };

    struct EnabledOverrideCandidateCacheEntry {
        bool initialized = false;
        bool allEnabled = true;
        std::vector<const PreparedOverride*> filtered;
    };
    std::unordered_map<const std::vector<const PreparedOverride*>*, EnabledOverrideCandidateCacheEntry> enabledOverrideCandidateCache;
    enabledOverrideCandidateCache.reserve(1024);

    auto enabledCandidatesFor = [&](const std::vector<const PreparedOverride*>& candidates) -> const std::vector<const PreparedOverride*>& {
        auto [it, _] = enabledOverrideCandidateCache.emplace(&candidates, EnabledOverrideCandidateCacheEntry {});
        EnabledOverrideCandidateCacheEntry& cacheEntry = it->second;
        if(!cacheEntry.initialized) {
            cacheEntry.initialized = true;
            cacheEntry.filtered.clear();
            cacheEntry.filtered.reserve(candidates.size());
            for(const PreparedOverride* candidate : candidates) {
                if(candidate != nullptr && candidate->override != nullptr && candidate->override->enabled) {
                    cacheEntry.filtered.push_back(candidate);
                } else {
                    cacheEntry.allEnabled = false;
                }
            }
        }
        return cacheEntry.allEnabled ? candidates : cacheEntry.filtered;
    };

    auto targetMaskFor = [](ChrOverride::Target target) -> uint8_t {
        return static_cast<uint8_t>(target == ChrOverride::Target::Sprite ? 0x2u : 0x1u);
    };

    auto coarseStateKeyFor = [&](ChrOverride::Target target, int currentPatternTable, bool hMirror, bool vMirror, bool bgPriority) {
        uint32_t stateKey = static_cast<uint32_t>(currentPatternTable & 0x03);
        stateKey |= static_cast<uint32_t>(target == ChrOverride::Target::Sprite ? 1u : 0u) << 2;
        stateKey |= static_cast<uint32_t>(hMirror ? 1u : 0u) << 3;
        stateKey |= static_cast<uint32_t>(vMirror ? 1u : 0u) << 4;
        stateKey |= static_cast<uint32_t>(bgPriority ? 1u : 0u) << 5;
        return stateKey;
    };

    struct FilteredOverrideCandidateCacheKey {
        const std::vector<const PreparedOverride*>* candidates = nullptr;
        uint64_t stateKey = 0;

        bool operator==(const FilteredOverrideCandidateCacheKey& other) const
        {
            return candidates == other.candidates && stateKey == other.stateKey;
        }
    };

    struct FilteredOverrideCandidateCacheKeyHash {
        size_t operator()(const FilteredOverrideCandidateCacheKey& key) const
        {
            const size_t ptrHash = std::hash<const void*>{}(key.candidates);
            const size_t stateHash = std::hash<uint64_t>{}(key.stateKey);
            return ptrHash ^ (stateHash + 0x9e3779b9u + (ptrHash << 6) + (ptrHash >> 2));
        }
    };

    struct FilteredOverrideCandidateCacheEntry {
        bool allTileOriginCacheable = true;
        std::vector<const PreparedOverride*> filtered;
    };

    std::unordered_map<FilteredOverrideCandidateCacheKey, FilteredOverrideCandidateCacheEntry, FilteredOverrideCandidateCacheKeyHash> filteredOverrideCandidateCache;
    filteredOverrideCandidateCache.reserve(4096);

    auto filteredCandidatesFor = [&](const std::vector<const PreparedOverride*>& candidates,
                                     ChrOverride::Target target,
                                     int currentPatternTable,
                                     const std::array<uint8_t, 3>& palette,
                                     bool hMirror,
                                     bool vMirror,
                                     bool bgPriority,
                                     bool allowDefaultTileFallback) -> const FilteredOverrideCandidateCacheEntry& {
        const auto& enabledCandidates = enabledCandidatesFor(candidates);
        const uint64_t paletteKey =
            static_cast<uint64_t>(palette[0] & 0x3Fu) |
            (static_cast<uint64_t>(palette[1] & 0x3Fu) << 6u) |
            (static_cast<uint64_t>(palette[2] & 0x3Fu) << 12u);
        const FilteredOverrideCandidateCacheKey cacheKey {
            &enabledCandidates,
            static_cast<uint64_t>(coarseStateKeyFor(target, currentPatternTable, hMirror, vMirror, bgPriority)) |
                (paletteKey << 8u) |
                (static_cast<uint64_t>(allowDefaultTileFallback ? 1u : 0u) << 26u)
        };
        auto [it, inserted] = filteredOverrideCandidateCache.emplace(cacheKey, FilteredOverrideCandidateCacheEntry {});
        if(inserted) {
            FilteredOverrideCandidateCacheEntry& cacheEntry = it->second;
            cacheEntry.filtered.reserve(enabledCandidates.size());
            const uint8_t targetMask = targetMaskFor(target);
            const uint8_t patternTableMask = static_cast<uint8_t>(1u << (currentPatternTable & 0x03));
            const uint8_t requirementBit = static_cast<uint8_t>(1u << ((hMirror ? 1 : 0) | (vMirror ? 2 : 0) | (bgPriority ? 4 : 0)));
            for(const PreparedOverride* candidate : enabledCandidates) {
                if(candidate == nullptr) {
                    continue;
                }
                if((candidate->targetMask & targetMask) == 0 ||
                   (candidate->patternTableMask & patternTableMask) == 0 ||
                   (candidate->requirementMask & requirementBit) == 0) {
                    continue;
                }
                if(candidate->override == nullptr ||
                   !paletteMatches(*candidate->override, palette, allowDefaultTileFallback)) {
                    continue;
                }
                cacheEntry.filtered.push_back(candidate);
                if(!candidate->tileOriginCacheable) {
                    cacheEntry.allTileOriginCacheable = false;
                }
            }
        }
        return it->second;
    };

    auto matchesOverride = [&](const PreparedOverride& preparedOverride, ChrOverride::Target target, int tileIndex, int fullTileIndex, const ConditionContext& ctx) {
        const ChrOverride& override = *preparedOverride.override;
        if(!override.enabled) {
            return false;
        }
        if(!override.wholeChr()) {
            if(override.absoluteTile) {
                if(override.tile != fullTileIndex) {
                    return false;
                }
            } else if(override.tile != tileIndex && override.tile != fullTileIndex) {
                return false;
            }
        }
        const uint32_t currentTileHash =
            target == ChrOverride::Target::Sprite
                ? ((ctx.spriteCandidate != nullptr && ctx.spriteCandidate->tileHash != 0) ? ctx.spriteCandidate->tileHash : tileHash(fullTileIndex))
                : ((ctx.backgroundPixel != nullptr && ctx.backgroundPixel->tileHash != 0) ? ctx.backgroundPixel->tileHash : tileHash(fullTileIndex));
        if(override.hasChrHash && currentTileHash != override.chrHash) {
            return false;
        }
        for(const MemoryCondition* condition : preparedOverride.runtimeConditions) {
            if(condition != nullptr && !conditionMatchesAt(*condition, ctx)) {
                return false;
            }
        }
        return true;
    };

    auto scanCandidates = [&](const std::vector<const PreparedOverride*>& candidates, bool allowDefaultTileFallback, ChrOverride::Target target, int tileIndex, int fullTileIndex, int currentPatternTable, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority, const ConditionContext& ctx, bool* tileOriginCacheable = nullptr) -> const PreparedOverride* {
        MODMANAGER_PROFILE_COUNT(scanCandidatesCalls, 1);
        const FilteredOverrideCandidateCacheEntry& filteredCandidates =
            filteredCandidatesFor(candidates, target, currentPatternTable, palette, hMirror, vMirror, bgPriority, allowDefaultTileFallback);
        if(tileOriginCacheable != nullptr && !filteredCandidates.allTileOriginCacheable) {
            *tileOriginCacheable = false;
        }
        for(const PreparedOverride* candidate : filteredCandidates.filtered) {
            if(matchesOverride(*candidate, target, tileIndex, fullTileIndex, ctx)) {
                return candidate;
            }
        }
        return nullptr;
    };

    auto scanMergedCandidates = [&](const std::vector<const PreparedOverride*>& staticCandidates, const std::vector<const PreparedOverride*>& dynamicCandidates, bool allowDefaultTileFallback, ChrOverride::Target target, int tileIndex, int fullTileIndex, int currentPatternTable, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority, const ConditionContext& ctx, bool* tileOriginCacheable = nullptr) -> const PreparedOverride* {
        MODMANAGER_PROFILE_COUNT(scanMergedCandidatesCalls, 1);
        const FilteredOverrideCandidateCacheEntry& filteredStaticCandidates =
            filteredCandidatesFor(staticCandidates, target, currentPatternTable, palette, hMirror, vMirror, bgPriority, allowDefaultTileFallback);
        const FilteredOverrideCandidateCacheEntry& filteredDynamicCandidates =
            filteredCandidatesFor(dynamicCandidates, target, currentPatternTable, palette, hMirror, vMirror, bgPriority, allowDefaultTileFallback);
        if(tileOriginCacheable != nullptr &&
           (!filteredStaticCandidates.allTileOriginCacheable || !filteredDynamicCandidates.allTileOriginCacheable)) {
            *tileOriginCacheable = false;
        }
        const std::vector<const PreparedOverride*>& enabledStaticCandidates = filteredStaticCandidates.filtered;
        const std::vector<const PreparedOverride*>& enabledDynamicCandidates = filteredDynamicCandidates.filtered;
        size_t staticIndex = 0;
        size_t dynamicIndex = 0;
        while(staticIndex < enabledStaticCandidates.size() || dynamicIndex < enabledDynamicCandidates.size()) {
            const PreparedOverride* candidate = nullptr;
            if(dynamicIndex >= enabledDynamicCandidates.size()) {
                candidate = enabledStaticCandidates[staticIndex++];
            } else if(staticIndex >= enabledStaticCandidates.size()) {
                candidate = enabledDynamicCandidates[dynamicIndex++];
            } else if(enabledStaticCandidates[staticIndex]->sequence <= enabledDynamicCandidates[dynamicIndex]->sequence) {
                candidate = enabledStaticCandidates[staticIndex++];
            } else {
                candidate = enabledDynamicCandidates[dynamicIndex++];
            }

            if(matchesOverride(*candidate, target, tileIndex, fullTileIndex, ctx)) {
                return candidate;
            }
        }
        return nullptr;
    };

    auto makeOverrideLookupKey = [](ChrOverride::Target target, int fullTileIndex, int currentPatternTable, uint32_t currentTileHash, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority) {
        uint64_t key = static_cast<uint64_t>(static_cast<uint32_t>(fullTileIndex));
        key ^= static_cast<uint64_t>(currentPatternTable & 0x03) << 32;
        key ^= static_cast<uint64_t>(target == ChrOverride::Target::Sprite ? 1 : 0) << 34;
        key ^= static_cast<uint64_t>(palette[0] & 0x3F) << 35;
        key ^= static_cast<uint64_t>(palette[1] & 0x3F) << 41;
        key ^= static_cast<uint64_t>(palette[2] & 0x3F) << 47;
        key ^= static_cast<uint64_t>(hMirror ? 1 : 0) << 53;
        key ^= static_cast<uint64_t>(vMirror ? 1 : 0) << 54;
        key ^= static_cast<uint64_t>(bgPriority ? 1 : 0) << 55;
        key ^= static_cast<uint64_t>(currentTileHash) * 0x9E3779B185EBCA87ULL;
        return key;
    };

    std::unordered_map<uint64_t, const PreparedOverride*> overrideLookupCache;
    overrideLookupCache.reserve(std::min<size_t>(preparedOverrides.size() * 32u, 32768u));
    std::unordered_map<uint64_t, const PreparedOverride*> tileOriginOverrideLookupCache;
    tileOriginOverrideLookupCache.reserve(std::min<size_t>(preparedOverrides.size() * 64u, 65536u));

    struct ResolvedBackgroundLayer {
        const PreparedBackground* prepared = nullptr;
        bool valid = false;
        int baseSrcX = 0;
        int baseSrcY = 0;
        const uint32_t* row0 = nullptr;
        const uint32_t* row1 = nullptr;
        int maxSub = 0;
        bool uniformBlock = false;
        uint32_t uniformColor = 0;
    };

    auto makeDynamicOverrideLookupKey = [](uint64_t baseKey, const ConditionContext& ctx) {
        int originX = ctx.nesX;
        int originY = ctx.nesY;
        if(ctx.spriteCandidate != nullptr) {
            originX -= static_cast<int>(ctx.spriteCandidate->offsetX);
            originY -= static_cast<int>(ctx.spriteCandidate->offsetY);
        } else if(ctx.backgroundPixel != nullptr) {
            originX -= static_cast<int>(ctx.backgroundPixel->offsetX);
            originY -= static_cast<int>(ctx.backgroundPixel->offsetY);
        }

        uint64_t key = baseKey;
        key ^= static_cast<uint64_t>(originX & 0x01FF) << 1;
        key ^= static_cast<uint64_t>(originY & 0x01FF) << 10;
        key ^= key >> 33;
        key *= 0xff51afd7ed558ccdULL;
        key ^= key >> 33;
        return key;
    };

    auto findOverride = [&](ChrOverride::Target target, int tileIndex, int fullTileIndex, int currentPatternTable, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority, const ConditionContext& ctx, bool* cacheableByOrigin = nullptr) -> const PreparedOverride* {
        MODMANAGER_PROFILE_SCOPE(ComposeChrFrameFindOverride);
        MODMANAGER_PROFILE_COUNT(overrideFindCalls, 1);
        static const std::vector<const PreparedOverride*> emptyCandidates;
        const uint32_t lookupHash = tileHash(fullTileIndex);
        const uint32_t currentTileHash =
            target == ChrOverride::Target::Sprite
                ? ((ctx.spriteCandidate != nullptr && ctx.spriteCandidate->tileHash != 0) ? ctx.spriteCandidate->tileHash : lookupHash)
                : ((ctx.backgroundPixel != nullptr && ctx.backgroundPixel->tileHash != 0) ? ctx.backgroundPixel->tileHash : lookupHash);
        const int resolvedFullTileIndex = canonicalSnapshotTileIndex(fullTileIndex, currentTileHash);
        const bool resolvedFullTileInRange =
            resolvedFullTileIndex >= 0 &&
            resolvedFullTileIndex < static_cast<int>(overridesByFullTile.size());
        const bool relativeTileInRange =
            tileIndex >= 0 &&
            tileIndex < static_cast<int>(overridesByRelativeTile.size());
        const bool hasDynamicWholeChrOverrides = !dynamicWholeChrOverrides.empty();
        const auto staticHashIt = overridesByChrHash.find(currentTileHash);
        const auto dynamicHashIt = dynamicOverridesByChrHash.find(currentTileHash);
        const bool hasStaticHashCandidates = staticHashIt != overridesByChrHash.end();
        const bool hasDynamicHashCandidates = dynamicHashIt != dynamicOverridesByChrHash.end();
        const bool hasStaticOverrideHash = staticOverrideHashes.find(currentTileHash) != staticOverrideHashes.end();
        const bool hasDefaultOverrideHash = defaultOverrideHashes.find(currentTileHash) != defaultOverrideHashes.end();
        const bool hasStaticOverflowTile = staticOverflowFullTiles.find(resolvedFullTileIndex) != staticOverflowFullTiles.end();
        const bool hasDynamicOverflowTile = dynamicOverflowFullTiles.find(resolvedFullTileIndex) != dynamicOverflowFullTiles.end();
        const bool hasDefaultOverflowTile = defaultOverflowFullTiles.find(resolvedFullTileIndex) != defaultOverflowFullTiles.end();
        const auto staticOverflowIt = hasStaticOverflowTile
            ? overridesByOverflowFullTile.find(resolvedFullTileIndex)
            : overridesByOverflowFullTile.end();
        const auto dynamicOverflowIt = hasDynamicOverflowTile
            ? dynamicOverridesByOverflowFullTile.find(resolvedFullTileIndex)
            : dynamicOverridesByOverflowFullTile.end();
        const uint64_t lookupKey = makeOverrideLookupKey(target, resolvedFullTileIndex, currentPatternTable, currentTileHash, palette, hMirror, vMirror, bgPriority);
        const bool hasTileOriginContext = ctx.spriteCandidate != nullptr || ctx.backgroundPixel != nullptr;
        const uint64_t tileOriginLookupKey = hasTileOriginContext ? makeDynamicOverrideLookupKey(lookupKey, ctx) : 0u;
        const bool hasStaticExactCandidates =
            hasWholeChrOverrides ||
            hasStaticOverrideHash ||
            (resolvedFullTileInRange &&
             hasStaticOverridesByFullTile[static_cast<size_t>(resolvedFullTileIndex)]) ||
            hasStaticOverflowTile ||
            (relativeTileInRange &&
             hasStaticOverridesByRelativeTile[static_cast<size_t>(tileIndex)]);
        const bool hasDynamicCandidates =
            hasDynamicWholeChrOverrides ||
            hasDynamicHashCandidates ||
            (resolvedFullTileInRange &&
             hasDynamicOverridesByFullTile[static_cast<size_t>(resolvedFullTileIndex)]) ||
            hasDynamicOverflowTile ||
            (relativeTileInRange &&
             hasDynamicOverridesByRelativeTile[static_cast<size_t>(tileIndex)]);
        const bool hasDefaultCandidates =
            hasWholeChrDefaultOverrides ||
            hasDefaultOverrideHash ||
            (resolvedFullTileInRange &&
             hasDefaultOverridesByFullTile[static_cast<size_t>(resolvedFullTileIndex)]) ||
            hasDefaultOverflowTile ||
            (relativeTileInRange &&
             hasDefaultOverridesByRelativeTile[static_cast<size_t>(tileIndex)]);
        if(hasTileOriginContext) {
            if(const auto it = tileOriginOverrideLookupCache.find(tileOriginLookupKey); it != tileOriginOverrideLookupCache.end()) {
                MODMANAGER_PROFILE_COUNT(tileOriginOverrideCacheHits, 1);
                return it->second;
            }
        }
        if(canUseOverrideLookupCache && !hasDynamicCandidates) {
            if(const auto it = overrideLookupCache.find(lookupKey); it != overrideLookupCache.end()) {
                MODMANAGER_PROFILE_COUNT(overrideLookupCacheHits, 1);
                return it->second;
            }
        }
        if(!hasDynamicCandidates && !hasStaticExactCandidates && !hasDefaultCandidates) {
            if(cacheableByOrigin != nullptr) {
                *cacheableByOrigin = false;
            }
            if(canUseOverrideLookupCache) {
                overrideLookupCache.emplace(lookupKey, nullptr);
                MODMANAGER_PROFILE_COUNT(overrideLookupStores, 1);
            }
            MODMANAGER_PROFILE_COUNT(overrideFastRejects, 1);
            return nullptr;
        }
        bool tileOriginCacheable = hasTileOriginContext;

        auto scanFullTile = [&](int lookupTile, bool allowDefaultTileFallback) -> const PreparedOverride* {
            if(lookupTile == resolvedFullTileIndex && resolvedFullTileInRange) {
                if(const PreparedOverride* found = scanCandidates(overridesByFullTile[static_cast<size_t>(lookupTile)], allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx, hasTileOriginContext ? &tileOriginCacheable : nullptr)) {
                    return found;
                }
            }
            if(lookupTile == resolvedFullTileIndex && staticOverflowIt != overridesByOverflowFullTile.end()) {
                if(const PreparedOverride* found = scanCandidates(staticOverflowIt->second, allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx, hasTileOriginContext ? &tileOriginCacheable : nullptr)) {
                    return found;
                }
            }
            return nullptr;
        };

        auto scanRelativeTile = [&](int lookupTile, bool allowDefaultTileFallback) -> const PreparedOverride* {
            if(lookupTile >= 0 && lookupTile < static_cast<int>(overridesByRelativeTile.size())) {
                if(const PreparedOverride* found = scanCandidates(overridesByRelativeTile[static_cast<size_t>(lookupTile)], allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx, hasTileOriginContext ? &tileOriginCacheable : nullptr)) {
                    return found;
                }
            }
            return nullptr;
        };

        auto scanHash = [&](bool allowDefaultTileFallback) -> const PreparedOverride* {
            if(hasStaticHashCandidates) {
                if(const PreparedOverride* found = scanCandidates(staticHashIt->second, allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx, hasTileOriginContext ? &tileOriginCacheable : nullptr)) {
                    return found;
                }
            }
            return nullptr;
        };

        auto scanMergedFullTile = [&](int lookupTile, bool allowDefaultTileFallback) -> const PreparedOverride* {
            if(lookupTile == resolvedFullTileIndex && resolvedFullTileInRange) {
                return scanMergedCandidates(
                    overridesByFullTile[static_cast<size_t>(lookupTile)],
                    dynamicOverridesByFullTile[static_cast<size_t>(lookupTile)],
                    allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx,
                    hasTileOriginContext ? &tileOriginCacheable : nullptr);
            }
            const auto& staticCandidates = staticOverflowIt != overridesByOverflowFullTile.end() ? staticOverflowIt->second : emptyCandidates;
            const auto& dynamicCandidates = dynamicOverflowIt != dynamicOverridesByOverflowFullTile.end() ? dynamicOverflowIt->second : emptyCandidates;
            if(staticCandidates.empty() && dynamicCandidates.empty()) {
                return nullptr;
            }
            return scanMergedCandidates(staticCandidates, dynamicCandidates, allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx,
                hasTileOriginContext ? &tileOriginCacheable : nullptr);
        };

        auto scanMergedRelativeTile = [&](int lookupTile, bool allowDefaultTileFallback) -> const PreparedOverride* {
            if(lookupTile < 0 || lookupTile >= static_cast<int>(overridesByRelativeTile.size())) {
                return nullptr;
            }
            return scanMergedCandidates(
                overridesByRelativeTile[static_cast<size_t>(lookupTile)],
                dynamicOverridesByRelativeTile[static_cast<size_t>(lookupTile)],
                allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx,
                hasTileOriginContext ? &tileOriginCacheable : nullptr);
        };

        auto scanMergedHash = [&](bool allowDefaultTileFallback) -> const PreparedOverride* {
            const auto& staticCandidates = hasStaticHashCandidates ? staticHashIt->second : emptyCandidates;
            const auto& dynamicCandidates = hasDynamicHashCandidates ? dynamicHashIt->second : emptyCandidates;
            return scanMergedCandidates(staticCandidates, dynamicCandidates, allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx,
                hasTileOriginContext ? &tileOriginCacheable : nullptr);
        };

        auto scanMergedWhole = [&](bool allowDefaultTileFallback) -> const PreparedOverride* {
            return scanMergedCandidates(wholeChrOverrides, dynamicWholeChrOverrides, allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx,
                hasTileOriginContext ? &tileOriginCacheable : nullptr);
        };

        const auto storeCachedResult = [&](const PreparedOverride* result) {
            if(cacheableByOrigin != nullptr) {
                *cacheableByOrigin = hasTileOriginContext && tileOriginCacheable;
            }
            if(canUseOverrideLookupCache && !hasDynamicCandidates) {
                overrideLookupCache.emplace(lookupKey, result);
                MODMANAGER_PROFILE_COUNT(overrideLookupStores, 1);
            }
            if(hasTileOriginContext && tileOriginCacheable) {
                tileOriginOverrideLookupCache.emplace(tileOriginLookupKey, result);
            }
            return result;
        };

        const PreparedOverride* found = nullptr;
        if(resolvedFullTileInRange || hasStaticOverflowTile || hasDynamicOverflowTile) {
            found = hasDynamicCandidates ? scanMergedFullTile(resolvedFullTileIndex, false) : scanFullTile(resolvedFullTileIndex, false);
        }
        if(found == nullptr && fullTileIndex != tileIndex && relativeTileInRange) {
            found = hasDynamicCandidates ? scanMergedRelativeTile(tileIndex, false) : scanRelativeTile(tileIndex, false);
        }
        if(found == nullptr && (hasStaticExactCandidates || hasDynamicHashCandidates)) {
            found = hasDynamicCandidates ? scanMergedHash(false) : scanHash(false);
        }
        if(found == nullptr && (hasWholeChrOverrides || hasDynamicWholeChrOverrides)) {
            found = hasDynamicCandidates
                ? scanMergedWhole(false)
                : scanCandidates(wholeChrOverrides, false, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx, hasTileOriginContext ? &tileOriginCacheable : nullptr);
        }
        if(found != nullptr) {
            return storeCachedResult(found);
        }
        if(!hasDefaultCandidates) {
            return storeCachedResult(nullptr);
        }

        if((resolvedFullTileInRange && hasDefaultOverridesByFullTile[static_cast<size_t>(resolvedFullTileIndex)]) ||
           hasDefaultOverflowTile) {
            found = hasDynamicCandidates ? scanMergedFullTile(resolvedFullTileIndex, true) : scanFullTile(resolvedFullTileIndex, true);
        }
        if(found == nullptr && hasDefaultOverrideHash) {
            found = hasDynamicCandidates ? scanMergedHash(true) : scanHash(true);
        }
        if(found == nullptr && fullTileIndex == tileIndex &&
           relativeTileInRange &&
           hasDefaultOverridesByRelativeTile[static_cast<size_t>(tileIndex)]) {
            found = hasDynamicCandidates ? scanMergedRelativeTile(tileIndex, true) : scanRelativeTile(tileIndex, true);
        }
        if(found == nullptr && (hasWholeChrDefaultOverrides || hasDynamicWholeChrOverrides)) {
            found = hasDynamicCandidates
                ? scanMergedWhole(true)
                : scanCandidates(wholeChrOverrides, true, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx, hasTileOriginContext ? &tileOriginCacheable : nullptr);
        }
        return storeCachedResult(found);
    };

    auto sampleOverridePixel = [&](uint32_t baseColor, uint32_t /*fallbackLayerColor*/, const PreparedOverride* prepared, int tileIndex, int offsetX, int offsetY, int subX, int subY, uint8_t /*colorLowBits*/, const std::array<uint8_t, 3>& palette, bool horizontalMirror, bool verticalMirror, bool preserveSourceAlpha = false) {
        if(prepared == nullptr) {
            return baseColor;
        }
        const ChrOverride* override = prepared->override;
        const DecodedImage* image = prepared->image;
        const int localOffsetX = offsetX & 0x07;
        const int localOffsetY = offsetY & 0x07;
        const int sourceScale = prepared->sourceScale;
        const int sourceSubX = std::clamp((subX * sourceScale) / scale, 0, sourceScale - 1);
        const int sourceSubY = std::clamp((subY * sourceScale) / scale, 0, sourceScale - 1);
        const int tileSubX = horizontalMirror ? (7 - localOffsetX) * sourceScale + (sourceScale - 1 - sourceSubX) : localOffsetX * sourceScale + sourceSubX;
        const int tileSubY = verticalMirror ? (7 - localOffsetY) * sourceScale + (sourceScale - 1 - sourceSubY) : localOffsetY * sourceScale + sourceSubY;
        int srcX = override->hasSourcePosition() ? override->sourceX + tileSubX : tileSubX;
        int srcY = override->hasSourcePosition() ? override->sourceY + tileSubY : tileSubY;
        const int tilePixelSize = 8 * sourceScale;
        const bool atlasImage =
            !override->hasSourcePosition() &&
            (override->wholeChr() ||
             override->sourceTileOffset > 0 ||
             (image->width >= override->columns * tilePixelSize && image->height > tilePixelSize));
        if(atlasImage) {
            int sourceColumn = 0;
            int sourceRow = 0;
            if(override->wholeChr()) {
                if(tileIndex < 0) {
                    return baseColor;
                }
                const int sourceTile = tileIndex + override->sourceTileOffset;
                if(prepared->wholeChrLayout == ChrOverride::SourceLayout::PatternTables) {
                    const int table = sourceTile / 256;
                    const int tileInTable = sourceTile & 0xFF;
                    sourceColumn = (table * 16) + (tileInTable & 0x0F);
                    sourceRow = tileInTable >> 4;
                } else {
                    sourceColumn = sourceTile % override->columns;
                    sourceRow = sourceTile / override->columns;
                }
            } else if(override->sourceLayout == ChrOverride::SourceLayout::PatternTables) {
                const int sourceTile = override->sourceTileOffset;
                const int table = sourceTile / 256;
                const int tileInTable = sourceTile & 0xFF;
                sourceColumn = (table * 16) + (tileInTable & 0x0F);
                sourceRow = tileInTable >> 4;
            } else {
                const int sourceTile = override->sourceTileOffset;
                sourceColumn = sourceTile % override->columns;
                sourceRow = sourceTile / override->columns;
            }
            srcX += sourceColumn * tilePixelSize;
            srcY += sourceRow * tilePixelSize;
        }
        if(srcX < 0 || srcY < 0 || srcX >= image->width || srcY >= image->height) {
            return baseColor;
        }
        const size_t sourcePixelIndex = static_cast<size_t>(srcY) * static_cast<size_t>(image->width) + static_cast<size_t>(srcX);
        const uint32_t sourcePixel = image->rgba[sourcePixelIndex];
        if(override->ignorePalette) {
            const uint8_t sourceAlpha = static_cast<uint8_t>((sourcePixel >> 24u) & 0xFFu);
            if(sourceAlpha == 0) {
                return baseColor;
            }
            const uint32_t opaqueSource = (sourcePixel & 0x00FFFFFFu) | 0xFF000000u;
            if(!preserveSourceAlpha) {
                return opaqueSource;
            }
            if(sourceAlpha == 0xFF) {
                return opaqueSource;
            }
            return blendPixel(baseColor, opaqueSource, sourceAlpha);
        }
        const uint8_t sourceAlpha = static_cast<uint8_t>((sourcePixel >> 24u) & 0xFFu);
        if(sourceAlpha == 0) {
            return baseColor;
        }
        if(!image->indexedFourColor || image->indexedPixels.size() != image->rgba.size()) {
            return baseColor;
        }
        const uint8_t sourcePaletteIndex = image->indexedPixels[sourcePixelIndex];
        if(sourcePaletteIndex == 0) {
            if(preserveSourceAlpha) {
                return baseColor;
            }
            const uint32_t mappedColor =
                (snapshot.paletteColors[snapshot.universalBgColor & 0x3F] & 0x00FFFFFFu) |
                (static_cast<uint32_t>(sourceAlpha) << 24u);
            return blendPixel(baseColor, mappedColor, 255);
        }
        const int paletteIndex = static_cast<int>(sourcePaletteIndex) - 1;
        if(paletteIndex < 0 || paletteIndex >= static_cast<int>(palette.size())) {
            return baseColor;
        }
        const uint32_t mappedColor =
            (snapshot.paletteColors[palette[static_cast<size_t>(paletteIndex)] & 0x3F] & 0x00FFFFFFu) |
            (static_cast<uint32_t>(sourceAlpha) << 24u);
        if(sourceAlpha == 0xFF) {
            return mappedColor;
        }
        return blendPixel(baseColor, mappedColor, 255);
    };

    const ConditionContext backgroundConditionContext = {};
    std::array<const PreparedBackground*, 40> activeBackgroundsByPriority = {};
    {
        MODMANAGER_PROFILE_SCOPE(ComposeChrFrameBackgroundPrep);
        for(const PreparedBackground& prepared : preparedBackgrounds) {
            if(activeBackgroundsByPriority[static_cast<size_t>(prepared.priority)] != nullptr) {
                continue;
            }
            if(prepared.replacement == nullptr || !prepared.replacement->enabled) {
                continue;
            }
            bool matches = true;
            for(const MemoryCondition* condition : prepared.runtimeConditions) {
                if(condition != nullptr && !conditionMatchesAt(*condition, backgroundConditionContext)) {
                    matches = false;
                    break;
                }
            }
            if(matches) {
                activeBackgroundsByPriority[static_cast<size_t>(prepared.priority)] = &prepared;
            }
        }
    }

    struct ScanlineBackgroundLayer {
        const PreparedBackground* prepared = nullptr;
        int srcBaseX = 0;
        int baseSrcY = 0;
        int minNesX = 0;
        int maxNesX = -1;
        const uint32_t* row0 = nullptr;
        const uint32_t* row1 = nullptr;
        int maxSub = 0;
    };

    std::vector<const PreparedBackground*> lowPriorityBackgrounds;
    std::vector<const PreparedBackground*> midBeforeTileBackgrounds;
    std::vector<const PreparedBackground*> midAfterTileBackgrounds;
    std::vector<const PreparedBackground*> highPriorityBackgrounds;
    lowPriorityBackgrounds.reserve(10);
    midBeforeTileBackgrounds.reserve(10);
    midAfterTileBackgrounds.reserve(10);
    highPriorityBackgrounds.reserve(10);
    for(int priority = 0; priority < 40; ++priority) {
        if(const PreparedBackground* preparedBackground = activeBackgroundsByPriority[static_cast<size_t>(priority)]; preparedBackground != nullptr) {
            if(priority < 10) {
                lowPriorityBackgrounds.push_back(preparedBackground);
            } else if(priority < 20) {
                midBeforeTileBackgrounds.push_back(preparedBackground);
            } else if(priority < 30) {
                midAfterTileBackgrounds.push_back(preparedBackground);
            } else {
                highPriorityBackgrounds.push_back(preparedBackground);
            }
        }
    }

    auto resolveBackgroundLayer = [&](const ScanlineBackgroundLayer& activeLayer, int nesX) {
        ResolvedBackgroundLayer resolved;
        if(activeLayer.prepared == nullptr) {
            return resolved;
        }

        const PreparedBackground& prepared = *activeLayer.prepared;
        resolved.prepared = &prepared;

        if(nesX < activeLayer.minNesX || nesX > activeLayer.maxNesX) {
            return resolved;
        }

        const DecodedImage& image = *prepared.image;
        resolved.valid = true;
        resolved.baseSrcX = (activeLayer.srcBaseX + nesX) * prepared.backgroundScale;
        resolved.baseSrcY = activeLayer.baseSrcY;
        resolved.row0 = activeLayer.row0;
        resolved.row1 = activeLayer.row1;
        resolved.maxSub = activeLayer.maxSub;
        if(prepared.backgroundScale == 1) {
            resolved.uniformBlock = true;
            resolved.uniformColor =
                image.rgba[static_cast<size_t>(resolved.baseSrcY) * static_cast<size_t>(image.width) +
                           static_cast<size_t>(resolved.baseSrcX)];
        }
        return resolved;
    };

    auto sampleResolvedBackgroundPixel = [&](uint32_t dstColor, const ResolvedBackgroundLayer& resolved, int subX, int subY) {
        if(!resolved.valid || resolved.prepared == nullptr) {
            return dstColor;
        }

        const PreparedBackground& prepared = *resolved.prepared;
        if(resolved.uniformBlock) {
            if(prepared.opaqueCopy && ((resolved.uniformColor >> 24u) & 0xFFu) == 0xFFu) {
                return resolved.uniformColor;
            }
            return blendPixel(dstColor, resolved.uniformColor, prepared.alphaScale);
        }
        if(resolved.row0 == nullptr || resolved.row1 == nullptr) {
            return dstColor;
        }
        const int srcX0 = resolved.baseSrcX + std::clamp(subX, 0, resolved.maxSub);
        const uint32_t* srcRow = subY <= 0 ? resolved.row0 : resolved.row1;
        const uint32_t src = srcRow[static_cast<size_t>(srcX0)];
        if(prepared.opaqueCopy && ((src >> 24u) & 0xFFu) == 0xFFu) {
            return src;
        }
        return blendPixel(dstColor, src, prepared.alphaScale);
    };

    auto blockIsUniform = [](const auto& resolvedLayers, size_t count) {
        for(size_t i = 0; i < count; ++i) {
            if(resolvedLayers[i].valid && !resolvedLayers[i].uniformBlock) {
                return false;
            }
        }
        return true;
    };

    auto composeUniformBackgroundLayers = [&](uint32_t color, const auto& resolvedLayers, size_t count) {
        for(size_t i = 0; i < count; ++i) {
            color = sampleResolvedBackgroundPixel(color, resolvedLayers[i], 0, 0);
        }
        return color;
    };

    bool lastBackgroundOverrideCacheValid = false;
    int lastBackgroundOverrideOriginX = 0;
    int lastBackgroundOverrideOriginY = 0;
    int lastBackgroundOverrideFullTileIndex = -1;
    uint32_t lastBackgroundOverrideTileHash = 0;
    std::array<uint8_t, 3> lastBackgroundOverridePalette = {};
    struct CachedBackgroundOverrideState {
        const PreparedOverride* override = nullptr;
        const DecodedImage* image = nullptr;
        const uint32_t* rgbaData = nullptr;
        const uint8_t* indexedPixelsData = nullptr;
        uint32_t fallbackColor = 0;
        std::array<uint8_t, 3> palette = {};
        std::array<uint32_t, 4> mappedPalette = {};
        int tileSrcX = 0;
        int tileSrcY = 0;
        int sourceScale = 1;
        int imageWidth = 0;
        int imageHeight = 0;
        bool ignorePalette = false;
        bool usesIndexedPalette = false;
        bool tileSrcValid = false;
    };
    CachedBackgroundOverrideState lastBackgroundOverrideState;
    const int activeNesY0 = std::max(0, activeTop / scale);
    const int activeNesY1 = std::min(PPU::SCREEN_HEIGHT, (activeBottom + scale - 1) / scale);
    std::vector<ScanlineBackgroundLayer> lowPriorityScanlineLayers;
    std::vector<ScanlineBackgroundLayer> midBeforeTileScanlineLayers;
    std::vector<ScanlineBackgroundLayer> midAfterTileScanlineLayers;
    std::vector<ScanlineBackgroundLayer> highPriorityScanlineLayers;
    lowPriorityScanlineLayers.reserve(lowPriorityBackgrounds.size());
    midBeforeTileScanlineLayers.reserve(midBeforeTileBackgrounds.size());
    midAfterTileScanlineLayers.reserve(midAfterTileBackgrounds.size());
    highPriorityScanlineLayers.reserve(highPriorityBackgrounds.size());
    std::unordered_map<uint64_t, const PreparedOverride*> spriteOverrideRowCache;
    spriteOverrideRowCache.reserve(256);
    static constexpr int kVisibleTileColumns = PPU::SCREEN_WIDTH / 8;
    static constexpr int kVisibleTileRows = (PPU::SCREEN_HEIGHT + 7) / 8;
    struct CachedBackgroundTileStateEntry {
        bool valid = false;
        int originX = 0;
        int originY = 0;
        int fullTileIndex = -1;
        uint32_t tileHash = 0;
        std::array<uint8_t, 3> palette = {};
        CachedBackgroundOverrideState state;
    };
    std::array<CachedBackgroundTileStateEntry, kVisibleTileColumns * kVisibleTileRows> backgroundTileStateCache = {};
    MODMANAGER_PROFILE_SCOPE(ComposeChrFrameMainLoop);
    for(int nesY = activeNesY0; nesY < activeNesY1; ++nesY) {
        spriteOverrideRowCache.clear();
        const uint32_t* srcRow = sourceFramebuffer + static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH;
        const int blockY0 = std::max(activeTop, nesY * scale);
        const int blockY1 = std::min(activeBottom, (nesY + 1) * scale);
        if(blockY0 >= blockY1) continue;
        bool bgOverrideRowCacheValid = false;
        int bgOverrideRowCacheOriginX = 0;
        int bgOverrideRowCacheOriginY = 0;
        int bgOverrideRowCacheFullTileIndex = -1;
        uint32_t bgOverrideRowCacheTileHash = 0;
        const PreparedOverride* bgOverrideRowCacheOverride = nullptr;
        std::array<uint8_t, 3> bgOverrideRowCachePalette = {};
        std::array<std::array<uint32_t, 4>, 8> bgOverrideRowCacheBlocks = {};
        std::array<bool, 8> bgOverrideRowCacheOpaqueBlocks = {};
        std::array<std::array<uint32_t, 64>, 8> bgOverrideRowCacheGeneralBlocks = {};
        std::array<bool, 8> bgOverrideRowCacheGeneralOpaqueBlocks = {};
        bool noSpriteDirectRowCacheValid = false;
        int noSpriteDirectRowCacheOriginX = 0;
        int noSpriteDirectRowCacheOriginY = 0;
        int noSpriteDirectRowCacheFullTileIndex = -1;
        uint32_t noSpriteDirectRowCacheTileHash = 0;
        const PreparedOverride* noSpriteDirectRowCacheOverride = nullptr;
        std::array<uint8_t, 3> noSpriteDirectRowCachePalette = {};
        std::array<std::array<uint32_t, 4>, 8> noSpriteDirectRowCacheBlocks = {};

        auto buildScanlineLayers = [&](const std::vector<const PreparedBackground*>& activeLayers, std::vector<ScanlineBackgroundLayer>& scanlineLayers) {
            scanlineLayers.clear();
            for(const PreparedBackground* preparedPtr : activeLayers) {
                if(preparedPtr == nullptr || preparedPtr->image == nullptr) {
                    continue;
                }
                const PreparedBackground& prepared = *preparedPtr;
                const BackgroundReplacement& replacement = *prepared.replacement;
                const DecodedImage& image = *prepared.image;
                const int bgScrollX = static_cast<int>(scrollXForLine(nesY) * replacement.parallaxX) + replacement.scrollX;
                const int bgScrollY = static_cast<int>(scrollYForLine(nesY) * replacement.parallaxY) + replacement.scrollY;
                const int srcNesY = replacement.sourceY + nesY + bgScrollY;
                if(srcNesY < 0 || (srcNesY + 1) * prepared.backgroundScale > image.height) {
                    continue;
                }

                ScanlineBackgroundLayer scanlineLayer;
                scanlineLayer.prepared = &prepared;
                scanlineLayer.srcBaseX = replacement.sourceX + bgScrollX;
                scanlineLayer.baseSrcY = srcNesY * prepared.backgroundScale;
                scanlineLayer.minNesX = -scanlineLayer.srcBaseX;
                scanlineLayer.maxNesX = (image.width / prepared.backgroundScale) - 1 - scanlineLayer.srcBaseX;
                scanlineLayer.maxSub = prepared.backgroundScale - 1;
                scanlineLayer.row0 = image.rgba.data() + static_cast<size_t>(scanlineLayer.baseSrcY) * static_cast<size_t>(image.width);
                const int row1Y = scanlineLayer.baseSrcY + std::clamp(1, 0, scanlineLayer.maxSub);
                scanlineLayer.row1 = image.rgba.data() + static_cast<size_t>(row1Y) * static_cast<size_t>(image.width);
                if(scanlineLayer.minNesX <= scanlineLayer.maxNesX) {
                    scanlineLayers.push_back(scanlineLayer);
                }
            }
        };

        buildScanlineLayers(lowPriorityBackgrounds, lowPriorityScanlineLayers);
        buildScanlineLayers(midBeforeTileBackgrounds, midBeforeTileScanlineLayers);
        buildScanlineLayers(midAfterTileBackgrounds, midAfterTileScanlineLayers);
        buildScanlineLayers(highPriorityBackgrounds, highPriorityScanlineLayers);
        std::array<CachedBackgroundOverrideState, PPU::SCREEN_WIDTH> resolvedBackgroundOverrideStates = {};
        std::array<const CachedBackgroundOverrideState*, PPU::SCREEN_WIDTH> resolvedBackgroundOverrideStatePtrs = {};

        auto resolveBackgroundOverrideState = [&](int nesX,
                                                 const PPU::DebugModBackgroundPixel& bgPixel,
                                                 CachedBackgroundOverrideState& outState,
                                                 bool& outCacheableByOrigin) {
            MODMANAGER_PROFILE_SCOPE(ComposeChrFrameBackgroundOverride);
            MODMANAGER_PROFILE_SCOPE(ComposeChrFrameBackgroundOverrideSetup);
            outState = {};
            outCacheableByOrigin = false;
            const int bgFullTileIndex = bgPixel.tileIndex != 0xFFFF ? static_cast<int>(bgPixel.tileIndex) : -1;
            outState.palette = { bgPixel.palette[0], bgPixel.palette[1], bgPixel.palette[2] };
            outState.fallbackColor = snapshot.paletteColors[bgPixel.paletteIndex & 0x3F];
            outState.mappedPalette[0] = snapshot.paletteColors[snapshot.universalBgColor & 0x3F];
            outState.mappedPalette[1] = snapshot.paletteColors[outState.palette[0] & 0x3F];
            outState.mappedPalette[2] = snapshot.paletteColors[outState.palette[1] & 0x3F];
            outState.mappedPalette[3] = snapshot.paletteColors[outState.palette[2] & 0x3F];
            if(bgFullTileIndex < 0) {
                return;
            }

            const int tileOriginX = nesX - static_cast<int>(bgPixel.offsetX);
            const int tileOriginY = nesY - static_cast<int>(bgPixel.offsetY);
            if(lastBackgroundOverrideCacheValid &&
               lastBackgroundOverrideOriginX == tileOriginX &&
               lastBackgroundOverrideOriginY == tileOriginY &&
               lastBackgroundOverrideFullTileIndex == bgFullTileIndex &&
               lastBackgroundOverrideTileHash == bgPixel.tileHash &&
               lastBackgroundOverridePalette == outState.palette) {
                outState = lastBackgroundOverrideState;
                return;
            }

            const ConditionContext context = { nesX, nesY, &bgPixel, nullptr };
            if(onlyWholeChrOverrides && fastBackgroundOverride != nullptr) {
                outState.override = fastBackgroundOverride;
                outCacheableByOrigin = true;
            } else {
                outState.override = findOverride(
                    ChrOverride::Target::Background,
                    bgFullTileIndex & 0xFF,
                    bgFullTileIndex,
                    bgFullTileIndex / 256,
                    outState.palette,
                    false,
                    false,
                    false,
                    context,
                    &outCacheableByOrigin);
            }
            if(outState.override != nullptr) {
                const PreparedOverride* preparedOverride = outState.override;
                const ChrOverride* override = preparedOverride->override;
                outState.image = preparedOverride->image;
                if(override != nullptr && outState.image != nullptr) {
                    outState.rgbaData = preparedOverride->rgbaData;
                    outState.indexedPixelsData = preparedOverride->indexedPixelsData;
                    outState.sourceScale = preparedOverride->sourceScale;
                    outState.imageWidth = preparedOverride->imageWidth;
                    outState.imageHeight = preparedOverride->imageHeight;
                    outState.ignorePalette = preparedOverride->ignorePalette;
                    outState.usesIndexedPalette = preparedOverride->usesIndexedPalette;
                    if(preparedOverride->fixedTileSourceValid) {
                        outState.tileSrcX = preparedOverride->fixedTileSrcX;
                        outState.tileSrcY = preparedOverride->fixedTileSrcY;
                        outState.tileSrcValid = true;
                    } else {
                        const int tilePixelSize = 8 * outState.sourceScale;
                        const int sourceTile = bgFullTileIndex + preparedOverride->sourceTileOffset;
                        int sourceColumn = 0;
                        int sourceRow = 0;
                        if(preparedOverride->wholeChrLayout == ChrOverride::SourceLayout::PatternTables) {
                            const int table = sourceTile / 256;
                            const int tileInTable = sourceTile & 0xFF;
                            sourceColumn = (table * 16) + (tileInTable & 0x0F);
                            sourceRow = tileInTable >> 4;
                        } else {
                            sourceColumn = sourceTile % preparedOverride->sourceColumns;
                            sourceRow = sourceTile / preparedOverride->sourceColumns;
                        }
                        outState.tileSrcX = sourceColumn * tilePixelSize;
                        outState.tileSrcY = sourceRow * tilePixelSize;
                        outState.tileSrcValid = true;
                    }
                }
            }

            lastBackgroundOverrideCacheValid = true;
            lastBackgroundOverrideOriginX = tileOriginX;
            lastBackgroundOverrideOriginY = tileOriginY;
            lastBackgroundOverrideFullTileIndex = bgFullTileIndex;
            lastBackgroundOverrideTileHash = bgPixel.tileHash;
            lastBackgroundOverridePalette = outState.palette;
            lastBackgroundOverrideState = outState;
        };

        bool backgroundOverrideTileRowCacheValid = false;
        int backgroundOverrideTileRowCacheOriginX = 0;
        int backgroundOverrideTileRowCacheOriginY = 0;
        int backgroundOverrideTileRowCacheFullTileIndex = -1;
        uint32_t backgroundOverrideTileRowCacheTileHash = 0;
        std::array<uint8_t, 3> backgroundOverrideTileRowCachePalette = {};
        const CachedBackgroundOverrideState* backgroundOverrideTileRowCacheState = nullptr;
        for(int nesX = 0; nesX < PPU::SCREEN_WIDTH; ++nesX) {
            const size_t pixelIndex = static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH + static_cast<size_t>(nesX);
            const PPU::DebugModBackgroundPixel* bgPixel =
                pixelIndex < backgroundPixelsCount ? &backgroundPixelsData[pixelIndex] : nullptr;
            if(bgPixel == nullptr || !bgPixel->valid) {
                resolvedBackgroundOverrideStatePtrs[static_cast<size_t>(nesX)] = nullptr;
                backgroundOverrideTileRowCacheValid = false;
                continue;
            }

            const int bgFullTileIndex = bgPixel->tileIndex != 0xFFFF ? static_cast<int>(bgPixel->tileIndex) : -1;
            const int tileOriginX = nesX - static_cast<int>(bgPixel->offsetX);
            const int tileOriginY = nesY - static_cast<int>(bgPixel->offsetY);
            const std::array<uint8_t, 3> palette = { bgPixel->palette[0], bgPixel->palette[1], bgPixel->palette[2] };
            CachedBackgroundTileStateEntry* cachedTileStateEntry = nullptr;
            if(tileOriginX >= 0 &&
               tileOriginY >= 0 &&
               (tileOriginX & 0x07) == 0 &&
               (tileOriginY & 0x07) == 0) {
                const int tileColumn = tileOriginX >> 3;
                const int tileRow = tileOriginY >> 3;
                if(tileColumn >= 0 && tileColumn < kVisibleTileColumns &&
                   tileRow >= 0 && tileRow < kVisibleTileRows) {
                    cachedTileStateEntry =
                        &backgroundTileStateCache[static_cast<size_t>(tileRow) * kVisibleTileColumns +
                                                  static_cast<size_t>(tileColumn)];
                    if(cachedTileStateEntry->valid &&
                       cachedTileStateEntry->originX == tileOriginX &&
                       cachedTileStateEntry->originY == tileOriginY &&
                       cachedTileStateEntry->fullTileIndex == bgFullTileIndex &&
                       cachedTileStateEntry->tileHash == bgPixel->tileHash &&
                       cachedTileStateEntry->palette == palette) {
                        resolvedBackgroundOverrideStatePtrs[static_cast<size_t>(nesX)] =
                            &cachedTileStateEntry->state;
                        backgroundOverrideTileRowCacheValid = true;
                        backgroundOverrideTileRowCacheOriginX = tileOriginX;
                        backgroundOverrideTileRowCacheOriginY = tileOriginY;
                        backgroundOverrideTileRowCacheFullTileIndex = bgFullTileIndex;
                        backgroundOverrideTileRowCacheTileHash = bgPixel->tileHash;
                        backgroundOverrideTileRowCachePalette = palette;
                        backgroundOverrideTileRowCacheState = &cachedTileStateEntry->state;
                        continue;
                    }
                }
            }
            if(backgroundOverrideTileRowCacheValid &&
               backgroundOverrideTileRowCacheOriginX == tileOriginX &&
               backgroundOverrideTileRowCacheOriginY == tileOriginY &&
               backgroundOverrideTileRowCacheFullTileIndex == bgFullTileIndex &&
               backgroundOverrideTileRowCacheTileHash == bgPixel->tileHash &&
               backgroundOverrideTileRowCachePalette == palette) {
                resolvedBackgroundOverrideStatePtrs[static_cast<size_t>(nesX)] = backgroundOverrideTileRowCacheState;
                continue;
            }

            CachedBackgroundOverrideState& resolvedState =
                resolvedBackgroundOverrideStates[static_cast<size_t>(nesX)];
            bool cacheableByOrigin = false;
            resolveBackgroundOverrideState(nesX, *bgPixel, resolvedState, cacheableByOrigin);
            resolvedBackgroundOverrideStatePtrs[static_cast<size_t>(nesX)] = &resolvedState;
            if(cacheableByOrigin && cachedTileStateEntry != nullptr) {
                cachedTileStateEntry->valid = true;
                cachedTileStateEntry->originX = tileOriginX;
                cachedTileStateEntry->originY = tileOriginY;
                cachedTileStateEntry->fullTileIndex = bgFullTileIndex;
                cachedTileStateEntry->tileHash = bgPixel->tileHash;
                cachedTileStateEntry->palette = palette;
                cachedTileStateEntry->state = resolvedState;
                resolvedBackgroundOverrideStatePtrs[static_cast<size_t>(nesX)] =
                    &cachedTileStateEntry->state;
            }
            backgroundOverrideTileRowCacheValid = true;
            backgroundOverrideTileRowCacheOriginX = tileOriginX;
            backgroundOverrideTileRowCacheOriginY = tileOriginY;
            backgroundOverrideTileRowCacheFullTileIndex = bgFullTileIndex;
            backgroundOverrideTileRowCacheTileHash = bgPixel->tileHash;
            backgroundOverrideTileRowCachePalette = palette;
            backgroundOverrideTileRowCacheState =
                resolvedBackgroundOverrideStatePtrs[static_cast<size_t>(nesX)];
        }

        auto initResolvedLayerStates = [&](const std::vector<ScanlineBackgroundLayer>& activeLayers, auto& resolvedLayers, size_t& resolvedCount, int nesX) {
            resolvedCount = std::min(activeLayers.size(), resolvedLayers.size());
            for(size_t i = 0; i < resolvedCount; ++i) {
                resolvedLayers[i] = resolveBackgroundLayer(activeLayers[i], nesX);
            }
        };
        auto advanceResolvedLayerStates = [&](const std::vector<ScanlineBackgroundLayer>& activeLayers, auto& resolvedLayers, size_t resolvedCount, int nesX) {
            for(size_t i = 0; i < resolvedCount; ++i) {
                const ScanlineBackgroundLayer& activeLayer = activeLayers[i];
                ResolvedBackgroundLayer& resolved = resolvedLayers[i];
                const PreparedBackground* prepared = activeLayer.prepared;
                if(prepared == nullptr) {
                    resolved = {};
                    continue;
                }

                resolved.prepared = prepared;
                resolved.baseSrcY = activeLayer.baseSrcY;
                resolved.row0 = activeLayer.row0;
                resolved.row1 = activeLayer.row1;
                resolved.maxSub = activeLayer.maxSub;
                const bool validNow = nesX >= activeLayer.minNesX && nesX <= activeLayer.maxNesX;
                if(!validNow) {
                    resolved.valid = false;
                    resolved.uniformBlock = false;
                    continue;
                }

                const bool wasValid = resolved.valid;
                resolved.valid = true;
                if(wasValid) {
                    resolved.baseSrcX += prepared->backgroundScale;
                } else {
                    resolved.baseSrcX = (activeLayer.srcBaseX + nesX) * prepared->backgroundScale;
                }
                if(prepared->backgroundScale == 1) {
                    resolved.uniformBlock = true;
                    resolved.uniformColor = activeLayer.row0[static_cast<size_t>(resolved.baseSrcX)];
                } else {
                    resolved.uniformBlock = false;
                }
            }
        };

        std::array<ResolvedBackgroundLayer, 10> resolvedLowPriorityBackgrounds = {};
        size_t resolvedLowPriorityBackgroundCount = 0;
        std::array<ResolvedBackgroundLayer, 10> resolvedMidBeforeTileBackgrounds = {};
        size_t resolvedMidBeforeTileBackgroundCount = 0;
        std::array<ResolvedBackgroundLayer, 10> resolvedMidAfterTileBackgrounds = {};
        size_t resolvedMidAfterTileBackgroundCount = 0;
        std::array<ResolvedBackgroundLayer, 10> resolvedHighPriorityBackgrounds = {};
        size_t resolvedHighPriorityBackgroundCount = 0;
        bool resolvedBackgroundStatesInitialized = false;

        for(int nesX = 0; nesX < PPU::SCREEN_WIDTH; ++nesX) {
            if(!resolvedBackgroundStatesInitialized) {
                initResolvedLayerStates(lowPriorityScanlineLayers, resolvedLowPriorityBackgrounds, resolvedLowPriorityBackgroundCount, nesX);
                initResolvedLayerStates(midBeforeTileScanlineLayers, resolvedMidBeforeTileBackgrounds, resolvedMidBeforeTileBackgroundCount, nesX);
                initResolvedLayerStates(midAfterTileScanlineLayers, resolvedMidAfterTileBackgrounds, resolvedMidAfterTileBackgroundCount, nesX);
                initResolvedLayerStates(highPriorityScanlineLayers, resolvedHighPriorityBackgrounds, resolvedHighPriorityBackgroundCount, nesX);
                resolvedBackgroundStatesInitialized = true;
            } else {
                advanceResolvedLayerStates(lowPriorityScanlineLayers, resolvedLowPriorityBackgrounds, resolvedLowPriorityBackgroundCount, nesX);
                advanceResolvedLayerStates(midBeforeTileScanlineLayers, resolvedMidBeforeTileBackgrounds, resolvedMidBeforeTileBackgroundCount, nesX);
                advanceResolvedLayerStates(midAfterTileScanlineLayers, resolvedMidAfterTileBackgrounds, resolvedMidAfterTileBackgroundCount, nesX);
                advanceResolvedLayerStates(highPriorityScanlineLayers, resolvedHighPriorityBackgrounds, resolvedHighPriorityBackgroundCount, nesX);
            }

            const uint32_t baseColor = srcRow[nesX];
            const size_t pixelIndex = static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH + static_cast<size_t>(nesX);
            const PPU::DebugModBackgroundPixel* bgPixel =
                pixelIndex < backgroundPixelsCount ? &backgroundPixelsData[pixelIndex] : nullptr;
            const PPU::DebugModSpritePixel* spritePixel =
                pixelIndex < rawSpritePixelsCount ? &rawSpritePixelsData[pixelIndex] : nullptr;
            const bool hasSpriteCandidates = spritePixel != nullptr && spritePixel->count > 0;
            CachedBackgroundOverrideState emptyBackgroundOverrideState = {};
            emptyBackgroundOverrideState.fallbackColor = baseColor;
            const CachedBackgroundOverrideState* backgroundOverrideStatePtr =
                resolvedBackgroundOverrideStatePtrs[static_cast<size_t>(nesX)];
            const CachedBackgroundOverrideState& backgroundOverrideState =
                backgroundOverrideStatePtr != nullptr ? *backgroundOverrideStatePtr : emptyBackgroundOverrideState;
            const PreparedOverride* backgroundOverride = backgroundOverrideState.override;
            const uint32_t backgroundFallbackColor = backgroundOverrideState.fallbackColor;
            const std::array<uint8_t, 3>& backgroundPalette = backgroundOverrideState.palette;
            const std::array<uint32_t, 4>& backgroundMappedPalette = backgroundOverrideState.mappedPalette;
            const DecodedImage* backgroundOverrideImage = backgroundOverrideState.image;
            const uint32_t* backgroundOverrideRgbaData = backgroundOverrideState.rgbaData;
            const uint8_t* backgroundOverrideIndexedPixelsData = backgroundOverrideState.indexedPixelsData;
            const int backgroundOverrideTileSrcX = backgroundOverrideState.tileSrcX;
            const int backgroundOverrideTileSrcY = backgroundOverrideState.tileSrcY;
            const int backgroundOverrideSourceScale = backgroundOverrideState.sourceScale;
            const int backgroundOverrideImageWidth = backgroundOverrideState.imageWidth;
            const int backgroundOverrideImageHeight = backgroundOverrideState.imageHeight;
            const bool backgroundOverrideIgnorePalette = backgroundOverrideState.ignorePalette;
            const bool backgroundOverrideUsesIndexedPalette = backgroundOverrideState.usesIndexedPalette;
            const bool backgroundOverrideTileSrcValid = backgroundOverrideState.tileSrcValid;
            bool hasAnyValidSpriteCandidate = false;
            if(hasSpriteCandidates) {
                for(int i = 0; i < static_cast<int>(spritePixel->count); ++i) {
                    if(spritePixel->candidates[static_cast<size_t>(i)].valid) {
                        hasAnyValidSpriteCandidate = true;
                        break;
                    }
                }
            }
            const int blockX0 = nesX * scale;
            const int blockX1 = std::min(width, blockX0 + scale);
            if(blockX0 >= blockX1) continue;
            const int subYStart = std::max(0, blockY0 - nesY * scale);
            const int subYEnd = std::min(scale, blockY1 - nesY * scale);
            if(subYStart >= subYEnd) continue;
            const int blockWidth = blockX1 - blockX0;
            std::array<uint32_t*, 8> dstRows = {};
            for(int subY = subYStart; subY < subYEnd; ++subY) {
                const int outY = nesY * scale + subY;
                dstRows[static_cast<size_t>(subY)] =
                    framebuffer.data() + static_cast<size_t>(outY) * static_cast<size_t>(width) + static_cast<size_t>(blockX0);
            }

            auto mappedBackgroundOverridePixelAt = [&](int srcX, int srcY) -> uint32_t {
                if(backgroundOverride == nullptr ||
                   !backgroundOverrideTileSrcValid ||
                   backgroundOverrideRgbaData == nullptr ||
                   srcX < 0 || srcY < 0 ||
                   srcX >= backgroundOverrideImageWidth ||
                   srcY >= backgroundOverrideImageHeight) {
                    return 0u;
                }
                const size_t sourcePixelIndex =
                    static_cast<size_t>(srcY) * static_cast<size_t>(backgroundOverrideImageWidth) + static_cast<size_t>(srcX);
                const uint32_t sourcePixel = backgroundOverrideRgbaData[sourcePixelIndex];
                const uint8_t sourceAlpha = static_cast<uint8_t>((sourcePixel >> 24u) & 0xFFu);
                if(backgroundOverrideIgnorePalette) {
                    return sourceAlpha == 0 ? 0u : ((sourcePixel & 0x00FFFFFFu) | (static_cast<uint32_t>(sourceAlpha) << 24u));
                }
                if(sourceAlpha == 0 || !backgroundOverrideUsesIndexedPalette || backgroundOverrideIndexedPixelsData == nullptr) {
                    return 0u;
                }
                const uint8_t sourcePaletteIndex = backgroundOverrideIndexedPixelsData[sourcePixelIndex];
                const uint32_t paletteColor =
                    sourcePaletteIndex == 0
                        ? backgroundMappedPalette[0]
                        : backgroundMappedPalette[static_cast<size_t>(sourcePaletteIndex)];
                return (paletteColor & 0x00FFFFFFu) | (static_cast<uint32_t>(sourceAlpha) << 24u);
            };

            auto blendMappedBackgroundOverridePixel = [&](uint32_t color, uint32_t mappedPixel) {
                const uint8_t alpha = static_cast<uint8_t>((mappedPixel >> 24u) & 0xFFu);
                if(alpha == 0) {
                    return color;
                }
                return alpha == 0xFF ? mappedPixel : blendPixel(color, mappedPixel, 255);
            };

            auto applyPreparedBackgroundBlockScale2 = [&](const PreparedBackground& prepared,
                                                         const uint32_t* row0,
                                                         const uint32_t* row1,
                                                         int baseSrcX,
                                                         int maxSub,
                                                         uint32_t& block00,
                                                         uint32_t& block01,
                                                         uint32_t& block10,
                                                         uint32_t& block11) {
                if(prepared.backgroundScale == 1) {
                    const uint32_t src = row0[static_cast<size_t>(baseSrcX)];
                    if(prepared.opaqueCopy && ((src >> 24u) & 0xFFu) == 0xFFu) {
                        block00 = src;
                        block01 = src;
                        block10 = src;
                        block11 = src;
                    } else {
                        block00 = blendPixel(block00, src, prepared.alphaScale);
                        block01 = blendPixel(block01, src, prepared.alphaScale);
                        block10 = blendPixel(block10, src, prepared.alphaScale);
                        block11 = blendPixel(block11, src, prepared.alphaScale);
                    }
                    return;
                }

                const int srcX0 = baseSrcX;
                const int srcX1 = baseSrcX + std::clamp(1, 0, maxSub);
                const uint32_t src00 = row0[static_cast<size_t>(srcX0)];
                const uint32_t src01 = row0[static_cast<size_t>(srcX1)];
                const uint32_t src10 = row1[static_cast<size_t>(srcX0)];
                const uint32_t src11 = row1[static_cast<size_t>(srcX1)];
                if(prepared.opaqueCopy &&
                    ((src00 >> 24u) & 0xFFu) == 0xFFu &&
                    ((src01 >> 24u) & 0xFFu) == 0xFFu &&
                    ((src10 >> 24u) & 0xFFu) == 0xFFu &&
                    ((src11 >> 24u) & 0xFFu) == 0xFFu) {
                    block00 = src00;
                    block01 = src01;
                    block10 = src10;
                    block11 = src11;
                } else {
                    block00 = blendPixel(block00, src00, prepared.alphaScale);
                    block01 = blendPixel(block01, src01, prepared.alphaScale);
                    block10 = blendPixel(block10, src10, prepared.alphaScale);
                    block11 = blendPixel(block11, src11, prepared.alphaScale);
                }
            };

            const bool canUseScale2NoSpriteDirectScanlinePath =
                scale == 2 &&
                blockWidth == 2 &&
                subYStart == 0 &&
                subYEnd == 2 &&
                !hasAnyValidSpriteCandidate;
            if(canUseScale2NoSpriteDirectScanlinePath) {
                auto applyScanlineLayerScale2 = [&](const ScanlineBackgroundLayer& layer,
                                                    int pixelX,
                                                    uint32_t& block00,
                                                    uint32_t& block01,
                                                    uint32_t& block10,
                                                    uint32_t& block11) {
                    if(pixelX < layer.minNesX || pixelX > layer.maxNesX || layer.prepared == nullptr) {
                        return;
                    }
                    const PreparedBackground& prepared = *layer.prepared;
                    const int baseSrcX = (layer.srcBaseX + pixelX) * prepared.backgroundScale;
                    applyPreparedBackgroundBlockScale2(prepared, layer.row0, layer.row1, baseSrcX, layer.maxSub, block00, block01, block10, block11);
                };

                auto sampleBackgroundOverrideScale2 = [&](uint32_t color, int subX, int subY) {
                    if(backgroundOverride == nullptr || bgPixel == nullptr || !backgroundOverrideTileSrcValid) {
                        return color;
                    }
                    const int localOffsetX = bgPixel->offsetX & 0x07;
                    const int localOffsetY = bgPixel->offsetY & 0x07;
                    const int sourceSubX = std::clamp((subX * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                    const int sourceSubY = std::clamp((subY * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                    const int srcX = backgroundOverrideTileSrcX + localOffsetX * backgroundOverrideSourceScale + sourceSubX;
                    const int srcY = backgroundOverrideTileSrcY + localOffsetY * backgroundOverrideSourceScale + sourceSubY;
                    return blendMappedBackgroundOverridePixel(color, mappedBackgroundOverridePixelAt(srcX, srcY));
                };

                auto ensureBackgroundOverrideRowCache = [&]() {
                    if(backgroundOverride == nullptr || bgPixel == nullptr || !backgroundOverrideTileSrcValid) {
                        return false;
                    }
                    const int tileOriginX = nesX - static_cast<int>(bgPixel->offsetX);
                    const int tileOriginY = nesY - static_cast<int>(bgPixel->offsetY);
                    const int bgFullTileIndex = bgPixel->tileIndex != 0xFFFF ? static_cast<int>(bgPixel->tileIndex) : -1;
                    if(bgOverrideRowCacheValid &&
                        bgOverrideRowCacheOriginX == tileOriginX &&
                        bgOverrideRowCacheOriginY == tileOriginY &&
                        bgOverrideRowCacheFullTileIndex == bgFullTileIndex &&
                        bgOverrideRowCacheTileHash == bgPixel->tileHash &&
                        bgOverrideRowCacheOverride == backgroundOverride &&
                        bgOverrideRowCachePalette == backgroundPalette) {
                        return true;
                    }

                    bgOverrideRowCacheValid = true;
                    bgOverrideRowCacheOriginX = tileOriginX;
                    bgOverrideRowCacheOriginY = tileOriginY;
                    bgOverrideRowCacheFullTileIndex = bgFullTileIndex;
                    bgOverrideRowCacheTileHash = bgPixel->tileHash;
                    bgOverrideRowCacheOverride = backgroundOverride;
                    bgOverrideRowCachePalette = backgroundPalette;
                    bgOverrideRowCacheOpaqueBlocks.fill(false);
                    bgOverrideRowCacheGeneralOpaqueBlocks.fill(false);

                    const int localOffsetY = bgPixel->offsetY & 0x07;
                    const int tileBaseX = backgroundOverrideTileSrcX;
                    const int tileBaseY = backgroundOverrideTileSrcY + localOffsetY * backgroundOverrideSourceScale;
                    for(int tileOffsetX = 0; tileOffsetX < 8; ++tileOffsetX) {
                        auto& block = bgOverrideRowCacheBlocks[static_cast<size_t>(tileOffsetX)];
                        bool opaqueBlock = true;
                        for(int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {
                            const int subX = sampleIndex & 1;
                            const int subY = sampleIndex >> 1;
                            const int sourceSubX = std::clamp((subX * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                            const int sourceSubY = std::clamp((subY * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                            const int srcX = tileBaseX + tileOffsetX * backgroundOverrideSourceScale + sourceSubX;
                            const int srcY = tileBaseY + sourceSubY;
                            const uint32_t mappedColor = mappedBackgroundOverridePixelAt(srcX, srcY);
                            if(((mappedColor >> 24u) & 0xFFu) != 0xFFu) {
                                opaqueBlock = false;
                            }
                            block[static_cast<size_t>(sampleIndex)] = mappedColor;
                        }
                        bgOverrideRowCacheOpaqueBlocks[static_cast<size_t>(tileOffsetX)] = opaqueBlock;

                        auto& generalBlock = bgOverrideRowCacheGeneralBlocks[static_cast<size_t>(tileOffsetX)];
                        bool generalOpaqueBlock = true;
                        const int sampleScale = std::min(scale, 8);
                        for(int subY = 0; subY < sampleScale; ++subY) {
                            for(int subX = 0; subX < sampleScale; ++subX) {
                                const int sourceSubX = std::clamp((subX * backgroundOverrideSourceScale) / scale, 0, backgroundOverrideSourceScale - 1);
                                const int sourceSubY = std::clamp((subY * backgroundOverrideSourceScale) / scale, 0, backgroundOverrideSourceScale - 1);
                                const int srcX = tileBaseX + tileOffsetX * backgroundOverrideSourceScale + sourceSubX;
                                const int srcY = tileBaseY + sourceSubY;
                                const uint32_t mappedColor = mappedBackgroundOverridePixelAt(srcX, srcY);
                                if(((mappedColor >> 24u) & 0xFFu) != 0xFFu) {
                                    generalOpaqueBlock = false;
                                }
                                generalBlock[static_cast<size_t>(subY) * 8u + static_cast<size_t>(subX)] = mappedColor;
                            }
                        }
                        bgOverrideRowCacheGeneralOpaqueBlocks[static_cast<size_t>(tileOffsetX)] = generalOpaqueBlock;
                    }
                    return true;
                };

                auto ensureNoSpriteDirectRowCache = [&]() {
                    if(bgPixel == nullptr || !bgPixel->valid) {
                        return false;
                    }
                    const int tileOriginX = nesX - static_cast<int>(bgPixel->offsetX);
                    const int tileOriginY = nesY - static_cast<int>(bgPixel->offsetY);
                    const int bgFullTileIndex = bgPixel->tileIndex != 0xFFFF ? static_cast<int>(bgPixel->tileIndex) : -1;
                    if(noSpriteDirectRowCacheValid &&
                        noSpriteDirectRowCacheOriginX == tileOriginX &&
                        noSpriteDirectRowCacheOriginY == tileOriginY &&
                        noSpriteDirectRowCacheFullTileIndex == bgFullTileIndex &&
                        noSpriteDirectRowCacheTileHash == bgPixel->tileHash &&
                        noSpriteDirectRowCacheOverride == backgroundOverride &&
                        noSpriteDirectRowCachePalette == backgroundPalette) {
                        return true;
                    }

                    noSpriteDirectRowCacheValid = true;
                    noSpriteDirectRowCacheOriginX = tileOriginX;
                    noSpriteDirectRowCacheOriginY = tileOriginY;
                    noSpriteDirectRowCacheFullTileIndex = bgFullTileIndex;
                    noSpriteDirectRowCacheTileHash = bgPixel->tileHash;
                    noSpriteDirectRowCacheOverride = backgroundOverride;
                    noSpriteDirectRowCachePalette = backgroundPalette;

                    const bool hasRowOverrideCache = backgroundOverride != nullptr && ensureBackgroundOverrideRowCache();
                    auto rowBlockHasOpaqueOverride = [&](int tileOffsetX) {
                        return hasRowOverrideCache &&
                            bgOverrideRowCacheOpaqueBlocks[static_cast<size_t>(tileOffsetX)];
                    };
                    const bool hasAnyRowLayers =
                        !lowPriorityScanlineLayers.empty() ||
                        !midBeforeTileScanlineLayers.empty() ||
                        !midAfterTileScanlineLayers.empty() ||
                        !highPriorityScanlineLayers.empty();
                    auto applyLayerListToRow = [&](const std::vector<ScanlineBackgroundLayer>& layers, bool skipOpaqueOverrideBlocks) {
                        for(const ScanlineBackgroundLayer& layer : layers) {
                            if(layer.prepared == nullptr || layer.prepared->image == nullptr) {
                                continue;
                            }
                            const int startOffset = std::max(0, layer.minNesX - tileOriginX);
                            const int endOffset = std::min(7, layer.maxNesX - tileOriginX);
                            if(startOffset > endOffset) {
                                continue;
                            }
                            const PreparedBackground& prepared = *layer.prepared;
                            for(int tileOffsetX = startOffset; tileOffsetX <= endOffset; ++tileOffsetX) {
                                if(skipOpaqueOverrideBlocks && rowBlockHasOpaqueOverride(tileOffsetX)) {
                                    continue;
                                }
                                auto& block = noSpriteDirectRowCacheBlocks[static_cast<size_t>(tileOffsetX)];
                                const int baseSrcX = (layer.srcBaseX + tileOriginX + tileOffsetX) * prepared.backgroundScale;
                                applyPreparedBackgroundBlockScale2(prepared, layer.row0, layer.row1, baseSrcX, layer.maxSub, block[0], block[1], block[2], block[3]);
                            }
                        }
                    };

                    for(int tileOffsetX = 0; tileOffsetX < 8; ++tileOffsetX) {
                        const int pixelX = tileOriginX + tileOffsetX;
                        auto& block = noSpriteDirectRowCacheBlocks[static_cast<size_t>(tileOffsetX)];
                        if(pixelX < 0 || pixelX >= PPU::SCREEN_WIDTH) {
                            block = {0u, 0u, 0u, 0u};
                            continue;
                        }
                        if(rowBlockHasOpaqueOverride(tileOffsetX)) {
                            block = bgOverrideRowCacheBlocks[static_cast<size_t>(tileOffsetX)];
                            continue;
                        }
                        const uint32_t base = disableOriginalBackgroundTiles ? universalBackgroundColor : srcRow[pixelX];
                        block = {base, base, base, base};
                    }

                    if(hasAnyRowLayers) {
                        applyLayerListToRow(lowPriorityScanlineLayers, true);
                        applyLayerListToRow(midBeforeTileScanlineLayers, true);
                    }

                    for(int tileOffsetX = 0; tileOffsetX < 8; ++tileOffsetX) {
                        const int pixelX = tileOriginX + tileOffsetX;
                        if(pixelX < 0 || pixelX >= PPU::SCREEN_WIDTH) {
                            continue;
                        }
                        auto& block = noSpriteDirectRowCacheBlocks[static_cast<size_t>(tileOffsetX)];
                        if(backgroundOverride != nullptr) {
                            if(rowBlockHasOpaqueOverride(tileOffsetX)) {
                                continue;
                            }
                            if(hasRowOverrideCache) {
                                const auto& overrideBlock = bgOverrideRowCacheBlocks[static_cast<size_t>(tileOffsetX)];
                                block[0] = blendMappedBackgroundOverridePixel(block[0], overrideBlock[0]);
                                block[1] = blendMappedBackgroundOverridePixel(block[1], overrideBlock[1]);
                                block[2] = blendMappedBackgroundOverridePixel(block[2], overrideBlock[2]);
                                block[3] = blendMappedBackgroundOverridePixel(block[3], overrideBlock[3]);
                            } else {
                                const auto sampleBackgroundOverrideScale2AtOffsetX = [&](uint32_t color, int localOffsetX, int subX, int subY) {
                                    if(backgroundOverride == nullptr || bgPixel == nullptr || !backgroundOverrideTileSrcValid) {
                                        return color;
                                    }
                                    const int localOffsetY = bgPixel->offsetY & 0x07;
                                    const int sourceSubX = std::clamp((subX * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                                    const int sourceSubY = std::clamp((subY * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                                    const int srcX = backgroundOverrideTileSrcX + localOffsetX * backgroundOverrideSourceScale + sourceSubX;
                                    const int srcY = backgroundOverrideTileSrcY + localOffsetY * backgroundOverrideSourceScale + sourceSubY;
                                    return blendMappedBackgroundOverridePixel(color, mappedBackgroundOverridePixelAt(srcX, srcY));
                                };
                                block[0] = sampleBackgroundOverrideScale2AtOffsetX(block[0], tileOffsetX, 0, 0);
                                block[1] = sampleBackgroundOverrideScale2AtOffsetX(block[1], tileOffsetX, 1, 0);
                                block[2] = sampleBackgroundOverrideScale2AtOffsetX(block[2], tileOffsetX, 0, 1);
                                block[3] = sampleBackgroundOverrideScale2AtOffsetX(block[3], tileOffsetX, 1, 1);
                            }
                        } else if(!disableOriginalBackgroundTiles) {
                            block = {backgroundFallbackColor, backgroundFallbackColor, backgroundFallbackColor, backgroundFallbackColor};
                        }
                    }

                    if(hasAnyRowLayers) {
                        applyLayerListToRow(midAfterTileScanlineLayers, false);
                        applyLayerListToRow(highPriorityScanlineLayers, false);
                    }
                    return true;
                };

                uint32_t block00 = disableOriginalBackgroundTiles ? universalBackgroundColor : baseColor;
                uint32_t block01 = block00;
                uint32_t block10 = block00;
                uint32_t block11 = block00;

                const int localOffsetX = (bgPixel != nullptr) ? (bgPixel->offsetX & 0x07) : 0;
                if(ensureNoSpriteDirectRowCache()) {
                    const auto& block = noSpriteDirectRowCacheBlocks[static_cast<size_t>(localOffsetX)];
                    block00 = block[0];
                    block01 = block[1];
                    block10 = block[2];
                    block11 = block[3];
                } else {
                    const bool hasBackgroundOverrideRowCache =
                        backgroundOverride != nullptr && ensureBackgroundOverrideRowCache();
                    const bool opaqueCurrentOverrideBlock =
                        hasBackgroundOverrideRowCache &&
                        bgOverrideRowCacheOpaqueBlocks[static_cast<size_t>(localOffsetX)];

                    if(opaqueCurrentOverrideBlock) {
                        const auto& block = bgOverrideRowCacheBlocks[static_cast<size_t>(localOffsetX)];
                        block00 = block[0];
                        block01 = block[1];
                        block10 = block[2];
                        block11 = block[3];
                    } else {
                        for(const ScanlineBackgroundLayer& layer : lowPriorityScanlineLayers) {
                            applyScanlineLayerScale2(layer, nesX, block00, block01, block10, block11);
                        }
                        for(const ScanlineBackgroundLayer& layer : midBeforeTileScanlineLayers) {
                            applyScanlineLayerScale2(layer, nesX, block00, block01, block10, block11);
                        }
                    }

                    if(bgPixel != nullptr && bgPixel->valid) {
                        if(backgroundOverride != nullptr) {
                            if(opaqueCurrentOverrideBlock) {
                            } else if(hasBackgroundOverrideRowCache) {
                                const auto& block = bgOverrideRowCacheBlocks[static_cast<size_t>(localOffsetX)];
                                const auto applyMapped = [&](uint32_t base, uint32_t mapped) {
                                    const uint8_t alpha = static_cast<uint8_t>((mapped >> 24u) & 0xFFu);
                                    if(alpha == 0) {
                                        return base;
                                    }
                                    return alpha == 0xFF ? mapped : blendPixel(base, mapped, 255);
                                };
                                block00 = applyMapped(block00, block[0]);
                                block01 = applyMapped(block01, block[1]);
                                block10 = applyMapped(block10, block[2]);
                                block11 = applyMapped(block11, block[3]);
                            } else {
                                block00 = sampleBackgroundOverrideScale2(block00, 0, 0);
                                block01 = sampleBackgroundOverrideScale2(block01, 1, 0);
                                block10 = sampleBackgroundOverrideScale2(block10, 0, 1);
                                block11 = sampleBackgroundOverrideScale2(block11, 1, 1);
                            }
                        } else if(!disableOriginalBackgroundTiles) {
                            block00 = backgroundFallbackColor;
                            block01 = backgroundFallbackColor;
                            block10 = backgroundFallbackColor;
                            block11 = backgroundFallbackColor;
                        }
                    }

                    for(const ScanlineBackgroundLayer& layer : midAfterTileScanlineLayers) {
                        applyScanlineLayerScale2(layer, nesX, block00, block01, block10, block11);
                    }

                    if(!highPriorityScanlineLayers.empty()) {
                        for(const ScanlineBackgroundLayer& layer : highPriorityScanlineLayers) {
                            applyScanlineLayerScale2(layer, nesX, block00, block01, block10, block11);
                        }
                    }
                }

                dstRows[0][0] = block00;
                dstRows[0][1] = block01;
                dstRows[1][0] = block10;
                dstRows[1][1] = block11;
                continue;
            }

            const bool lowUniform = blockIsUniform(resolvedLowPriorityBackgrounds, resolvedLowPriorityBackgroundCount);
            const bool midBeforeUniform = blockIsUniform(resolvedMidBeforeTileBackgrounds, resolvedMidBeforeTileBackgroundCount);
            const bool midAfterUniform = blockIsUniform(resolvedMidAfterTileBackgrounds, resolvedMidAfterTileBackgroundCount);
            const bool highUniform = blockIsUniform(resolvedHighPriorityBackgrounds, resolvedHighPriorityBackgroundCount);
            const bool canFillUniformBlock =
                backgroundOverride == nullptr &&
                !hasAnyValidSpriteCandidate &&
                lowUniform &&
                midBeforeUniform &&
                midAfterUniform &&
                highUniform;
            const uint32_t initialColor =
                disableOriginalBackgroundTiles
                    ? universalBackgroundColor
                    : (hasAnyValidSpriteCandidate ? universalBackgroundColor : baseColor);
            const bool canPrecomposeBeforeBg = lowUniform && midBeforeUniform;
            const uint32_t precomposedBeforeBgColor =
                canPrecomposeBeforeBg
                    ? composeUniformBackgroundLayers(
                        composeUniformBackgroundLayers(initialColor, resolvedLowPriorityBackgrounds, resolvedLowPriorityBackgroundCount),
                        resolvedMidBeforeTileBackgrounds,
                        resolvedMidBeforeTileBackgroundCount)
                    : initialColor;
            const bool bgValid = bgPixel != nullptr && bgPixel->valid;
            const bool applyBackgroundFallback =
                bgValid &&
                !disableOriginalBackgroundTiles &&
                bgPixel->colorLowBits != 0;
            const bool hasMidAfterBackgrounds = resolvedMidAfterTileBackgroundCount > 0;
            const bool hasHighPriorityBackgrounds = resolvedHighPriorityBackgroundCount > 0;
            auto applyResolvedLayerToScale2Block = [&](const ResolvedBackgroundLayer& resolved,
                                                       uint32_t& block00,
                                                       uint32_t& block01,
                                                       uint32_t& block10,
                                                       uint32_t& block11) {
                if(!resolved.valid || resolved.prepared == nullptr) {
                    return;
                }

                const PreparedBackground& prepared = *resolved.prepared;
                if(resolved.uniformBlock) {
                    if(prepared.opaqueCopy && ((resolved.uniformColor >> 24u) & 0xFFu) == 0xFFu) {
                        block00 = resolved.uniformColor;
                        block01 = resolved.uniformColor;
                        block10 = resolved.uniformColor;
                        block11 = resolved.uniformColor;
                    } else {
                        block00 = blendPixel(block00, resolved.uniformColor, prepared.alphaScale);
                        block01 = blendPixel(block01, resolved.uniformColor, prepared.alphaScale);
                        block10 = blendPixel(block10, resolved.uniformColor, prepared.alphaScale);
                        block11 = blendPixel(block11, resolved.uniformColor, prepared.alphaScale);
                    }
                    return;
                }
                if(resolved.row0 == nullptr || resolved.row1 == nullptr) {
                    return;
                }
                applyPreparedBackgroundBlockScale2(
                    prepared,
                    resolved.row0,
                    resolved.row1,
                    resolved.baseSrcX,
                    resolved.maxSub,
                    block00,
                    block01,
                    block10,
                    block11);
            };
            if(canFillUniformBlock) {
                uint32_t color = precomposedBeforeBgColor;
                if(applyBackgroundFallback) {
                    color = backgroundFallbackColor;
                }
                color = composeUniformBackgroundLayers(color, resolvedMidAfterTileBackgrounds, resolvedMidAfterTileBackgroundCount);
                color = composeUniformBackgroundLayers(color, resolvedHighPriorityBackgrounds, resolvedHighPriorityBackgroundCount);

                for(int outY = blockY0; outY < blockY1; ++outY) {
                    uint32_t* dstRow = framebuffer.data() + static_cast<size_t>(outY) * static_cast<size_t>(width);
                    for(int outX = blockX0; outX < blockX1; ++outX) {
                        dstRow[outX] = color;
                    }
                }
                continue;
            }

            const bool canUseScale2NoSpriteFastPath =
                scale == 2 &&
                blockWidth == 2 &&
                subYStart == 0 &&
                subYEnd == 2 &&
                !hasAnyValidSpriteCandidate;
            if(canUseScale2NoSpriteFastPath) {
                auto sampleBackgroundOverrideScale2 = [&](uint32_t color, int subX, int subY) {
                    if(backgroundOverride == nullptr || bgPixel == nullptr || !backgroundOverrideTileSrcValid) {
                        return color;
                    }
                    const int localOffsetX = bgPixel->offsetX & 0x07;
                    const int localOffsetY = bgPixel->offsetY & 0x07;
                    const int sourceScale = backgroundOverrideSourceScale;
                    const int sourceSubX = std::clamp((subX * sourceScale) / 2, 0, sourceScale - 1);
                    const int sourceSubY = std::clamp((subY * sourceScale) / 2, 0, sourceScale - 1);
                    const int srcX = backgroundOverrideTileSrcX + localOffsetX * sourceScale + sourceSubX;
                    const int srcY = backgroundOverrideTileSrcY + localOffsetY * sourceScale + sourceSubY;
                    return blendMappedBackgroundOverridePixel(color, mappedBackgroundOverridePixelAt(srcX, srcY));
                };

                auto getOpaqueBackgroundOverrideColorScale2 = [&](int subX, int subY, uint32_t& outColor) {
                    if(backgroundOverride == nullptr || bgPixel == nullptr || !backgroundOverrideTileSrcValid) {
                        return false;
                    }

                    const int localOffsetX = bgPixel->offsetX & 0x07;
                    const int localOffsetY = bgPixel->offsetY & 0x07;
                    const int sourceScale = backgroundOverrideSourceScale;
                    const int sourceSubX = std::clamp((subX * sourceScale) / 2, 0, sourceScale - 1);
                    const int sourceSubY = std::clamp((subY * sourceScale) / 2, 0, sourceScale - 1);
                    const int srcX = backgroundOverrideTileSrcX + localOffsetX * sourceScale + sourceSubX;
                    const int srcY = backgroundOverrideTileSrcY + localOffsetY * sourceScale + sourceSubY;
                    const uint32_t mappedPixel = mappedBackgroundOverridePixelAt(srcX, srcY);
                    if(((mappedPixel >> 24u) & 0xFFu) != 0xFFu) {
                        return false;
                    }
                    outColor = mappedPixel;
                    return true;
                };

                auto ensureBackgroundOverrideRowCache = [&]() {
                    if(backgroundOverride == nullptr || bgPixel == nullptr || !backgroundOverrideTileSrcValid || backgroundOverrideImage == nullptr) {
                        return false;
                    }
                    const int tileOriginX = nesX - static_cast<int>(bgPixel->offsetX);
                    const int tileOriginY = nesY - static_cast<int>(bgPixel->offsetY);
                    const int bgFullTileIndex = bgPixel->tileIndex != 0xFFFF ? static_cast<int>(bgPixel->tileIndex) : -1;
                    if(bgOverrideRowCacheValid &&
                        bgOverrideRowCacheOriginX == tileOriginX &&
                        bgOverrideRowCacheOriginY == tileOriginY &&
                        bgOverrideRowCacheFullTileIndex == bgFullTileIndex &&
                        bgOverrideRowCacheTileHash == bgPixel->tileHash &&
                        bgOverrideRowCacheOverride == backgroundOverride &&
                        bgOverrideRowCachePalette == backgroundPalette) {
                        return true;
                    }

                    bgOverrideRowCacheValid = true;
                    bgOverrideRowCacheOriginX = tileOriginX;
                    bgOverrideRowCacheOriginY = tileOriginY;
                    bgOverrideRowCacheFullTileIndex = bgFullTileIndex;
                    bgOverrideRowCacheTileHash = bgPixel->tileHash;
                    bgOverrideRowCacheOverride = backgroundOverride;
                    bgOverrideRowCachePalette = backgroundPalette;
                    bgOverrideRowCacheOpaqueBlocks.fill(false);

                    const int localOffsetY = bgPixel->offsetY & 0x07;
                    const int tileBaseX = backgroundOverrideTileSrcX;
                    const int tileBaseY = backgroundOverrideTileSrcY + localOffsetY * backgroundOverrideSourceScale;
                    for(int tileOffsetX = 0; tileOffsetX < 8; ++tileOffsetX) {
                        auto& block = bgOverrideRowCacheBlocks[static_cast<size_t>(tileOffsetX)];
                        bool opaqueBlock = true;
                        for(int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {
                            const int subX = sampleIndex & 1;
                            const int subY = sampleIndex >> 1;
                            const int sourceSubX = std::clamp((subX * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                            const int sourceSubY = std::clamp((subY * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                            const int srcX = tileBaseX + tileOffsetX * backgroundOverrideSourceScale + sourceSubX;
                            const int srcY = tileBaseY + sourceSubY;
                            const uint32_t mappedColor = mappedBackgroundOverridePixelAt(srcX, srcY);
                            if(((mappedColor >> 24u) & 0xFFu) != 0xFFu) {
                                opaqueBlock = false;
                            }
                            block[static_cast<size_t>(sampleIndex)] = mappedColor;
                        }
                        bgOverrideRowCacheOpaqueBlocks[static_cast<size_t>(tileOffsetX)] = opaqueBlock;
                    }
                    return true;
                };

                uint32_t block00 = canPrecomposeBeforeBg ? precomposedBeforeBgColor : initialColor;
                uint32_t block01 = block00;
                uint32_t block10 = block00;
                uint32_t block11 = block00;
                const int localOffsetX = (bgPixel != nullptr) ? (bgPixel->offsetX & 0x07) : 0;
                const bool hasBackgroundOverrideRowCache =
                    bgValid && backgroundOverride != nullptr && ensureBackgroundOverrideRowCache();
                const bool opaqueOverrideBlock =
                    hasBackgroundOverrideRowCache &&
                    bgOverrideRowCacheOpaqueBlocks[static_cast<size_t>(localOffsetX)];
                if(opaqueOverrideBlock) {
                    const auto& block = bgOverrideRowCacheBlocks[static_cast<size_t>(localOffsetX)];
                    block00 = block[0];
                    block01 = block[1];
                    block10 = block[2];
                    block11 = block[3];
                } else if(bgValid && backgroundOverride != nullptr) {
                    getOpaqueBackgroundOverrideColorScale2(0, 0, block00);
                    getOpaqueBackgroundOverrideColorScale2(1, 0, block01);
                    getOpaqueBackgroundOverrideColorScale2(0, 1, block10);
                    getOpaqueBackgroundOverrideColorScale2(1, 1, block11);
                }

                if(!opaqueOverrideBlock && !canPrecomposeBeforeBg) {
                    for(size_t i = 0; i < resolvedLowPriorityBackgroundCount; ++i) {
                        applyResolvedLayerToScale2Block(resolvedLowPriorityBackgrounds[i], block00, block01, block10, block11);
                    }
                    for(size_t i = 0; i < resolvedMidBeforeTileBackgroundCount; ++i) {
                        applyResolvedLayerToScale2Block(resolvedMidBeforeTileBackgrounds[i], block00, block01, block10, block11);
                    }
                }

                if(!opaqueOverrideBlock && bgValid) {
                    if(backgroundOverride != nullptr) {
                        if(hasBackgroundOverrideRowCache) {
                            const auto& block = bgOverrideRowCacheBlocks[static_cast<size_t>(localOffsetX)];
                            const uint32_t src00 = block[0];
                            const uint32_t src01 = block[1];
                            const uint32_t src10 = block[2];
                            const uint32_t src11 = block[3];
                            block00 = blendMappedBackgroundOverridePixel(block00, src00);
                            block01 = blendMappedBackgroundOverridePixel(block01, src01);
                            block10 = blendMappedBackgroundOverridePixel(block10, src10);
                            block11 = blendMappedBackgroundOverridePixel(block11, src11);
                        } else {
                            block00 = sampleBackgroundOverrideScale2(block00, 0, 0);
                            block01 = sampleBackgroundOverrideScale2(block01, 1, 0);
                            block10 = sampleBackgroundOverrideScale2(block10, 0, 1);
                            block11 = sampleBackgroundOverrideScale2(block11, 1, 1);
                        }
                    } else if(applyBackgroundFallback) {
                        block00 = backgroundFallbackColor;
                        block01 = backgroundFallbackColor;
                        block10 = backgroundFallbackColor;
                        block11 = backgroundFallbackColor;
                    }
                }

                if(hasMidAfterBackgrounds) {
                    for(size_t i = 0; i < resolvedMidAfterTileBackgroundCount; ++i) {
                        applyResolvedLayerToScale2Block(resolvedMidAfterTileBackgrounds[i], block00, block01, block10, block11);
                    }
                }


                if(hasHighPriorityBackgrounds) {
                    for(size_t i = 0; i < resolvedHighPriorityBackgroundCount; ++i) {
                        applyResolvedLayerToScale2Block(resolvedHighPriorityBackgrounds[i], block00, block01, block10, block11);
                    }
                }

                dstRows[0][0] = block00;
                dstRows[0][1] = block01;
                dstRows[1][0] = block10;
                dstRows[1][1] = block11;
                continue;
            }

            std::array<uint32_t, 64> baseBlockColors = {};
            auto fillBlockColors = [&](uint32_t color) {
                for(int subY = subYStart; subY < subYEnd; ++subY) {
                    uint32_t* blockRow = baseBlockColors.data() + static_cast<size_t>(subY) * 8u;
                    for(int subX = 0; subX < blockWidth; ++subX) {
                        blockRow[static_cast<size_t>(subX)] = color;
                    }
                }
            };
            const bool canUseScale2BlockPath =
                scale == 2 &&
                blockWidth == 2 &&
                subYStart == 0 &&
                subYEnd == 2;

            auto applyResolvedLayerToBlock = [&](const ResolvedBackgroundLayer& resolved) {
                if(!resolved.valid || resolved.prepared == nullptr) {
                    return;
                }

                const PreparedBackground& prepared = *resolved.prepared;
                if(resolved.uniformBlock) {
                    if(prepared.opaqueCopy && ((resolved.uniformColor >> 24u) & 0xFFu) == 0xFFu) {
                        for(int subY = subYStart; subY < subYEnd; ++subY) {
                            uint32_t* blockRow = baseBlockColors.data() + static_cast<size_t>(subY) * 8u;
                            for(int subX = 0; subX < blockWidth; ++subX) {
                                blockRow[static_cast<size_t>(subX)] = resolved.uniformColor;
                            }
                        }
                    } else {
                        for(int subY = subYStart; subY < subYEnd; ++subY) {
                            uint32_t* blockRow = baseBlockColors.data() + static_cast<size_t>(subY) * 8u;
                            for(int subX = 0; subX < blockWidth; ++subX) {
                                blockRow[static_cast<size_t>(subX)] =
                                    blendPixel(blockRow[static_cast<size_t>(subX)], resolved.uniformColor, prepared.alphaScale);
                            }
                        }
                    }
                    return;
                }
                if(resolved.row0 == nullptr || resolved.row1 == nullptr) {
                    return;
                }

                for(int subY = subYStart; subY < subYEnd; ++subY) {
                    uint32_t* blockRow = baseBlockColors.data() + static_cast<size_t>(subY) * 8u;
                    const uint32_t* srcRow = subY <= 0 ? resolved.row0 : resolved.row1;
                    for(int subX = 0; subX < blockWidth; ++subX) {
                        const uint32_t src = srcRow[static_cast<size_t>(resolved.baseSrcX + std::clamp(subX, 0, resolved.maxSub))];
                        if(prepared.opaqueCopy && ((src >> 24u) & 0xFFu) == 0xFFu) {
                            blockRow[static_cast<size_t>(subX)] = src;
                        } else {
                            blockRow[static_cast<size_t>(subX)] =
                                blendPixel(blockRow[static_cast<size_t>(subX)], src, prepared.alphaScale);
                        }
                    }
                }
            };

            auto applyResolvedLayersToBlock = [&](const auto& resolvedLayers, size_t count) {
                if(count == 0) {
                    return;
                }
                if(canUseScale2BlockPath) {
                    uint32_t block00 = baseBlockColors[0];
                    uint32_t block01 = baseBlockColors[1];
                    uint32_t block10 = baseBlockColors[8];
                    uint32_t block11 = baseBlockColors[9];
                    for(size_t i = 0; i < count; ++i) {
                        applyResolvedLayerToScale2Block(resolvedLayers[i], block00, block01, block10, block11);
                    }
                    baseBlockColors[0] = block00;
                    baseBlockColors[1] = block01;
                    baseBlockColors[8] = block10;
                    baseBlockColors[9] = block11;
                    return;
                }
                for(size_t i = 0; i < count; ++i) {
                    applyResolvedLayerToBlock(resolvedLayers[i]);
                }
            };

            auto ensureGeneralBackgroundOverrideRowCache = [&]() {
                if(backgroundOverride == nullptr ||
                   bgPixel == nullptr ||
                   !backgroundOverrideTileSrcValid ||
                   backgroundOverrideImage == nullptr) {
                    return false;
                }
                const int tileOriginX = nesX - static_cast<int>(bgPixel->offsetX);
                const int tileOriginY = nesY - static_cast<int>(bgPixel->offsetY);
                const int bgFullTileIndex = bgPixel->tileIndex != 0xFFFF ? static_cast<int>(bgPixel->tileIndex) : -1;
                if(bgOverrideRowCacheValid &&
                   bgOverrideRowCacheOriginX == tileOriginX &&
                   bgOverrideRowCacheOriginY == tileOriginY &&
                   bgOverrideRowCacheFullTileIndex == bgFullTileIndex &&
                   bgOverrideRowCacheTileHash == bgPixel->tileHash &&
                   bgOverrideRowCacheOverride == backgroundOverride &&
                   bgOverrideRowCachePalette == backgroundPalette) {
                    return true;
                }

                bgOverrideRowCacheValid = true;
                bgOverrideRowCacheOriginX = tileOriginX;
                bgOverrideRowCacheOriginY = tileOriginY;
                bgOverrideRowCacheFullTileIndex = bgFullTileIndex;
                bgOverrideRowCacheTileHash = bgPixel->tileHash;
                bgOverrideRowCacheOverride = backgroundOverride;
                bgOverrideRowCachePalette = backgroundPalette;
                bgOverrideRowCacheOpaqueBlocks.fill(false);
                bgOverrideRowCacheGeneralOpaqueBlocks.fill(false);

                const int localOffsetY = bgPixel->offsetY & 0x07;
                const int tileBaseX = backgroundOverrideTileSrcX;
                const int tileBaseY = backgroundOverrideTileSrcY + localOffsetY * backgroundOverrideSourceScale;
                const int sampleScale = std::min(scale, 8);
                for(int tileOffsetX = 0; tileOffsetX < 8; ++tileOffsetX) {
                    auto& scale2Block = bgOverrideRowCacheBlocks[static_cast<size_t>(tileOffsetX)];
                    bool opaqueScale2Block = true;
                    for(int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {
                        const int subX = sampleIndex & 1;
                        const int subY = sampleIndex >> 1;
                        const int sourceSubX = std::clamp((subX * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                        const int sourceSubY = std::clamp((subY * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                        const int srcX = tileBaseX + tileOffsetX * backgroundOverrideSourceScale + sourceSubX;
                        const int srcY = tileBaseY + sourceSubY;
                        const uint32_t mappedColor = mappedBackgroundOverridePixelAt(srcX, srcY);
                        if(((mappedColor >> 24u) & 0xFFu) != 0xFFu) {
                            opaqueScale2Block = false;
                        }
                        scale2Block[static_cast<size_t>(sampleIndex)] = mappedColor;
                    }
                    bgOverrideRowCacheOpaqueBlocks[static_cast<size_t>(tileOffsetX)] = opaqueScale2Block;

                    auto& generalBlock = bgOverrideRowCacheGeneralBlocks[static_cast<size_t>(tileOffsetX)];
                    bool opaqueGeneralBlock = true;
                    for(int subY = 0; subY < sampleScale; ++subY) {
                        for(int subX = 0; subX < sampleScale; ++subX) {
                            const int sourceSubX = std::clamp((subX * backgroundOverrideSourceScale) / scale, 0, backgroundOverrideSourceScale - 1);
                            const int sourceSubY = std::clamp((subY * backgroundOverrideSourceScale) / scale, 0, backgroundOverrideSourceScale - 1);
                            const int srcX = tileBaseX + tileOffsetX * backgroundOverrideSourceScale + sourceSubX;
                            const int srcY = tileBaseY + sourceSubY;
                            const uint32_t mappedColor = mappedBackgroundOverridePixelAt(srcX, srcY);
                            if(((mappedColor >> 24u) & 0xFFu) != 0xFFu) {
                                opaqueGeneralBlock = false;
                            }
                            generalBlock[static_cast<size_t>(subY) * 8u + static_cast<size_t>(subX)] = mappedColor;
                        }
                    }
                    bgOverrideRowCacheGeneralOpaqueBlocks[static_cast<size_t>(tileOffsetX)] = opaqueGeneralBlock;
                }
                return true;
            };

            auto applyCachedBackgroundOverrideToBlock = [&]() -> bool {
                if(backgroundOverride == nullptr ||
                   bgPixel == nullptr ||
                   !backgroundOverrideTileSrcValid) {
                    return false;
                }

                const int localOffsetX = bgPixel->offsetX & 0x07;
                const int localOffsetY = bgPixel->offsetY & 0x07;

                if(canUseScale2BlockPath) {
                    MODMANAGER_PROFILE_SCOPE(ComposeChrFrameBackgroundOverrideCachedApply);
                    const int sourceScale = backgroundOverrideSourceScale;
                    const int tileBaseX = backgroundOverrideTileSrcX + localOffsetX * sourceScale;
                    const int tileBaseY = backgroundOverrideTileSrcY + localOffsetY * sourceScale;
                    std::array<int, 2> srcXs = {};
                    std::array<int, 2> srcYs = {};
                    for(int subX = 0; subX < 2; ++subX) {
                        srcXs[static_cast<size_t>(subX)] =
                            tileBaseX + std::clamp((subX * sourceScale) / scale, 0, sourceScale - 1);
                    }
                    for(int subY = 0; subY < 2; ++subY) {
                        srcYs[static_cast<size_t>(subY)] =
                            tileBaseY + std::clamp((subY * sourceScale) / scale, 0, sourceScale - 1);
                    }
                    baseBlockColors[0] = blendMappedBackgroundOverridePixel(
                        baseBlockColors[0],
                        mappedBackgroundOverridePixelAt(srcXs[0], srcYs[0]));
                    baseBlockColors[1] = blendMappedBackgroundOverridePixel(
                        baseBlockColors[1],
                        mappedBackgroundOverridePixelAt(srcXs[1], srcYs[0]));
                    baseBlockColors[8] = blendMappedBackgroundOverridePixel(
                        baseBlockColors[8],
                        mappedBackgroundOverridePixelAt(srcXs[0], srcYs[1]));
                    baseBlockColors[9] = blendMappedBackgroundOverridePixel(
                        baseBlockColors[9],
                        mappedBackgroundOverridePixelAt(srcXs[1], srcYs[1]));
                    return true;
                }

                const bool canUseGeneralRowCache =
                    scale <= 8 &&
                    blockWidth <= 8 &&
                    ensureGeneralBackgroundOverrideRowCache();
                if(canUseGeneralRowCache) {
                    MODMANAGER_PROFILE_SCOPE(ComposeChrFrameBackgroundOverrideCachedApply);
                    const auto& overrideBlock =
                        bgOverrideRowCacheGeneralBlocks[static_cast<size_t>(localOffsetX)];
                    const bool opaqueBlock =
                        bgOverrideRowCacheGeneralOpaqueBlocks[static_cast<size_t>(localOffsetX)];
                    for(int subY = subYStart; subY < subYEnd; ++subY) {
                        uint32_t* blockRow = baseBlockColors.data() + static_cast<size_t>(subY) * 8u;
                        const size_t rowOffset = static_cast<size_t>(subY) * 8u;
                        if(opaqueBlock) {
                            for(int subX = 0; subX < blockWidth; ++subX) {
                                blockRow[static_cast<size_t>(subX)] =
                                    overrideBlock[rowOffset + static_cast<size_t>(subX)];
                            }
                        } else {
                            for(int subX = 0; subX < blockWidth; ++subX) {
                                blockRow[static_cast<size_t>(subX)] = blendMappedBackgroundOverridePixel(
                                    blockRow[static_cast<size_t>(subX)],
                                    overrideBlock[rowOffset + static_cast<size_t>(subX)]);
                            }
                        }
                    }
                    return true;
                }

                MODMANAGER_PROFILE_SCOPE(ComposeChrFrameBackgroundOverrideFallbackSample);
                const int sourceScale = backgroundOverrideSourceScale;
                const int tileBaseX = backgroundOverrideTileSrcX + localOffsetX * sourceScale;
                const int tileBaseY = backgroundOverrideTileSrcY + localOffsetY * sourceScale;
                std::array<int, 8> srcXs = {};
                std::array<int, 8> srcYs = {};
                for(int subX = 0; subX < blockWidth; ++subX) {
                    srcXs[static_cast<size_t>(subX)] =
                        tileBaseX + std::clamp((subX * sourceScale) / scale, 0, sourceScale - 1);
                }
                for(int subY = subYStart; subY < subYEnd; ++subY) {
                    srcYs[static_cast<size_t>(subY)] =
                        tileBaseY + std::clamp((subY * sourceScale) / scale, 0, sourceScale - 1);
                }

                for(int subY = subYStart; subY < subYEnd; ++subY) {
                    uint32_t* blockRow = baseBlockColors.data() + static_cast<size_t>(subY) * 8u;
                    const int srcY = srcYs[static_cast<size_t>(subY)];
                    for(int subX = 0; subX < blockWidth; ++subX) {
                        blockRow[static_cast<size_t>(subX)] = blendMappedBackgroundOverridePixel(
                            blockRow[static_cast<size_t>(subX)],
                            mappedBackgroundOverridePixelAt(srcXs[static_cast<size_t>(subX)], srcY));
                    }
                }
                return true;
            };

            auto applyBackgroundOverrideToBlock = [&]() {
                if(!bgValid) {
                    return;
                }
                if(backgroundOverride != nullptr) {
                    if(applyCachedBackgroundOverrideToBlock()) {
                        return;
                    }
                    MODMANAGER_PROFILE_SCOPE(ComposeChrFrameBackgroundOverrideFallbackSample);
                    if(canUseScale2BlockPath) {
                        baseBlockColors[0] = sampleOverridePixel(
                            baseBlockColors[0],
                            backgroundFallbackColor,
                            backgroundOverride,
                            bgPixel->tileIndex,
                            bgPixel->offsetX,
                            bgPixel->offsetY,
                            0,
                            0,
                            bgPixel->colorLowBits,
                            backgroundPalette,
                            false,
                            false
                        );
                        baseBlockColors[1] = sampleOverridePixel(
                            baseBlockColors[1],
                            backgroundFallbackColor,
                            backgroundOverride,
                            bgPixel->tileIndex,
                            bgPixel->offsetX,
                            bgPixel->offsetY,
                            1,
                            0,
                            bgPixel->colorLowBits,
                            backgroundPalette,
                            false,
                            false
                        );
                        baseBlockColors[8] = sampleOverridePixel(
                            baseBlockColors[8],
                            backgroundFallbackColor,
                            backgroundOverride,
                            bgPixel->tileIndex,
                            bgPixel->offsetX,
                            bgPixel->offsetY,
                            0,
                            1,
                            bgPixel->colorLowBits,
                            backgroundPalette,
                            false,
                            false
                        );
                        baseBlockColors[9] = sampleOverridePixel(
                            baseBlockColors[9],
                            backgroundFallbackColor,
                            backgroundOverride,
                            bgPixel->tileIndex,
                            bgPixel->offsetX,
                            bgPixel->offsetY,
                            1,
                            1,
                            bgPixel->colorLowBits,
                            backgroundPalette,
                            false,
                            false
                        );
                        return;
                    }
                    for(int subY = subYStart; subY < subYEnd; ++subY) {
                        uint32_t* blockRow = baseBlockColors.data() + static_cast<size_t>(subY) * 8u;
                        for(int subX = 0; subX < blockWidth; ++subX) {
                            blockRow[static_cast<size_t>(subX)] = sampleOverridePixel(
                                blockRow[static_cast<size_t>(subX)],
                                backgroundFallbackColor,
                                backgroundOverride,
                                bgPixel->tileIndex,
                                bgPixel->offsetX,
                                bgPixel->offsetY,
                                subX,
                                subY,
                                bgPixel->colorLowBits,
                                backgroundPalette,
                                false,
                                false
                            );
                        }
                    }
                } else if(applyBackgroundFallback) {
                    fillBlockColors(backgroundFallbackColor);
                }
            };

            fillBlockColors(canPrecomposeBeforeBg ? precomposedBeforeBgColor : initialColor);
            if(!canPrecomposeBeforeBg) {
                applyResolvedLayersToBlock(resolvedLowPriorityBackgrounds, resolvedLowPriorityBackgroundCount);
            }

            struct ResolvedSpriteCandidate {
                const PPU::DebugModSpriteCandidate* candidate = nullptr;
                const PreparedOverride* spriteOverride = nullptr;
                uint32_t fixedFallbackColor = 0;
                std::array<uint8_t, 3> spritePalette = {};
                std::array<uint32_t, 64> overrideSampleColors = {};
                int candidateIndex = std::numeric_limits<int>::max();
                uint64_t overrideCoverageMask = 0;
                bool fallbackUsesCurrentColor = true;
                bool overrideSamplesComputed = false;
                bool participates = false;
            };

            std::array<ResolvedSpriteCandidate, 8> resolvedBehindSpriteCandidates = {};
            size_t resolvedBehindSpriteCandidateCount = 0;
            std::array<ResolvedSpriteCandidate, 8> resolvedFrontSpriteCandidates = {};
            size_t resolvedFrontSpriteCandidateCount = 0;
            const auto spriteFallbackColorFor = [&](const PPU::DebugModSpriteCandidate& candidate) {
                const std::array<uint8_t, 3> spritePalette = { candidate.palette[0], candidate.palette[1], candidate.palette[2] };
                const int spritePaletteIndex = std::clamp(static_cast<int>(candidate.colorLowBits), 1, 3) - 1;
                if(spritePaletteIndex >= 0 && spritePaletteIndex < static_cast<int>(spritePalette.size())) {
                    return snapshot.paletteColors[spritePalette[static_cast<size_t>(spritePaletteIndex)] & 0x3F];
                }
                return baseColor;
            };
            const auto allowOriginalSpriteFallbackFor = [&](const PPU::DebugModSpriteCandidate& candidate) {
                return candidate.colorLowBits != 0;
            };
            const auto makeSpriteOverrideRowCacheKey = [&](const PPU::DebugModSpriteCandidate& candidate,
                                                           int spriteFullTileIndex,
                                                           const std::array<uint8_t, 3>& spritePalette) {
                uint64_t key = static_cast<uint64_t>(static_cast<uint32_t>(spriteFullTileIndex));
                key ^= static_cast<uint64_t>((nesX - static_cast<int>(candidate.offsetX)) & 0x01FF) << 16u;
                key ^= static_cast<uint64_t>((nesY - static_cast<int>(candidate.offsetY)) & 0x01FF) << 25u;
                key ^= static_cast<uint64_t>(spritePalette[0] & 0x3F) << 34u;
                key ^= static_cast<uint64_t>(spritePalette[1] & 0x3F) << 40u;
                key ^= static_cast<uint64_t>(spritePalette[2] & 0x3F) << 46u;
                key ^= static_cast<uint64_t>(candidate.horizontalMirror ? 1u : 0u) << 52u;
                key ^= static_cast<uint64_t>(candidate.verticalMirror ? 1u : 0u) << 53u;
                key ^= static_cast<uint64_t>(candidate.behindBackground ? 1u : 0u) << 54u;
                key ^= static_cast<uint64_t>(candidate.tileHash) * 0x9E3779B185EBCA87ULL;
                return key;
            };

            {
                MODMANAGER_PROFILE_SCOPE(ComposeChrFrameSpriteResolve);
                if(spritePixel != nullptr && spritePixel->count > 0) {
                    for(int i = static_cast<int>(spritePixel->count) - 1; i >= 0; --i) {
                        const PPU::DebugModSpriteCandidate& candidate = spritePixel->candidates[static_cast<size_t>(i)];
                        if(!candidate.valid) {
                            continue;
                        }

                        ResolvedSpriteCandidate resolvedCandidate;
                        resolvedCandidate.candidate = &candidate;
                        resolvedCandidate.spritePalette = { candidate.palette[0], candidate.palette[1], candidate.palette[2] };
                        resolvedCandidate.candidateIndex = i;
                        const bool allowOriginalSpriteFallback = allowOriginalSpriteFallbackFor(candidate);
                        resolvedCandidate.fallbackUsesCurrentColor = !allowOriginalSpriteFallback;
                        if(allowOriginalSpriteFallback) {
                            resolvedCandidate.fixedFallbackColor = spriteFallbackColorFor(candidate);
                        }

                        const int spriteFullTileIndex = candidate.tileIndex != 0xFFFF ? static_cast<int>(candidate.tileIndex) : -1;
                        if(spriteFullTileIndex >= 0) {
                            const ConditionContext context = { nesX, nesY, bgPixel, &candidate };
                            const uint64_t spriteOverrideRowCacheKey =
                                makeSpriteOverrideRowCacheKey(candidate, spriteFullTileIndex, resolvedCandidate.spritePalette);
                            if(const auto it = spriteOverrideRowCache.find(spriteOverrideRowCacheKey); it != spriteOverrideRowCache.end()) {
                                resolvedCandidate.spriteOverride = it->second;
                            } else {
                                bool cacheableByOrigin = false;
                                resolvedCandidate.spriteOverride =
                                    findOverride(
                                        ChrOverride::Target::Sprite,
                                        spriteFullTileIndex & 0xFF,
                                        spriteFullTileIndex,
                                        spriteFullTileIndex / 256,
                                        resolvedCandidate.spritePalette,
                                        candidate.horizontalMirror,
                                        candidate.verticalMirror,
                                        candidate.behindBackground,
                                        context,
                                        &cacheableByOrigin);
                                if(cacheableByOrigin) {
                                    spriteOverrideRowCache.emplace(spriteOverrideRowCacheKey, resolvedCandidate.spriteOverride);
                                }
                            }
                        }
                        resolvedCandidate.participates = resolvedCandidate.spriteOverride != nullptr || candidate.colorLowBits != 0;
                        if(!resolvedCandidate.participates) {
                            continue;
                        }

                        if(candidate.behindBackground) {
                            if(resolvedBehindSpriteCandidateCount < resolvedBehindSpriteCandidates.size()) {
                                resolvedBehindSpriteCandidates[resolvedBehindSpriteCandidateCount++] = resolvedCandidate;
                            }
                        } else {
                            if(resolvedFrontSpriteCandidateCount < resolvedFrontSpriteCandidates.size()) {
                                resolvedFrontSpriteCandidates[resolvedFrontSpriteCandidateCount++] = resolvedCandidate;
                            }
                        }
                    }
                }
            }
            const bool applyBehindSprites = resolvedBehindSpriteCandidateCount > 0;
            const bool applyFrontSprites = resolvedFrontSpriteCandidateCount > 0;
            auto overrideSampleIndexFor = [](int subX, int subY) {
                return static_cast<size_t>(subY) * 8u + static_cast<size_t>(subX);
            };
            auto ensureResolvedSpriteCandidateSamples = [&](ResolvedSpriteCandidate& resolvedCandidate) {
                if(resolvedCandidate.overrideSamplesComputed) {
                    return;
                }
                resolvedCandidate.overrideSamplesComputed = true;
                resolvedCandidate.overrideCoverageMask = 0;
                if(!resolvedCandidate.participates || resolvedCandidate.candidate == nullptr || resolvedCandidate.spriteOverride == nullptr) {
                    return;
                }

                const PPU::DebugModSpriteCandidate& candidate = *resolvedCandidate.candidate;
                const PreparedOverride* prepared = resolvedCandidate.spriteOverride;
                const ChrOverride* override = prepared->override;
                const DecodedImage* image = prepared->image;
                if(override == nullptr || image == nullptr) {
                    return;
                }

                const int localOffsetX = static_cast<int>(candidate.offsetX) & 0x07;
                const int localOffsetY = static_cast<int>(candidate.offsetY) & 0x07;
                const int sourceScale = prepared->sourceScale;
                const int tilePixelSize = 8 * sourceScale;
                int sourceBaseX = override->hasSourcePosition() ? override->sourceX : 0;
                int sourceBaseY = override->hasSourcePosition() ? override->sourceY : 0;
                if(!override->hasSourcePosition()) {
                    const bool atlasImage =
                        override->wholeChr() ||
                        override->sourceTileOffset > 0 ||
                        (image->width >= override->columns * tilePixelSize && image->height > tilePixelSize);
                    if(atlasImage) {
                        int sourceColumn = 0;
                        int sourceRow = 0;
                        if(override->wholeChr()) {
                            const int spriteTileIndex = candidate.tileIndex != 0xFFFF ? static_cast<int>(candidate.tileIndex) : -1;
                            if(spriteTileIndex < 0) {
                                return;
                            }
                            const int sourceTile = spriteTileIndex + override->sourceTileOffset;
                            if(prepared->wholeChrLayout == ChrOverride::SourceLayout::PatternTables) {
                                const int table = sourceTile / 256;
                                const int tileInTable = sourceTile & 0xFF;
                                sourceColumn = (table * 16) + (tileInTable & 0x0F);
                                sourceRow = tileInTable >> 4;
                            } else {
                                sourceColumn = sourceTile % override->columns;
                                sourceRow = sourceTile / override->columns;
                            }
                        } else if(override->sourceLayout == ChrOverride::SourceLayout::PatternTables) {
                            const int sourceTile = override->sourceTileOffset;
                            const int table = sourceTile / 256;
                            const int tileInTable = sourceTile & 0xFF;
                            sourceColumn = (table * 16) + (tileInTable & 0x0F);
                            sourceRow = tileInTable >> 4;
                        } else {
                            const int sourceTile = override->sourceTileOffset;
                            sourceColumn = sourceTile % override->columns;
                            sourceRow = sourceTile / override->columns;
                        }
                        sourceBaseX = sourceColumn * tilePixelSize;
                        sourceBaseY = sourceRow * tilePixelSize;
                    }
                }

                for(int subY = subYStart; subY < subYEnd; ++subY) {
                    const int sourceSubY = std::clamp((subY * sourceScale) / scale, 0, sourceScale - 1);
                    const int tileSubY =
                        candidate.verticalMirror
                            ? (7 - localOffsetY) * sourceScale + (sourceScale - 1 - sourceSubY)
                            : localOffsetY * sourceScale + sourceSubY;
                    const int srcY = sourceBaseY + tileSubY;
                    if(srcY < 0 || srcY >= image->height) {
                        continue;
                    }
                    for(int subX = 0; subX < blockWidth; ++subX) {
                        const int sourceSubX = std::clamp((subX * sourceScale) / scale, 0, sourceScale - 1);
                        const int tileSubX =
                            candidate.horizontalMirror
                                ? (7 - localOffsetX) * sourceScale + (sourceScale - 1 - sourceSubX)
                                : localOffsetX * sourceScale + sourceSubX;
                        const int srcX = sourceBaseX + tileSubX;
                        if(srcX < 0 || srcX >= image->width) {
                            continue;
                        }

                        const size_t sourcePixelIndex = static_cast<size_t>(srcY) * static_cast<size_t>(image->width) + static_cast<size_t>(srcX);
                        const uint32_t sourcePixel = image->rgba[sourcePixelIndex];
                        const uint8_t sourceAlpha = static_cast<uint8_t>((sourcePixel >> 24u) & 0xFFu);
                        if(sourceAlpha == 0) {
                            continue;
                        }

                        uint32_t sampleColor = 0;
                        if(override->ignorePalette) {
                            sampleColor = (sourcePixel & 0x00FFFFFFu) | (static_cast<uint32_t>(sourceAlpha) << 24u);
                        } else {
                            if(!image->indexedFourColor || image->indexedPixels.size() != image->rgba.size()) {
                                continue;
                            }
                            const uint8_t sourcePaletteIndex = image->indexedPixels[sourcePixelIndex];
                            if(sourcePaletteIndex == 0) {
                                continue;
                            }
                            const int paletteIndex = static_cast<int>(sourcePaletteIndex) - 1;
                            if(paletteIndex < 0 || paletteIndex >= static_cast<int>(resolvedCandidate.spritePalette.size())) {
                                continue;
                            }
                            sampleColor =
                                (snapshot.paletteColors[resolvedCandidate.spritePalette[static_cast<size_t>(paletteIndex)] & 0x3F] & 0x00FFFFFFu) |
                                (static_cast<uint32_t>(sourceAlpha) << 24u);
                        }

                        const size_t sampleIndex = overrideSampleIndexFor(subX, subY);
                        resolvedCandidate.overrideSampleColors[sampleIndex] = sampleColor;
                        resolvedCandidate.overrideCoverageMask |= (uint64_t{1} << sampleIndex);
                    }
                }
            };

            auto blockedByHigherPriorityBehindSprite = [&](int frontCandidateIndex, int subX, int subY) {
                for(size_t behindIndex = 0; behindIndex < resolvedBehindSpriteCandidateCount; ++behindIndex) {
                    ResolvedSpriteCandidate& behindCandidate = resolvedBehindSpriteCandidates[behindIndex];
                    if(behindCandidate.candidateIndex >= frontCandidateIndex) {
                        continue;
                    }
                    if(behindCandidate.spriteOverride == nullptr) {
                        if(behindCandidate.candidate != nullptr && allowOriginalSpriteFallbackFor(*behindCandidate.candidate)) {
                            return true;
                        }
                        continue;
                    }
                    ensureResolvedSpriteCandidateSamples(behindCandidate);
                    if((behindCandidate.overrideCoverageMask & (uint64_t{1} << overrideSampleIndexFor(subX, subY))) != 0) {
                        return true;
                    }
                }
                return false;
            };

            auto applySpriteLayersToBlock = [&](const std::array<ResolvedSpriteCandidate, 8>& candidates,
                                               size_t count,
                                               bool blockByBehindSprites,
                                               bool requireTransparentOriginalBackground) {
                if(requireTransparentOriginalBackground &&
                   bgPixel != nullptr &&
                   bgPixel->valid &&
                   bgPixel->colorLowBits != 0) {
                    return;
                }
                const bool canUseScale2SpriteBlockPath =
                    scale == 2 &&
                    blockWidth == 2 &&
                    subYStart == 0 &&
                    subYEnd == 2 &&
                    !blockByBehindSprites &&
                    !requireTransparentOriginalBackground;
                if(canUseScale2SpriteBlockPath) {
                    uint32_t block00 = baseBlockColors[0];
                    uint32_t block01 = baseBlockColors[1];
                    uint32_t block10 = baseBlockColors[8];
                    uint32_t block11 = baseBlockColors[9];
                    for(size_t i = 0; i < count; ++i) {
                        const ResolvedSpriteCandidate& resolvedCandidate = candidates[i];
                        if(!resolvedCandidate.participates) {
                            continue;
                        }
                        const PPU::DebugModSpriteCandidate& candidate = *resolvedCandidate.candidate;
                        uint32_t spriteFallback00 = resolvedCandidate.fallbackUsesCurrentColor ? block00 : resolvedCandidate.fixedFallbackColor;
                        uint32_t spriteFallback01 = resolvedCandidate.fallbackUsesCurrentColor ? block01 : resolvedCandidate.fixedFallbackColor;
                        uint32_t spriteFallback10 = resolvedCandidate.fallbackUsesCurrentColor ? block10 : resolvedCandidate.fixedFallbackColor;
                        uint32_t spriteFallback11 = resolvedCandidate.fallbackUsesCurrentColor ? block11 : resolvedCandidate.fixedFallbackColor;
                        if(resolvedCandidate.spriteOverride != nullptr) {
                            ensureResolvedSpriteCandidateSamples(const_cast<ResolvedSpriteCandidate&>(resolvedCandidate));
                            if((resolvedCandidate.overrideCoverageMask & (uint64_t{1} << overrideSampleIndexFor(0, 0))) != 0) {
                                block00 = blendPixel(block00, resolvedCandidate.overrideSampleColors[overrideSampleIndexFor(0, 0)], 255);
                            }
                            if((resolvedCandidate.overrideCoverageMask & (uint64_t{1} << overrideSampleIndexFor(1, 0))) != 0) {
                                block01 = blendPixel(block01, resolvedCandidate.overrideSampleColors[overrideSampleIndexFor(1, 0)], 255);
                            }
                            if((resolvedCandidate.overrideCoverageMask & (uint64_t{1} << overrideSampleIndexFor(0, 1))) != 0) {
                                block10 = blendPixel(block10, resolvedCandidate.overrideSampleColors[overrideSampleIndexFor(0, 1)], 255);
                            }
                            if((resolvedCandidate.overrideCoverageMask & (uint64_t{1} << overrideSampleIndexFor(1, 1))) != 0) {
                                block11 = blendPixel(block11, resolvedCandidate.overrideSampleColors[overrideSampleIndexFor(1, 1)], 255);
                            }
                        } else if(allowOriginalSpriteFallbackFor(candidate)) {
                            block00 = spriteFallback00;
                            block01 = spriteFallback01;
                            block10 = spriteFallback10;
                            block11 = spriteFallback11;
                        }
                    }
                    baseBlockColors[0] = block00;
                    baseBlockColors[1] = block01;
                    baseBlockColors[8] = block10;
                    baseBlockColors[9] = block11;
                    return;
                }

                for(int subY = subYStart; subY < subYEnd; ++subY) {
                    uint32_t* blockRow = baseBlockColors.data() + static_cast<size_t>(subY) * 8u;
                    for(int subX = 0; subX < scale; ++subX) {
                        uint32_t color = blockRow[static_cast<size_t>(subX)];
                        for(size_t i = 0; i < count; ++i) {
                            const ResolvedSpriteCandidate& resolvedCandidate = candidates[i];
                            if(!resolvedCandidate.participates) {
                                continue;
                            }
                            const PPU::DebugModSpriteCandidate& candidate = *resolvedCandidate.candidate;
                            if(blockByBehindSprites &&
                               blockedByHigherPriorityBehindSprite(resolvedCandidate.candidateIndex, subX, subY)) {
                                continue;
                            }
                            const uint32_t spriteFallbackColor =
                                resolvedCandidate.fallbackUsesCurrentColor ? color : resolvedCandidate.fixedFallbackColor;
                            if(resolvedCandidate.spriteOverride != nullptr) {
                                ensureResolvedSpriteCandidateSamples(const_cast<ResolvedSpriteCandidate&>(resolvedCandidate));
                                const size_t sampleIndex = overrideSampleIndexFor(subX, subY);
                                if((resolvedCandidate.overrideCoverageMask & (uint64_t{1} << sampleIndex)) != 0) {
                                    color = blendPixel(color, resolvedCandidate.overrideSampleColors[sampleIndex], 255);
                                }
                            } else if(allowOriginalSpriteFallbackFor(candidate)) {
                                color = spriteFallbackColor;
                            }
                        }
                        blockRow[static_cast<size_t>(subX)] = color;
                    }
                }
            };

            {
                MODMANAGER_PROFILE_SCOPE(ComposeChrFrameSpriteLayers);
                if(!canPrecomposeBeforeBg) {
                    applyResolvedLayersToBlock(resolvedMidBeforeTileBackgrounds, resolvedMidBeforeTileBackgroundCount);
                }
                {
                    MODMANAGER_PROFILE_SCOPE(ComposeChrFrameBackgroundOverride);
                    applyBackgroundOverrideToBlock();
                }
                if(hasMidAfterBackgrounds) {
                    applyResolvedLayersToBlock(resolvedMidAfterTileBackgrounds, resolvedMidAfterTileBackgroundCount);
                }
                if(applyBehindSprites) {
                    applySpriteLayersToBlock(
                        resolvedBehindSpriteCandidates,
                        resolvedBehindSpriteCandidateCount,
                        false,
                        true);
                }
            }
            {
                MODMANAGER_PROFILE_SCOPE(ComposeChrFrameSpriteLayers);
                if(applyFrontSprites) {
                    applySpriteLayersToBlock(
                        resolvedFrontSpriteCandidates,
                        resolvedFrontSpriteCandidateCount,
                        applyBehindSprites,
                        false);
                }
            }

            if(!hasAnyValidSpriteCandidate) {
                MODMANAGER_PROFILE_SCOPE(ComposeChrFrameFinalWrite);
                if(hasHighPriorityBackgrounds) {
                    applyResolvedLayersToBlock(resolvedHighPriorityBackgrounds, resolvedHighPriorityBackgroundCount);
                }

                for(int subY = subYStart; subY < subYEnd; ++subY) {
                    std::memcpy(
                        dstRows[static_cast<size_t>(subY)],
                        baseBlockColors.data() + static_cast<size_t>(subY) * 8u,
                        static_cast<size_t>(blockWidth) * sizeof(uint32_t));
                }
                continue;
            }

            auto writeBlockRows = [&]() {
                for(int subY = subYStart; subY < subYEnd; ++subY) {
                    std::memcpy(
                        dstRows[static_cast<size_t>(subY)],
                        baseBlockColors.data() + static_cast<size_t>(subY) * 8u,
                        static_cast<size_t>(blockWidth) * sizeof(uint32_t));
                }
            };

            if(!hasHighPriorityBackgrounds) {
                MODMANAGER_PROFILE_SCOPE(ComposeChrFrameFinalWrite);
                writeBlockRows();
                continue;
            }

            MODMANAGER_PROFILE_SCOPE(ComposeChrFrameFinalWrite);
            applyResolvedLayersToBlock(resolvedHighPriorityBackgrounds, resolvedHighPriorityBackgroundCount);

            writeBlockRows();
        }
    }

}

uint32_t ModManager::blendPixel(uint32_t dst, uint32_t src, int alphaScale)
{
    const int srcA = static_cast<int>((src >> 24) & 0xFFu);
    const int blendA = (srcA * std::clamp(alphaScale, 0, 255)) / 255;
    if(blendA <= 0) return dst;
    if(blendA >= 255) return src;

    const int invA = 255 - blendA;
    const int srcR = static_cast<int>(src & 0xFFu);
    const int srcG = static_cast<int>((src >> 8) & 0xFFu);
    const int srcB = static_cast<int>((src >> 16) & 0xFFu);
    const int dstR = static_cast<int>(dst & 0xFFu);
    const int dstG = static_cast<int>((dst >> 8) & 0xFFu);
    const int dstB = static_cast<int>((dst >> 16) & 0xFFu);
    const uint32_t outR = static_cast<uint32_t>((srcR * blendA + dstR * invA) / 255);
    const uint32_t outG = static_cast<uint32_t>((srcG * blendA + dstG * invA) / 255);
    const uint32_t outB = static_cast<uint32_t>((srcB * blendA + dstB * invA) / 255);
    return 0xFF000000u | outR | (outG << 8) | (outB << 16);
}

uint32_t ModManager::hashChrTile(PPU& ppu, int tileIndex)
{
    const int baseAddress = (tileIndex & 0x01FF) * 16;
    uint8_t bytes[16] = {};
    for(int offset = 0; offset < 16; ++offset) {
        bytes[offset] = ppu.debugPeekPpuMemory(static_cast<uint16_t>(baseAddress + offset));
    }

    uint32_t hash = 0;
    for(size_t i = 0; i < std::size(bytes); i += sizeof(uint32_t)) {
        uint32_t chunk = 0;
        std::memcpy(&chunk, bytes + i, sizeof(uint32_t));
        hash += chunk;
        hash = (hash << 2) | (hash >> 30);
    }
    return hash;
}
