#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include <vector>
#include <string>
#include <cstdint>
#include <fstream>

static bool readBinaryFile(std::string_view path, std::vector<uint8_t>& data)
{
    std::ifstream f(std::string(path), std::ios::binary);

    if(f.is_open())
    {
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

#endif