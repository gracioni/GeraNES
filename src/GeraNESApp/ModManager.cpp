#include "GeraNESApp/ModManager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>

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

sol::object tableObject(const sol::table& source, const char* key)
{
    return source[key].get<sol::object>();
}

std::string tableString(const sol::table& source, const char* key, const std::string& fallback)
{
    sol::object value = tableObject(source, key);
    return value.is<std::string>() ? value.as<std::string>() : fallback;
}

int tableInt(const sol::table& source, const char* key, int fallback)
{
    sol::object value = tableObject(source, key);
    return value.is<int>() ? value.as<int>() : fallback;
}

int tableIntIndex(const sol::table& source, int key, int fallback)
{
    sol::object value = source[key].get<sol::object>();
    return value.is<int>() ? value.as<int>() : fallback;
}

bool tableBool(const sol::table& source, const char* key, bool fallback)
{
    sol::object value = tableObject(source, key);
    return value.is<bool>() ? value.as<bool>() : fallback;
}

float tableFloat(const sol::table& source, const char* key, float fallback)
{
    sol::object value = tableObject(source, key);
    return value.is<float>() ? value.as<float>() : (value.is<double>() ? static_cast<float>(value.as<double>()) : fallback);
}

ModManager::ChrOverride::Target parseChrTarget(std::string value, ModManager::ChrOverride::Target fallback)
{
    value = toLower(value);
    if(value == "background" || value == "bg") {
        return ModManager::ChrOverride::Target::Background;
    }
    if(value == "sprite" || value == "sprites" || value == "spr") {
        return ModManager::ChrOverride::Target::Sprite;
    }
    if(value == "both" || value == "all") {
        return ModManager::ChrOverride::Target::Both;
    }
    return fallback;
}

ModManager::ChrOverride::SourceLayout parseChrSourceLayout(std::string value, ModManager::ChrOverride::SourceLayout fallback)
{
    value = toLower(value);
    if(value == "auto" || value.empty()) {
        return ModManager::ChrOverride::SourceLayout::Auto;
    }
    if(value == "linear" || value == "atlas" || value == "columns") {
        return ModManager::ChrOverride::SourceLayout::Linear;
    }
    if(value == "pattern_tables" || value == "pattern-tables" || value == "pattern_tables_16x16" ||
       value == "nes_chr" || value == "ppu_chr" || value == "ppu_viewer") {
        return ModManager::ChrOverride::SourceLayout::PatternTables;
    }
    return fallback;
}
}

void ModManager::clear()
{
    m_originalRomPath.clear();
    m_effectiveRomPath.clear();
    m_modPath.clear();
    m_active = false;
    m_scriptLoaded = false;
    m_resolutionMultiplier = 1;
    m_customPalette.reset();
    m_chrOverrides.clear();
    m_backgroundReplacements.clear();
    m_imageCache.clear();
    m_lua = sol::state();
}

ModManager::LoadRequest ModManager::prepareRomLoad(const std::filesystem::path& romPath, bool useModIfAvailable)
{
    clear();

    LoadRequest request;
    request.romPath = romPath;
    request.effectiveRomPath = romPath;

    if(!useModIfAvailable) {
        request.message = "Mod loading disabled.";
        return request;
    }

    const std::filesystem::path modPath = findModPath(romPath);
    if(modPath.empty()) {
        request.message = "No mod file found.";
        return request;
    }

    m_originalRomPath = romPath;
    m_effectiveRomPath = romPath;
    m_modPath = modPath;
    m_active = true;
    request.modPath = modPath;
    request.modLoaded = true;

    const auto ipsData = readZipEntry(modPath, "rom.ips");
    if(!ipsData.has_value()) {
        request.message = "Mod loaded.";
        return request;
    }

    RomFile baseRom;
    if(!baseRom.open(romPath.string()) || !baseRom.error().empty()) {
        request.message = "Mod found, but base ROM could not be read for rom.ips.";
        Logger::instance().log(request.message, Logger::Type::ERROR);
        return request;
    }

    std::string patchError;
    auto patchedRom = applyIpsPatch(baseRom.dataBytes(), *ipsData, patchError);
    if(!patchedRom.has_value()) {
        request.message = "Mod found, but rom.ips failed: " + patchError;
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
    request.message = "Mod loaded with rom.ips.";
    m_effectiveRomPath = patchedPath;
    return request;
}

bool ModManager::loadScriptForCurrentMod()
{
    if(!m_active || m_modPath.empty()) return false;
    m_scriptLoaded = false;
    m_customPalette.reset();
    m_chrOverrides.clear();
    m_backgroundReplacements.clear();
    m_imageCache.clear();
    const auto script = readZipEntry(m_modPath, "script.lua");
    if(!script.has_value()) {
        Logger::instance().log("Mod loaded without script.lua.", Logger::Type::INFO);
        return true;
    }

    m_lua = sol::state();
    m_lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
    bindApi(nullptr);

    const std::string scriptText(reinterpret_cast<const char*>(script->data()), script->size());
    sol::protected_function_result result = m_lua.safe_script(scriptText, sol::script_pass_on_error);
    if(!result.valid()) {
        sol::error err = result;
        Logger::instance().log(std::string("Mod script.lua error: ") + err.what(), Logger::Type::ERROR);
        return false;
    }

    std::unordered_map<std::string, bool> chrAssetValidity;
    for(ChrOverride& override : m_chrOverrides) {
        if(!override.enabled || override.assetPath.empty()) {
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
                    Logger::Type::ERROR);
            }
            it = chrAssetValidity.emplace(normalizedPath, valid).first;
        }
        if(!it->second) {
            override.enabled = false;
        }
    }

    m_scriptLoaded = true;
    Logger::instance().log("Mod script.lua loaded.", Logger::Type::INFO);
    return true;
}

void ModManager::onFrame(GeraNESEmu& emu)
{
    if(!m_active || !m_scriptLoaded) return;
    emu.getConsole().ppu().debugSetModRenderCaptureEnabled(true);
    if(m_customPalette.has_value()) {
        emu.getConsole().ppu().setColorPalette(*m_customPalette);
    }
    bindApi(&emu);
    sol::object callback = m_lua["on_frame"];
    if(callback.get_type() != sol::type::function) return;

    sol::protected_function onFrame = callback;
    sol::protected_function_result result = onFrame();
    if(!result.valid()) {
        sol::error err = result;
        Logger::instance().log(std::string("Mod on_frame error: ") + err.what(), Logger::Type::ERROR);
    }
}

