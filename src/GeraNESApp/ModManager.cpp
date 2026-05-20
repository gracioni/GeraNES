#include "GeraNESApp/ModManager.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "GeraNES/RomFile.h"
#include "GeraNESApp/AppSettings.h"
#include "logger/logger.h"
#include "zip/zip.h"

namespace {
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
}

void ModManager::clear()
{
    m_originalRomPath.clear();
    m_effectiveRomPath.clear();
    m_modPath.clear();
    m_active = false;
    m_scriptLoaded = false;
    m_resolutionMultiplier = 1;
    m_chrOverrides.clear();
    m_backgroundReplacements.clear();
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

    m_scriptLoaded = true;
    Logger::instance().log("Mod script.lua loaded.", Logger::Type::INFO);
    return true;
}

void ModManager::onFrame(GeraNESEmu& emu)
{
    if(!m_active || !m_scriptLoaded) return;
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
    if(zip_entry_open(zip, normalizedEntry.c_str()) != 0) {
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
    api.set_function("add_chr_override", [this](int tile, const std::string& assetPath, sol::optional<bool> ignorePalette) {
        m_chrOverrides.push_back(ChrOverride{
            std::max(0, tile),
            normalizeZipPath(assetPath),
            ignorePalette.value_or(false)
        });
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
