#pragma once

#include <cstdint>

#include <vector>
#include <fstream>

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
            int step = 1;

            if(!littleEndian()) {
                step = -1;
                pointer += size-1;
            }

            for(size_t i = 0; i < size; i++){
                _data.push_back(*pointer);
                pointer += step;
            }
        }

        const std::vector<uint8_t>& getData() {
            return _data;
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
        size_t _index = 0;
        bool _error = false;

    public:

        void single(uint8_t* pointer, size_t size) override
        {
            int step = 1;

            if(!littleEndian()) {
                step = -1;
                pointer += size-1;
            }

            for(size_t i = 0; i < size; i++) {

                if(_index >= _data.size()){
                    _error = true;
                    break;
                }

                *pointer = _data[_index];
                pointer += step;
                _index++;
            }
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

                f.read((char*)&_data[0],size);

                f.close();

                return true;
            }

            _error = true;

            return false;
        }


        void setData(const std::vector<uint8_t>& data) {
            _data = data;
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
