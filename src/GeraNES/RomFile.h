#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <array>

#include "util/Crc32.h"

#include "zip/zip.h"

#include "defines.h"
#include "util/MapperUtil.h"
#include "logger/logger.h"
#include "util/FileUtil.h"

#include <filesystem>
namespace fs = std::filesystem;

#include "libips.h"
#include "libups.h"
#include "libbps.h"

class RomFile {

private:

    enum class Patch {NONE, IPS, UPS, BPS};

    // Real filesystem path used to reopen the ROM source. For direct ROMs this
    // is the ROM file itself; for patches this is the patch file so clean-boot
    // state loads can reapply the same effective ROM image.
    std::string m_sourcePath;
    // Base ROM path used when loading a patch file. Empty for direct ROM files.
    std::string m_patchBasePath;
    // Selected entry inside an archive source. Empty for direct ROM files.
    std::string m_archiveEntryPath;
    // Effective ROM filename presented to the rest of the emulator/UI.
    std::string m_fileName;
    uint32_t m_crc32;
    std::string m_error;
    std::vector<uint8_t> m_data;
    std::array<uint8_t, 32> m_contentHash = {};

    static std::string toLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    static bool isSupportedArchiveRomEntry(const std::string& entryName) {
        const std::string ext = toLower(fs::path(entryName).extension().string());
        return ext == ".nes" || ext == ".fds" || ext == ".nsf";
    }

    static std::string selectZipEntry(const std::vector<std::string>& entries) {
        if(entries.empty()) return {};

        for(const auto& entry : entries) {
            if(isSupportedArchiveRomEntry(entry)) {
                return entry;
            }
        }

        return entries.front();
    }

    static std::array<uint8_t, 32> calcContentHash32(const std::vector<uint8_t>& data) {
        std::array<uint8_t, 32> hash = {};
        constexpr std::array<uint64_t, 4> offsets = {
            1469598103934665603ull,
            1099511628211ull,
            7809847782465536322ull,
            1609587929392839161ull
        };
        constexpr std::array<uint64_t, 4> primes = {
            1099511628211ull,
            14029467366897019727ull,
            1609587929392839161ull,
            9650029242287828579ull
        };

        for(size_t lane = 0; lane < offsets.size(); ++lane) {
            uint64_t value = offsets[lane] ^ (static_cast<uint64_t>(data.size()) << (lane * 7u));
            for(uint8_t byte : data) {
                value ^= static_cast<uint64_t>(byte) + (static_cast<uint64_t>(lane) << 8u);
                value *= primes[lane];
                value ^= value >> 32u;
            }

            for(size_t byteIndex = 0; byteIndex < 8u; ++byteIndex) {
                hash[(lane * 8u) + byteIndex] = static_cast<uint8_t>((value >> (byteIndex * 8u)) & 0xFFu);
            }
        }

        return hash;
    }

    static const std::vector<std::string> getZpFileEntries(const std::string& filename) {

        std::vector<std::string> ret;

        struct zip_t *zip = zip_open(filename.c_str(), 0, 'r');
        int i, n = zip_entries_total(zip);
        for (i = 0; i < n; ++i) {
            zip_entry_openbyindex(zip, i);
            {
                const char *name = zip_entry_name(zip);
                int isdir = zip_entry_isdir(zip);

                if(!isdir) ret.push_back(name);
            }
            zip_entry_close(zip);
        }
        zip_close(zip);

        return ret;
    }

    static const std::vector<uint8_t> readZipFile(const std::string& filename, const std::string& entry) {
        char *buf = NULL;
        size_t bufsize = 0;

        struct zip_t *zip = zip_open(filename.c_str(), 0, 'r');
        {
            zip_entry_open(zip, entry.c_str());
            {
                zip_entry_read(zip, (void **)&buf, &bufsize);
            }
            zip_entry_close(zip);
        }
        zip_close(zip);

        if(buf == NULL || bufsize == 0) {
            free(buf);
            return {};
        }

        auto ret = std::vector<uint8_t>(&buf[0], &buf[bufsize]);

        free(buf);

        return ret;
    }

    bool isPatchFile(const std::string& path) {
        std::vector<std::string> patches = {".ips", ".ups", ".bps"};
        std::string ext = toLower(fs::path(path).extension().string());
        return std::find(patches.begin(), patches.end(), ext) != patches.end();
    }

    Patch getPatchType(const std::string& path) {
        std::string ext = toLower(fs::path(path).extension().string());
        if(ext == ".ips") return Patch::IPS;
        if(ext == ".bps") return Patch::BPS;
        if(ext == ".ups") return Patch::UPS;
        return Patch::NONE;
    }

public:

    static RomFile& emptyRomFile() {
        static RomFile emptyRomFile;
        return emptyRomFile;
    }

