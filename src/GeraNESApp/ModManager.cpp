#include "GeraNESApp/ModManager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>

#include "GeraNES/RomFile.h"
#include "GeraNESApp/AppSettings.h"
#include "logger/logger.h"
#include "stb_image.h"
#include "zip/zip.h"

namespace {
using mz_ulong = unsigned long;
extern "C" int mz_uncompress(unsigned char* pDest, mz_ulong* pDest_len, const unsigned char* pSource, mz_ulong source_len);
constexpr int MZ_OK = 0;
constexpr uint8_t kPngSignature[8] = { 0x89u, 'P', 'N', 'G', '\r', '\n', 0x1Au, '\n' };


bool evaluateMemoryCondition(const ModManager::MemoryCondition& condition, uint32_t actual)
{
    uint32_t expected = condition.value;
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
    int paletteColorCount = 0;
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
    decoded.paletteColorCount = static_cast<int>(paletteColorCount);
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
    try {
        size_t index = 0;
        uint32_t value = static_cast<uint32_t>(std::stoul(text, &index, 16));
        return index == text.size() ? value : fallback;
    } catch(...) {
        return fallback;
    }
}

uint32_t parseDecOrHexValue(const std::string& text, uint32_t fallback = 0)
{
    if(text.empty()) return fallback;
    const bool looksHex = text.find_first_of("ABCDEFabcdef") != std::string::npos;
    try {
        size_t index = 0;
        const uint32_t value = static_cast<uint32_t>(std::stoul(text, &index, looksHex ? 16 : 10));
        return index == text.size() ? value : fallback;
    } catch(...) {
        return fallback;
    }
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
    if(text.empty()) {
        return std::nullopt;
    }

    try {
        size_t index = 0;
        const int value = std::stoi(text, &index, 10);
        if(index != text.size()) {
            return std::nullopt;
        }
        return value;
    } catch(...) {
        return std::nullopt;
    }
}

std::optional<int> parseSignedDecimalIntStrict(const std::string& text)
{
    if(text.empty()) {
        return std::nullopt;
    }

    try {
        size_t index = 0;
        const int value = std::stoi(text, &index, 10);
        if(index != text.size()) {
            return std::nullopt;
        }
        return value;
    } catch(...) {
        return std::nullopt;
    }
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
    m_active = false;
    m_scriptLoaded = false;
    m_resolutionMultiplier = 1;
    m_disableOriginalTiles = false;
    m_disableContours = false;
    m_customPalette.reset();
    m_chrOverrides.clear();
    m_backgroundReplacements.clear();
    m_supportedRomHashes.clear();
    m_patchAssetPath.clear();
    m_patchExpectedRomHash.clear();
    m_hdAudioConfig = {};
    m_hdAudioRuntime->clearConfig();
    m_frameConditionState = {};
    m_frameConditionPlan = {};
    m_lastFrameConditionUpdate = UINT32_MAX;
    m_imageCache.clear();
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

    std::error_code ec;
    const bool exists = std::filesystem::exists(modSourcePath, ec);
    if(ec || !exists) {
        error = "Mod source does not exist.";
        return false;
    }

    const bool isDirectory = std::filesystem::is_directory(modSourcePath, ec);
    if(ec) {
        error = "Failed to inspect mod source.";
        return false;
    }
    const bool isFile = std::filesystem::is_regular_file(modSourcePath, ec);
    if(ec) {
        error = "Failed to inspect mod source.";
        return false;
    }
    if(!isDirectory && !isFile) {
        error = "Mod source must be a folder or zip file.";
        return false;
    }
    if(isFile) {
        std::string extension = toLower(modSourcePath.extension().string());
        if(extension != ".zip" && extension != ".mod") {
            error = "Mod file must be a .zip or .mod archive.";
            return false;
        }
    }

    m_modPath = modSourcePath;
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
    if(m_hdAudioRuntime) {
        m_hdAudioRuntime->resetRuntime();
    }
}

void ModManager::onStateLoaded(uint32_t frameCount)
{
    m_lastFrameConditionUpdate = UINT32_MAX;
    m_frameConditionState = {};
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
    if(!m_active || !emu.valid()) {
        snapshot = {};
        framebuffer.clear();
        return false;
    }

    updateFrameConditionsForFrame(emu);

    PPU& ppu = emu.getConsole().ppu();
    snapshot = {};
    for(int y = 0; y < PPU::SCREEN_HEIGHT; ++y) {
        snapshot.scrollXByLine[static_cast<size_t>(y)] = ppu.debugPresentedScanlineScrollX(y);
        snapshot.scrollYByLine[static_cast<size_t>(y)] = ppu.debugPresentedScanlineScrollY(y);
    }
    snapshot.scrollX = snapshot.scrollXByLine[0];
    snapshot.scrollY = snapshot.scrollYByLine[0];
    snapshot.universalBgColor = static_cast<uint8_t>(ppu.debugPeekPpuMemory(0x3F00) & 0x3F);
    snapshot.backgroundPixelsView = ppu.debugPresentedBackgroundPixelsData();
    snapshot.backgroundPixelsViewCount = ppu.debugPresentedBackgroundPixelsCount();
    snapshot.spritePixelsView = ppu.debugPresentedSpritePixelsData();
    snapshot.spritePixelsViewCount = ppu.debugPresentedSpritePixelsCount();
    snapshot.frameConditionStateView = &m_frameConditionState;
    for(size_t i = 0; i < snapshot.paletteColors.size(); ++i) {
        snapshot.paletteColors[i] = ppu.NESToRGBAColor(static_cast<uint8_t>(i));
    }
    if(m_renderComposeCacheDirty || !m_renderComposeCache.valid || m_renderComposeCache.scale != std::max(1, m_resolutionMultiplier)) {
        rebuildRenderComposeCache();
    }
    const bool needsTileHashes = captureDebugSnapshot || m_renderComposeCache.needsTileHashes;
    if(needsTileHashes) {
        for(size_t i = 0; i < snapshot.tileHashes.size(); ++i) {
            snapshot.tileHashes[i] = ppu.debugHashChrTile(static_cast<int>(i));
        }
    }

    const int scale = std::clamp(m_resolutionMultiplier, 1, 8);
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
        ppu.debugCopyPresentedBackgroundPixels(snapshot.backgroundPixels);
        ppu.debugCopyPresentedSpritePixels(snapshot.spritePixels);
        snapshot.frameConditionState = m_frameConditionState;
    }
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
        Logger::instance().log(request.message, Logger::Type::ERROR);
        return request;
    }

    RomFile baseRom;
    if(!baseRom.open(romPath.string()) || !baseRom.error().empty()) {
        request.message = "Mod selected, but base ROM could not be read for Mesen patch.";
        Logger::instance().log(request.message, Logger::Type::ERROR);
        return request;
    }

    if(!m_patchExpectedRomHash.empty()) {
        const std::string actualHash = toLower(sha1Hex(baseRom.dataBytes()));
        if(actualHash != m_patchExpectedRomHash) {
            request.message = "Mod selected, but ROM hash does not match hires.txt patch.";
            Logger::instance().log(request.message, Logger::Type::ERROR);
            return request;
        }
    }

    std::string patchError;
    auto patchedRom = applyIpsPatch(baseRom.dataBytes(), *ipsData, patchError);
    if(!patchedRom.has_value()) {
        request.message = "Mod selected, but hires.txt patch failed: " + patchError;
        Logger::instance().log(request.message, Logger::Type::ERROR);
        return request;
    }

    const std::filesystem::path cacheDir = AppSettings::storageDirectory() / "mod-cache";
    const std::filesystem::path patchedPath = cacheDir / (safeCacheStem(romPath) + ".modded.nes");
    std::string writeError;
    if(!writeBinaryFile(patchedPath, *patchedRom, writeError)) {
        request.message = "Mod found, but patched ROM could not be cached: " + writeError;
        Logger::instance().log(request.message, Logger::Type::ERROR);
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
    m_customPalette.reset();
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
        if(!override.enabled || override.assetPath.empty()) {
            continue;
        }
        if(override.ignorePalette) {
            if(decodedImage(override.assetPath) == nullptr) {
                Logger::instance().log(
                    "Failed to load CHR image asset: " + override.assetPath,
                    Logger::Type::WARNING);
                override.enabled = false;
            }
            continue;
        }
        const std::string normalizedPath = normalizeZipPath(override.assetPath);
        auto it = chrAssetValidity.find(normalizedPath);
        if(it == chrAssetValidity.end()) {
            const DecodedImage* image = decodedImage(normalizedPath);
            const bool valid = image != nullptr && image->indexedPng && image->indexedFourColor;
            if(!valid) {
                Logger::instance().log(
                    "CHR sheet must be an indexed PNG with exactly 4 colors: " + normalizedPath,
                    Logger::Type::WARNING);
            }
            it = chrAssetValidity.emplace(normalizedPath, valid).first;
        }
        if(!it->second) {
            override.enabled = false;
        }
    }

    rebuildFrameConditionPlan();
    preloadStartupAssets();
    m_scriptLoaded = true;
    Logger::instance().log("Mesen hires.txt loaded.", Logger::Type::INFO);
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
        if(!override.enabled || override.assetPath.empty()) {
            continue;
        }

        const bool hasOnlyGlobalConditions = std::none_of(
            override.conditions.begin(),
            override.conditions.end(),
            [](const MemoryCondition& condition) {
                return condition.kind == MemoryCondition::Kind::TileAtPosition ||
                       condition.kind == MemoryCondition::Kind::TileNearby ||
                       condition.kind == MemoryCondition::Kind::SpriteAtPosition ||
                       condition.kind == MemoryCondition::Kind::SpriteNearby;
            });

        if(hasOnlyGlobalConditions) {
            addImageAsset(override.assetPath);
        }
    }

    for(const BackgroundReplacement& replacement : m_backgroundReplacements) {
        if(!replacement.enabled || replacement.assetPath.empty()) {
            continue;
        }

        const bool alwaysOn = std::none_of(
            replacement.conditions.begin(),
            replacement.conditions.end(),
            [](const MemoryCondition& condition) {
                return condition.kind == MemoryCondition::Kind::MemoryCheck ||
                       condition.kind == MemoryCondition::Kind::FrameRange ||
                       condition.kind == MemoryCondition::Kind::TileAtPosition ||
                       condition.kind == MemoryCondition::Kind::TileNearby ||
                       condition.kind == MemoryCondition::Kind::SpriteAtPosition ||
                       condition.kind == MemoryCondition::Kind::SpriteNearby;
            });

        if(alwaysOn) {
            addImageAsset(replacement.assetPath);
        }
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

    if(!m_hdAudioConfig.bgmFilesById.empty()) {
        auto firstBgm = std::min_element(
            m_hdAudioConfig.bgmFilesById.begin(),
            m_hdAudioConfig.bgmFilesById.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        if(firstBgm != m_hdAudioConfig.bgmFilesById.end() && !firstBgm->second.assetPath.empty()) {
            audioAssets.push_back(normalizeZipPath(firstBgm->second.assetPath));
        }
    }

    std::vector<std::pair<int, std::string>> sortedSfx;
    sortedSfx.reserve(m_hdAudioConfig.sfxFilesById.size());
    for(const auto& [trackId, assetPath] : m_hdAudioConfig.sfxFilesById) {
        sortedSfx.emplace_back(trackId, normalizeZipPath(assetPath));
    }
    std::sort(sortedSfx.begin(), sortedSfx.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    constexpr size_t MaxStartupSfxPreload = 8;
    for(size_t i = 0; i < sortedSfx.size() && i < MaxStartupSfxPreload; ++i) {
        if(!sortedSfx[i].second.empty()) {
            audioAssets.push_back(sortedSfx[i].second);
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
        Logger::instance().log("Selected mod does not contain hires.txt.", Logger::Type::ERROR);
        return false;
    }

    const std::string text(reinterpret_cast<const char*>(hiresData->data()), hiresData->size());
    std::vector<std::string> images;
    std::unordered_map<std::string, std::vector<MemoryCondition>> namedConditions;
    int nextPriority = 1000000;
    int lineNumber = 0;
    m_disableOriginalTiles = false;
    m_disableContours = false;
    m_supportedRomHashes.clear();
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
                Logger::instance().log("Invalid Mesen condition prefix on line " + std::to_string(lineNumber), Logger::Type::ERROR);
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
                } else if(option == "disablecontours") {
                    m_disableContours = true;
                } else if(option == "alternateregisterrange") {
                    m_hdAudioConfig.alternateRegisterRange = true;
                }
            }
            continue;
        }
        if(line.rfind("<overscan>", 0) == 0) {
            continue;
        }
        if(line.rfind("<supportedrom>", 0) == 0) {
            const std::string hash = toLower(trimMesenToken(line.substr(14)));
            if(!hash.empty()) {
                m_supportedRomHashes.push_back(hash);
            }
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
            try {
                m_resolutionMultiplier = std::clamp(std::stoi(trimMesenToken(line.substr(7))), 1, 8);
            } catch(...) {
                Logger::instance().log("Invalid Mesen <scale> on line " + std::to_string(lineNumber), Logger::Type::ERROR);
            }
            continue;
        }
        if(line.rfind("<img>", 0) == 0) {
            const std::string imagePath = normalizeZipPath(trimMesenToken(line.substr(5)));
            if(!sourceHasEntry(imagePath)) {
                Logger::instance().log("Mesen image not found: " + imagePath, Logger::Type::WARNING);
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
                    Logger::instance().log("Mesen BGM not found: " + bgmTrack.assetPath, Logger::Type::WARNING);
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
                    Logger::instance().log("Mesen SFX not found: " + assetPath, Logger::Type::WARNING);
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
            if((normalizedType == "memorycheckconstant" || normalizedType == "ppumemorycheckconstant") && tokens.size() >= 5) {
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
                Logger::instance().log("Invalid Mesen <tile> on line " + std::to_string(lineNumber), Logger::Type::ERROR);
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
                int imageIndex = -1;
                try {
                    imageIndex = std::stoi(tokens[0]);
                } catch(...) {
                    imageIndex = -1;
                }

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
                Logger::instance().log("Mesen <tile> references invalid image index on line " + std::to_string(lineNumber), Logger::Type::ERROR);
                continue;
            }
            m_chrOverrides.push_back(std::move(override));
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
        Logger::instance().log("Mesen hires.txt did not define any supported graphics replacements.", Logger::Type::ERROR);
        return false;
    }
    if(m_hdAudioRuntime) {
        m_hdAudioRuntime->setConfig(m_hdAudioConfig);
    }
    return true;
}

void ModManager::onFrame(GeraNESEmu& emu)
{
    if(!m_active || !m_scriptLoaded) return;
    const bool needsGraphicsCapture =
        !emu.isRewinding() &&
        (!m_chrOverrides.empty() || !m_backgroundReplacements.empty());
    emu.getConsole().ppu().debugSetModRenderCaptureEnabled(needsGraphicsCapture);
    if(m_customPalette.has_value()) {
        emu.getConsole().ppu().setColorPalette(*m_customPalette);
    }
    updateFrameConditionsForFrame(emu);
}

void ModManager::updateFrameConditionsForFrame(GeraNESEmu& emu)
{
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

    FrameConditionState frameConditionState;
    frameConditionState.frameCount = frameCount;
    frameConditionState.memoryValues.resize(m_frameConditionPlan.uniqueMemoryReads.size());

    auto memoryConditionMatchesCached = [&](const MemoryCondition& condition) {
        const uint32_t actual =
            condition.readIndex < frameConditionState.memoryValues.size()
                ? frameConditionState.memoryValues[condition.readIndex]
                : 0u;
        return evaluateMemoryCondition(condition, actual);
    };

    auto frameRangeConditionMatches = [&](const MemoryCondition& condition) {
        const uint32_t range = std::max(1u, condition.value);
        const bool match = (frameConditionState.frameCount % range) >= condition.address;
        return condition.inverted ? !match : match;
    };

    auto globalConditionsMatch = [&](const FrameConditionPlan::RuleConditions& conditions) {
        for(const MemoryCondition& condition : conditions.memoryConditions) {
            if(!memoryConditionMatchesCached(condition)) {
                return false;
            }
        }
        for(const MemoryCondition& condition : conditions.frameRangeConditions) {
            if(!frameRangeConditionMatches(condition)) {
                return false;
            }
        }
        return true;
    };

    for(const FrameConditionPlan::CachedMemoryRead& cachedRead : m_frameConditionPlan.uniqueMemoryReads) {
        if(cachedRead.readIndex < frameConditionState.memoryValues.size()) {
            frameConditionState.memoryValues[cachedRead.readIndex] = readMemoryValue(cachedRead.condition, emu);
        }
    }

    bool renderComposeCacheChanged = false;
    const size_t chrCount = std::min(m_chrOverrides.size(), m_frameConditionPlan.chrOverrideGlobalConditions.size());
    for(size_t i = 0; i < chrCount; ++i) {
        const bool enabled = globalConditionsMatch(m_frameConditionPlan.chrOverrideGlobalConditions[i]);
        if(m_chrOverrides[i].enabled != enabled) {
            renderComposeCacheChanged = true;
            m_chrOverrides[i].enabled = enabled;
        }
    }
    for(size_t i = chrCount; i < m_chrOverrides.size(); ++i) {
        if(!m_chrOverrides[i].enabled) {
            renderComposeCacheChanged = true;
            m_chrOverrides[i].enabled = true;
        }
    }

    const size_t bgCount = std::min(m_backgroundReplacements.size(), m_frameConditionPlan.backgroundGlobalConditions.size());
    for(size_t i = 0; i < bgCount; ++i) {
        const bool enabled = globalConditionsMatch(m_frameConditionPlan.backgroundGlobalConditions[i]);
        if(m_backgroundReplacements[i].enabled != enabled) {
            renderComposeCacheChanged = true;
            m_backgroundReplacements[i].enabled = enabled;
        }
    }
    for(size_t i = bgCount; i < m_backgroundReplacements.size(); ++i) {
        if(!m_backgroundReplacements[i].enabled) {
            renderComposeCacheChanged = true;
            m_backgroundReplacements[i].enabled = true;
        }
    }
    m_frameConditionState = std::move(frameConditionState);
    m_lastFrameConditionUpdate = frameCount;
    if(renderComposeCacheChanged) {
        m_renderComposeCacheDirty = true;
    }
}

void ModManager::rebuildFrameConditionPlan()
{
    m_frameConditionPlan = {};

    auto appendGlobalConditions = [&](const std::vector<MemoryCondition>& source, FrameConditionPlan::RuleConditions& globals) {
        for(const MemoryCondition& condition : source) {
            if(condition.kind == MemoryCondition::Kind::FrameRange) {
                globals.frameRangeConditions.push_back(condition);
                continue;
            }
            if(condition.kind != MemoryCondition::Kind::MemoryCheck) {
                continue;
            }
            MemoryCondition compiledCondition = condition;
            if(compiledCondition.memoryCacheKey == 0) {
                compiledCondition.memoryCacheKey = makeMemoryCacheKey(
                    compiledCondition.memorySource,
                    compiledCondition.address,
                    compiledCondition.word,
                    compiledCondition.scale);
            }
            const uint64_t key = compiledCondition.memoryCacheKey;
            const auto duplicateIt = std::find_if(
                m_frameConditionPlan.uniqueMemoryReads.begin(),
                m_frameConditionPlan.uniqueMemoryReads.end(),
                [key](const FrameConditionPlan::CachedMemoryRead& cachedRead) { return cachedRead.key == key; });
            if(duplicateIt == m_frameConditionPlan.uniqueMemoryReads.end()) {
                FrameConditionPlan::CachedMemoryRead cachedRead;
                cachedRead.key = key;
                cachedRead.readIndex = m_frameConditionPlan.uniqueMemoryReads.size();
                cachedRead.condition = compiledCondition;
                m_frameConditionPlan.uniqueMemoryReads.push_back(std::move(cachedRead));
                compiledCondition.readIndex = m_frameConditionPlan.uniqueMemoryReads.back().readIndex;
            } else {
                compiledCondition.readIndex = duplicateIt->readIndex;
            }
            globals.memoryConditions.push_back(std::move(compiledCondition));
        }
    };

    m_frameConditionPlan.chrOverrideGlobalConditions.resize(m_chrOverrides.size());
    for(size_t i = 0; i < m_chrOverrides.size(); ++i) {
        appendGlobalConditions(m_chrOverrides[i].conditions, m_frameConditionPlan.chrOverrideGlobalConditions[i]);
    }

    m_frameConditionPlan.backgroundGlobalConditions.resize(m_backgroundReplacements.size());
    for(size_t i = 0; i < m_backgroundReplacements.size(); ++i) {
        appendGlobalConditions(m_backgroundReplacements[i].conditions, m_frameConditionPlan.backgroundGlobalConditions[i]);
    }
}

void ModManager::rebuildRenderComposeCache()
{
    m_renderComposeCache = {};
    m_renderComposeCache.scale = std::max(1, m_resolutionMultiplier);
    m_renderComposeCache.needsTileHashes = false;

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

    m_renderComposeCache.activeOverrides.reserve(m_chrOverrides.size());
    for(const ChrOverride& override : m_chrOverrides) {
        if(override.enabled && !override.assetPath.empty()) {
            m_renderComposeCache.activeOverrides.push_back(&override);
        }
    }
    if(!m_renderComposeCache.activeOverrides.empty()) {
        std::stable_sort(m_renderComposeCache.activeOverrides.begin(), m_renderComposeCache.activeOverrides.end(), [](const ChrOverride* a, const ChrOverride* b) {
            return a->priority > b->priority;
        });
    }

    m_renderComposeCache.preparedOverrides.reserve(m_renderComposeCache.activeOverrides.size());
    for(const ChrOverride* override : m_renderComposeCache.activeOverrides) {
        RenderPreparedOverride prepared;
        prepared.override = override;
        prepared.sequence = m_renderComposeCache.preparedOverrides.size();
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
        if(override->hasChrHash) {
            m_renderComposeCache.needsTileHashes = true;
        }
        for(const MemoryCondition* condition : prepared.runtimeConditions) {
            if(condition != nullptr && condition->hasExpectedTile && condition->expectedTileByHash) {
                m_renderComposeCache.needsTileHashes = true;
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
        m_renderComposeCache.preparedOverrides.push_back(std::move(prepared));
        const RenderPreparedOverride* preparedPtr = &m_renderComposeCache.preparedOverrides.back();
        if(override->wholeChr()) {
            if(override->hasChrHash) {
                m_renderComposeCache.staticOverrideHashes.insert(override->chrHash);
                if(override->defaultTile) {
                    m_renderComposeCache.defaultOverrideHashes.insert(override->chrHash);
                }
                if(preparedPtr->hasDynamicConditions) {
                    m_renderComposeCache.dynamicOverridesByChrHash[override->chrHash].push_back(preparedPtr);
                } else {
                    m_renderComposeCache.overridesByChrHash[override->chrHash].push_back(preparedPtr);
                }
            } else if(preparedPtr->hasDynamicConditions) {
                if(override->defaultTile) {
                    m_renderComposeCache.hasWholeChrDefaultOverrides = true;
                }
                m_renderComposeCache.dynamicWholeChrOverrides.push_back(preparedPtr);
            } else {
                m_renderComposeCache.hasWholeChrOverrides = true;
                if(override->defaultTile) {
                    m_renderComposeCache.hasWholeChrDefaultOverrides = true;
                }
                m_renderComposeCache.wholeChrOverrides.push_back(preparedPtr);
            }
        } else if(override->absoluteTile) {
            if(override->tile >= 0 && override->tile < static_cast<int>(m_renderComposeCache.overridesByFullTile.size())) {
                m_renderComposeCache.hasStaticOverridesByFullTile[static_cast<size_t>(override->tile)] = true;
                if(override->defaultTile) {
                    m_renderComposeCache.hasDefaultOverridesByFullTile[static_cast<size_t>(override->tile)] = true;
                }
                if(preparedPtr->hasDynamicConditions) {
                    m_renderComposeCache.dynamicOverridesByFullTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                } else {
                    m_renderComposeCache.overridesByFullTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                }
            }
        } else if(override->tile >= 0) {
            if(override->tile < static_cast<int>(m_renderComposeCache.overridesByRelativeTile.size())) {
                m_renderComposeCache.hasStaticOverridesByRelativeTile[static_cast<size_t>(override->tile)] = true;
                if(override->defaultTile) {
                    m_renderComposeCache.hasDefaultOverridesByRelativeTile[static_cast<size_t>(override->tile)] = true;
                }
                if(preparedPtr->hasDynamicConditions) {
                    m_renderComposeCache.dynamicOverridesByRelativeTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                } else {
                    m_renderComposeCache.overridesByRelativeTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                }
            } else if(override->tile < static_cast<int>(m_renderComposeCache.overridesByFullTile.size())) {
                m_renderComposeCache.hasStaticOverridesByFullTile[static_cast<size_t>(override->tile)] = true;
                if(override->defaultTile) {
                    m_renderComposeCache.hasDefaultOverridesByFullTile[static_cast<size_t>(override->tile)] = true;
                }
                if(preparedPtr->hasDynamicConditions) {
                    m_renderComposeCache.dynamicOverridesByFullTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                } else {
                    m_renderComposeCache.overridesByFullTile[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                }
            }
        }
    }

    for(size_t i = 0; i < m_renderComposeCache.dynamicOverridesByFullTile.size(); ++i) {
        m_renderComposeCache.hasDynamicOverridesByFullTile[i] = !m_renderComposeCache.dynamicOverridesByFullTile[i].empty();
    }
    for(size_t i = 0; i < m_renderComposeCache.dynamicOverridesByRelativeTile.size(); ++i) {
        m_renderComposeCache.hasDynamicOverridesByRelativeTile[i] = !m_renderComposeCache.dynamicOverridesByRelativeTile[i].empty();
    }
    m_renderComposeCache.dynamicOverrideHashes.reserve(m_renderComposeCache.dynamicOverridesByChrHash.size());
    for(const auto& [hash, _] : m_renderComposeCache.dynamicOverridesByChrHash) {
        m_renderComposeCache.dynamicOverrideHashes.insert(hash);
    }

    m_renderComposeCache.preparedBackgrounds.reserve(m_backgroundReplacements.size());
    for(const BackgroundReplacement& replacement : m_backgroundReplacements) {
        if(!replacement.enabled || replacement.assetPath.empty()) {
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

    m_renderComposeCache.valid = true;
    m_renderComposeCacheDirty = false;
}

std::optional<std::vector<uint8_t>> ModManager::readAsset(const std::string& assetPath) const
{
    if(!m_active) return std::nullopt;
    return readSourceEntry(normalizeZipPath(assetPath));
}

bool ModManager::isFolderSource() const
{
    if(m_modPath.empty()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::is_directory(m_modPath, ec);
}

std::string ModManager::normalizeZipPath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    while(!path.empty() && path.front() == '/') {
        path.erase(path.begin());
    }
    return path;
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

std::optional<std::vector<uint8_t>> ModManager::readFileEntry(const std::filesystem::path& rootPath, const std::string& entryName)
{
    const auto resolvedPath = resolveFolderEntryPath(rootPath, entryName);
    if(!resolvedPath.has_value()) {
        return std::nullopt;
    }

    std::error_code ec;
    if(!std::filesystem::exists(*resolvedPath, ec) || !std::filesystem::is_regular_file(*resolvedPath, ec)) {
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
    if(isFolderSource()) {
        const auto resolvedPath = resolveFolderEntryPath(m_modPath, entryName);
        if(!resolvedPath.has_value()) {
            return false;
        }
        std::error_code ec;
        return std::filesystem::exists(*resolvedPath, ec) && std::filesystem::is_regular_file(*resolvedPath, ec);
    }
    return zipHasEntry(m_modPath, entryName);
}

std::optional<std::vector<uint8_t>> ModManager::readSourceEntry(const std::string& entryName) const
{
    if(m_modPath.empty()) {
        return std::nullopt;
    }
    if(isFolderSource()) {
        return readFileEntry(m_modPath, entryName);
    }
    return readZipEntry(m_modPath, entryName);
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

bool ModManager::zipHasEntry(const std::filesystem::path& zipPath, const std::string& entryName)
{
    zip_t* zip = zip_open(zipPath.string().c_str(), 0, 'r');
    if(zip == nullptr) return false;
    const int openResult = zip_entry_open(zip, normalizeZipPath(entryName).c_str());
    if(openResult == 0) zip_entry_close(zip);
    zip_close(zip);
    return openResult == 0;
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
    return evaluateMemoryCondition(condition, actual);
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
            Logger::instance().log("Failed to load mod image asset: " + normalizedPath, Logger::Type::WARNING);
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

std::optional<uint32_t> ModManager::debugReadDecodedImagePixel(const std::string& assetPath, int x, int y)
{
    const DecodedImage* image = decodedImage(assetPath);
    if(image == nullptr || x < 0 || y < 0 || x >= image->width || y >= image->height) {
        return std::nullopt;
    }
    return image->rgba[static_cast<size_t>(y) * static_cast<size_t>(image->width) + static_cast<size_t>(x)];
}

std::optional<uint32_t> ModManager::debugReadAssetPixelDirect(const std::string& assetPath, int x, int y)
{
    const std::string normalizedPath = normalizeZipPath(assetPath);
    const auto data = readAsset(normalizedPath);
    if(!data.has_value()) {
        return std::nullopt;
    }
    const auto decoded = decodeImage(*data);
    if(!decoded.has_value() || x < 0 || y < 0 || x >= decoded->width || y >= decoded->height) {
        return std::nullopt;
    }
    return decoded->rgba[static_cast<size_t>(y) * static_cast<size_t>(decoded->width) + static_cast<size_t>(x)];
}

std::vector<std::string> ModManager::debugListImageAssets() const
{
    std::vector<std::string> assets;
    assets.reserve(m_chrOverrides.size() + m_backgroundReplacements.size());

    auto appendUnique = [&](const std::string& assetPath) {
        if(assetPath.empty()) {
            return;
        }
        const std::string normalizedPath = normalizeZipPath(assetPath);
        if(std::find(assets.begin(), assets.end(), normalizedPath) == assets.end()) {
            assets.push_back(normalizedPath);
        }
    };

    for(const ChrOverride& override : m_chrOverrides) {
        if(override.enabled) {
            appendUnique(override.assetPath);
        }
    }
    for(const BackgroundReplacement& replacement : m_backgroundReplacements) {
        if(replacement.enabled) {
            appendUnique(replacement.assetPath);
        }
    }

    std::sort(assets.begin(), assets.end());
    return assets;
}

std::optional<ModManager::DebugDecodedImage> ModManager::debugCopyDecodedImage(const std::string& assetPath)
{
    const DecodedImage* image = decodedImage(assetPath);
    if(image == nullptr) {
        return std::nullopt;
    }

    DebugDecodedImage copy;
    copy.width = image->width;
    copy.height = image->height;
    copy.rgba = image->rgba;
    return copy;
}

std::optional<ModManager::DebugComposePixel> ModManager::debugComposePixel(const uint32_t* sourceFramebuffer, const ChrRenderSnapshot& snapshot, int scale, int nesX, int nesY, const std::string& filterText)
{
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
        if(override.enabled && !override.assetPath.empty()) {
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
        if(!replacement.enabled || replacement.assetPath.empty()) {
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
        if(tileIndex < 0 || tileIndex > 0x01FF) {
            return 0u;
        }
        return snapshot.tileHashes[static_cast<size_t>(tileIndex)];
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
            if(condition.expectedTileByHash) {
                const uint32_t pixelHash = pixel.tileHash != 0 ? pixel.tileHash : tileHash(pixel.tileIndex);
                if(pixelHash != condition.expectedTileHash) {
                    return false;
                }
            } else if(condition.expectedTileIndex >= 0 && static_cast<int>(pixel.tileIndex) != condition.expectedTileIndex) {
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
            if(condition.expectedTileByHash) {
                const uint32_t candidateHash = candidate.tileHash != 0 ? candidate.tileHash : tileHash(candidate.tileIndex);
                if(candidateHash != condition.expectedTileHash) {
                    return false;
                }
            } else if(condition.expectedTileIndex >= 0 && static_cast<int>(candidate.tileIndex) != condition.expectedTileIndex) {
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
            match = evaluateMemoryCondition(condition, actual);
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
                const int originX = ctx.backgroundPixel != nullptr ? (ctx.nesX - static_cast<int>(ctx.backgroundPixel->offsetX)) : ctx.nesX;
                const int originY = ctx.backgroundPixel != nullptr ? (ctx.nesY - static_cast<int>(ctx.backgroundPixel->offsetY)) : ctx.nesY;
                targetX = originX + condition.x;
                targetY = originY + condition.y;
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
                const int originX = ctx.spriteCandidate != nullptr ? (ctx.nesX - static_cast<int>(ctx.spriteCandidate->offsetX)) : ctx.nesX;
                const int originY = ctx.spriteCandidate != nullptr ? (ctx.nesY - static_cast<int>(ctx.spriteCandidate->offsetY)) : ctx.nesY;
                targetX = originX + condition.x * xSign;
                targetY = originY + condition.y * ySign;
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
        }
        if(condition.kind == MemoryCondition::Kind::MemoryCheck) {
            return match;
        }
        return condition.inverted ? !match : match;
    };

    auto conditionsMatchAt = [&](const std::vector<MemoryCondition>& conditions, const ConditionContext& ctx) {
        for(const MemoryCondition& condition : conditions) {
            if(!conditionMatchesAt(condition, ctx)) {
                return false;
            }
        }
        return true;
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
            out << " (" << condition.memoryType << "[$"
                << std::uppercase << std::hex << condition.address
                << "]=" << actual
                << " " << condition.op << " " << condition.value;
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
                const int originX = ctx.backgroundPixel != nullptr ? (ctx.nesX - static_cast<int>(ctx.backgroundPixel->offsetX)) : ctx.nesX;
                const int originY = ctx.backgroundPixel != nullptr ? (ctx.nesY - static_cast<int>(ctx.backgroundPixel->offsetY)) : ctx.nesY;
                targetX = originX + condition.x;
                targetY = originY + condition.y;
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
                const int originX = ctx.spriteCandidate != nullptr ? (ctx.nesX - static_cast<int>(ctx.spriteCandidate->offsetX)) : ctx.nesX;
                const int originY = ctx.spriteCandidate != nullptr ? (ctx.nesY - static_cast<int>(ctx.spriteCandidate->offsetY)) : ctx.nesY;
                targetX = originX + condition.x * xSign;
                targetY = originY + condition.y * ySign;
            }
            out << " (sprite @" << targetX << "," << targetY << ")";
            break;
        }
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
        const ChrOverride& override = *preparedOverride.override;
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

    auto scanCandidates = [&](const std::vector<const PreparedOverride*>& candidates, ChrOverride::Target target, bool allowDefaultTileFallback, int tileIndex, int fullTileIndex, int currentPatternTable, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority, const ConditionContext& ctx) -> const PreparedOverride* {
        for(const PreparedOverride* candidate : candidates) {
            if(matchesOverride(*candidate, target, allowDefaultTileFallback, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx)) {
                return candidate;
            }
        }
        return nullptr;
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
    uint32_t color = m_disableOriginalTiles ? 0x00000000u : result.baseColor;
    result.finalColor = color;

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
        const int priority = std::clamp(prepared.replacement->priority, 0, 39);
        DebugComposeStage debugStage;
        debugStage.valid = true;
        debugStage.stage = "bg select";
        debugStage.assetPath = prepared.replacement->assetPath;
        debugStage.priority = priority;

        if(activeBackgroundsByPriority[static_cast<size_t>(priority)] != nullptr) {
            debugStage.returnedBaseColor = true;
            debugStage.reason = "skipped after earlier selected background";
            if(backgroundDebugCounts[static_cast<size_t>(priority)] < 6 || debugAssetMatchesFilter(prepared.replacement->assetPath)) {
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

    auto sampleOverrideStage = [&](uint32_t baseColor, uint32_t fallbackLayerColor, const PreparedOverride* prepared, int tileIndex, int offsetX, int offsetY, const std::array<uint8_t, 3>& palette, bool horizontalMirror, bool verticalMirror, bool preserveSourceAlpha, const char* stageName) -> std::pair<uint32_t, DebugComposeStage> {
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
                stage.reason = preserveSourceAlpha ? "raw rgba alpha zero -> sprite fallback" : "raw rgba alpha zero";
                return { preserveSourceAlpha ? fallbackLayerColor : baseColor, stage };
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
            stage.reason = preserveSourceAlpha ? "indexed source alpha zero -> sprite fallback" : "indexed source alpha zero";
            return { preserveSourceAlpha ? fallbackLayerColor : baseColor, stage };
        }
        if(!image->indexedFourColor || image->indexedPixels.size() != image->rgba.size()) {
            stage.returnedBaseColor = true;
            stage.reason = preserveSourceAlpha ? "not indexed four-color png -> sprite fallback" : "not indexed four-color png";
            return { preserveSourceAlpha ? fallbackLayerColor : baseColor, stage };
        }

        const uint8_t sourcePaletteIndex = image->indexedPixels[sourcePixelIndex];
        stage.indexedPaletteIndex = static_cast<int>(sourcePaletteIndex);
        if(sourcePaletteIndex == 0) {
            if(preserveSourceAlpha) {
                stage.returnedBaseColor = true;
                stage.reason = "indexed color 0 -> sprite fallback";
                return { fallbackLayerColor, stage };
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
        backgroundFallbackColor = m_disableOriginalTiles ? 0x00000000u : snapshot.paletteColors[bgPixel->paletteIndex & 0x3F];
    }
    const bool backgroundOpaque = bgPixel != nullptr && bgPixel->valid && bgPixel->colorLowBits != 0;

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
            if(backgroundOpaque || !m_disableOriginalTiles) {
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

    int lowestBgSpriteCandidate = std::numeric_limits<int>::max();
    if(spritePixel != nullptr) {
        for(int i = 0; i < static_cast<int>(spritePixel->count); ++i) {
            const PPU::DebugModSpriteCandidate& candidate = spritePixel->candidates[static_cast<size_t>(i)];
            if(candidate.valid && candidate.behindBackground) {
                lowestBgSpriteCandidate = std::min(lowestBgSpriteCandidate, i);
            }
        }
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

    if(spritePixel != nullptr && spritePixel->count > 0) {
        auto applySpriteCandidate = [&](const PPU::DebugModSpriteCandidate& candidate, int candidateIndex, const char* bucket) {
            DebugComposeStage header;
            header.valid = true;
            header.stage = bucket;
            header.priority = candidateIndex;

            const uint32_t spriteFallbackColor = m_disableOriginalTiles ? color : spriteFallbackColorFor(candidate);
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
            auto [newColor, stage] = sampleOverrideStage(color, spriteFallbackColor, spriteOverride, candidate.tileIndex, candidate.offsetX, candidate.offsetY, spritePalette, candidate.horizontalMirror, candidate.verticalMirror, true, "sprite override");
            stage.priority = candidateIndex;
            if(spriteOverride == nullptr && !m_disableOriginalTiles && candidate.colorLowBits != 0) {
                newColor = spriteFallbackColor;
                stage.reason = "no override, using NES sprite fallback";
                stage.returnedBaseColor = false;
            }
            color = newColor;
            result.spriteStages.push_back(stage);
        };

        for(int i = static_cast<int>(spritePixel->count) - 1; i >= 0; --i) {
            const PPU::DebugModSpriteCandidate& candidate = spritePixel->candidates[static_cast<size_t>(i)];
            if(!candidate.valid || !candidate.behindBackground) {
                continue;
            }
            if(backgroundOpaque) {
                continue;
            }
            applySpriteCandidate(candidate, i, "sprite behind-bg");
        }

        for(int i = static_cast<int>(spritePixel->count) - 1; i >= 0; --i) {
            const PPU::DebugModSpriteCandidate& candidate = spritePixel->candidates[static_cast<size_t>(i)];
            if(!candidate.valid || candidate.behindBackground) {
                continue;
            }
            if(lowestBgSpriteCandidate <= i) {
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
    stbi_uc* pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels, 4);
    if(pixels == nullptr || width <= 0 || height <= 0) {
        stbi_image_free(pixels);
        return std::nullopt;
    }

    DecodedImage image;
    image.width = width;
    image.height = height;
    image.rgba.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
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
        image.indexedPng = true;
        image.indexedFourColor = true;
        image.paletteColorCount = indexed->paletteColorCount;
    }

    return image;
}

void ModManager::composeChrFrame(std::vector<uint32_t>& framebuffer, int width, int height, int activeTop, int activeBottom, int scale, const uint32_t* sourceFramebuffer, const ChrRenderSnapshot& snapshot, const std::vector<const ChrOverride*>* activeOverrideFilter)
{

    if(sourceFramebuffer == nullptr || framebuffer.empty() || width <= 0 || height <= 0 || scale <= 0) {
        return;
    }

    const FrameConditionState& frameConditionState =
        snapshot.frameConditionStateView != nullptr ? *snapshot.frameConditionStateView : snapshot.frameConditionState;

    if(m_chrOverrides.empty()) {
        for(int nesY = std::max(0, activeTop / scale); nesY < std::min(PPU::SCREEN_HEIGHT, (activeBottom + scale - 1) / scale); ++nesY) {
            const uint32_t* srcRow = sourceFramebuffer + static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH;
            for(int sy = 0; sy < scale; ++sy) {
                const int outY = nesY * scale + sy;
                if(outY < activeTop || outY >= activeBottom || outY < 0 || outY >= height) continue;
                uint32_t* dstRow = framebuffer.data() + static_cast<size_t>(outY) * static_cast<size_t>(width);
                for(int nesX = 0; nesX < PPU::SCREEN_WIDTH; ++nesX) {
                    const uint32_t color = srcRow[nesX];
                    for(int sx = 0; sx < scale; ++sx) {
                        const int outX = nesX * scale + sx;
                        if(outX >= 0 && outX < width) {
                            dstRow[outX] = color;
                        }
                    }
                }
            }
        }
        return;
    }

    std::vector<const ChrOverride*> activeOverridesLocal;
    std::vector<RenderPreparedOverride> preparedOverridesLocal;
    std::array<std::vector<const RenderPreparedOverride*>, 512> overridesByFullTileLocal;
    std::array<std::vector<const RenderPreparedOverride*>, 256> overridesByRelativeTileLocal;
    std::array<std::vector<const RenderPreparedOverride*>, 512> dynamicOverridesByFullTileLocal;
    std::array<std::vector<const RenderPreparedOverride*>, 256> dynamicOverridesByRelativeTileLocal;
    std::unordered_map<uint32_t, std::vector<const RenderPreparedOverride*>> overridesByChrHashLocal;
    std::unordered_map<uint32_t, std::vector<const RenderPreparedOverride*>> dynamicOverridesByChrHashLocal;
    std::vector<const RenderPreparedOverride*> wholeChrOverridesLocal;
    std::vector<const RenderPreparedOverride*> dynamicWholeChrOverridesLocal;
    std::array<bool, 512> hasStaticOverridesByFullTileLocal = {};
    std::array<bool, 256> hasStaticOverridesByRelativeTileLocal = {};
    std::unordered_set<uint32_t> staticOverrideHashesLocal;
    bool hasWholeChrOverridesLocal = false;
    std::array<bool, 512> hasDefaultOverridesByFullTileLocal = {};
    std::array<bool, 256> hasDefaultOverridesByRelativeTileLocal = {};
    std::unordered_set<uint32_t> defaultOverrideHashesLocal;
    bool hasWholeChrDefaultOverridesLocal = false;
    std::array<bool, 512> hasDynamicOverridesByFullTileLocal = {};
    std::array<bool, 256> hasDynamicOverridesByRelativeTileLocal = {};
    std::unordered_set<uint32_t> dynamicOverrideHashesLocal;
    std::vector<RenderPreparedBackground> preparedBackgroundsLocal;
    const std::vector<const ChrOverride*>* activeOverridesPtr = nullptr;

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

    const std::vector<RenderPreparedOverride>* preparedOverridesPtr = nullptr;
    const std::array<std::vector<const RenderPreparedOverride*>, 512>* overridesByFullTilePtr = nullptr;
    const std::array<std::vector<const RenderPreparedOverride*>, 256>* overridesByRelativeTilePtr = nullptr;
    const std::array<std::vector<const RenderPreparedOverride*>, 512>* dynamicOverridesByFullTilePtr = nullptr;
    const std::array<std::vector<const RenderPreparedOverride*>, 256>* dynamicOverridesByRelativeTilePtr = nullptr;
    const std::unordered_map<uint32_t, std::vector<const RenderPreparedOverride*>>* overridesByChrHashPtr = nullptr;
    const std::unordered_map<uint32_t, std::vector<const RenderPreparedOverride*>>* dynamicOverridesByChrHashPtr = nullptr;
    const std::vector<const RenderPreparedOverride*>* wholeChrOverridesPtr = nullptr;
    const std::vector<const RenderPreparedOverride*>* dynamicWholeChrOverridesPtr = nullptr;
    const std::array<bool, 512>* hasStaticOverridesByFullTilePtr = nullptr;
    const std::array<bool, 256>* hasStaticOverridesByRelativeTilePtr = nullptr;
    const std::unordered_set<uint32_t>* staticOverrideHashesPtr = nullptr;
    bool hasWholeChrOverrides = false;
    const std::array<bool, 512>* hasDefaultOverridesByFullTilePtr = nullptr;
    const std::array<bool, 256>* hasDefaultOverridesByRelativeTilePtr = nullptr;
    const std::unordered_set<uint32_t>* defaultOverrideHashesPtr = nullptr;
    bool hasWholeChrDefaultOverrides = false;
    const std::array<bool, 512>* hasDynamicOverridesByFullTilePtr = nullptr;
    const std::array<bool, 256>* hasDynamicOverridesByRelativeTilePtr = nullptr;
    const std::unordered_set<uint32_t>* dynamicOverrideHashesPtr = nullptr;
    const std::vector<RenderPreparedBackground>* preparedBackgroundsPtr = nullptr;

    if(activeOverrideFilter == nullptr) {
        if(m_renderComposeCacheDirty || !m_renderComposeCache.valid || m_renderComposeCache.scale != std::max(1, m_resolutionMultiplier)) {
            rebuildRenderComposeCache();
        }
        activeOverridesPtr = &m_renderComposeCache.activeOverrides;
        preparedOverridesPtr = &m_renderComposeCache.preparedOverrides;
        overridesByFullTilePtr = &m_renderComposeCache.overridesByFullTile;
        overridesByRelativeTilePtr = &m_renderComposeCache.overridesByRelativeTile;
        dynamicOverridesByFullTilePtr = &m_renderComposeCache.dynamicOverridesByFullTile;
        dynamicOverridesByRelativeTilePtr = &m_renderComposeCache.dynamicOverridesByRelativeTile;
        overridesByChrHashPtr = &m_renderComposeCache.overridesByChrHash;
        dynamicOverridesByChrHashPtr = &m_renderComposeCache.dynamicOverridesByChrHash;
        wholeChrOverridesPtr = &m_renderComposeCache.wholeChrOverrides;
        dynamicWholeChrOverridesPtr = &m_renderComposeCache.dynamicWholeChrOverrides;
        hasStaticOverridesByFullTilePtr = &m_renderComposeCache.hasStaticOverridesByFullTile;
        hasStaticOverridesByRelativeTilePtr = &m_renderComposeCache.hasStaticOverridesByRelativeTile;
        staticOverrideHashesPtr = &m_renderComposeCache.staticOverrideHashes;
        hasWholeChrOverrides = m_renderComposeCache.hasWholeChrOverrides;
        hasDefaultOverridesByFullTilePtr = &m_renderComposeCache.hasDefaultOverridesByFullTile;
        hasDefaultOverridesByRelativeTilePtr = &m_renderComposeCache.hasDefaultOverridesByRelativeTile;
        defaultOverrideHashesPtr = &m_renderComposeCache.defaultOverrideHashes;
        hasWholeChrDefaultOverrides = m_renderComposeCache.hasWholeChrDefaultOverrides;
        hasDynamicOverridesByFullTilePtr = &m_renderComposeCache.hasDynamicOverridesByFullTile;
        hasDynamicOverridesByRelativeTilePtr = &m_renderComposeCache.hasDynamicOverridesByRelativeTile;
        dynamicOverrideHashesPtr = &m_renderComposeCache.dynamicOverrideHashes;
        preparedBackgroundsPtr = &m_renderComposeCache.preparedBackgrounds;
    } else {
        activeOverridesLocal = *activeOverrideFilter;
        if(!activeOverridesLocal.empty()) {
            std::stable_sort(activeOverridesLocal.begin(), activeOverridesLocal.end(), [](const ChrOverride* a, const ChrOverride* b) {
                return a->priority > b->priority;
            });
        }
        preparedOverridesLocal.reserve(activeOverridesLocal.size());
        for(const ChrOverride* override : activeOverridesLocal) {
            RenderPreparedOverride prepared;
            prepared.override = override;
            prepared.sequence = preparedOverridesLocal.size();
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
            preparedOverridesLocal.push_back(std::move(prepared));
            const RenderPreparedOverride* preparedPtr = &preparedOverridesLocal.back();
            if(override->wholeChr()) {
                if(override->hasChrHash) {
                    staticOverrideHashesLocal.insert(override->chrHash);
                    if(override->defaultTile) {
                        defaultOverrideHashesLocal.insert(override->chrHash);
                    }
                    if(preparedPtr->hasDynamicConditions) {
                        dynamicOverridesByChrHashLocal[override->chrHash].push_back(preparedPtr);
                    } else {
                        overridesByChrHashLocal[override->chrHash].push_back(preparedPtr);
                    }
                } else if(preparedPtr->hasDynamicConditions) {
                    if(override->defaultTile) {
                        hasWholeChrDefaultOverridesLocal = true;
                    }
                    dynamicWholeChrOverridesLocal.push_back(preparedPtr);
                } else {
                    hasWholeChrOverridesLocal = true;
                    if(override->defaultTile) {
                        hasWholeChrDefaultOverridesLocal = true;
                    }
                    wholeChrOverridesLocal.push_back(preparedPtr);
                }
            } else if(override->absoluteTile) {
                if(override->tile >= 0 && override->tile < static_cast<int>(overridesByFullTileLocal.size())) {
                    hasStaticOverridesByFullTileLocal[static_cast<size_t>(override->tile)] = true;
                    if(override->defaultTile) {
                        hasDefaultOverridesByFullTileLocal[static_cast<size_t>(override->tile)] = true;
                    }
                    if(preparedPtr->hasDynamicConditions) {
                        dynamicOverridesByFullTileLocal[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    } else {
                        overridesByFullTileLocal[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    }
                }
            } else if(override->tile >= 0) {
                if(override->tile < static_cast<int>(overridesByRelativeTileLocal.size())) {
                    hasStaticOverridesByRelativeTileLocal[static_cast<size_t>(override->tile)] = true;
                    if(override->defaultTile) {
                        hasDefaultOverridesByRelativeTileLocal[static_cast<size_t>(override->tile)] = true;
                    }
                    if(preparedPtr->hasDynamicConditions) {
                        dynamicOverridesByRelativeTileLocal[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    } else {
                        overridesByRelativeTileLocal[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    }
                } else if(override->tile < static_cast<int>(overridesByFullTileLocal.size())) {
                    hasStaticOverridesByFullTileLocal[static_cast<size_t>(override->tile)] = true;
                    if(override->defaultTile) {
                        hasDefaultOverridesByFullTileLocal[static_cast<size_t>(override->tile)] = true;
                    }
                    if(preparedPtr->hasDynamicConditions) {
                        dynamicOverridesByFullTileLocal[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    } else {
                        overridesByFullTileLocal[static_cast<size_t>(override->tile)].push_back(preparedPtr);
                    }
                }
            }
        }
        for(size_t i = 0; i < dynamicOverridesByFullTileLocal.size(); ++i) {
            hasDynamicOverridesByFullTileLocal[i] = !dynamicOverridesByFullTileLocal[i].empty();
        }
        for(size_t i = 0; i < dynamicOverridesByRelativeTileLocal.size(); ++i) {
            hasDynamicOverridesByRelativeTileLocal[i] = !dynamicOverridesByRelativeTileLocal[i].empty();
        }
        dynamicOverrideHashesLocal.reserve(dynamicOverridesByChrHashLocal.size());
        for(const auto& [hash, _] : dynamicOverridesByChrHashLocal) {
            dynamicOverrideHashesLocal.insert(hash);
        }
        preparedBackgroundsLocal.reserve(m_backgroundReplacements.size());
        for(const BackgroundReplacement& replacement : m_backgroundReplacements) {
            if(!replacement.enabled || replacement.assetPath.empty()) {
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
            preparedBackgroundsLocal.push_back(std::move(prepared));
        }
        activeOverridesPtr = &activeOverridesLocal;
        preparedOverridesPtr = &preparedOverridesLocal;
        overridesByFullTilePtr = &overridesByFullTileLocal;
        overridesByRelativeTilePtr = &overridesByRelativeTileLocal;
        dynamicOverridesByFullTilePtr = &dynamicOverridesByFullTileLocal;
        dynamicOverridesByRelativeTilePtr = &dynamicOverridesByRelativeTileLocal;
        overridesByChrHashPtr = &overridesByChrHashLocal;
        dynamicOverridesByChrHashPtr = &dynamicOverridesByChrHashLocal;
        wholeChrOverridesPtr = &wholeChrOverridesLocal;
        dynamicWholeChrOverridesPtr = &dynamicWholeChrOverridesLocal;
        hasStaticOverridesByFullTilePtr = &hasStaticOverridesByFullTileLocal;
        hasStaticOverridesByRelativeTilePtr = &hasStaticOverridesByRelativeTileLocal;
        staticOverrideHashesPtr = &staticOverrideHashesLocal;
        hasWholeChrOverrides = hasWholeChrOverridesLocal;
        hasDefaultOverridesByFullTilePtr = &hasDefaultOverridesByFullTileLocal;
        hasDefaultOverridesByRelativeTilePtr = &hasDefaultOverridesByRelativeTileLocal;
        defaultOverrideHashesPtr = &defaultOverrideHashesLocal;
        hasWholeChrDefaultOverrides = hasWholeChrDefaultOverridesLocal;
        hasDynamicOverridesByFullTilePtr = &hasDynamicOverridesByFullTileLocal;
        hasDynamicOverridesByRelativeTilePtr = &hasDynamicOverridesByRelativeTileLocal;
        dynamicOverrideHashesPtr = &dynamicOverrideHashesLocal;
        preparedBackgroundsPtr = &preparedBackgroundsLocal;
    }

    const auto& activeOverrides = *activeOverridesPtr;
    const auto& preparedOverrides = *preparedOverridesPtr;
    const auto& overridesByFullTile = *overridesByFullTilePtr;
    const auto& overridesByRelativeTile = *overridesByRelativeTilePtr;
    const auto& dynamicOverridesByFullTile = *dynamicOverridesByFullTilePtr;
    const auto& dynamicOverridesByRelativeTile = *dynamicOverridesByRelativeTilePtr;
    const auto& overridesByChrHash = *overridesByChrHashPtr;
    const auto& dynamicOverridesByChrHash = *dynamicOverridesByChrHashPtr;
    const auto& wholeChrOverrides = *wholeChrOverridesPtr;
    const auto& dynamicWholeChrOverrides = *dynamicWholeChrOverridesPtr;
    const auto& hasStaticOverridesByFullTile = *hasStaticOverridesByFullTilePtr;
    const auto& hasStaticOverridesByRelativeTile = *hasStaticOverridesByRelativeTilePtr;
    const auto& staticOverrideHashes = *staticOverrideHashesPtr;
    const auto& hasDefaultOverridesByFullTile = *hasDefaultOverridesByFullTilePtr;
    const auto& hasDefaultOverridesByRelativeTile = *hasDefaultOverridesByRelativeTilePtr;
    const auto& defaultOverrideHashes = *defaultOverrideHashesPtr;
    const auto& hasDynamicOverridesByFullTile = *hasDynamicOverridesByFullTilePtr;
    const auto& hasDynamicOverridesByRelativeTile = *hasDynamicOverridesByRelativeTilePtr;
    const auto& dynamicOverrideHashes = *dynamicOverrideHashesPtr;
    const auto& preparedBackgrounds = *preparedBackgroundsPtr;
    const bool canUseOverrideLookupCache = !preparedOverrides.empty();
    using PreparedOverride = RenderPreparedOverride;
    using PreparedBackground = RenderPreparedBackground;

    if(preparedOverrides.empty() && preparedBackgrounds.empty()) {
        for(int nesY = std::max(0, activeTop / scale); nesY < std::min(PPU::SCREEN_HEIGHT, (activeBottom + scale - 1) / scale); ++nesY) {
            const uint32_t* srcRow = sourceFramebuffer + static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH;
            for(int sy = 0; sy < scale; ++sy) {
                const int outY = nesY * scale + sy;
                if(outY < activeTop || outY >= activeBottom || outY < 0 || outY >= height) continue;
                uint32_t* dstRow = framebuffer.data() + static_cast<size_t>(outY) * static_cast<size_t>(width);
                for(int nesX = 0; nesX < PPU::SCREEN_WIDTH; ++nesX) {
                    const uint32_t color = srcRow[nesX];
                    for(int sx = 0; sx < scale; ++sx) {
                        const int outX = nesX * scale + sx;
                        if(outX >= 0 && outX < width) {
                            dstRow[outX] = color;
                        }
                    }
                }
            }
        }
        return;
    }

    auto tileHash = [&](int tileIndex) {
        if(tileIndex < 0 || tileIndex > 0x01FF) {
            return 0u;
        }
        return snapshot.tileHashes[static_cast<size_t>(tileIndex)];
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
            if(condition.expectedTileByHash) {
                const uint32_t pixelHash = pixel.tileHash != 0 ? pixel.tileHash : tileHash(pixel.tileIndex);
                if(pixelHash != condition.expectedTileHash) {
                    return false;
                }
            } else if(condition.expectedTileIndex >= 0 && static_cast<int>(pixel.tileIndex) != condition.expectedTileIndex) {
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
            if(condition.expectedTileByHash) {
                const uint32_t candidateHash = candidate.tileHash != 0 ? candidate.tileHash : tileHash(candidate.tileIndex);
                if(candidateHash != condition.expectedTileHash) {
                    return false;
                }
            } else if(condition.expectedTileIndex >= 0 && static_cast<int>(candidate.tileIndex) != condition.expectedTileIndex) {
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
            match = evaluateMemoryCondition(condition, actual);
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
                const int originX = ctx.backgroundPixel != nullptr ? (ctx.nesX - static_cast<int>(ctx.backgroundPixel->offsetX)) : ctx.nesX;
                const int originY = ctx.backgroundPixel != nullptr ? (ctx.nesY - static_cast<int>(ctx.backgroundPixel->offsetY)) : ctx.nesY;
                targetX = originX + condition.x;
                targetY = originY + condition.y;
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
                const int originX = ctx.spriteCandidate != nullptr ? (ctx.nesX - static_cast<int>(ctx.spriteCandidate->offsetX)) : ctx.nesX;
                const int originY = ctx.spriteCandidate != nullptr ? (ctx.nesY - static_cast<int>(ctx.spriteCandidate->offsetY)) : ctx.nesY;
                targetX = originX + condition.x * xSign;
                targetY = originY + condition.y * ySign;
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
        }
        if(condition.kind == MemoryCondition::Kind::MemoryCheck) {
            return match;
        }
        return condition.inverted ? !match : match;
    };

    auto conditionsMatchAt = [&](const std::vector<MemoryCondition>& conditions, const ConditionContext& ctx) {
        for(const MemoryCondition& condition : conditions) {
            if(!conditionMatchesAt(condition, ctx)) {
                return false;
            }
        }
        return true;
    };

    auto matchesRequirement = [](int requirement, bool value) {
        return requirement == 0 || (requirement > 0 && value) || (requirement < 0 && !value);
    };

    auto matchesOverride = [&](const PreparedOverride& preparedOverride, ChrOverride::Target target, bool allowDefaultTileFallback, int tileIndex, int fullTileIndex, int currentPatternTable, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority, const ConditionContext& ctx) {
        const ChrOverride& override = *preparedOverride.override;
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

    auto scanCandidates = [&](const std::vector<const PreparedOverride*>& candidates, bool allowDefaultTileFallback, ChrOverride::Target target, int tileIndex, int fullTileIndex, int currentPatternTable, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority, const ConditionContext& ctx) -> const PreparedOverride* {
        for(const PreparedOverride* candidate : candidates) {
            if(matchesOverride(*candidate, target, allowDefaultTileFallback, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx)) {
                return candidate;
            }
        }
        return nullptr;
    };

    auto scanMergedCandidates = [&](const std::vector<const PreparedOverride*>& staticCandidates, const std::vector<const PreparedOverride*>& dynamicCandidates, bool allowDefaultTileFallback, ChrOverride::Target target, int tileIndex, int fullTileIndex, int currentPatternTable, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority, const ConditionContext& ctx) -> const PreparedOverride* {
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

    auto makeOverrideLookupKey = [](ChrOverride::Target target, int fullTileIndex, int currentPatternTable, uint32_t currentTileHash, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority) {
        uint64_t key = static_cast<uint64_t>(fullTileIndex & 0x01FF);
        key |= static_cast<uint64_t>(currentPatternTable & 0x03) << 9;
        key |= static_cast<uint64_t>(target == ChrOverride::Target::Sprite ? 1 : 0) << 11;
        key |= static_cast<uint64_t>(palette[0] & 0x3F) << 12;
        key |= static_cast<uint64_t>(palette[1] & 0x3F) << 18;
        key |= static_cast<uint64_t>(palette[2] & 0x3F) << 24;
        key |= static_cast<uint64_t>(hMirror ? 1 : 0) << 30;
        key |= static_cast<uint64_t>(vMirror ? 1 : 0) << 31;
        key |= static_cast<uint64_t>(bgPriority ? 1 : 0) << 32;
        key ^= static_cast<uint64_t>(currentTileHash) << 33;
        return key;
    };

    std::unordered_map<uint64_t, const PreparedOverride*> overrideLookupCache;
    overrideLookupCache.reserve(std::min<size_t>(preparedOverrides.size() * 32u, 32768u));
    std::unordered_map<uint64_t, const PreparedOverride*> dynamicOverrideLookupCache;
    dynamicOverrideLookupCache.reserve(std::min<size_t>(preparedOverrides.size() * 64u, 65536u));

    struct ResolvedBackgroundLayer {
        const PreparedBackground* prepared = nullptr;
        bool valid = false;
        int baseSrcX = 0;
        int baseSrcY = 0;
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

    auto findOverride = [&](ChrOverride::Target target, int tileIndex, int fullTileIndex, int currentPatternTable, const std::array<uint8_t, 3>& palette, bool hMirror, bool vMirror, bool bgPriority, const ConditionContext& ctx) -> const PreparedOverride* {
        const uint32_t lookupHash = tileHash(fullTileIndex);
        const uint32_t currentTileHash =
            target == ChrOverride::Target::Sprite
                ? ((ctx.spriteCandidate != nullptr && ctx.spriteCandidate->tileHash != 0) ? ctx.spriteCandidate->tileHash : lookupHash)
                : ((ctx.backgroundPixel != nullptr && ctx.backgroundPixel->tileHash != 0) ? ctx.backgroundPixel->tileHash : lookupHash);
        const uint64_t lookupKey = makeOverrideLookupKey(target, fullTileIndex, currentPatternTable, currentTileHash, palette, hMirror, vMirror, bgPriority);
        const bool hasStaticExactCandidates =
            hasWholeChrOverrides ||
            (staticOverrideHashes.find(lookupHash) != staticOverrideHashes.end()) ||
            (fullTileIndex >= 0 && fullTileIndex < static_cast<int>(hasStaticOverridesByFullTile.size()) &&
             hasStaticOverridesByFullTile[static_cast<size_t>(fullTileIndex)]) ||
            (tileIndex >= 0 && tileIndex < static_cast<int>(hasStaticOverridesByRelativeTile.size()) &&
             hasStaticOverridesByRelativeTile[static_cast<size_t>(tileIndex)]);
        const bool hasDynamicCandidates =
            !dynamicWholeChrOverrides.empty() ||
            (dynamicOverrideHashes.find(lookupHash) != dynamicOverrideHashes.end()) ||
            (fullTileIndex >= 0 && fullTileIndex < static_cast<int>(dynamicOverridesByFullTile.size()) &&
             hasDynamicOverridesByFullTile[static_cast<size_t>(fullTileIndex)]) ||
            (tileIndex >= 0 && tileIndex < static_cast<int>(dynamicOverridesByRelativeTile.size()) &&
             hasDynamicOverridesByRelativeTile[static_cast<size_t>(tileIndex)]);
        const bool hasDefaultCandidates =
            hasWholeChrDefaultOverrides ||
            (defaultOverrideHashes.find(lookupHash) != defaultOverrideHashes.end()) ||
            (fullTileIndex >= 0 && fullTileIndex < static_cast<int>(hasDefaultOverridesByFullTile.size()) &&
             hasDefaultOverridesByFullTile[static_cast<size_t>(fullTileIndex)]) ||
            (tileIndex >= 0 && tileIndex < static_cast<int>(hasDefaultOverridesByRelativeTile.size()) &&
             hasDefaultOverridesByRelativeTile[static_cast<size_t>(tileIndex)]);
        if(canUseOverrideLookupCache && !hasDynamicCandidates) {
            if(const auto it = overrideLookupCache.find(lookupKey); it != overrideLookupCache.end()) {
                return it->second;
            }
        }
        if(!hasDynamicCandidates && !hasStaticExactCandidates && !hasDefaultCandidates) {
            if(canUseOverrideLookupCache) {
                overrideLookupCache.emplace(lookupKey, nullptr);
            }
            return nullptr;
        }
        const uint64_t dynamicLookupKey = hasDynamicCandidates ? makeDynamicOverrideLookupKey(lookupKey, ctx) : 0u;
        if(hasDynamicCandidates) {
            if(const auto it = dynamicOverrideLookupCache.find(dynamicLookupKey); it != dynamicOverrideLookupCache.end()) {
                return it->second;
            }
        }

        auto scanFullTile = [&](int lookupTile, bool allowDefaultTileFallback) -> const PreparedOverride* {
            if(lookupTile >= 0 && lookupTile < static_cast<int>(overridesByFullTile.size())) {
                if(const PreparedOverride* found = scanCandidates(overridesByFullTile[static_cast<size_t>(lookupTile)], allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx)) {
                    return found;
                }
            }
            return nullptr;
        };

        auto scanRelativeTile = [&](int lookupTile, bool allowDefaultTileFallback) -> const PreparedOverride* {
            if(lookupTile >= 0 && lookupTile < static_cast<int>(overridesByRelativeTile.size())) {
                if(const PreparedOverride* found = scanCandidates(overridesByRelativeTile[static_cast<size_t>(lookupTile)], allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx)) {
                    return found;
                }
            }
            return nullptr;
        };

        auto scanHash = [&](bool allowDefaultTileFallback) -> const PreparedOverride* {
            if(const auto it = overridesByChrHash.find(lookupHash); it != overridesByChrHash.end()) {
                if(const PreparedOverride* found = scanCandidates(it->second, allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx)) {
                    return found;
                }
            }
            return nullptr;
        };

        auto scanMergedFullTile = [&](int lookupTile, bool allowDefaultTileFallback) -> const PreparedOverride* {
            if(lookupTile < 0 || lookupTile >= static_cast<int>(overridesByFullTile.size())) {
                return nullptr;
            }
            return scanMergedCandidates(
                overridesByFullTile[static_cast<size_t>(lookupTile)],
                dynamicOverridesByFullTile[static_cast<size_t>(lookupTile)],
                allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx);
        };

        auto scanMergedRelativeTile = [&](int lookupTile, bool allowDefaultTileFallback) -> const PreparedOverride* {
            if(lookupTile < 0 || lookupTile >= static_cast<int>(overridesByRelativeTile.size())) {
                return nullptr;
            }
            return scanMergedCandidates(
                overridesByRelativeTile[static_cast<size_t>(lookupTile)],
                dynamicOverridesByRelativeTile[static_cast<size_t>(lookupTile)],
                allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx);
        };

        auto scanMergedHash = [&](bool allowDefaultTileFallback) -> const PreparedOverride* {
            static const std::vector<const PreparedOverride*> emptyCandidates;
            const auto staticIt = overridesByChrHash.find(lookupHash);
            const auto dynamicIt = dynamicOverridesByChrHash.find(lookupHash);
            const auto& staticCandidates = staticIt != overridesByChrHash.end() ? staticIt->second : emptyCandidates;
            const auto& dynamicCandidates = dynamicIt != dynamicOverridesByChrHash.end() ? dynamicIt->second : emptyCandidates;
            return scanMergedCandidates(staticCandidates, dynamicCandidates, allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx);
        };

        auto scanMergedWhole = [&](bool allowDefaultTileFallback) -> const PreparedOverride* {
            return scanMergedCandidates(wholeChrOverrides, dynamicWholeChrOverrides, allowDefaultTileFallback, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx);
        };

        if(!hasDynamicCandidates) {
            if(fullTileIndex >= 0 && fullTileIndex < static_cast<int>(hasStaticOverridesByFullTile.size()) &&
               hasStaticOverridesByFullTile[static_cast<size_t>(fullTileIndex)]) {
                if(const PreparedOverride* found = scanFullTile(fullTileIndex, false)) {
                    if(canUseOverrideLookupCache) {
                        overrideLookupCache.emplace(lookupKey, found);
                    }
                    return found;
                }
            }
            if(fullTileIndex != tileIndex &&
               tileIndex >= 0 && tileIndex < static_cast<int>(hasStaticOverridesByRelativeTile.size()) &&
               hasStaticOverridesByRelativeTile[static_cast<size_t>(tileIndex)]) {
                if(const PreparedOverride* found = scanRelativeTile(tileIndex, false)) {
                    if(canUseOverrideLookupCache) {
                        overrideLookupCache.emplace(lookupKey, found);
                    }
                    return found;
                }
            }
            if(staticOverrideHashes.find(lookupHash) != staticOverrideHashes.end()) {
                if(const PreparedOverride* found = scanHash(false)) {
                    if(canUseOverrideLookupCache) {
                        overrideLookupCache.emplace(lookupKey, found);
                    }
                    return found;
                }
            }
            if(hasWholeChrOverrides) {
                if(const PreparedOverride* found = scanCandidates(wholeChrOverrides, false, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx)) {
                    if(canUseOverrideLookupCache) {
                        overrideLookupCache.emplace(lookupKey, found);
                    }
                    return found;
                }
            }

            if(!hasDefaultCandidates) {
                if(canUseOverrideLookupCache) {
                    overrideLookupCache.emplace(lookupKey, nullptr);
                }
                return nullptr;
            }

            if(fullTileIndex >= 0 && fullTileIndex < static_cast<int>(hasDefaultOverridesByFullTile.size()) &&
               hasDefaultOverridesByFullTile[static_cast<size_t>(fullTileIndex)]) {
                if(const PreparedOverride* found = scanFullTile(fullTileIndex, true)) {
                    if(canUseOverrideLookupCache) {
                        overrideLookupCache.emplace(lookupKey, found);
                    }
                    return found;
                }
            }
            if(defaultOverrideHashes.find(lookupHash) != defaultOverrideHashes.end()) {
                if(const PreparedOverride* found = scanHash(true)) {
                    if(canUseOverrideLookupCache) {
                        overrideLookupCache.emplace(lookupKey, found);
                    }
                    return found;
                }
            }
            if(fullTileIndex == tileIndex &&
               tileIndex >= 0 && tileIndex < static_cast<int>(hasDefaultOverridesByRelativeTile.size()) &&
               hasDefaultOverridesByRelativeTile[static_cast<size_t>(tileIndex)]) {
                if(const PreparedOverride* found = scanRelativeTile(tileIndex, true)) {
                    if(canUseOverrideLookupCache) {
                        overrideLookupCache.emplace(lookupKey, found);
                    }
                    return found;
                }
            }
            if(hasWholeChrDefaultOverrides) {
                if(const PreparedOverride* found = scanCandidates(wholeChrOverrides, true, target, tileIndex, fullTileIndex, currentPatternTable, palette, hMirror, vMirror, bgPriority, ctx)) {
                    if(canUseOverrideLookupCache) {
                        overrideLookupCache.emplace(lookupKey, found);
                    }
                    return found;
                }
            }

            if(canUseOverrideLookupCache) {
                overrideLookupCache.emplace(lookupKey, nullptr);
            }
            return nullptr;
        }

        if(fullTileIndex >= 0 && fullTileIndex < static_cast<int>(overridesByFullTile.size())) {
            if(const PreparedOverride* exact = scanMergedFullTile(fullTileIndex, false)) {
                dynamicOverrideLookupCache.emplace(dynamicLookupKey, exact);
                return exact;
            }
        }
        if(fullTileIndex != tileIndex &&
           tileIndex >= 0 && tileIndex < static_cast<int>(overridesByRelativeTile.size())) {
            if(const PreparedOverride* exact = scanMergedRelativeTile(tileIndex, false)) {
                dynamicOverrideLookupCache.emplace(dynamicLookupKey, exact);
                return exact;
            }
        }
        if(hasStaticExactCandidates || dynamicOverrideHashes.find(lookupHash) != dynamicOverrideHashes.end()) {
            if(const PreparedOverride* exact = scanMergedHash(false)) {
                dynamicOverrideLookupCache.emplace(dynamicLookupKey, exact);
                return exact;
            }
        }
        if(hasWholeChrOverrides || !dynamicWholeChrOverrides.empty()) {
            if(const PreparedOverride* exact = scanMergedWhole(false)) {
                dynamicOverrideLookupCache.emplace(dynamicLookupKey, exact);
                return exact;
            }
        }

        if(!hasDefaultCandidates) {
            dynamicOverrideLookupCache.emplace(dynamicLookupKey, nullptr);
            return nullptr;
        }
        if(defaultOverrideHashes.find(lookupHash) != defaultOverrideHashes.end()) {
            if(const PreparedOverride* dynamicDefault = scanMergedHash(true)) {
                dynamicOverrideLookupCache.emplace(dynamicLookupKey, dynamicDefault);
                return dynamicDefault;
            }
        }
        if(fullTileIndex >= 0 && fullTileIndex < static_cast<int>(hasDefaultOverridesByFullTile.size()) &&
           hasDefaultOverridesByFullTile[static_cast<size_t>(fullTileIndex)]) {
            if(const PreparedOverride* dynamicDefault = scanMergedFullTile(fullTileIndex, true)) {
                dynamicOverrideLookupCache.emplace(dynamicLookupKey, dynamicDefault);
                return dynamicDefault;
            }
        }
        if(fullTileIndex != tileIndex) {
            dynamicOverrideLookupCache.emplace(dynamicLookupKey, nullptr);
            return nullptr;
        }
        if(tileIndex >= 0 && tileIndex < static_cast<int>(hasDefaultOverridesByRelativeTile.size()) &&
           hasDefaultOverridesByRelativeTile[static_cast<size_t>(tileIndex)]) {
            if(const PreparedOverride* relativeDefaultMatch = scanMergedRelativeTile(tileIndex, true)) {
                dynamicOverrideLookupCache.emplace(dynamicLookupKey, relativeDefaultMatch);
                return relativeDefaultMatch;
            }
        }
        if(hasWholeChrDefaultOverrides || !dynamicWholeChrOverrides.empty()) {
            if(const PreparedOverride* dynamicDefault = scanMergedWhole(true)) {
                dynamicOverrideLookupCache.emplace(dynamicLookupKey, dynamicDefault);
                return dynamicDefault;
            }
        }
        dynamicOverrideLookupCache.emplace(dynamicLookupKey, nullptr);
        return nullptr;
    };

    auto sampleOverridePixel = [&](uint32_t baseColor, uint32_t fallbackLayerColor, const PreparedOverride* prepared, int tileIndex, int offsetX, int offsetY, int subX, int subY, uint8_t /*colorLowBits*/, const std::array<uint8_t, 3>& palette, bool horizontalMirror, bool verticalMirror, bool preserveSourceAlpha = false) {
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
                return preserveSourceAlpha ? fallbackLayerColor : baseColor;
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
            return preserveSourceAlpha ? fallbackLayerColor : baseColor;
        }
        if(!image->indexedFourColor || image->indexedPixels.size() != image->rgba.size()) {
            return preserveSourceAlpha ? fallbackLayerColor : baseColor;
        }
        const uint8_t sourcePaletteIndex = image->indexedPixels[sourcePixelIndex];
        if(sourcePaletteIndex == 0) {
            if(preserveSourceAlpha) {
                return fallbackLayerColor;
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
    for(const PreparedBackground& prepared : preparedBackgrounds) {
        if(activeBackgroundsByPriority[static_cast<size_t>(prepared.priority)] != nullptr) {
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

    struct ActiveBackgroundLayer {
        const PreparedBackground* prepared = nullptr;
        int bgScrollX = 0;
        int bgScrollY = 0;
    };

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

    std::vector<ActiveBackgroundLayer> lowPriorityBackgrounds;
    std::vector<ActiveBackgroundLayer> midBeforeTileBackgrounds;
    std::vector<ActiveBackgroundLayer> midAfterTileBackgrounds;
    std::vector<ActiveBackgroundLayer> highPriorityBackgrounds;
    lowPriorityBackgrounds.reserve(10);
    midBeforeTileBackgrounds.reserve(10);
    midAfterTileBackgrounds.reserve(10);
    highPriorityBackgrounds.reserve(10);
    for(int priority = 0; priority < 40; ++priority) {
        if(const PreparedBackground* preparedBackground = activeBackgroundsByPriority[static_cast<size_t>(priority)]; preparedBackground != nullptr) {
            const BackgroundReplacement& replacement = *preparedBackground->replacement;
            ActiveBackgroundLayer activeLayer;
            activeLayer.prepared = preparedBackground;
            activeLayer.bgScrollX = static_cast<int>(scrollXForLine(0) * replacement.parallaxX) + replacement.scrollX;
            activeLayer.bgScrollY = static_cast<int>(scrollYForLine(0) * replacement.parallaxY) + replacement.scrollY;
            if(priority < 10) {
                lowPriorityBackgrounds.push_back(activeLayer);
            } else if(priority < 20) {
                midBeforeTileBackgrounds.push_back(activeLayer);
            } else if(priority < 30) {
                midAfterTileBackgrounds.push_back(activeLayer);
            } else {
                highPriorityBackgrounds.push_back(activeLayer);
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
        const DecodedImage& image = *prepared.image;
        const int srcX = resolved.baseSrcX + std::clamp(subX, 0, prepared.backgroundScale - 1);
        const int srcY = resolved.baseSrcY + std::clamp(subY, 0, prepared.backgroundScale - 1);
        const uint32_t src = image.rgba[static_cast<size_t>(srcY) * static_cast<size_t>(image.width) + static_cast<size_t>(srcX)];
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

    const bool onlyWholeChrOverrides = std::all_of(activeOverrides.begin(), activeOverrides.end(), [](const ChrOverride* override) {
        return override != nullptr &&
               override->wholeChr() &&
               !override->hasChrHash &&
               override->paletteIndices.empty() &&
               override->patternTable < 0 &&
               !override->defaultTile &&
               !override->absoluteTile;
    });
    const PreparedOverride* fastBackgroundOverride = nullptr;
    if(onlyWholeChrOverrides) {
        for(const PreparedOverride& prepared : preparedOverrides) {
            const ChrOverride* override = prepared.override;
            if(override == nullptr) {
                continue;
            }
            if(!prepared.hasDynamicConditions &&
               (override->target == ChrOverride::Target::Both || override->target == ChrOverride::Target::Background) &&
               fastBackgroundOverride == nullptr) {
                fastBackgroundOverride = &prepared;
            }
        }
    }

    bool lastBackgroundOverrideCacheValid = false;
    int lastBackgroundOverrideOriginX = 0;
    int lastBackgroundOverrideOriginY = 0;
    int lastBackgroundOverrideFullTileIndex = -1;
    uint32_t lastBackgroundOverrideTileHash = 0;
    std::array<uint8_t, 3> lastBackgroundOverridePalette = {};
    const PreparedOverride* lastBackgroundOverride = nullptr;
    const int activeNesY0 = std::max(0, activeTop / scale);
    const int activeNesY1 = std::min(PPU::SCREEN_HEIGHT, (activeBottom + scale - 1) / scale);
    for(int nesY = activeNesY0; nesY < activeNesY1; ++nesY) {
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
        bool noSpriteDirectRowCacheValid = false;
        int noSpriteDirectRowCacheOriginX = 0;
        int noSpriteDirectRowCacheOriginY = 0;
        int noSpriteDirectRowCacheFullTileIndex = -1;
        uint32_t noSpriteDirectRowCacheTileHash = 0;
        const PreparedOverride* noSpriteDirectRowCacheOverride = nullptr;
        std::array<uint8_t, 3> noSpriteDirectRowCachePalette = {};
        std::array<std::array<uint32_t, 4>, 8> noSpriteDirectRowCacheBlocks = {};

        auto buildScanlineLayers = [&](const std::vector<ActiveBackgroundLayer>& activeLayers) {
            std::vector<ScanlineBackgroundLayer> scanlineLayers;
            scanlineLayers.reserve(activeLayers.size());
            for(const ActiveBackgroundLayer& activeLayer : activeLayers) {
                if(activeLayer.prepared == nullptr || activeLayer.prepared->image == nullptr) {
                    continue;
                }
                const PreparedBackground& prepared = *activeLayer.prepared;
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
            return scanlineLayers;
        };

        const std::vector<ScanlineBackgroundLayer> lowPriorityScanlineLayers = buildScanlineLayers(lowPriorityBackgrounds);
        const std::vector<ScanlineBackgroundLayer> midBeforeTileScanlineLayers = buildScanlineLayers(midBeforeTileBackgrounds);
        const std::vector<ScanlineBackgroundLayer> midAfterTileScanlineLayers = buildScanlineLayers(midAfterTileBackgrounds);
        const std::vector<ScanlineBackgroundLayer> highPriorityScanlineLayers = buildScanlineLayers(highPriorityBackgrounds);

        for(int nesX = 0; nesX < PPU::SCREEN_WIDTH; ++nesX) {
            const uint32_t baseColor = srcRow[nesX];
            const size_t pixelIndex = static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH + static_cast<size_t>(nesX);
            const PPU::DebugModBackgroundPixel* bgPixel =
                snapshot.backgroundPixelsView != nullptr
                    ? (pixelIndex < snapshot.backgroundPixelsViewCount ? &snapshot.backgroundPixelsView[pixelIndex] : nullptr)
                    : (pixelIndex < snapshot.backgroundPixels.size() ? &snapshot.backgroundPixels[pixelIndex] : nullptr);
            const PPU::DebugModSpritePixel* spritePixel =
                snapshot.spritePixelsView != nullptr
                    ? (pixelIndex < snapshot.spritePixelsViewCount ? &snapshot.spritePixelsView[pixelIndex] : nullptr)
                    : (pixelIndex < snapshot.spritePixels.size() ? &snapshot.spritePixels[pixelIndex] : nullptr);
            const bool hasSpriteCandidates = spritePixel != nullptr && spritePixel->count > 0;

            const PreparedOverride* backgroundOverride = nullptr;
            uint32_t backgroundFallbackColor = baseColor;
            std::array<uint8_t, 3> backgroundPalette = {};
            std::array<uint32_t, 4> backgroundMappedPalette = {};
            const DecodedImage* backgroundOverrideImage = nullptr;
            int backgroundOverrideTileSrcX = 0;
            int backgroundOverrideTileSrcY = 0;
            int backgroundOverrideSourceScale = 1;
            bool backgroundOverrideTileSrcValid = false;

            if(bgPixel != nullptr && bgPixel->valid) {
                const int bgFullTileIndex = bgPixel->tileIndex != 0xFFFF ? static_cast<int>(bgPixel->tileIndex) : -1;
                backgroundPalette = { bgPixel->palette[0], bgPixel->palette[1], bgPixel->palette[2] };
                if(bgFullTileIndex >= 0) {
                    const int tileOriginX = nesX - static_cast<int>(bgPixel->offsetX);
                    const int tileOriginY = nesY - static_cast<int>(bgPixel->offsetY);
                    if(lastBackgroundOverrideCacheValid &&
                        lastBackgroundOverrideOriginX == tileOriginX &&
                        lastBackgroundOverrideOriginY == tileOriginY &&
                        lastBackgroundOverrideFullTileIndex == bgFullTileIndex &&
                        lastBackgroundOverrideTileHash == bgPixel->tileHash &&
                        lastBackgroundOverridePalette == backgroundPalette) {
                        backgroundOverride = lastBackgroundOverride;
                    } else {
                        const ConditionContext context = { nesX, nesY, bgPixel, nullptr };
                        backgroundOverride =
                            onlyWholeChrOverrides && fastBackgroundOverride != nullptr
                                ? fastBackgroundOverride
                                : findOverride(ChrOverride::Target::Background, bgFullTileIndex & 0xFF, bgFullTileIndex, bgFullTileIndex / 256, backgroundPalette, false, false, false, context);
                        lastBackgroundOverrideCacheValid = true;
                        lastBackgroundOverrideOriginX = tileOriginX;
                        lastBackgroundOverrideOriginY = tileOriginY;
                        lastBackgroundOverrideFullTileIndex = bgFullTileIndex;
                        lastBackgroundOverrideTileHash = bgPixel->tileHash;
                        lastBackgroundOverridePalette = backgroundPalette;
                        lastBackgroundOverride = backgroundOverride;
                    }
                    if(backgroundOverride != nullptr) {
                        const ChrOverride* override = backgroundOverride->override;
                        backgroundOverrideImage = backgroundOverride->image;
                        if(override != nullptr && backgroundOverrideImage != nullptr) {
                            backgroundOverrideSourceScale = backgroundOverride->sourceScale;
                            if(override->hasSourcePosition()) {
                                backgroundOverrideTileSrcX = override->sourceX;
                                backgroundOverrideTileSrcY = override->sourceY;
                                backgroundOverrideTileSrcValid = true;
                            } else {
                                const int tilePixelSize = 8 * backgroundOverrideSourceScale;
                                const bool atlasImage =
                                    override->wholeChr() ||
                                    override->sourceTileOffset > 0 ||
                                    (backgroundOverrideImage->width >= override->columns * tilePixelSize &&
                                     backgroundOverrideImage->height > tilePixelSize);
                                if(atlasImage) {
                                    int sourceColumn = 0;
                                    int sourceRow = 0;
                                    if(override->wholeChr()) {
                                        const int sourceTile = bgFullTileIndex + override->sourceTileOffset;
                                        if(backgroundOverride->wholeChrLayout == ChrOverride::SourceLayout::PatternTables) {
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
                                    backgroundOverrideTileSrcX = sourceColumn * tilePixelSize;
                                    backgroundOverrideTileSrcY = sourceRow * tilePixelSize;
                                    backgroundOverrideTileSrcValid = true;
                                } else {
                                    backgroundOverrideTileSrcX = 0;
                                    backgroundOverrideTileSrcY = 0;
                                    backgroundOverrideTileSrcValid = true;
                                }
                            }
                        }
                    }
                }
                backgroundFallbackColor = m_disableOriginalTiles ? 0x00000000u : snapshot.paletteColors[bgPixel->paletteIndex & 0x3F];
                backgroundMappedPalette[0] = snapshot.paletteColors[snapshot.universalBgColor & 0x3F];
                backgroundMappedPalette[1] = snapshot.paletteColors[backgroundPalette[0] & 0x3F];
                backgroundMappedPalette[2] = snapshot.paletteColors[backgroundPalette[1] & 0x3F];
                backgroundMappedPalette[3] = snapshot.paletteColors[backgroundPalette[2] & 0x3F];
            }
            const bool backgroundOpaque = bgPixel != nullptr && bgPixel->valid && bgPixel->colorLowBits != 0;
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
                    if(prepared.backgroundScale == 1) {
                        const uint32_t src = layer.row0[static_cast<size_t>(baseSrcX)];
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
                    const int srcX1 = baseSrcX + std::clamp(1, 0, layer.maxSub);
                    const uint32_t src00 = layer.row0[static_cast<size_t>(srcX0)];
                    const uint32_t src01 = layer.row0[static_cast<size_t>(srcX1)];
                    const uint32_t src10 = layer.row1[static_cast<size_t>(srcX0)];
                    const uint32_t src11 = layer.row1[static_cast<size_t>(srcX1)];
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

                auto sampleBackgroundOverrideScale2 = [&](uint32_t color, int subX, int subY) {
                    if(backgroundOverride == nullptr || bgPixel == nullptr || !backgroundOverrideTileSrcValid || backgroundOverrideImage == nullptr) {
                        return color;
                    }
                    const ChrOverride* override = backgroundOverride->override;
                    if(override == nullptr) {
                        return color;
                    }
                    const int localOffsetX = bgPixel->offsetX & 0x07;
                    const int localOffsetY = bgPixel->offsetY & 0x07;
                    const int sourceSubX = std::clamp((subX * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                    const int sourceSubY = std::clamp((subY * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                    const int srcX = backgroundOverrideTileSrcX + localOffsetX * backgroundOverrideSourceScale + sourceSubX;
                    const int srcY = backgroundOverrideTileSrcY + localOffsetY * backgroundOverrideSourceScale + sourceSubY;
                    if(srcX < 0 || srcY < 0 || srcX >= backgroundOverrideImage->width || srcY >= backgroundOverrideImage->height) {
                        return color;
                    }
                    const size_t sourcePixelIndex =
                        static_cast<size_t>(srcY) * static_cast<size_t>(backgroundOverrideImage->width) + static_cast<size_t>(srcX);
                    const uint32_t sourcePixel = backgroundOverrideImage->rgba[sourcePixelIndex];
                    const uint8_t sourceAlpha = static_cast<uint8_t>((sourcePixel >> 24u) & 0xFFu);
                    if(override->ignorePalette) {
                        if(sourceAlpha == 0) {
                            return color;
                        }
                        const uint32_t opaqueSource = (sourcePixel & 0x00FFFFFFu) | 0xFF000000u;
                        return sourceAlpha == 0xFF ? opaqueSource : blendPixel(color, opaqueSource, sourceAlpha);
                    }
                    if(sourceAlpha == 0 || !backgroundOverrideImage->indexedFourColor || backgroundOverrideImage->indexedPixels.size() != backgroundOverrideImage->rgba.size()) {
                        return color;
                    }
                    const uint8_t sourcePaletteIndex = backgroundOverrideImage->indexedPixels[sourcePixelIndex];
                    const uint32_t mappedColor =
                        ((sourcePaletteIndex == 0 ? backgroundMappedPalette[0] : backgroundMappedPalette[static_cast<size_t>(sourcePaletteIndex)]) & 0x00FFFFFFu) |
                        (static_cast<uint32_t>(sourceAlpha) << 24u);
                    return sourceAlpha == 0xFF ? mappedColor : blendPixel(color, mappedColor, 255);
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

                    const ChrOverride* override = backgroundOverride->override;
                    if(override == nullptr) {
                        bgOverrideRowCacheValid = false;
                        return false;
                    }

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
                            uint32_t mappedColor = 0;
                            if(srcX < 0 || srcY < 0 || srcX >= backgroundOverrideImage->width || srcY >= backgroundOverrideImage->height) {
                                opaqueBlock = false;
                                mappedColor = 0;
                            } else {
                                const size_t sourcePixelIndex =
                                    static_cast<size_t>(srcY) * static_cast<size_t>(backgroundOverrideImage->width) + static_cast<size_t>(srcX);
                                const uint32_t sourcePixel = backgroundOverrideImage->rgba[sourcePixelIndex];
                                const uint8_t sourceAlpha = static_cast<uint8_t>((sourcePixel >> 24u) & 0xFFu);
                                if(override->ignorePalette) {
                                    mappedColor = sourceAlpha == 0 ? 0u : ((sourcePixel & 0x00FFFFFFu) | (static_cast<uint32_t>(sourceAlpha) << 24u));
                                } else if(!backgroundOverrideImage->indexedFourColor || backgroundOverrideImage->indexedPixels.size() != backgroundOverrideImage->rgba.size()) {
                                    mappedColor = 0;
                                } else {
                                    const uint8_t sourcePaletteIndex = backgroundOverrideImage->indexedPixels[sourcePixelIndex];
                                    const uint32_t paletteColor =
                                        sourcePaletteIndex == 0
                                            ? backgroundMappedPalette[0]
                                            : backgroundMappedPalette[static_cast<size_t>(sourcePaletteIndex)];
                                    mappedColor = (paletteColor & 0x00FFFFFFu) | (static_cast<uint32_t>(sourceAlpha) << 24u);
                                }
                                if(((mappedColor >> 24u) & 0xFFu) != 0xFFu) {
                                    opaqueBlock = false;
                                }
                            }
                            block[static_cast<size_t>(sampleIndex)] = mappedColor;
                        }
                        bgOverrideRowCacheOpaqueBlocks[static_cast<size_t>(tileOffsetX)] = opaqueBlock;
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
                    const bool hasAnyRowLayers =
                        !lowPriorityScanlineLayers.empty() ||
                        !midBeforeTileScanlineLayers.empty() ||
                        !midAfterTileScanlineLayers.empty() ||
                        !highPriorityScanlineLayers.empty();
                    auto applyLayerListToRow = [&](const std::vector<ScanlineBackgroundLayer>& layers) {
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
                            if(prepared.backgroundScale == 1) {
                                for(int tileOffsetX = startOffset; tileOffsetX <= endOffset; ++tileOffsetX) {
                                    auto& block = noSpriteDirectRowCacheBlocks[static_cast<size_t>(tileOffsetX)];
                                    const int srcX = layer.srcBaseX + tileOriginX + tileOffsetX;
                                    const uint32_t src = layer.row0[static_cast<size_t>(srcX)];
                                    if(prepared.opaqueCopy && ((src >> 24u) & 0xFFu) == 0xFFu) {
                                        block[0] = src;
                                        block[1] = src;
                                        block[2] = src;
                                        block[3] = src;
                                    } else {
                                        block[0] = blendPixel(block[0], src, prepared.alphaScale);
                                        block[1] = blendPixel(block[1], src, prepared.alphaScale);
                                        block[2] = blendPixel(block[2], src, prepared.alphaScale);
                                        block[3] = blendPixel(block[3], src, prepared.alphaScale);
                                    }
                                }
                                continue;
                            }

                            for(int tileOffsetX = startOffset; tileOffsetX <= endOffset; ++tileOffsetX) {
                                auto& block = noSpriteDirectRowCacheBlocks[static_cast<size_t>(tileOffsetX)];
                                const int srcX0 = (layer.srcBaseX + tileOriginX + tileOffsetX) * prepared.backgroundScale;
                                const int srcX1 = srcX0 + std::clamp(1, 0, layer.maxSub);
                                const uint32_t src00 = layer.row0[static_cast<size_t>(srcX0)];
                                const uint32_t src01 = layer.row0[static_cast<size_t>(srcX1)];
                                const uint32_t src10 = layer.row1[static_cast<size_t>(srcX0)];
                                const uint32_t src11 = layer.row1[static_cast<size_t>(srcX1)];
                                if(prepared.opaqueCopy &&
                                    ((src00 >> 24u) & 0xFFu) == 0xFFu &&
                                    ((src01 >> 24u) & 0xFFu) == 0xFFu &&
                                    ((src10 >> 24u) & 0xFFu) == 0xFFu &&
                                    ((src11 >> 24u) & 0xFFu) == 0xFFu) {
                                    block[0] = src00;
                                    block[1] = src01;
                                    block[2] = src10;
                                    block[3] = src11;
                                } else {
                                    block[0] = blendPixel(block[0], src00, prepared.alphaScale);
                                    block[1] = blendPixel(block[1], src01, prepared.alphaScale);
                                    block[2] = blendPixel(block[2], src10, prepared.alphaScale);
                                    block[3] = blendPixel(block[3], src11, prepared.alphaScale);
                                }
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
                        const uint32_t base = m_disableOriginalTiles ? 0x00000000u : srcRow[pixelX];
                        block = {base, base, base, base};
                    }

                    if(hasAnyRowLayers) {
                        applyLayerListToRow(lowPriorityScanlineLayers);
                        applyLayerListToRow(midBeforeTileScanlineLayers);
                    }

                    for(int tileOffsetX = 0; tileOffsetX < 8; ++tileOffsetX) {
                        const int pixelX = tileOriginX + tileOffsetX;
                        if(pixelX < 0 || pixelX >= PPU::SCREEN_WIDTH) {
                            continue;
                        }
                        auto& block = noSpriteDirectRowCacheBlocks[static_cast<size_t>(tileOffsetX)];
                        if(backgroundOverride != nullptr) {
                            if(hasRowOverrideCache) {
                                const auto& overrideBlock = bgOverrideRowCacheBlocks[static_cast<size_t>(tileOffsetX)];
                                const auto applyMapped = [&](uint32_t base, uint32_t mapped) {
                                    const uint8_t alpha = static_cast<uint8_t>((mapped >> 24u) & 0xFFu);
                                    if(alpha == 0) {
                                        return base;
                                    }
                                    return alpha == 0xFF ? mapped : blendPixel(base, mapped, 255);
                                };
                                block[0] = applyMapped(block[0], overrideBlock[0]);
                                block[1] = applyMapped(block[1], overrideBlock[1]);
                                block[2] = applyMapped(block[2], overrideBlock[2]);
                                block[3] = applyMapped(block[3], overrideBlock[3]);
                            } else {
                                const auto sampleBackgroundOverrideScale2AtOffsetX = [&](uint32_t color, int localOffsetX, int subX, int subY) {
                                    if(backgroundOverride == nullptr || !backgroundOverrideTileSrcValid || backgroundOverrideImage == nullptr) {
                                        return color;
                                    }
                                    const ChrOverride* override = backgroundOverride->override;
                                    if(override == nullptr) {
                                        return color;
                                    }
                                    const int localOffsetY = bgPixel->offsetY & 0x07;
                                    const int sourceSubX = std::clamp((subX * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                                    const int sourceSubY = std::clamp((subY * backgroundOverrideSourceScale) / 2, 0, backgroundOverrideSourceScale - 1);
                                    const int srcX = backgroundOverrideTileSrcX + localOffsetX * backgroundOverrideSourceScale + sourceSubX;
                                    const int srcY = backgroundOverrideTileSrcY + localOffsetY * backgroundOverrideSourceScale + sourceSubY;
                                    if(srcX < 0 || srcY < 0 || srcX >= backgroundOverrideImage->width || srcY >= backgroundOverrideImage->height) {
                                        return color;
                                    }
                                    const size_t sourcePixelIndex =
                                        static_cast<size_t>(srcY) * static_cast<size_t>(backgroundOverrideImage->width) + static_cast<size_t>(srcX);
                                    const uint32_t sourcePixel = backgroundOverrideImage->rgba[sourcePixelIndex];
                                    const uint8_t sourceAlpha = static_cast<uint8_t>((sourcePixel >> 24u) & 0xFFu);
                                    if(override->ignorePalette) {
                                        if(sourceAlpha == 0) {
                                            return color;
                                        }
                                        const uint32_t opaqueSource = (sourcePixel & 0x00FFFFFFu) | 0xFF000000u;
                                        return sourceAlpha == 0xFF ? opaqueSource : blendPixel(color, opaqueSource, sourceAlpha);
                                    }
                                    if(sourceAlpha == 0 || !backgroundOverrideImage->indexedFourColor || backgroundOverrideImage->indexedPixels.size() != backgroundOverrideImage->rgba.size()) {
                                        return color;
                                    }
                                    const uint8_t sourcePaletteIndex = backgroundOverrideImage->indexedPixels[sourcePixelIndex];
                                    const uint32_t mappedColor =
                                        ((sourcePaletteIndex == 0 ? backgroundMappedPalette[0] : backgroundMappedPalette[static_cast<size_t>(sourcePaletteIndex)]) & 0x00FFFFFFu) |
                                        (static_cast<uint32_t>(sourceAlpha) << 24u);
                                    return sourceAlpha == 0xFF ? mappedColor : blendPixel(color, mappedColor, 255);
                                };
                                block[0] = sampleBackgroundOverrideScale2AtOffsetX(block[0], tileOffsetX, 0, 0);
                                block[1] = sampleBackgroundOverrideScale2AtOffsetX(block[1], tileOffsetX, 1, 0);
                                block[2] = sampleBackgroundOverrideScale2AtOffsetX(block[2], tileOffsetX, 0, 1);
                                block[3] = sampleBackgroundOverrideScale2AtOffsetX(block[3], tileOffsetX, 1, 1);
                            }
                        } else if(backgroundOpaque || !m_disableOriginalTiles) {
                            block = {backgroundFallbackColor, backgroundFallbackColor, backgroundFallbackColor, backgroundFallbackColor};
                        }
                    }

                    if(hasAnyRowLayers) {
                        applyLayerListToRow(midAfterTileScanlineLayers);
                        applyLayerListToRow(highPriorityScanlineLayers);
                    }
                    return true;
                };

                uint32_t block00 = m_disableOriginalTiles ? 0x00000000u : baseColor;
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
                    for(const ScanlineBackgroundLayer& layer : lowPriorityScanlineLayers) {
                        applyScanlineLayerScale2(layer, nesX, block00, block01, block10, block11);
                    }
                    for(const ScanlineBackgroundLayer& layer : midBeforeTileScanlineLayers) {
                        applyScanlineLayerScale2(layer, nesX, block00, block01, block10, block11);
                    }

                    if(bgPixel != nullptr && bgPixel->valid) {
                        if(backgroundOverride != nullptr) {
                            if(ensureBackgroundOverrideRowCache()) {
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
                        } else if(backgroundOpaque || !m_disableOriginalTiles) {
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

            std::array<ResolvedBackgroundLayer, 10> resolvedLowPriorityBackgrounds = {};
            size_t resolvedLowPriorityBackgroundCount = 0;
            for(const ScanlineBackgroundLayer& activeLayer : lowPriorityScanlineLayers) {
                if(resolvedLowPriorityBackgroundCount >= resolvedLowPriorityBackgrounds.size()) {
                    break;
                }
                resolvedLowPriorityBackgrounds[resolvedLowPriorityBackgroundCount++] = resolveBackgroundLayer(activeLayer, nesX);
            }

            std::array<ResolvedBackgroundLayer, 10> resolvedMidBeforeTileBackgrounds = {};
            size_t resolvedMidBeforeTileBackgroundCount = 0;
            for(const ScanlineBackgroundLayer& activeLayer : midBeforeTileScanlineLayers) {
                if(resolvedMidBeforeTileBackgroundCount >= resolvedMidBeforeTileBackgrounds.size()) {
                    break;
                }
                resolvedMidBeforeTileBackgrounds[resolvedMidBeforeTileBackgroundCount++] = resolveBackgroundLayer(activeLayer, nesX);
            }

            std::array<ResolvedBackgroundLayer, 10> resolvedMidAfterTileBackgrounds = {};
            size_t resolvedMidAfterTileBackgroundCount = 0;
            for(const ScanlineBackgroundLayer& activeLayer : midAfterTileScanlineLayers) {
                if(resolvedMidAfterTileBackgroundCount >= resolvedMidAfterTileBackgrounds.size()) {
                    break;
                }
                resolvedMidAfterTileBackgrounds[resolvedMidAfterTileBackgroundCount++] = resolveBackgroundLayer(activeLayer, nesX);
            }

            std::array<ResolvedBackgroundLayer, 10> resolvedHighPriorityBackgrounds = {};
            size_t resolvedHighPriorityBackgroundCount = 0;
            for(const ScanlineBackgroundLayer& activeLayer : highPriorityScanlineLayers) {
                if(resolvedHighPriorityBackgroundCount >= resolvedHighPriorityBackgrounds.size()) {
                    break;
                }
                resolvedHighPriorityBackgrounds[resolvedHighPriorityBackgroundCount++] = resolveBackgroundLayer(activeLayer, nesX);
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
            const uint32_t initialColor = m_disableOriginalTiles ? 0x00000000u : baseColor;
            const bool canPrecomposeBeforeBg = lowUniform && midBeforeUniform;
            const uint32_t precomposedBeforeBgColor =
                canPrecomposeBeforeBg
                    ? composeUniformBackgroundLayers(
                        composeUniformBackgroundLayers(initialColor, resolvedLowPriorityBackgrounds, resolvedLowPriorityBackgroundCount),
                        resolvedMidBeforeTileBackgrounds,
                        resolvedMidBeforeTileBackgroundCount)
                    : initialColor;
            const bool bgValid = bgPixel != nullptr && bgPixel->valid;
            const bool applyBackgroundFallback = bgValid && (backgroundOpaque || !m_disableOriginalTiles);
            const bool hasMidAfterBackgrounds = resolvedMidAfterTileBackgroundCount > 0;
            const bool hasHighPriorityBackgrounds = resolvedHighPriorityBackgroundCount > 0;
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
                    if(backgroundOverride == nullptr || bgPixel == nullptr || !backgroundOverrideTileSrcValid || backgroundOverrideImage == nullptr) {
                        return color;
                    }

                    const ChrOverride* override = backgroundOverride->override;
                    const DecodedImage* image = backgroundOverrideImage;
                    if(override == nullptr) {
                        return color;
                    }

                    const int localOffsetX = bgPixel->offsetX & 0x07;
                    const int localOffsetY = bgPixel->offsetY & 0x07;
                    const int sourceScale = backgroundOverrideSourceScale;
                    const int sourceSubX = std::clamp((subX * sourceScale) / 2, 0, sourceScale - 1);
                    const int sourceSubY = std::clamp((subY * sourceScale) / 2, 0, sourceScale - 1);
                    const int srcX = backgroundOverrideTileSrcX + localOffsetX * sourceScale + sourceSubX;
                    const int srcY = backgroundOverrideTileSrcY + localOffsetY * sourceScale + sourceSubY;

                    if(srcX < 0 || srcY < 0 || srcX >= image->width || srcY >= image->height) {
                        return color;
                    }

                    const size_t sourcePixelIndex =
                        static_cast<size_t>(srcY) * static_cast<size_t>(image->width) + static_cast<size_t>(srcX);
                    const uint32_t sourcePixel = image->rgba[sourcePixelIndex];
                    const uint8_t sourceAlpha = static_cast<uint8_t>((sourcePixel >> 24u) & 0xFFu);
                    if(override->ignorePalette) {
                        if(sourceAlpha == 0) {
                            return color;
                        }
                        const uint32_t opaqueSource = (sourcePixel & 0x00FFFFFFu) | 0xFF000000u;
                        return sourceAlpha == 0xFF ? opaqueSource : blendPixel(color, opaqueSource, sourceAlpha);
                    }

                    if(sourceAlpha == 0) {
                        return color;
                    }
                    if(!image->indexedFourColor || image->indexedPixels.size() != image->rgba.size()) {
                        return color;
                    }
                    const uint8_t sourcePaletteIndex = image->indexedPixels[sourcePixelIndex];
                    if(sourcePaletteIndex == 0) {
                        const uint32_t mappedColor =
                            (backgroundMappedPalette[0] & 0x00FFFFFFu) |
                            (static_cast<uint32_t>(sourceAlpha) << 24u);
                        if(sourceAlpha == 0xFF) {
                            return mappedColor;
                        }
                        return blendPixel(color, mappedColor, 255);
                    }

                    const int paletteIndex = static_cast<int>(sourcePaletteIndex) - 1;
                    if(paletteIndex < 0 || paletteIndex >= static_cast<int>(backgroundPalette.size())) {
                        return color;
                    }
                    const uint32_t mappedColor =
                        (backgroundMappedPalette[static_cast<size_t>(paletteIndex + 1)] & 0x00FFFFFFu) |
                        (static_cast<uint32_t>(sourceAlpha) << 24u);
                    if(sourceAlpha == 0xFF) {
                        return mappedColor;
                    }
                    return blendPixel(color, mappedColor, 255);
                };

                auto applyResolvedLayerScale2 = [&](const ResolvedBackgroundLayer& resolved,
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

                    const DecodedImage& image = *prepared.image;
                    const int maxSub = prepared.backgroundScale - 1;
                    const int srcX0 = resolved.baseSrcX;
                    const int srcX1 = resolved.baseSrcX + std::clamp(1, 0, maxSub);
                    const int srcY0 = resolved.baseSrcY;
                    const int srcY1 = resolved.baseSrcY + std::clamp(1, 0, maxSub);
                    const size_t row0 = static_cast<size_t>(srcY0) * static_cast<size_t>(image.width);
                    const size_t row1 = static_cast<size_t>(srcY1) * static_cast<size_t>(image.width);
                    const uint32_t src00 = image.rgba[row0 + static_cast<size_t>(srcX0)];
                    const uint32_t src01 = image.rgba[row0 + static_cast<size_t>(srcX1)];
                    const uint32_t src10 = image.rgba[row1 + static_cast<size_t>(srcX0)];
                    const uint32_t src11 = image.rgba[row1 + static_cast<size_t>(srcX1)];
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

                auto getOpaqueBackgroundOverrideColorScale2 = [&](int subX, int subY, uint32_t& outColor) {
                    if(backgroundOverride == nullptr || bgPixel == nullptr || !backgroundOverrideTileSrcValid || backgroundOverrideImage == nullptr) {
                        return false;
                    }

                    const ChrOverride* override = backgroundOverride->override;
                    const DecodedImage* image = backgroundOverrideImage;
                    if(override == nullptr) {
                        return false;
                    }

                    const int localOffsetX = bgPixel->offsetX & 0x07;
                    const int localOffsetY = bgPixel->offsetY & 0x07;
                    const int sourceScale = backgroundOverrideSourceScale;
                    const int sourceSubX = std::clamp((subX * sourceScale) / 2, 0, sourceScale - 1);
                    const int sourceSubY = std::clamp((subY * sourceScale) / 2, 0, sourceScale - 1);
                    const int srcX = backgroundOverrideTileSrcX + localOffsetX * sourceScale + sourceSubX;
                    const int srcY = backgroundOverrideTileSrcY + localOffsetY * sourceScale + sourceSubY;

                    if(srcX < 0 || srcY < 0 || srcX >= image->width || srcY >= image->height) {
                        return false;
                    }

                    const size_t sourcePixelIndex =
                        static_cast<size_t>(srcY) * static_cast<size_t>(image->width) + static_cast<size_t>(srcX);
                    const uint32_t sourcePixel = image->rgba[sourcePixelIndex];
                    const uint8_t sourceAlpha = static_cast<uint8_t>((sourcePixel >> 24u) & 0xFFu);
                    if(sourceAlpha != 0xFF) {
                        return false;
                    }

                    if(override->ignorePalette) {
                        outColor = (sourcePixel & 0x00FFFFFFu) | 0xFF000000u;
                        return true;
                    }
                    if(!image->indexedFourColor || image->indexedPixels.size() != image->rgba.size()) {
                        return false;
                    }

                    const uint8_t sourcePaletteIndex = image->indexedPixels[sourcePixelIndex];
                    if(sourcePaletteIndex == 0) {
                        outColor = backgroundMappedPalette[0];
                        return true;
                    }

                    const int paletteIndex = static_cast<int>(sourcePaletteIndex) - 1;
                    if(paletteIndex < 0 || paletteIndex >= static_cast<int>(backgroundPalette.size())) {
                        return false;
                    }
                    outColor = backgroundMappedPalette[static_cast<size_t>(paletteIndex + 1)];
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

                    const ChrOverride* override = backgroundOverride->override;
                    if(override == nullptr) {
                        bgOverrideRowCacheValid = false;
                        return false;
                    }

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
                            uint32_t mappedColor = 0;
                            if(srcX < 0 || srcY < 0 || srcX >= backgroundOverrideImage->width || srcY >= backgroundOverrideImage->height) {
                                opaqueBlock = false;
                                mappedColor = 0;
                            } else {
                                const size_t sourcePixelIndex =
                                    static_cast<size_t>(srcY) * static_cast<size_t>(backgroundOverrideImage->width) + static_cast<size_t>(srcX);
                                const uint32_t sourcePixel = backgroundOverrideImage->rgba[sourcePixelIndex];
                                const uint8_t sourceAlpha = static_cast<uint8_t>((sourcePixel >> 24u) & 0xFFu);
                                if(override->ignorePalette) {
                                    mappedColor = sourceAlpha == 0 ? 0u : ((sourcePixel & 0x00FFFFFFu) | (static_cast<uint32_t>(sourceAlpha) << 24u));
                                } else if(!backgroundOverrideImage->indexedFourColor || backgroundOverrideImage->indexedPixels.size() != backgroundOverrideImage->rgba.size()) {
                                    mappedColor = 0;
                                } else {
                                    const uint8_t sourcePaletteIndex = backgroundOverrideImage->indexedPixels[sourcePixelIndex];
                                    const uint32_t paletteColor =
                                        sourcePaletteIndex == 0
                                            ? backgroundMappedPalette[0]
                                            : backgroundMappedPalette[static_cast<size_t>(sourcePaletteIndex)];
                                    mappedColor = (paletteColor & 0x00FFFFFFu) | (static_cast<uint32_t>(sourceAlpha) << 24u);
                                }
                                if(((mappedColor >> 24u) & 0xFFu) != 0xFFu) {
                                    opaqueBlock = false;
                                }
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
                        applyResolvedLayerScale2(resolvedLowPriorityBackgrounds[i], block00, block01, block10, block11);
                    }
                    for(size_t i = 0; i < resolvedMidBeforeTileBackgroundCount; ++i) {
                        applyResolvedLayerScale2(resolvedMidBeforeTileBackgrounds[i], block00, block01, block10, block11);
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
                            const auto applyMapped = [&](uint32_t base, uint32_t mapped) {
                                const uint8_t alpha = static_cast<uint8_t>((mapped >> 24u) & 0xFFu);
                                if(alpha == 0) {
                                    return base;
                                }
                                return alpha == 0xFF ? mapped : blendPixel(base, mapped, 255);
                            };
                            block00 = applyMapped(block00, src00);
                            block01 = applyMapped(block01, src01);
                            block10 = applyMapped(block10, src10);
                            block11 = applyMapped(block11, src11);
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
                        applyResolvedLayerScale2(resolvedMidAfterTileBackgrounds[i], block00, block01, block10, block11);
                    }
                }


                if(hasHighPriorityBackgrounds) {
                    for(size_t i = 0; i < resolvedHighPriorityBackgroundCount; ++i) {
                        applyResolvedLayerScale2(resolvedHighPriorityBackgrounds[i], block00, block01, block10, block11);
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

            auto applyResolvedLayersToBlock = [&](const auto& resolvedLayers, size_t count) {
                if(count == 0) {
                    return;
                }
                for(size_t i = 0; i < count; ++i) {
                    for(int subY = subYStart; subY < subYEnd; ++subY) {
                        uint32_t* blockRow = baseBlockColors.data() + static_cast<size_t>(subY) * 8u;
                        for(int subX = 0; subX < blockWidth; ++subX) {
                            blockRow[static_cast<size_t>(subX)] =
                                sampleResolvedBackgroundPixel(blockRow[static_cast<size_t>(subX)], resolvedLayers[i], subX, subY);
                        }
                    }
                }
            };

            auto applyBackgroundOverrideToBlock = [&]() {
                if(!bgValid) {
                    return;
                }
                if(backgroundOverride != nullptr) {
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
                applyResolvedLayersToBlock(resolvedMidBeforeTileBackgrounds, resolvedMidBeforeTileBackgroundCount);
            }
            applyBackgroundOverrideToBlock();
            if(hasMidAfterBackgrounds) {
                applyResolvedLayersToBlock(resolvedMidAfterTileBackgrounds, resolvedMidAfterTileBackgroundCount);
            }

            if(!hasAnyValidSpriteCandidate) {
                if(hasHighPriorityBackgrounds) {
                    for(int subY = subYStart; subY < subYEnd; ++subY) {
                        uint32_t* blockRow = baseBlockColors.data() + static_cast<size_t>(subY) * 8u;
                        for(int subX = 0; subX < scale; ++subX) {
                            uint32_t color = blockRow[static_cast<size_t>(subX)];
                            for(size_t i = 0; i < resolvedHighPriorityBackgroundCount; ++i) {
                                color = sampleResolvedBackgroundPixel(color, resolvedHighPriorityBackgrounds[i], subX, subY);
                            }
                            blockRow[static_cast<size_t>(subX)] = color;
                        }
                    }
                }

                for(int subY = subYStart; subY < subYEnd; ++subY) {
                    std::memcpy(
                        dstRows[static_cast<size_t>(subY)],
                        baseBlockColors.data() + static_cast<size_t>(subY) * 8u,
                        static_cast<size_t>(blockWidth) * sizeof(uint32_t));
                }
                continue;
            }

            struct ResolvedSpriteCandidate {
                const PPU::DebugModSpriteCandidate* candidate = nullptr;
                const PreparedOverride* spriteOverride = nullptr;
                uint32_t fixedFallbackColor = 0;
                std::array<uint8_t, 3> spritePalette = {};
                bool fallbackUsesCurrentColor = true;
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

            int lowestBgSpriteCandidate = std::numeric_limits<int>::max();
            for(int i = 0; i < static_cast<int>(spritePixel->count); ++i) {
                const PPU::DebugModSpriteCandidate& candidate = spritePixel->candidates[static_cast<size_t>(i)];
                if(candidate.valid && candidate.behindBackground) {
                    lowestBgSpriteCandidate = std::min(lowestBgSpriteCandidate, i);
                }
            }

            for(int i = static_cast<int>(spritePixel->count) - 1; i >= 0; --i) {
                const PPU::DebugModSpriteCandidate& candidate = spritePixel->candidates[static_cast<size_t>(i)];
                if(!candidate.valid) {
                    continue;
                }

                ResolvedSpriteCandidate resolvedCandidate;
                resolvedCandidate.candidate = &candidate;
                resolvedCandidate.spritePalette = { candidate.palette[0], candidate.palette[1], candidate.palette[2] };
                resolvedCandidate.fallbackUsesCurrentColor = candidate.colorLowBits == 0 || m_disableOriginalTiles;
                if(!resolvedCandidate.fallbackUsesCurrentColor) {
                    resolvedCandidate.fixedFallbackColor = spriteFallbackColorFor(candidate);
                }

                const int spriteFullTileIndex = candidate.tileIndex != 0xFFFF ? static_cast<int>(candidate.tileIndex) : -1;
                if(spriteFullTileIndex >= 0) {
                    const ConditionContext context = { nesX, nesY, bgPixel, &candidate };
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
                            context);
                }

                if(candidate.behindBackground) {
                    if(resolvedBehindSpriteCandidateCount < resolvedBehindSpriteCandidates.size()) {
                        resolvedBehindSpriteCandidates[resolvedBehindSpriteCandidateCount++] = resolvedCandidate;
                    }
                } else if(i < lowestBgSpriteCandidate) {
                    if(resolvedFrontSpriteCandidateCount < resolvedFrontSpriteCandidates.size()) {
                        resolvedFrontSpriteCandidates[resolvedFrontSpriteCandidateCount++] = resolvedCandidate;
                    }
                }
            }
            const bool applyBehindSprites = !backgroundOpaque && resolvedBehindSpriteCandidateCount > 0;
            const bool applyFrontSprites = resolvedFrontSpriteCandidateCount > 0;

            auto applySpriteLayersToBlock = [&]() {
                const bool canUseScale2SpriteBlockPath =
                    scale == 2 &&
                    blockWidth == 2 &&
                    subYStart == 0 &&
                    subYEnd == 2;
                if(canUseScale2SpriteBlockPath) {
                    uint32_t block00 = baseBlockColors[0];
                    uint32_t block01 = baseBlockColors[1];
                    uint32_t block10 = baseBlockColors[8];
                    uint32_t block11 = baseBlockColors[9];
                    const auto applyResolvedSpriteListToBlock = [&](const std::array<ResolvedSpriteCandidate, 8>& candidates, size_t count) {
                        for(size_t i = 0; i < count; ++i) {
                            const ResolvedSpriteCandidate& resolvedCandidate = candidates[i];
                            const PPU::DebugModSpriteCandidate& candidate = *resolvedCandidate.candidate;
                            uint32_t spriteFallback00 = resolvedCandidate.fallbackUsesCurrentColor ? block00 : resolvedCandidate.fixedFallbackColor;
                            uint32_t spriteFallback01 = resolvedCandidate.fallbackUsesCurrentColor ? block01 : resolvedCandidate.fixedFallbackColor;
                            uint32_t spriteFallback10 = resolvedCandidate.fallbackUsesCurrentColor ? block10 : resolvedCandidate.fixedFallbackColor;
                            uint32_t spriteFallback11 = resolvedCandidate.fallbackUsesCurrentColor ? block11 : resolvedCandidate.fixedFallbackColor;
                            if(resolvedCandidate.spriteOverride != nullptr) {
                                block00 = sampleOverridePixel(
                                    block00, spriteFallback00, resolvedCandidate.spriteOverride, candidate.tileIndex,
                                    candidate.offsetX, candidate.offsetY, 0, 0, candidate.colorLowBits,
                                    resolvedCandidate.spritePalette, candidate.horizontalMirror, candidate.verticalMirror, true);
                                block01 = sampleOverridePixel(
                                    block01, spriteFallback01, resolvedCandidate.spriteOverride, candidate.tileIndex,
                                    candidate.offsetX, candidate.offsetY, 1, 0, candidate.colorLowBits,
                                    resolvedCandidate.spritePalette, candidate.horizontalMirror, candidate.verticalMirror, true);
                                block10 = sampleOverridePixel(
                                    block10, spriteFallback10, resolvedCandidate.spriteOverride, candidate.tileIndex,
                                    candidate.offsetX, candidate.offsetY, 0, 1, candidate.colorLowBits,
                                    resolvedCandidate.spritePalette, candidate.horizontalMirror, candidate.verticalMirror, true);
                                block11 = sampleOverridePixel(
                                    block11, spriteFallback11, resolvedCandidate.spriteOverride, candidate.tileIndex,
                                    candidate.offsetX, candidate.offsetY, 1, 1, candidate.colorLowBits,
                                    resolvedCandidate.spritePalette, candidate.horizontalMirror, candidate.verticalMirror, true);
                            } else if(!m_disableOriginalTiles && candidate.colorLowBits != 0) {
                                block00 = spriteFallback00;
                                block01 = spriteFallback01;
                                block10 = spriteFallback10;
                                block11 = spriteFallback11;
                            }
                        }
                    };

                    if(applyBehindSprites) {
                        applyResolvedSpriteListToBlock(resolvedBehindSpriteCandidates, resolvedBehindSpriteCandidateCount);
                    }
                    if(applyFrontSprites) {
                        applyResolvedSpriteListToBlock(resolvedFrontSpriteCandidates, resolvedFrontSpriteCandidateCount);
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
                        if(applyBehindSprites) {
                            for(size_t i = 0; i < resolvedBehindSpriteCandidateCount; ++i) {
                                const ResolvedSpriteCandidate& resolvedCandidate = resolvedBehindSpriteCandidates[i];
                                const PPU::DebugModSpriteCandidate& candidate = *resolvedCandidate.candidate;
                                const uint32_t spriteFallbackColor =
                                    resolvedCandidate.fallbackUsesCurrentColor ? color : resolvedCandidate.fixedFallbackColor;
                                if(resolvedCandidate.spriteOverride != nullptr) {
                                    color = sampleOverridePixel(
                                        color,
                                        spriteFallbackColor,
                                        resolvedCandidate.spriteOverride,
                                        candidate.tileIndex,
                                        candidate.offsetX,
                                        candidate.offsetY,
                                        subX,
                                        subY,
                                        candidate.colorLowBits,
                                        resolvedCandidate.spritePalette,
                                        candidate.horizontalMirror,
                                        candidate.verticalMirror,
                                        true
                                    );
                                } else if(!m_disableOriginalTiles && candidate.colorLowBits != 0) {
                                    color = spriteFallbackColor;
                                }
                            }
                        }

                        if(applyFrontSprites) {
                            for(size_t i = 0; i < resolvedFrontSpriteCandidateCount; ++i) {
                                const ResolvedSpriteCandidate& resolvedCandidate = resolvedFrontSpriteCandidates[i];
                                const PPU::DebugModSpriteCandidate& candidate = *resolvedCandidate.candidate;
                                const uint32_t spriteFallbackColor =
                                    resolvedCandidate.fallbackUsesCurrentColor ? color : resolvedCandidate.fixedFallbackColor;
                                if(resolvedCandidate.spriteOverride != nullptr) {
                                    color = sampleOverridePixel(
                                        color,
                                        spriteFallbackColor,
                                        resolvedCandidate.spriteOverride,
                                        candidate.tileIndex,
                                        candidate.offsetX,
                                        candidate.offsetY,
                                        subX,
                                        subY,
                                        candidate.colorLowBits,
                                        resolvedCandidate.spritePalette,
                                        candidate.horizontalMirror,
                                        candidate.verticalMirror,
                                        true
                                    );
                                } else if(!m_disableOriginalTiles && candidate.colorLowBits != 0) {
                                    color = spriteFallbackColor;
                                }
                            }
                        }
                        blockRow[static_cast<size_t>(subX)] = color;
                    }
                }
            };

            auto applyHighPriorityLayersToBlock = [&]() {
                const bool canUseScale2HighBlockPath =
                    scale == 2 &&
                    blockWidth == 2 &&
                    subYStart == 0 &&
                    subYEnd == 2;
                if(canUseScale2HighBlockPath) {
                    uint32_t block00 = baseBlockColors[0];
                    uint32_t block01 = baseBlockColors[1];
                    uint32_t block10 = baseBlockColors[8];
                    uint32_t block11 = baseBlockColors[9];
                    for(size_t i = 0; i < resolvedHighPriorityBackgroundCount; ++i) {
                        const ResolvedBackgroundLayer& resolved = resolvedHighPriorityBackgrounds[i];
                        if(!resolved.valid || resolved.prepared == nullptr) {
                            continue;
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
                            continue;
                        }
                        const DecodedImage& image = *prepared.image;
                        const int maxSub = prepared.backgroundScale - 1;
                        const int srcX0 = resolved.baseSrcX;
                        const int srcX1 = resolved.baseSrcX + std::clamp(1, 0, maxSub);
                        const int srcY0 = resolved.baseSrcY;
                        const int srcY1 = resolved.baseSrcY + std::clamp(1, 0, maxSub);
                        const size_t row0 = static_cast<size_t>(srcY0) * static_cast<size_t>(image.width);
                        const size_t row1 = static_cast<size_t>(srcY1) * static_cast<size_t>(image.width);
                        const uint32_t src00 = image.rgba[row0 + static_cast<size_t>(srcX0)];
                        const uint32_t src01 = image.rgba[row0 + static_cast<size_t>(srcX1)];
                        const uint32_t src10 = image.rgba[row1 + static_cast<size_t>(srcX0)];
                        const uint32_t src11 = image.rgba[row1 + static_cast<size_t>(srcX1)];
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
                        for(size_t i = 0; i < resolvedHighPriorityBackgroundCount; ++i) {
                            color = sampleResolvedBackgroundPixel(color, resolvedHighPriorityBackgrounds[i], subX, subY);
                        }
                        blockRow[static_cast<size_t>(subX)] = color;
                    }
                }
            };

            auto writeBlockRows = [&]() {
                for(int subY = subYStart; subY < subYEnd; ++subY) {
                    std::memcpy(
                        dstRows[static_cast<size_t>(subY)],
                        baseBlockColors.data() + static_cast<size_t>(subY) * 8u,
                        static_cast<size_t>(blockWidth) * sizeof(uint32_t));
                }
            };

            if(!hasHighPriorityBackgrounds) {
                applySpriteLayersToBlock();
                writeBlockRows();
                continue;
            }

            applySpriteLayersToBlock();
            applyHighPriorityLayersToBlock();

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
