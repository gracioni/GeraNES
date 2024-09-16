#ifndef ROMFILE_H
#define ROMFILE_H

#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include "util/Crc32.h"

#include "zip/zip.h"

#include "defines.h"
#include "functions.h"

class RomFile {

private:

    std::string m_fullpath;
    std::string m_fileName;
    std::string m_fileHash;
    std::string m_error;
    std::vector<uint8_t> m_data;

    static const std::vector<std::string> getZpFileEntries(const std::string& filename) {

        std::vector<std::string> ret;

        struct zip_t *zip = zip_open(filename.c_str(), 0, 'r');
        int i, n = zip_entries_total(zip);
        for (i = 0; i < n; ++i) {
            zip_entry_openbyindex(zip, i);
            {
                const char *name = zip_entry_name(zip);
                int isdir = zip_entry_isdir(zip);
                unsigned long long size = zip_entry_size(zip);
                unsigned int crc32 = zip_entry_crc32(zip);

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
        zip_stream_close(zip);

        auto ret = std::vector<uint8_t>(&buf[0], &buf[bufsize]);

        free(buf);

        return ret;
    }

public:

    static RomFile emptyRomFile;

    bool open(const std::string& path) {

        auto zipEntries = getZpFileEntries(path);

        if(zipEntries.size() > 0) {
            m_data = readZipFile(path, zipEntries[0]);
            m_fileName = basename(zipEntries[0]);
        }
        else {

            std::ifstream f(path.c_str(), std::ios::binary);

            if(f.is_open())
            {
                std::streampos begin,end;
                begin = f.tellg();
                f.seekg (0, std::ios::end);
                end = f.tellg();
                f.seekg (0, std::ios::beg);

                int size = end - begin;

                m_data.clear();
                m_data.resize(size);

                f.read((char*)&m_data[0],size);

                f.close();
            }
            else {
                m_error = "file not found";
                return false;
            }

            m_fileName = basename(path);
        }

        m_fullpath = path;

        uint32_t crc = Crc32::calc((const char*)&m_data[0], m_data.size());     
        m_fileHash = Crc32::toString(crc);

        return true;
    }

    GERANES_INLINE std::string fileCrc32() const { return m_fileHash; }
    GERANES_INLINE std::string fileName() const { return m_fileName; }
    GERANES_INLINE std::string fullPath() const { return m_fullpath; }
    GERANES_INLINE std::string error() const { return m_error; }
    GERANES_INLINE uint8_t  data(size_t addr) const { return m_data[addr]; }
    GERANES_INLINE size_t size() const { return m_data.size(); }

    std::string debug() {

        std::stringstream aux;

        aux << "File name: "<< fileName() << std::endl;
        aux << "Full path: "<< fullPath() << std::endl;
        aux << "File crc32: "<< fileCrc32() << std::endl;
        aux << "File Size: " << size() << " bytes";

        return aux.str();
    }

};

#endif // ROMFILE_H