    bool open(const std::string& path) {

        std::string realRomPath = path;
        std::string selectedZipEntry;
        bool isPatch = isPatchFile(path);
        m_error.clear();
        m_sourcePath.clear();
        m_patchBasePath.clear();
        m_archiveEntryPath.clear();
        m_fileName.clear();
        m_data.clear();
        m_contentHash = {};

        auto zipEntries = getZpFileEntries(path);

        if(zipEntries.size() > 0) {
            selectedZipEntry = selectZipEntry(zipEntries);
            if(selectedZipEntry.empty()) {
                m_error = std::string("zip archive '") + path + "' is empty";
                return false;
            }
            m_data = readZipFile(path, selectedZipEntry);
            m_fileName = basename(selectedZipEntry);
            m_sourcePath = path;
            m_archiveEntryPath = selectedZipEntry;
        }
        else {

            if(isPatch) {
                std::string ext = fs::path(path).extension().string();
                const std::string from = ext;
                const std::string to = ".nes";
                size_t pos = realRomPath.rfind(from);
                if (pos != std::string::npos)
                    realRomPath.replace(pos, from.size(), to);
            }

            if(!readBinaryFile(realRomPath, m_data)) {
                m_error = std::string("file '") + realRomPath + "' not found";
                return false;
            }

            m_fileName = basename(isPatch ? path : realRomPath);
            if(isPatch) {
                m_sourcePath = path;
                m_patchBasePath = realRomPath;
            } else {
                m_sourcePath = realRomPath;
            }
        }

        if(isPatch) {
            if(!applyPatch(path)) return false;
        }

        m_crc32 = Crc32::calc((const char*)&m_data[0], m_data.size());
        m_contentHash = calcContentHash32(m_data);

        log();

        return true;
    }

    GERANES_INLINE uint32_t fileCrc32() const { return m_crc32; }
    GERANES_INLINE std::array<uint8_t, 32> contentHash32() const { return m_contentHash; }
    GERANES_INLINE std::string fileName() const { return m_fileName; }
    GERANES_INLINE std::string fullPath() const { return m_sourcePath; }
    GERANES_INLINE std::string sourcePath() const { return m_sourcePath; }
    GERANES_INLINE std::string patchBasePath() const { return m_patchBasePath; }
    GERANES_INLINE std::string archiveEntryPath() const { return m_archiveEntryPath; }
    GERANES_INLINE std::string error() const { return m_error; }
    GERANES_INLINE std::string displayPath() const {
        if(!m_archiveEntryPath.empty()) {
            return m_sourcePath + " > " + m_archiveEntryPath;
        }
        return m_fileName.empty() ? m_sourcePath : m_fileName;
    }
    GERANES_INLINE uint8_t  data(size_t addr) const { return m_data[addr]; }
    GERANES_INLINE size_t size() const { return m_data.size(); }

    bool applyPatch(const std::string patchFilePath) { 

        std::vector<uint8_t> patchData;

        if(!readBinaryFile(patchFilePath, patchData)) {
            m_error = std::string("file '") + patchFilePath + "' not found";
            return false;
        }    
                   
        mem original = {m_data.data(), m_data.size()};
        mem patch = {patchData.data(), patchData.size()};
        mem out;

        switch(getPatchType(patchFilePath)) {
            case Patch::IPS: {               
                auto result = ips_apply(patch, original, &out);
                if(result == ips_ok) {
                    m_data.clear();
                    m_data.resize(out.len);
                    memcpy(m_data.data(), out.ptr, out.len);
                    ips_free(out);
                }                
                else {
                    ips_free(out);
                    m_error = "invalid ips patch file";
                    return false;
                }
                break;
            }
            case Patch::UPS: {               
                auto result = ups_apply(patch, original, &out);
                if(result == ups_ok) {
                    m_data.clear();
                    m_data.resize(out.len);
                    memcpy(m_data.data(), out.ptr, out.len);
                    ups_free(out);
                }                
                else {
                    ups_free(out);
                    m_error = "invalid ups patch file";
                    return false;
                }
                break;
            }
            case Patch::BPS: {               
                   
                auto result = bps_apply(patch, original, &out, nullptr, false);
                if(result == bps_ok) {
                    m_data.clear();
                    m_data.resize(out.len);
                    memcpy(m_data.data(), out.ptr, out.len);
                    bps_free(out);
                }                
                else {
                    bps_free(out);
                    m_error = "invalid bps patch file";
                    return false;
                }
                break;
            }
            default:
                m_error = "invalid patch file";
                return false;
        }
        
        return true;
    }

    void log() {

        std::stringstream aux;

        aux << "(ROM) File name: "<< fileName() << std::endl;
        if(!m_archiveEntryPath.empty()) {
            aux << "(ROM) Source archive: " << sourcePath() << std::endl;
        } else if(!m_patchBasePath.empty()) {
            aux << "(ROM) Source patch: " << sourcePath() << std::endl;
            aux << "(ROM) Patch base: " << patchBasePath() << std::endl;
        } else {
            aux << "(ROM) Source path: "<< sourcePath() << std::endl;
        }
        aux << "(ROM) File Size: " << size() << " bytes" << std::endl; 
        aux << "(ROM) File CRC32: "<< Crc32::toString(fileCrc32());

        Logger::instance().log(aux.str(), Logger::Type::INFO);
    }

};
