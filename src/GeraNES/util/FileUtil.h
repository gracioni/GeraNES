#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>

inline std::vector<std::filesystem::path>& extraBinarySearchPaths()
{
    static std::vector<std::filesystem::path> paths;
    return paths;
}

inline void clearExtraBinarySearchPaths()
{
    extraBinarySearchPaths().clear();
}

inline void addExtraBinarySearchPath(const std::filesystem::path& path)
{
    if(path.empty()) return;

    auto normalized = path.lexically_normal();
    auto& paths = extraBinarySearchPaths();
    for(const auto& existing : paths) {
        if(existing == normalized) return;
    }
    paths.push_back(normalized);
}

static bool readBinaryFile(std::string_view path, std::vector<uint8_t>& data)
{
    auto tryRead = [&](const std::filesystem::path& filePath) -> bool {
        std::ifstream f(filePath, std::ios::binary);

        if(!f.is_open()) {
            return false;
        }

        std::streampos begin,end;
        begin = f.tellg();
        f.seekg (0, std::ios::end);
        end = f.tellg();
        f.seekg (0, std::ios::beg);

        int size = end - begin;

        data.clear();
        data.resize(size);

        f.read((char*)data.data(),size);

        f.close();

        return true;
    };

    const std::filesystem::path requestedPath(path);
    if(tryRead(requestedPath)) {
        return true;
    }

    if(!requestedPath.is_absolute()) {
        for(const auto& basePath : extraBinarySearchPaths()) {
            if(tryRead(basePath / requestedPath)) {
                return true;
            }
        }
    }

    return false;
}

static std::string basename(const std::string& filename)
{
    if (filename.empty()) {
        return {};
    }

    auto len = filename.length();
    auto index = filename.find_last_of("/\\");

    if (index == std::string::npos) {
        return filename;
    }

    if (index + 1 >= len) {

        len--;
        index = filename.substr(0, len).find_last_of("/\\");

        if (len == 0) {
            return filename;
        }

        if (index == 0) {
            return filename.substr(1, len - 1);
        }

        if (index == std::string::npos) {
            return filename.substr(0, len);
        }

        return filename.substr(index + 1, len - index - 1);
    }

    return filename.substr(index + 1, len - index);
}
