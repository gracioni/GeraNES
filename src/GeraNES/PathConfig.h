#pragma once

#include <filesystem>

namespace GeraNES {

inline std::filesystem::path& contentRootStorage()
{
    static std::filesystem::path path = std::filesystem::current_path();
    return path;
}

inline void setContentRoot(const std::filesystem::path& path)
{
    if(path.empty()) {
        return;
    }
    contentRootStorage() = path;
}

inline const std::filesystem::path& contentRoot()
{
    return contentRootStorage();
}

inline std::filesystem::path resolveContentPath(const std::filesystem::path& relativePath)
{
    if(relativePath.empty()) {
        return contentRoot();
    }
    return contentRoot() / relativePath;
}

} // namespace GeraNES
