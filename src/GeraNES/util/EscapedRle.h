#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace GeraNES {

namespace EscapedRle
{
    static constexpr uint8_t ESCAPE = 0xFF;
    static constexpr uint8_t MIN_RUN_LENGTH = 4;

    inline std::vector<uint8_t> encode(const uint8_t* data, size_t size)
    {
        std::vector<uint8_t> encoded;
        if(data == nullptr || size == 0) return encoded;

        encoded.reserve(size);

        size_t index = 0;
        while(index < size) {
            const uint8_t value = data[index];
            size_t runLength = 1;
            while(index + runLength < size &&
                  data[index + runLength] == value &&
                  runLength < 255) {
                ++runLength;
            }

            if(value == ESCAPE || runLength >= MIN_RUN_LENGTH) {
                encoded.push_back(ESCAPE);
                encoded.push_back(static_cast<uint8_t>(runLength));
                encoded.push_back(value);
                index += runLength;
                continue;
            }

            encoded.push_back(value);
            ++index;
        }

        return encoded;
    }

    inline bool decode(const uint8_t* encoded,
                       size_t encodedSize,
                       uint8_t* output,
                       size_t outputSize)
    {
        if(output == nullptr) return encodedSize == 0 && outputSize == 0;
        if(encoded == nullptr) return encodedSize == 0;

        size_t sourceIndex = 0;
        size_t destIndex = 0;
        while(sourceIndex < encodedSize) {
            const uint8_t value = encoded[sourceIndex++];
            if(value != ESCAPE) {
                if(destIndex >= outputSize) return false;
                output[destIndex++] = value;
                continue;
            }

            if(encodedSize - sourceIndex < 2) return false;

            const size_t runLength = encoded[sourceIndex++];
            const uint8_t runValue = encoded[sourceIndex++];
            if(runLength == 0 || runLength > outputSize - destIndex) return false;

            std::memset(output + destIndex, runValue, runLength);
            destIndex += runLength;
        }

        return destIndex == outputSize;
    }
}

} // namespace GeraNES