std::optional<std::vector<uint8_t>> ModManager::readAsset(const std::string& assetPath) const
{
    if(!m_active) return std::nullopt;
    return readZipEntry(m_modPath, normalizeZipPath(assetPath));
}

std::filesystem::path ModManager::findModPath(const std::filesystem::path& romPath)
{
    if(romPath.empty()) return {};
    std::filesystem::path modPath = romPath;
    modPath.replace_extension(".mod");
    std::error_code ec;
    if(std::filesystem::exists(modPath, ec) && std::filesystem::is_regular_file(modPath, ec)) {
        return modPath;
    }
    return {};
}

std::string ModManager::normalizeZipPath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    while(!path.empty() && path.front() == '/') {
        path.erase(path.begin());
    }
    return path;
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

void ModManager::bindApi(GeraNESEmu* emu)
{
    sol::table api = m_lua["mod"].get_or_create<sol::table>();
    api.set_function("set_resolution_multiplier", [this](int multiplier) {
        m_resolutionMultiplier = std::clamp(multiplier, 1, 8);
    });
    api.set_function("set_palette", [this](const std::string& assetPath) {
        const std::string normalizedPath = normalizeZipPath(assetPath);
        const auto data = readAsset(normalizedPath);
        if(!data.has_value()) {
            Logger::instance().log("Failed to load mod palette asset: " + normalizedPath, Logger::Type::ERROR);
            return;
        }
        if(data->size() < 64u * 3u) {
            Logger::instance().log("Invalid mod palette asset: " + normalizedPath, Logger::Type::ERROR);
            return;
        }

        std::array<uint32_t, 64> palette = {};
        for(size_t i = 0; i < palette.size(); ++i) {
            const size_t offset = i * 3u;
            const uint32_t r = (*data)[offset + 0u];
            const uint32_t g = (*data)[offset + 1u];
            const uint32_t b = (*data)[offset + 2u];
            palette[i] = 0xFF000000u | r | (g << 8) | (b << 16);
        }
        m_customPalette = palette;
    });
    api.set_function("add_chr_override", [this](int tile, const std::string& assetPath, sol::optional<bool> ignorePalette) {
        ChrOverride override;
        override.tile = std::max(0, tile);
        override.assetPath = normalizeZipPath(assetPath);
        override.ignorePalette = ignorePalette.value_or(false);
        m_chrOverrides.push_back(std::move(override));
    });
    api.set_function("add_chr", [this](const sol::table& table) {
        m_chrOverrides.push_back(parseChrOverrideTable(table));
    });
    api.set_function("add_chr_sheet", [this](const std::string& assetPath, sol::optional<sol::table> options) {
        ChrOverride override;
        override.tile = -1;
        override.assetPath = normalizeZipPath(assetPath);
        if(options) {
            ChrOverride parsed = parseChrOverrideTable(*options);
            parsed.tile = -1;
            if(parsed.assetPath.empty()) parsed.assetPath = override.assetPath;
            override = std::move(parsed);
        }
        m_chrOverrides.push_back(std::move(override));
    });
    api.set_function("clear_chr_overrides", [this]() {
        m_chrOverrides.clear();
    });
    api.set_function(
        "set_background_replacement",
        [this](const std::string& id, const std::string& assetPath, sol::optional<int> x, sol::optional<int> y, sol::optional<int> width, sol::optional<int> height) {
            BackgroundReplacement replacement;
            replacement.id = id;
            replacement.assetPath = normalizeZipPath(assetPath);
            replacement.x = x.value_or(0);
            replacement.y = y.value_or(0);
            replacement.width = width.value_or(256);
            replacement.height = height.value_or(240);
            auto it = std::find_if(m_backgroundReplacements.begin(), m_backgroundReplacements.end(), [&](const BackgroundReplacement& item) {
                return item.id == replacement.id;
            });
            if(it == m_backgroundReplacements.end()) {
                m_backgroundReplacements.push_back(std::move(replacement));
            } else {
                *it = std::move(replacement);
            }
        });
    api.set_function("add_background_layer", [this](const std::string& id, const sol::table& table) {
        BackgroundReplacement replacement = parseBackgroundReplacementTable(id, table);
        auto it = std::find_if(m_backgroundReplacements.begin(), m_backgroundReplacements.end(), [&](const BackgroundReplacement& item) {
            return item.id == replacement.id;
        });
        if(it == m_backgroundReplacements.end()) {
            m_backgroundReplacements.push_back(std::move(replacement));
        } else {
            *it = std::move(replacement);
        }
    });
    api.set_function("clear_background_layers", [this]() {
        m_backgroundReplacements.clear();
    });
    api.set_function("set_layer_enabled", [this](const std::string& id, bool enabled) {
        auto it = std::find_if(m_backgroundReplacements.begin(), m_backgroundReplacements.end(), [&](const BackgroundReplacement& item) {
            return item.id == id;
        });
        if(it != m_backgroundReplacements.end()) {
            it->enabled = enabled;
        }
    });
    api.set_function("asset_exists", [this](const std::string& assetPath) {
        return zipHasEntry(m_modPath, normalizeZipPath(assetPath));
    });
    api.set_function("read_memory", [this, emu](const std::string& type, uint32_t address) {
        return static_cast<int>(readMemory(emu, type, address));
    });
    api.set_function("read_cpu", [this, emu](uint32_t address) {
        return static_cast<int>(readMemory(emu, "cpu", address));
    });
    api.set_function("read_ppu", [this, emu](uint32_t address) {
        return static_cast<int>(readMemory(emu, "ppu", address));
    });
    api.set_function("read_oam", [this, emu](uint32_t address) {
        return static_cast<int>(readMemory(emu, "oam", address));
    });
    api.set_function("chr_tile_hash", [emu](int tile, sol::optional<int> patternTable) {
        if(emu == nullptr) return 0;
        PPU& ppu = emu->getConsole().ppu();
        const int table = std::clamp(patternTable.value_or(tile >= 256 ? 1 : ppu.debugBackgroundPatternTableAddress() / 0x1000), 0, 1);
        const int tileInTable = std::clamp(tile & 0xFF, 0, 255);
        return static_cast<int>(hashChrTile(ppu, table * 0x1000 + tileInTable * 16));
    });
}

