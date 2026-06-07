#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace AndroidFileDialog
{
    struct PickedFile
    {
        std::string cachePath;
        std::string displayName;
        std::string uri;
    };

    bool pickFileToCache(const std::vector<std::string>& mimeTypes, std::string& outPath, std::string* outError = nullptr);
    bool pickFileToCacheWithMetadata(const std::vector<std::string>& mimeTypes, PickedFile& outFile, std::string* outError = nullptr);
    bool copyDocumentUriToCache(const std::string& uri, PickedFile& outFile, std::string* outError = nullptr);
    bool pickFolderToCache(std::string& outPath, std::string* outError = nullptr);
    bool saveBytesWithDocumentPicker(const std::string& suggestedName,
                                     const std::string& mimeType,
                                     const std::vector<uint8_t>& bytes,
                                     std::string* outError = nullptr);
}
