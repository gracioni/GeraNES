#pragma once

#include <cstdint>
#include <cstring>

#include <vector>
#include <fstream>
#include <utility>

#include "util/map_util.h"

#include <filesystem>
namespace fs = std::filesystem;

class SerializationBase
{
    private:

        bool _littleEndian;

    public:

        SerializationBase()
        {
            _littleEndian = false;
            uint32_t n = 0x04030201;
            if( *reinterpret_cast<char*>(&n) == 0x01) _littleEndian = true;
        }

        virtual void single(uint8_t* pointer, size_t size) = 0;

        void array(uint8_t* pointer, size_t typeSize, size_t nelements)
        {
            if(nelements == 0 || typeSize == 0) return;

            // Fast path: contiguous byte copy preserves layout on little-endian
            // hosts for raw/trivial payloads that are already byte-oriented.
            if(littleEndian()) {
                single(pointer, typeSize * nelements);
                return;
            }

            for(size_t i = 0; i < nelements; i++) {
                single(pointer, typeSize);
                pointer += typeSize;
            }
        }

        bool littleEndian()
        {
            return _littleEndian;
        }
};

class Serialize : public SerializationBase
{
    private:

        std::vector<uint8_t> _data;

    public:

        void single(uint8_t* pointer, size_t size) override
        {
            if(size == 0) return;

            if(littleEndian()) {
                const size_t offset = _data.size();
                _data.resize(offset + size);
                std::memcpy(_data.data() + offset, pointer, size);
                return;
            }

            // Big-endian fallback: emit bytes in reverse memory order.
            for(size_t i = 0; i < size; ++i) {
                _data.push_back(pointer[size - 1 - i]);
            }
        }

        void reserve(size_t size)
        {
            _data.reserve(size);
        }

        void clear()
        {
            _data.clear();
        }

        const std::vector<uint8_t>& getData() const
        {
            return _data;
        }

        std::vector<uint8_t> takeData()
        {
            return std::move(_data);
        }

        bool saveToFile(const std::string& fileName)
        { 
            std::string dir = fs::path(fileName).parent_path().string();
            if(!fs::exists(dir)) fs::create_directory(dir);

            std::ofstream f(fileName, std::ios::binary | std::ios::trunc);
            if(f.is_open()) {
                f.write(reinterpret_cast<char*>(&_data[0]), _data.size());
                f.close();
                return true;
            }

            return false;
        }

};

class Deserialize : public SerializationBase
{
    private:

        std::vector<uint8_t> _data;
        const uint8_t* _dataView = nullptr;
        size_t _dataViewSize = 0;
        bool _useDataView = false;
        size_t _index = 0;
        bool _error = false;

    public:

        void single(uint8_t* pointer, size_t size) override
        {
            if(size == 0) return;

            const size_t dataSize = _useDataView ? _dataViewSize : _data.size();
            if(_index + size > dataSize){
                _error = true;
                return;
            }

            if(littleEndian()) {
                if(_useDataView) {
                    std::memcpy(pointer, _dataView + _index, size);
                } else {
                    std::memcpy(pointer, _data.data() + _index, size);
                }
                _index += size;
                return;
            }

            const uint8_t* source = _useDataView ? (_dataView + _index) : (_data.data() + _index);
            // Big-endian fallback: restore bytes in reverse memory order.
            for(size_t i = 0; i < size; ++i) {
                pointer[size - 1 - i] = source[i];
            }
            _index += size;
        }

        bool loadFromFile(const std::string& fileName)
        {
            std::ifstream f(fileName, std::ios::binary);

            if(f.is_open()) {

                std::streampos begin,end;
                begin = f.tellg();
                f.seekg (0, std::ios::end);
                end = f.tellg();
                f.seekg (0, std::ios::beg);

                size_t size = end-begin;

                _data.clear();
                _data.resize(size);
                _useDataView = false;
                _dataView = nullptr;
                _dataViewSize = 0;
                _index = 0;
                _error = false;

                f.read((char*)&_data[0],size);

                f.close();

                return true;
            }

            _error = true;

            return false;
        }


        void setData(const std::vector<uint8_t>& data) {
            _data = data;
            _useDataView = false;
            _dataView = nullptr;
            _dataViewSize = 0;
            _index = 0;
            _error = false;
        }

        void setData(const uint8_t* data, size_t size) {
            _data.clear();
            _useDataView = true;
            _dataView = data;
            _dataViewSize = size;
            _index = 0;
            _error = (data == nullptr && size > 0);
        }

        bool error()
        {
            return _error;
        }

};

class SerializationSize : public SerializationBase
{
private:

    size_t m_size = 0;

public:

    void single(uint8_t* /*pointer*/, size_t size) override
    {
        m_size += size;
    }

    size_t size() const {
        return m_size;
    }

};

#define SERIALIZEDATA(serializer, data) serializer.single(reinterpret_cast<uint8_t*>(&data), sizeof(data));

template<typename Map>
void serialize_map(SerializationBase& s, Map& m) {
    auto array = map_to_array(m);
    uint32_t arraySize = array.size();
    SERIALIZEDATA(s, arraySize);

    if(array.size() != arraySize) array.resize(arraySize);

    s.array(
        reinterpret_cast<uint8_t*>(array.data()),
        sizeof(typename decltype(array)::value_type),
        arraySize);

    array_to_map(array, m);
}