uint8_t ModManager::readMemory(GeraNESEmu* emu, const std::string& type, uint32_t address) const
{
    if(emu == nullptr) return 0;
    const std::string normalizedType = toLower(type);
    if(normalizedType == "cpu") {
        return emu->debugPeekCpuMemory(static_cast<uint16_t>(address));
    }
    if(normalizedType == "ppu") {
        return emu->getConsole().ppu().debugPeekPpuMemory(static_cast<uint16_t>(address));
    }
    if(normalizedType == "oam" || normalizedType == "primary_oam") {
        return emu->getConsole().ppu().debugPeekPrimaryOam(static_cast<uint8_t>(address));
    }
    if(normalizedType == "secondary_oam") {
        return emu->getConsole().ppu().debugPeekSecondaryOam(static_cast<uint8_t>(address));
    }
    return 0;
}

uint32_t ModManager::readCameraValue(const BackgroundReplacement::CameraSource& source, GeraNESEmu& emu) const
{
    if(!source.enabled) return 0;
    uint32_t low = readMemory(&emu, source.memoryType, source.address);
    if(source.word) {
        const uint32_t high = readMemory(&emu, source.memoryType, source.address + 1u);
        low |= high << 8u;
    }
    const int scale = std::max(1, source.scale);
    return low / static_cast<uint32_t>(scale);
}

