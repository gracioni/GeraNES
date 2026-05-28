#pragma once

#include "imgui.h"

namespace FontAwesomeIcons {
inline constexpr const char* kMusic = "\xef\x80\x81";
inline constexpr const char* kGear = "\xef\x80\x93";
inline constexpr const char* kDownload = "\xef\x80\x99";
inline constexpr const char* kVolumeHigh = "\xef\x80\xa8";
inline constexpr const char* kPlay = "\xef\x81\x8b";
inline constexpr const char* kPause = "\xef\x81\x8c";
inline constexpr const char* kStop = "\xef\x81\x8d";
inline constexpr const char* kCircleQuestion = "\xef\x81\x99";
inline constexpr const char* kCircleInfo = "\xef\x81\x9a";
inline constexpr const char* kExpand = "\xef\x81\xa5";
inline constexpr const char* kFolder = "\xef\x81\xbb";
inline constexpr const char* kFolderOpen = "\xef\x81\xbc";
inline constexpr const char* kGlobe = "\xef\x82\xac";
inline constexpr const char* kWrench = "\xef\x82\xad";
inline constexpr const char* kFloppyDisk = "\xef\x83\x87";
inline constexpr const char* kGamepad = "\xef\x84\x9b";
inline constexpr const char* kKeyboard = "\xef\x84\x9c";
inline constexpr const char* kPuzzlePiece = "\xef\x84\xae";
inline constexpr const char* kBug = "\xef\x86\x88";
inline constexpr const char* kDatabase = "\xef\x87\x80";
inline constexpr const char* kFileZipper = "\xef\x87\x86";
inline constexpr const char* kClockRotateLeft = "\xef\x87\x9a";
inline constexpr const char* kSliders = "\xef\x87\x9e";
inline constexpr const char* kWifi = "\xef\x87\xab";
inline constexpr const char* kTv = "\xef\x89\xac";
inline constexpr const char* kMicrochip = "\xef\x8b\x9b";
inline constexpr const char* kRightFromBracket = "\xef\x8b\xb5";
inline constexpr const char* kRotateRight = "\xef\x8b\xb9";
inline constexpr const char* kClipboard = "\xef\x8c\xa8";
inline constexpr const char* kMemory = "\xef\x94\xb8";
inline constexpr const char* kPalette = "\xef\x94\xbf";
inline constexpr const char* kBookOpen = "\xef\x94\x98";
inline constexpr const char* kWandMagicSparkles = "\xee\x8b\x8a";
inline constexpr const char* kXmark = "\xef\x80\x8d";
inline constexpr const char* kCalculator = "\xef\x87\xac";
inline constexpr const char* kFile = "\xef\x85\x9b";

inline const ImWchar* glyphRanges()
{
    static const ImWchar ranges[] = {
        0xe2ca, 0xe2ca,
        0xf001, 0xf001,
        0xf013, 0xf013,
        0xf019, 0xf019,
        0xf028, 0xf028,
        0xf04b, 0xf04d,
        0xf059, 0xf05a,
        0xf065, 0xf065,
        0xf07b, 0xf07c,
        0xf0ac, 0xf0ad,
        0xf0c7, 0xf0c7,
        0xf11b, 0xf11c,
        0xf12e, 0xf12e,
        0xf188, 0xf188,
        0xf1c0, 0xf1c0,
        0xf1c6, 0xf1c6,
        0xf1da, 0xf1de,
        0xf1eb, 0xf1eb,
        0xf26c, 0xf26c,
        0xf2db, 0xf2db,
        0xf2f5, 0xf2f5,
        0xf2f9, 0xf2f9,
        0xf328, 0xf328,
        0xf518, 0xf518,
        0xf538, 0xf53f,
        0,
    };
    return ranges;
}
}
