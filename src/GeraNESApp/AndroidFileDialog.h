#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace AndroidFileDialog
{
    bool pickFileToCache(const std::vector<std::string>& mimeTypes, std::string& outPath, std::string* outError = nullptr);
    bool pickFolderToCache(std::string& outPath, std::string* outError = nullptr);
    bool saveBytesWithDocumentPicker(const std::string& suggestedName,
                                     const std::string& mimeType,
                                     const std::vector<uint8_t>& bytes,
                                     std::string* outError = nullptr);
}