uint32_t ModManager::readMemoryValue(const MemoryCondition& source, GeraNESEmu& emu) const
{
    uint32_t low = readMemory(&emu, source.memoryType, source.address);
    if(source.word) {
        const uint32_t high = readMemory(&emu, source.memoryType, source.address + 1u);
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
    uint32_t actual = readMemoryValue(condition, emu);
    uint32_t expected = condition.value;
    if(condition.hasMask) {
        actual &= condition.mask;
        expected &= condition.mask;
    }

    const std::string op = toLower(condition.op);
    if(op == "in" || op == "any_of" || op == "anyof") {
        for(uint32_t value : condition.values) {
            if(condition.hasMask) value &= condition.mask;
            if(actual == value) return true;
        }
        return false;
    }
    if(op == "!=" || op == "~=" || op == "not_equal" || op == "not_equals") return actual != expected;
    if(op == ">" || op == "greater_than" || op == "greater") return actual > expected;
    if(op == ">=" || op == "greater_or_equal" || op == "greater_equals") return actual >= expected;
    if(op == "<" || op == "less_than" || op == "less") return actual < expected;
    if(op == "<=" || op == "less_or_equal" || op == "less_equals") return actual <= expected;
    if(op == "bit_set" || op == "bits_set") return (actual & expected) == expected;
    if(op == "bit_clear" || op == "bits_clear") return (actual & expected) == 0;
    return actual == expected;
}

std::optional<ModManager::MemoryCondition> ModManager::parseMemoryConditionObject(const sol::object& object)
{
    if(object.get_type() != sol::type::table) return std::nullopt;
    sol::table conditionTable = object.as<sol::table>();
    MemoryCondition parsed;
    parsed.memoryType = tableString(conditionTable, "type", tableString(conditionTable, "memory", parsed.memoryType));
    parsed.address = static_cast<uint32_t>(tableInt(conditionTable, "address", tableIntIndex(conditionTable, 1, 0)));
    parsed.word = tableBool(conditionTable, "word", parsed.word);
    parsed.scale = tableInt(conditionTable, "scale", parsed.scale);
    parsed.op = tableString(conditionTable, "op", tableString(conditionTable, "operator", parsed.op));

    auto readValueField = [&](const char* key, uint32_t& target) {
        sol::object value = tableObject(conditionTable, key);
        if(value.is<int>()) {
            target = static_cast<uint32_t>(value.as<int>());
            return true;
        }
        return false;
    };
    if(!readValueField("value", parsed.value)) {
        parsed.value = static_cast<uint32_t>(tableIntIndex(conditionTable, 2, static_cast<int>(parsed.value)));
    }
    if(readValueField("equals", parsed.value) || readValueField("equal", parsed.value)) {
        parsed.op = "==";
    } else if(readValueField("not_equals", parsed.value) || readValueField("notEqual", parsed.value)) {
        parsed.op = "!=";
    } else if(readValueField("greater_than", parsed.value) || readValueField("greaterThan", parsed.value)) {
        parsed.op = ">";
    } else if(readValueField("greater_or_equal", parsed.value) || readValueField("greaterOrEqual", parsed.value)) {
        parsed.op = ">=";
    } else if(readValueField("less_than", parsed.value) || readValueField("lessThan", parsed.value)) {
        parsed.op = "<";
    } else if(readValueField("less_or_equal", parsed.value) || readValueField("lessOrEqual", parsed.value)) {
        parsed.op = "<=";
    } else if(readValueField("bit_set", parsed.value) || readValueField("bitsSet", parsed.value)) {
        parsed.op = "bit_set";
    } else if(readValueField("bit_clear", parsed.value) || readValueField("bitsClear", parsed.value)) {
        parsed.op = "bit_clear";
    }

    sol::object mask = tableObject(conditionTable, "mask");
    if(mask.is<int>()) {
        parsed.hasMask = true;
        parsed.mask = static_cast<uint32_t>(mask.as<int>());
    }

    sol::object values = tableObject(conditionTable, "values");
    if(values.get_type() == sol::type::none) values = tableObject(conditionTable, "any_of");
    if(values.get_type() == sol::type::none) values = tableObject(conditionTable, "anyOf");
    if(values.get_type() == sol::type::table) {
        sol::table valueTable = values.as<sol::table>();
        for(const auto& item : valueTable) {
            if(item.second.is<int>()) {
                parsed.values.push_back(static_cast<uint32_t>(item.second.as<int>()));
            }
        }
        parsed.op = "in";
    }

    return parsed;
}

ModManager::ChrOverride ModManager::parseChrOverrideTable(const sol::table& table)
{
    ChrOverride override;
    sol::object tileObject = tableObject(table, "tile");
    if(tileObject.get_type() == sol::type::none) tileObject = tableObject(table, "tile_index");
    if(tileObject.get_type() == sol::type::none) tileObject = tableObject(table, "tileIndex");
    if(tileObject.is<int>()) {
        override.tile = std::max(0, tileObject.as<int>());
    }

    override.assetPath = normalizeZipPath(tableString(table, "asset", tableString(table, "assetPath", override.assetPath)));
    override.ignorePalette = tableBool(table, "ignore_palette", tableBool(table, "ignorePalette", override.ignorePalette));
    override.enabled = tableBool(table, "enabled", override.enabled);
    override.priority = tableInt(table, "priority", override.priority);
    sol::object target = tableObject(table, "target");
    if(target.get_type() == sol::type::none) target = tableObject(table, "layer");
    if(target.get_type() == sol::type::none) target = tableObject(table, "type");
    if(target.is<std::string>()) {
        override.target = parseChrTarget(target.as<std::string>(), override.target);
    }
    override.patternTable = tableInt(table, "pattern_table", tableInt(table, "patternTable", override.patternTable));
    override.patternTable = std::clamp(override.patternTable, -1, 1);
    override.columns = std::max(1, tableInt(table, "columns", override.columns));
    sol::object layout = tableObject(table, "layout");
    if(layout.get_type() == sol::type::none) layout = tableObject(table, "source_layout");
    if(layout.get_type() == sol::type::none) layout = tableObject(table, "sourceLayout");
    if(layout.is<std::string>()) {
        override.sourceLayout = parseChrSourceLayout(layout.as<std::string>(), override.sourceLayout);
    }
    if(tableBool(table, "pattern_tables", tableBool(table, "patternTables", false))) {
        override.sourceLayout = ChrOverride::SourceLayout::PatternTables;
    }
    override.sourceX = tableInt(table, "source_x", tableInt(table, "sourceX", override.sourceX));
    override.sourceY = tableInt(table, "source_y", tableInt(table, "sourceY", override.sourceY));
    override.sourceTileOffset = std::max(0, tableInt(table, "source_tile_offset", tableInt(table, "sourceTileOffset", override.sourceTileOffset)));
    override.exactPaletteOrder = tableBool(table, "exact_palette_order", tableBool(table, "exactPaletteOrder", override.exactPaletteOrder));
    override.absoluteTile = tableBool(table, "absolute_tile", tableBool(table, "absoluteTile", override.absoluteTile));

    sol::object chrHash = tableObject(table, "chr_hash");
    if(chrHash.get_type() == sol::type::none) chrHash = tableObject(table, "chrHash");
    if(chrHash.get_type() == sol::type::none) chrHash = tableObject(table, "hash");
    if(chrHash.is<int>()) {
        override.hasChrHash = true;
        override.chrHash = static_cast<uint32_t>(chrHash.as<int>());
    }
    override.defaultTile = tableBool(table, "default_tile", tableBool(table, "defaultTile", override.defaultTile));

    auto addPaletteIndex = [&](int value) {
        const uint8_t index = static_cast<uint8_t>(std::clamp(value, 0, 63));
        if(std::find(override.paletteIndices.begin(), override.paletteIndices.end(), index) == override.paletteIndices.end()) {
            override.paletteIndices.push_back(index);
        }
    };
    auto addPaletteIndicesFromObject = [&](const sol::object& object) {
        if(object.is<int>()) {
            addPaletteIndex(object.as<int>());
            return;
        }
        if(object.get_type() != sol::type::table) return;
        sol::table indices = object.as<sol::table>();
        for(const auto& item : indices) {
            if(item.second.is<int>()) {
                addPaletteIndex(item.second.as<int>());
            }
        }
    };
    addPaletteIndicesFromObject(tableObject(table, "palette_indices"));
    addPaletteIndicesFromObject(tableObject(table, "paletteIndices"));
    addPaletteIndicesFromObject(tableObject(table, "palette"));

    auto addConditionFromObject = [&](const sol::object& object) {
        if(auto condition = parseMemoryConditionObject(object)) {
            override.conditions.push_back(*condition);
        }
    };
    addConditionFromObject(tableObject(table, "condition"));
    addConditionFromObject(tableObject(table, "enabled_when"));
    addConditionFromObject(tableObject(table, "enabledWhen"));
    sol::object conditions = tableObject(table, "conditions");
    if(conditions.get_type() == sol::type::table) {
        sol::table conditionList = conditions.as<sol::table>();
        for(const auto& item : conditionList) {
            addConditionFromObject(item.second);
        }
    }

    return override;
}

bool ModManager::patternMatches(const BackgroundReplacement& replacement, GeraNESEmu& emu) const
{
    PPU& ppu = emu.getConsole().ppu();
    for(const auto& sample : replacement.detectPattern) {
        if(sample.x < 0 || sample.y < 0 || sample.x >= PPU::SCREEN_WIDTH || sample.y >= PPU::SCREEN_HEIGHT) {
            return false;
        }
        if(readBackgroundPaletteIndexAt(ppu, sample.x, sample.y) != sample.paletteIndex) {
            return false;
        }
    }
    return true;
}

bool ModManager::replacementPixelAllowed(const BackgroundReplacement& replacement, uint32_t pixel, uint8_t paletteIndex) const
{
    if(!replacement.replaceOnlyPaletteIndices.empty()) {
        return std::find(
            replacement.replaceOnlyPaletteIndices.begin(),
            replacement.replaceOnlyPaletteIndices.end(),
            paletteIndex
        ) != replacement.replaceOnlyPaletteIndices.end();
    }

    if(replacement.replaceOnlyColors.empty()) {
        return !replacement.preserveUnmatchedPixels;
    }

    const int r = static_cast<int>(pixel & 0xFFu);
    const int g = static_cast<int>((pixel >> 8) & 0xFFu);
    const int b = static_cast<int>((pixel >> 16) & 0xFFu);
    for(const auto& key : replacement.replaceOnlyColors) {
        if(std::abs(r - static_cast<int>(key.r)) <= key.tolerance &&
           std::abs(g - static_cast<int>(key.g)) <= key.tolerance &&
           std::abs(b - static_cast<int>(key.b)) <= key.tolerance) {
            return true;
        }
    }
    return false;
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
        cacheIt = m_imageCache.emplace(normalizedPath, std::move(decoded)).first;
        if(!cacheIt->second.has_value()) {
            Logger::instance().log("Failed to load mod image asset: " + normalizedPath, Logger::Type::ERROR);
        }
    }
    return cacheIt->second.has_value() ? &*cacheIt->second : nullptr;
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

ModManager::BackgroundReplacement ModManager::parseBackgroundReplacementTable(const std::string& id, const sol::table& table)
{
    auto getInt = [](const sol::table& source, const char* key, int fallback) {
        sol::object value = source[key].get<sol::object>();
        return value.is<int>() ? value.as<int>() : fallback;
    };
    auto getFloat = [](const sol::table& source, const char* key, float fallback) {
        sol::object value = source[key].get<sol::object>();
        return value.is<float>() || value.is<double>() || value.is<int>() ? value.as<float>() : fallback;
    };
    auto getBool = [](const sol::table& source, const char* key, bool fallback) {
        sol::object value = source[key].get<sol::object>();
        return value.is<bool>() ? value.as<bool>() : fallback;
    };
    auto getString = [](const sol::table& source, const char* key, const std::string& fallback) {
        sol::object value = source[key].get<sol::object>();
        return value.is<std::string>() ? value.as<std::string>() : fallback;
    };
    auto getIntIndex = [](const sol::table& source, int key, int fallback) {
        sol::object value = source[key].get<sol::object>();
        return value.is<int>() ? value.as<int>() : fallback;
    };

    BackgroundReplacement replacement;
    replacement.id = id;
    replacement.assetPath = normalizeZipPath(getString(table, "asset", getString(table, "assetPath", "")));
    replacement.x = getInt(table, "x", replacement.x);
    replacement.y = getInt(table, "y", replacement.y);
    replacement.width = getInt(table, "width", replacement.width);
    replacement.height = getInt(table, "height", replacement.height);
    replacement.sourceX = getInt(table, "source_x", getInt(table, "sourceX", replacement.sourceX));
    replacement.sourceY = getInt(table, "source_y", getInt(table, "sourceY", replacement.sourceY));
    replacement.opacity = getFloat(table, "opacity", replacement.opacity);
    replacement.parallaxX = getFloat(table, "parallax_x", getFloat(table, "parallaxX", replacement.parallaxX));
    replacement.parallaxY = getFloat(table, "parallax_y", getFloat(table, "parallaxY", replacement.parallaxY));
    replacement.scrollX = getInt(table, "scroll_x", getInt(table, "scrollX", replacement.scrollX));
    replacement.scrollY = getInt(table, "scroll_y", getInt(table, "scrollY", replacement.scrollY));
    replacement.repeatX = getBool(table, "repeat_x", getBool(table, "repeatX", replacement.repeatX));
    replacement.repeatY = getBool(table, "repeat_y", getBool(table, "repeatY", replacement.repeatY));
    replacement.enabled = getBool(table, "enabled", replacement.enabled);
    replacement.priority = getInt(table, "priority", replacement.priority);
    replacement.preserveUnmatchedPixels = getBool(table, "preserve_unmatched_pixels", getBool(table, "preserveUnmatchedPixels", replacement.preserveUnmatchedPixels));

    auto parseCamera = [&](const sol::object& object) {
        BackgroundReplacement::CameraSource source;
        if(object.get_type() != sol::type::table) return source;
        sol::table table = object.as<sol::table>();
        source.enabled = true;
        source.memoryType = getString(table, "type", getString(table, "memory", source.memoryType));
        source.address = static_cast<uint32_t>(getInt(table, "address", 0));
        source.word = getBool(table, "word", source.word);
        source.scale = getInt(table, "scale", source.scale);
        return source;
    };
    replacement.cameraX = parseCamera(table["camera_x"].get<sol::object>());
    if(!replacement.cameraX.enabled) replacement.cameraX = parseCamera(table["cameraX"].get<sol::object>());
    replacement.cameraY = parseCamera(table["camera_y"].get<sol::object>());
    if(!replacement.cameraY.enabled) replacement.cameraY = parseCamera(table["cameraY"].get<sol::object>());

    auto addConditionFromObject = [&](const sol::object& object) {
        if(auto condition = parseMemoryConditionObject(object)) {
            replacement.conditions.push_back(*condition);
        }
    };

    addConditionFromObject(table["condition"].get<sol::object>());
    addConditionFromObject(table["enabled_when"].get<sol::object>());
    addConditionFromObject(table["enabledWhen"].get<sol::object>());
    sol::object conditions = table["conditions"].get<sol::object>();
    if(conditions.get_type() == sol::type::table) {
        sol::table conditionList = conditions.as<sol::table>();
        for(const auto& item : conditionList) {
            addConditionFromObject(item.second);
        }
    }

    auto parseColor = [&](const sol::table& colorTable, int defaultTolerance) {
        BackgroundReplacement::ColorKey key;
        key.r = static_cast<uint8_t>(std::clamp(getIntIndex(colorTable, 1, getInt(colorTable, "r", 0)), 0, 255));
        key.g = static_cast<uint8_t>(std::clamp(getIntIndex(colorTable, 2, getInt(colorTable, "g", 0)), 0, 255));
        key.b = static_cast<uint8_t>(std::clamp(getIntIndex(colorTable, 3, getInt(colorTable, "b", 0)), 0, 255));
        key.tolerance = getInt(colorTable, "tolerance", defaultTolerance);
        return key;
    };
    auto addPaletteIndex = [&](int value) {
        const uint8_t index = static_cast<uint8_t>(std::clamp(value, 0, 63));
        if(std::find(replacement.replaceOnlyPaletteIndices.begin(), replacement.replaceOnlyPaletteIndices.end(), index) == replacement.replaceOnlyPaletteIndices.end()) {
            replacement.replaceOnlyPaletteIndices.push_back(index);
        }
    };
    auto addPaletteIndicesFromObject = [&](const sol::object& object) {
        if(object.is<int>()) {
            addPaletteIndex(object.as<int>());
            return;
        }
        if(object.get_type() != sol::type::table) return;

        sol::table indices = object.as<sol::table>();
        for(const auto& item : indices) {
            if(item.second.is<int>()) {
                addPaletteIndex(item.second.as<int>());
            }
        }
    };

    sol::object replaceColors = table["replace_only_colors"].get<sol::object>();
    if(replaceColors.get_type() == sol::type::none) replaceColors = table["replaceOnlyColors"].get<sol::object>();
    if(replaceColors.get_type() == sol::type::table) {
        sol::table colors = replaceColors.as<sol::table>();
        for(const auto& item : colors) {
            if(item.second.get_type() == sol::type::table) {
                replacement.replaceOnlyColors.push_back(parseColor(item.second.as<sol::table>(), 0));
            }
        }
    }

    sol::object replacePaletteIndices = table["replace_palette_indices"].get<sol::object>();
    if(replacePaletteIndices.get_type() == sol::type::none) replacePaletteIndices = table["replacePaletteIndices"].get<sol::object>();
    if(replacePaletteIndices.get_type() == sol::type::none) replacePaletteIndices = table["palette_indices"].get<sol::object>();
    if(replacePaletteIndices.get_type() == sol::type::none) replacePaletteIndices = table["paletteIndices"].get<sol::object>();
    addPaletteIndicesFromObject(replacePaletteIndices);

    sol::object patternObject = table["detect_pattern"].get<sol::object>();
    if(patternObject.get_type() == sol::type::none) patternObject = table["detectPattern"].get<sol::object>();
    if(patternObject.get_type() == sol::type::table) {
        sol::table pattern = patternObject.as<sol::table>();
        addPaletteIndicesFromObject(pattern["palette_index"].get<sol::object>());
        addPaletteIndicesFromObject(pattern["paletteIndex"].get<sol::object>());
        addPaletteIndicesFromObject(pattern["palette_indices"].get<sol::object>());
        addPaletteIndicesFromObject(pattern["paletteIndices"].get<sol::object>());
        addPaletteIndicesFromObject(pattern["nes_color"].get<sol::object>());
        addPaletteIndicesFromObject(pattern["nesColor"].get<sol::object>());
        addPaletteIndicesFromObject(pattern["nes_colors"].get<sol::object>());
        addPaletteIndicesFromObject(pattern["nesColors"].get<sol::object>());

        for(const auto& item : pattern) {
            if(item.second.is<int>()) {
                addPaletteIndex(item.second.as<int>());
                continue;
            }
            if(item.second.get_type() != sol::type::table) continue;
            sol::table sampleTable = item.second.as<sol::table>();
            const bool hasX = sampleTable["x"].get<sol::object>().get_type() != sol::type::none || sampleTable[1].get<sol::object>().get_type() != sol::type::none;
            const bool hasY = sampleTable["y"].get<sol::object>().get_type() != sol::type::none || sampleTable[2].get<sol::object>().get_type() != sol::type::none;
            if(!hasX || !hasY) {
                addPaletteIndicesFromObject(sampleTable["palette_index"].get<sol::object>());
                addPaletteIndicesFromObject(sampleTable["paletteIndex"].get<sol::object>());
                addPaletteIndicesFromObject(sampleTable["palette_indices"].get<sol::object>());
                addPaletteIndicesFromObject(sampleTable["paletteIndices"].get<sol::object>());
                addPaletteIndicesFromObject(sampleTable["nes_color"].get<sol::object>());
                addPaletteIndicesFromObject(sampleTable["nesColor"].get<sol::object>());
                addPaletteIndicesFromObject(sampleTable["nes_colors"].get<sol::object>());
                addPaletteIndicesFromObject(sampleTable["nesColors"].get<sol::object>());
                continue;
            }
            BackgroundReplacement::PatternPixel sample;
            sample.x = getInt(sampleTable, "x", getIntIndex(sampleTable, 1, 0));
            sample.y = getInt(sampleTable, "y", getIntIndex(sampleTable, 2, 0));
            sample.paletteIndex = static_cast<uint8_t>(std::clamp(
                getInt(sampleTable, "palette_index",
                    getInt(sampleTable, "paletteIndex",
                        getInt(sampleTable, "nes_color",
                            getInt(sampleTable, "nesColor", getIntIndex(sampleTable, 3, 0))))),
                0,
                63
            ));
            replacement.detectPattern.push_back(sample);
        }
    }

    return replacement;
}

void ModManager::composeChrFrame(std::vector<uint32_t>& framebuffer, int width, int height, int activeTop, int activeBottom, int scale, const uint32_t* sourceFramebuffer, const ChrRenderSnapshot& snapshot, const std::vector<const ChrOverride*>* activeOverrideFilter)
{
    if(sourceFramebuffer == nullptr || framebuffer.empty() || width <= 0 || height <= 0 || scale <= 0) {
        return;
    }

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

    struct PreparedOverride {
        const ChrOverride* override = nullptr;
        const DecodedImage* image = nullptr;
        int sourceScale = 1;
        ChrOverride::SourceLayout wholeChrLayout = ChrOverride::SourceLayout::PatternTables;
    };

    std::vector<const ChrOverride*> activeOverrides;
    if(activeOverrideFilter != nullptr) {
        activeOverrides = *activeOverrideFilter;
    } else {
        activeOverrides.reserve(m_chrOverrides.size());
        for(const ChrOverride& override : m_chrOverrides) {
            if(override.enabled && !override.assetPath.empty()) {
                activeOverrides.push_back(&override);
            }
        }
    }
    if(activeOverrides.empty()) {
        return;
    }
    std::stable_sort(activeOverrides.begin(), activeOverrides.end(), [](const ChrOverride* a, const ChrOverride* b) {
        return a->priority > b->priority;
    });

    std::vector<PreparedOverride> preparedOverrides;
    preparedOverrides.reserve(activeOverrides.size());
    std::unordered_map<const ChrOverride*, const PreparedOverride*> preparedByOverride;
    std::unordered_map<int, std::vector<const ChrOverride*>> overridesByTile;
    std::vector<const ChrOverride*> wholeChrOverrides;
    for(const ChrOverride* override : activeOverrides) {
        PreparedOverride prepared;
        prepared.override = override;
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
        preparedOverrides.push_back(prepared);
        preparedByOverride[override] = &preparedOverrides.back();
        if(override->wholeChr()) {
            wholeChrOverrides.push_back(override);
        } else {
            overridesByTile[override->tile].push_back(override);
        }
    }
    if(preparedOverrides.empty()) {
        return;
    }

    auto fullTileIndexFromPatternAddress = [](int patternAddress) {
        if(patternAddress >= 0 && patternAddress != 0xFFFF) {
            return (patternAddress >> 4) & 0x01FF;
        }
        return -1;
    };

    auto tileHash = [&](int patternAddress) {
        if(patternAddress < 0 || patternAddress == 0xFFFF) {
            return 0u;
        }
        const size_t tileIndex = static_cast<size_t>((patternAddress >> 4) & 0x01FF);
        return snapshot.tileHashes[tileIndex];
    };

    auto paletteMatches = [](const ChrOverride& override, const std::array<uint8_t, 3>& palette, bool allowDefaultTileFallback) {
        if(override.paletteIndices.empty()) {
            return true;
        }
        if(allowDefaultTileFallback && override.defaultTile) {
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

    auto matchesOverride = [&](const ChrOverride& override, ChrOverride::Target target, bool allowDefaultTileFallback, int tileIndex, int fullTileIndex, int patternAddress, int currentPatternTable, const std::array<uint8_t, 3>& palette) {
        if(override.target != ChrOverride::Target::Both && override.target != target) {
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
        if(override.hasChrHash && tileHash(patternAddress) != override.chrHash) {
            return false;
        }
        if(!paletteMatches(override, palette, allowDefaultTileFallback)) {
            return false;
        }
        return true;
    };

    auto findOverride = [&](ChrOverride::Target target, int tileIndex, int fullTileIndex, int patternAddress, int currentPatternTable, const std::array<uint8_t, 3>& palette) -> const ChrOverride* {
        auto scanCandidates = [&](const std::vector<const ChrOverride*>& candidates, bool allowDefaultTileFallback) -> const ChrOverride* {
            for(const ChrOverride* candidate : candidates) {
                if(matchesOverride(*candidate, target, allowDefaultTileFallback, tileIndex, fullTileIndex, patternAddress, currentPatternTable, palette)) {
                    return candidate;
                }
            }
            return nullptr;
        };

        auto scanExactTile = [&](int lookupTile, bool allowDefaultTileFallback) -> const ChrOverride* {
            if(const auto it = overridesByTile.find(lookupTile); it != overridesByTile.end()) {
                if(const ChrOverride* found = scanCandidates(it->second, allowDefaultTileFallback)) {
                    return found;
                }
            }
            return nullptr;
        };

        auto scanAllExact = [&]() -> const ChrOverride* {
            if(const auto fullIt = overridesByTile.find(fullTileIndex); fullIt != overridesByTile.end()) {
                if(const ChrOverride* found = scanCandidates(fullIt->second, false)) {
                    return found;
                }
            }
            if(fullTileIndex != tileIndex) {
                if(const auto tileIt = overridesByTile.find(tileIndex); tileIt != overridesByTile.end()) {
                    if(const ChrOverride* found = scanCandidates(tileIt->second, false)) {
                        return found;
                    }
                }
            }
            return scanCandidates(wholeChrOverrides, false);
        };

        if(const ChrOverride* exact = scanAllExact()) {
            return exact;
        }
        if(const ChrOverride* defaultMatch = scanExactTile(fullTileIndex, true)) {
            return defaultMatch;
        }
        if(fullTileIndex != tileIndex) {
            return nullptr;
        }
        return scanExactTile(tileIndex, true);
    };

    auto sampleOverridePixel = [&](uint32_t baseColor, uint32_t /*fallbackLayerColor*/, const ChrOverride* override, int patternAddress, int offsetX, int offsetY, int subX, int subY, uint8_t /*colorLowBits*/, const std::array<uint8_t, 3>& palette, bool horizontalMirror, bool verticalMirror, bool preserveSourceAlpha = false) {
        if(override == nullptr) {
            return baseColor;
        }
        const auto preparedIt = preparedByOverride.find(override);
        if(preparedIt == preparedByOverride.end()) {
            return baseColor;
        }
        const PreparedOverride* prepared = preparedIt->second;
        const DecodedImage* image = prepared->image;
        const int capturedTileIndex = fullTileIndexFromPatternAddress(patternAddress);
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
                if(capturedTileIndex < 0) {
                    return baseColor;
                }
                const int sourceTile = capturedTileIndex + override->sourceTileOffset;
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
            const uint32_t opaqueSource = (sourcePixel & 0x00FFFFFFu) | 0xFF000000u;
            if(!preserveSourceAlpha) {
                return opaqueSource;
            }
            if(sourceAlpha == 0) {
                return baseColor;
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
            return baseColor;
        }
        const int paletteIndex = static_cast<int>(sourcePaletteIndex) - 1;
        if(paletteIndex < 0 || paletteIndex >= static_cast<int>(palette.size())) {
            return baseColor;
        }
        const uint32_t mappedColor =
            (snapshot.paletteColors[palette[static_cast<size_t>(paletteIndex)] & 0x3F] & 0x00FFFFFFu) |
            (static_cast<uint32_t>(sourceAlpha) << 24u);
        return blendPixel(baseColor, mappedColor, 255);
    };

    const bool onlyWholeChrOverrides = std::all_of(activeOverrides.begin(), activeOverrides.end(), [](const ChrOverride* override) {
        return override->wholeChr() && !override->hasChrHash && override->paletteIndices.empty() &&
               override->patternTable < 0 && !override->defaultTile && !override->absoluteTile;
    });
    const ChrOverride* fastBackgroundOverride = nullptr;
    const ChrOverride* fastSpriteOverride = nullptr;
    if(onlyWholeChrOverrides) {
        for(const ChrOverride* override : activeOverrides) {
            if((override->target == ChrOverride::Target::Both || override->target == ChrOverride::Target::Background) && fastBackgroundOverride == nullptr) {
                fastBackgroundOverride = override;
            }
            if((override->target == ChrOverride::Target::Both || override->target == ChrOverride::Target::Sprite) && fastSpriteOverride == nullptr) {
                fastSpriteOverride = override;
            }
        }
    }

    const int activeNesY0 = std::max(0, activeTop / scale);
    const int activeNesY1 = std::min(PPU::SCREEN_HEIGHT, (activeBottom + scale - 1) / scale);
    for(int nesY = activeNesY0; nesY < activeNesY1; ++nesY) {
        const uint32_t* srcRow = sourceFramebuffer + static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH;
        const int blockY0 = std::max(activeTop, nesY * scale);
        const int blockY1 = std::min(activeBottom, (nesY + 1) * scale);
        if(blockY0 >= blockY1) continue;

        for(int nesX = 0; nesX < PPU::SCREEN_WIDTH; ++nesX) {
            const uint32_t baseColor = srcRow[nesX];
            const size_t pixelIndex = static_cast<size_t>(nesY) * PPU::SCREEN_WIDTH + static_cast<size_t>(nesX);
            const PPU::DebugModBackgroundPixel* bgPixel =
                pixelIndex < snapshot.backgroundPixels.size() ? &snapshot.backgroundPixels[pixelIndex] : nullptr;
            const PPU::DebugModSpritePixel* spritePixel =
                pixelIndex < snapshot.spritePixels.size() ? &snapshot.spritePixels[pixelIndex] : nullptr;

            const ChrOverride* backgroundOverride = nullptr;
            const ChrOverride* spriteOverride = nullptr;
            uint32_t backgroundFallbackColor = baseColor;
            uint32_t spriteFallbackColor = baseColor;
            bool spriteVisible = false;

            if(bgPixel != nullptr && bgPixel->valid) {
                const int bgFullTileIndex = fullTileIndexFromPatternAddress(bgPixel->patternAddress);
                const std::array<uint8_t, 3> bgPalette = { bgPixel->palette[0], bgPixel->palette[1], bgPixel->palette[2] };
                if(bgFullTileIndex >= 0) {
                    backgroundOverride =
                        onlyWholeChrOverrides
                            ? fastBackgroundOverride
                            : findOverride(ChrOverride::Target::Background, bgFullTileIndex & 0xFF, bgFullTileIndex, bgPixel->patternAddress, bgFullTileIndex / 256, bgPalette);
                }
                backgroundFallbackColor = snapshot.paletteColors[bgPixel->paletteIndex & 0x3F];
            }

            if(spritePixel != nullptr && spritePixel->valid && spritePixel->colorLowBits != 0) {
                const int spriteFullTileIndex = fullTileIndexFromPatternAddress(spritePixel->patternAddress);
                const std::array<uint8_t, 3> spritePalette = { spritePixel->palette[0], spritePixel->palette[1], spritePixel->palette[2] };
                if(spriteFullTileIndex >= 0) {
                    spriteOverride =
                        onlyWholeChrOverrides
                            ? fastSpriteOverride
                            : findOverride(ChrOverride::Target::Sprite, spriteFullTileIndex & 0xFF, spriteFullTileIndex, spritePixel->patternAddress, spriteFullTileIndex / 256, spritePalette);
                }
                const int spritePaletteIndex = std::clamp(static_cast<int>(spritePixel->colorLowBits), 1, 3) - 1;
                if(spritePaletteIndex >= 0 && spritePaletteIndex < static_cast<int>(spritePalette.size())) {
                    spriteFallbackColor = snapshot.paletteColors[spritePalette[static_cast<size_t>(spritePaletteIndex)] & 0x3F];
                }
                const bool backgroundOpaque = bgPixel != nullptr && bgPixel->valid && bgPixel->colorLowBits != 0;
                spriteVisible = !spritePixel->behindBackground || !backgroundOpaque;
            }

            const int blockX0 = nesX * scale;
            const int blockX1 = std::min(width, blockX0 + scale);
            if(blockX0 >= blockX1) continue;
            for(int subY = 0; subY < scale; ++subY) {
                const int outY = nesY * scale + subY;
                if(outY < activeTop || outY >= activeBottom || outY < 0 || outY >= height) continue;
                uint32_t* dstRow = framebuffer.data() + static_cast<size_t>(outY) * static_cast<size_t>(width);
                for(int subX = 0; subX < scale; ++subX) {
                    const int outX = blockX0 + subX;
                    if(outX < 0 || outX >= width) continue;
                    uint32_t color = baseColor;

                    if(bgPixel != nullptr && bgPixel->valid) {
                        color = backgroundFallbackColor;
                        if(backgroundOverride != nullptr) {
                            const std::array<uint8_t, 3> bgPalette = { bgPixel->palette[0], bgPixel->palette[1], bgPixel->palette[2] };
                            color = sampleOverridePixel(
                                color,
                                backgroundFallbackColor,
                                backgroundOverride,
                                bgPixel->patternAddress,
                                bgPixel->offsetX,
                                bgPixel->offsetY,
                                subX,
                                subY,
                                bgPixel->colorLowBits,
                                bgPalette,
                                false,
                                false
                            );
                        }
                    }

                    if(spriteVisible) {
                        if(spriteOverride != nullptr) {
                            const std::array<uint8_t, 3> spritePalette = { spritePixel->palette[0], spritePixel->palette[1], spritePixel->palette[2] };
                            color = sampleOverridePixel(
                                color,
                                spriteFallbackColor,
                                spriteOverride,
                                spritePixel->patternAddress,
                                spritePixel->offsetX,
                                spritePixel->offsetY,
                                subX,
                                subY,
                                spritePixel->colorLowBits,
                                spritePalette,
                                spritePixel->horizontalMirror,
                                spritePixel->verticalMirror,
                                true
                            );
                        } else {
                            color = spriteFallbackColor;
                        }
                    }
                    dstRow[outX] = color;
                }
            }
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

uint32_t ModManager::hashChrTile(PPU& ppu, int patternAddress)
{
    uint32_t hash = 2166136261u;
    for(int offset = 0; offset < 16; ++offset) {
        hash ^= static_cast<uint32_t>(ppu.debugPeekPpuMemory(static_cast<uint16_t>(patternAddress + offset)));
        hash *= 16777619u;
    }
    return hash;
}

uint8_t ModManager::readBackgroundPaletteIndexAt(PPU& ppu, int screenX, int screenY)
{
    auto wrapCoord = [](int value, int size) {
        if(size <= 0) return 0;
        value %= size;
        if(value < 0) value += size;
        return value;
    };

    const int worldX = wrapCoord(ppu.getVirtualScrollX() + screenX, 512);
    const int worldY = wrapCoord(ppu.getVirtualScrollY() + screenY, 480);
    const int nametableCol = worldX >= 256 ? 1 : 0;
    const int nametableRow = worldY >= 240 ? 1 : 0;
    const int nametableIndex = nametableRow * 2 + nametableCol;
    const int localX = worldX % 256;
    const int localY = worldY % 240;
    const int tileX = localX / 8;
    const int tileY = localY / 8;
    const int fineX = localX & 0x07;
    const int fineY = localY & 0x07;

    const uint16_t nametableBase = static_cast<uint16_t>(0x2000 + nametableIndex * 0x400);
    const uint8_t tileIndex = ppu.debugPeekPpuMemory(static_cast<uint16_t>(nametableBase + tileY * 32 + tileX));
    const uint16_t patternAddress = static_cast<uint16_t>(ppu.debugBackgroundPatternTableAddress() + tileIndex * 16 + fineY);
    const uint8_t lowPlane = ppu.debugPeekPpuMemory(patternAddress);
    const uint8_t highPlane = ppu.debugPeekPpuMemory(static_cast<uint16_t>(patternAddress + 8));
    const int bit = 7 - fineX;
    const uint8_t colorLowBits = static_cast<uint8_t>(((lowPlane >> bit) & 0x01) | (((highPlane >> bit) & 0x01) << 1));
    if(colorLowBits == 0) {
        return static_cast<uint8_t>(ppu.debugPeekPpuMemory(0x3F00) & 0x3F);
    }

    const uint16_t attrAddress = static_cast<uint16_t>(nametableBase + 0x03C0 + (tileY / 4) * 8 + (tileX / 4));
    const uint8_t attrByte = ppu.debugPeekPpuMemory(attrAddress);
    const int attrShift = ((tileY & 0x02) << 1) | (tileX & 0x02);
    const uint8_t paletteHighBits = static_cast<uint8_t>((attrByte >> attrShift) & 0x03);
    const uint8_t paletteRamIndex = static_cast<uint8_t>((paletteHighBits << 2) | colorLowBits);
    return static_cast<uint8_t>(ppu.debugPeekPpuMemory(static_cast<uint16_t>(0x3F00 + paletteRamIndex)) & 0x3F);
}
