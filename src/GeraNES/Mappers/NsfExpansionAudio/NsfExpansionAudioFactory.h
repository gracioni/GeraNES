#pragma once

#include <memory>
#include <sstream>

#include "NsfExpansionAudio.h"
#include "Vrc6NsfExpansionAudio.h"
#include "Mmc5NsfExpansionAudio.h"
#include "Namco163NsfExpansionAudio.h"
#include "Sunsoft5BNsfExpansionAudio.h"
#include "logger/logger.h"

class NsfExpansionAudioFactory
{
public:
    static std::unique_ptr<NsfExpansionAudio> createForNsf(uint8_t soundChipFlags)
    {
        constexpr uint8_t CHIP_VRC6 = 0x01;
        constexpr uint8_t CHIP_VRC7 = 0x02;
        constexpr uint8_t CHIP_FDS  = 0x04;
        constexpr uint8_t CHIP_MMC5 = 0x08;
        constexpr uint8_t CHIP_N163 = 0x10;
        constexpr uint8_t CHIP_S5B  = 0x20;

        if(soundChipFlags == 0) {
            return nullptr;
        }

        std::unique_ptr<NsfExpansionAudio> audio;
        uint8_t unsupportedFlags = soundChipFlags;

        if((soundChipFlags & CHIP_VRC6) != 0) {
            audio = std::make_unique<Vrc6NsfExpansionAudio>();
            unsupportedFlags = static_cast<uint8_t>(unsupportedFlags & ~CHIP_VRC6);
        }
        else if((soundChipFlags & CHIP_MMC5) != 0) {
            audio = std::make_unique<Mmc5NsfExpansionAudio>();
            unsupportedFlags = static_cast<uint8_t>(unsupportedFlags & ~CHIP_MMC5);
        }
        else if((soundChipFlags & CHIP_N163) != 0) {
            audio = std::make_unique<Namco163NsfExpansionAudio>();
            unsupportedFlags = static_cast<uint8_t>(unsupportedFlags & ~CHIP_N163);
        }
        else if((soundChipFlags & CHIP_S5B) != 0) {
            audio = std::make_unique<Sunsoft5BNsfExpansionAudio>();
            unsupportedFlags = static_cast<uint8_t>(unsupportedFlags & ~CHIP_S5B);
        }

        if(unsupportedFlags != 0) {
            std::ostringstream ss;
            ss << "NSF expansion audio warning: unsupported chip flags 0x"
               << std::hex << static_cast<int>(unsupportedFlags)
               << " will be ignored";
            Logger::instance().log(ss.str(), Logger::Type::WARNING);
        }

        if(audio == nullptr) {
            std::ostringstream ss;
            ss << "NSF expansion audio warning: chip flags 0x"
               << std::hex << static_cast<int>(soundChipFlags)
               << " are not implemented; playback will use only base channels";
            Logger::instance().log(ss.str(), Logger::Type::WARNING);
        }

        (void)CHIP_VRC7;
        (void)CHIP_FDS;

        return audio;
    }
};
