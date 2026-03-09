#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if defined(GERANES_LIBRETRO_USE_CMRC_DB)
#include "cmrc/cmrc.hpp"
#endif

#include "GeraNES/GeraNESEmu.h"
#include "GeraNES/GameDatabase.h"
#include "GeraNES/PPU.h"
#include "GeraNES/Serialization.h"
#include "GeraNESApp/AudioOutputBase.h"
#include "logger/logger.h"

#if defined(GERANES_LIBRETRO_USE_CMRC_DB)
CMRC_DECLARE(libretro_db_assets);
#else
extern unsigned char geranes_libretro_embedded_db[];
extern unsigned int geranes_libretro_embedded_db_size;
#endif

extern "C" {

typedef bool (*retro_environment_t)(unsigned, void*);
typedef void (*retro_video_refresh_t)(const void*, unsigned, unsigned, size_t);
typedef void (*retro_audio_sample_t)(int16_t, int16_t);
typedef size_t (*retro_audio_sample_batch_t)(const int16_t*, size_t);
typedef void (*retro_input_poll_t)(void);
typedef int16_t (*retro_input_state_t)(unsigned, unsigned, unsigned, unsigned);

struct retro_game_info {
    const char* path;
    const void* data;
    size_t size;
    const char* meta;
};

struct retro_system_info {
    const char* library_name;
    const char* library_version;
    const char* valid_extensions;
    bool need_fullpath;
    bool block_extract;
};

struct retro_game_geometry {
    unsigned base_width;
    unsigned base_height;
    unsigned max_width;
    unsigned max_height;
    float aspect_ratio;
};

struct retro_system_timing {
    double fps;
    double sample_rate;
};

struct retro_system_av_info {
    struct retro_game_geometry geometry;
    struct retro_system_timing timing;
};

struct retro_message {
    const char* msg;
    unsigned frames;
};

enum retro_pixel_format {
    RETRO_PIXEL_FORMAT_0RGB1555 = 0,
    RETRO_PIXEL_FORMAT_XRGB8888 = 1,
    RETRO_PIXEL_FORMAT_RGB565 = 2
};

enum {
    RETRO_ENVIRONMENT_SET_MESSAGE = 6,
    RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY = 9,
    RETRO_ENVIRONMENT_GET_LIBRETRO_PATH = 19,
    RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY = 30,
    RETRO_ENVIRONMENT_SET_PIXEL_FORMAT = 10,
    RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME = 18
};

enum {
    RETRO_DEVICE_JOYPAD = 1
};

enum {
    RETRO_DEVICE_ID_JOYPAD_B = 0,
    RETRO_DEVICE_ID_JOYPAD_SELECT = 2,
    RETRO_DEVICE_ID_JOYPAD_START = 3,
    RETRO_DEVICE_ID_JOYPAD_UP = 4,
    RETRO_DEVICE_ID_JOYPAD_DOWN = 5,
    RETRO_DEVICE_ID_JOYPAD_LEFT = 6,
    RETRO_DEVICE_ID_JOYPAD_RIGHT = 7,
    RETRO_DEVICE_ID_JOYPAD_A = 8
};

enum {
    RETRO_REGION_NTSC = 0,
    RETRO_REGION_PAL = 1
};

enum {
    RETRO_MEMORY_SAVE_RAM = 0
};

static constexpr unsigned RETRO_API_VERSION = 1;

}

#if defined(_WIN32)
#define RETRO_API extern "C" __declspec(dllexport)
#else
#define RETRO_API extern "C"
#endif

namespace {

constexpr int kSampleRate = 44100;

retro_environment_t g_environmentCb = nullptr;
retro_video_refresh_t g_videoCb = nullptr;
retro_audio_sample_t g_audioCb = nullptr;
retro_audio_sample_batch_t g_audioBatchCb = nullptr;
retro_input_poll_t g_inputPollCb = nullptr;
retro_input_state_t g_inputStateCb = nullptr;

class LibretroAudioOutput final : public AudioOutputBase {
private:
    uint32_t m_sampleAccumulator = 0;
    std::vector<int16_t> m_stereoBuffer;

public:
    bool init() override
    {
        reset();
        initChannels(kSampleRate);
        return true;
    }

    void reset()
    {
        m_sampleAccumulator = 0;
        m_stereoBuffer.clear();
        AudioOutputBase::clearBuffers();
    }

    void render(uint32_t dt, bool silenceFlag) override
    {
        m_sampleAccumulator += dt * kSampleRate;

        while(m_sampleAccumulator >= 1000) {
            const float mixed = silenceFlag ? 0.0f : mix();
            const float scaled = mixed * 32767.0f;
            int sample = static_cast<int>(scaled);

            if(sample > 32767) sample = 32767;
            else if(sample < -32768) sample = -32768;

            const auto s = static_cast<int16_t>(sample);
            m_stereoBuffer.push_back(s);
            m_stereoBuffer.push_back(s);

            m_sampleAccumulator -= 1000;
        }
    }

    void submit()
    {
        if(g_audioBatchCb != nullptr && !m_stereoBuffer.empty()) {
            g_audioBatchCb(m_stereoBuffer.data(), m_stereoBuffer.size() / 2);
        }
        else if(g_audioCb != nullptr && !m_stereoBuffer.empty()) {
            for(size_t i = 0; i < m_stereoBuffer.size(); i += 2) {
                g_audioCb(m_stereoBuffer[i], m_stereoBuffer[i + 1]);
            }
        }

        m_stereoBuffer.clear();
    }
};

LibretroAudioOutput g_audio;
GeraNESEmu g_emu(g_audio);

std::array<uint32_t, PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT> g_videoFrame{};
std::string g_tempRomPath;

bool g_gameLoaded = false;
uint32_t g_frameTimeMs = 16;
std::string g_dbPath;
bool g_loggerBound = false;

void frontendMessage(const std::string& msg, unsigned frames = 180);

class LibretroLogSink final : public SigSlot::SigSlotBase {
public:
    void onLog(const std::string& msg, Logger::Type type)
    {
        if(msg.empty()) return;

        unsigned frames = 120;
        if(type == Logger::Type::WARNING) frames = 180;
        else if(type == Logger::Type::ERROR) frames = 300;

        frontendMessage(msg, frames);
    }
};

LibretroLogSink g_logSink;

void updateTimingFromRegion()
{
    const auto fps = g_emu.getRegionFPS();
    g_frameTimeMs = fps > 0 ? static_cast<uint32_t>((1000 + (fps / 2)) / fps) : 16;
}

bool writeTempRom(const void* data, size_t size, std::string& outPath)
{
    if(data == nullptr || size == 0) return false;

    namespace fs = std::filesystem;

    const auto tempDir = fs::temp_directory_path();
    const auto fileName = "geranes_libretro_rom.nes";
    outPath = (tempDir / fileName).string();

    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if(!out.is_open()) return false;

    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return out.good();
}

void updateControllerState(unsigned port)
{
    if(g_inputStateCb == nullptr) return;

    const bool b = g_inputStateCb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) != 0;
    const bool a = g_inputStateCb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) != 0;
    const bool select = g_inputStateCb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT) != 0;
    const bool start = g_inputStateCb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START) != 0;
    const bool up = g_inputStateCb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) != 0;
    const bool down = g_inputStateCb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) != 0;
    const bool left = g_inputStateCb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) != 0;
    const bool right = g_inputStateCb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) != 0;

    if(port == 0) {
        g_emu.setController1Buttons(a, b, select, start, up, down, left, right);
    }
    else if(port == 1) {
        g_emu.setController2Buttons(a, b, select, start, up, down, left, right);
    }
}

void convertVideoFrame()
{
    const uint32_t* src = g_emu.getFramebuffer();

    for(size_t i = 0; i < g_videoFrame.size(); ++i) {
        const uint32_t p = src[i]; // 0xAABBGGRR
        const uint32_t r = (p & 0x000000FFu) << 16;
        const uint32_t g = (p & 0x0000FF00u);
        const uint32_t b = (p & 0x00FF0000u) >> 16;
        g_videoFrame[i] = r | g | b;
    }
}

void frontendMessage(const std::string& msg, unsigned frames)
{
    if(g_environmentCb == nullptr) return;

    retro_message m{};
    m.msg = msg.c_str();
    m.frames = frames;
    g_environmentCb(RETRO_ENVIRONMENT_SET_MESSAGE, &m);
}

void tryAddDbCandidate(unsigned command, std::vector<std::filesystem::path>& outCandidates)
{
    if(g_environmentCb == nullptr) return;

    const char* dir = nullptr;
    if(!g_environmentCb(command, &dir) || dir == nullptr || std::strlen(dir) == 0) return;

    outCandidates.push_back(std::filesystem::path(dir) / "db.txt");
}

void configureDatabasePath()
{
    namespace fs = std::filesystem;

    std::vector<fs::path> candidates;
    if(g_environmentCb != nullptr) {
        const char* systemDir = nullptr;
        if(g_environmentCb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &systemDir) && systemDir != nullptr && std::strlen(systemDir) > 0) {
            const fs::path base(systemDir);
            // Preferred libretro layout for per-core assets.
            candidates.push_back(base / "geranes" / "db.txt");
        }
    }

    tryAddDbCandidate(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, candidates);

    if(g_environmentCb != nullptr) {
        const char* corePath = nullptr;
        if(g_environmentCb(RETRO_ENVIRONMENT_GET_LIBRETRO_PATH, &corePath) && corePath != nullptr && std::strlen(corePath) > 0) {
            candidates.push_back(fs::path(corePath).parent_path() / "db.txt");
        }
    }

    candidates.push_back("db.txt");

    std::error_code ec;
    for(const auto& candidate : candidates) {
        if(fs::exists(candidate, ec)) {
            g_dbPath = candidate.string();
            GameDatabase::setDatabasePath(g_dbPath);
            return;
        }
    }

    // Fallback: use the database embedded in the core binary.
    const auto tempPath = fs::temp_directory_path() / "geranes_libretro_db.txt";
#if defined(GERANES_LIBRETRO_USE_CMRC_DB)
    try {
        auto fsDb = cmrc::libretro_db_assets::get_filesystem();
        cmrc::file dbFile = fsDb.exists("data/db.txt") ? fsDb.open("data/db.txt") : fsDb.open("db.txt");

        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if(out.is_open()) {
            out.write(dbFile.begin(), static_cast<std::streamsize>(dbFile.size()));
            if(out.good()) {
                g_dbPath = tempPath.string();
                GameDatabase::setDatabasePath(g_dbPath);
                frontendMessage("Using embedded db.txt fallback.", 180);
                return;
            }
        }
    }
    catch(...) {
    }
#else
    if(geranes_libretro_embedded_db_size > 0) {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if(out.is_open()) {
            out.write(reinterpret_cast<const char*>(geranes_libretro_embedded_db),
                      static_cast<std::streamsize>(geranes_libretro_embedded_db_size));
            if(out.good()) {
                g_dbPath = tempPath.string();
                GameDatabase::setDatabasePath(g_dbPath);
                frontendMessage("Using embedded db.txt fallback.", 180);
                return;
            }
        }
    }
#endif

    g_dbPath.clear();
    GameDatabase::setDatabasePath("db.txt");
}

}

RETRO_API void retro_set_environment(retro_environment_t cb)
{
    g_environmentCb = cb;
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb)
{
    g_videoCb = cb;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb)
{
    g_audioCb = cb;
}

RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
    g_audioBatchCb = cb;
}

RETRO_API void retro_set_input_poll(retro_input_poll_t cb)
{
    g_inputPollCb = cb;
}

RETRO_API void retro_set_input_state(retro_input_state_t cb)
{
    g_inputStateCb = cb;
}

RETRO_API void retro_init(void)
{
    if(!g_loggerBound) {
        Logger::instance().signalLog.bind(&LibretroLogSink::onLog, &g_logSink);
        g_loggerBound = true;
    }

    if(g_environmentCb != nullptr) {
        retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
        g_environmentCb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

        bool noGame = false;
        g_environmentCb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &noGame);
    }

    configureDatabasePath();
    g_audio.init();
}

RETRO_API void retro_deinit(void)
{
    if(g_gameLoaded) {
        g_emu.close();
        g_gameLoaded = false;
    }

    g_audio.reset();
}

RETRO_API unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info(struct retro_system_info* info)
{
    if(info == nullptr) return;

    info->library_name = "GeraNES";
    info->library_version = GERANES_VERSION;
    info->valid_extensions = "nes|fds|zip|ips|ups|bps";
    info->need_fullpath = true;
    info->block_extract = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info* info)
{
    if(info == nullptr) return;

    info->geometry.base_width = PPU::SCREEN_WIDTH;
    info->geometry.base_height = PPU::SCREEN_HEIGHT;
    info->geometry.max_width = PPU::SCREEN_WIDTH;
    info->geometry.max_height = PPU::SCREEN_HEIGHT;
    info->geometry.aspect_ratio = 4.0f / 3.0f;

    const auto fps = g_gameLoaded ? static_cast<double>(g_emu.getRegionFPS()) : 60.0;
    info->timing.fps = fps;
    info->timing.sample_rate = static_cast<double>(kSampleRate);
}

RETRO_API void retro_set_controller_port_device(unsigned, unsigned)
{
}

RETRO_API void retro_reset(void)
{
    if(g_gameLoaded) g_emu.reset();
}

RETRO_API void retro_run(void)
{
    if(g_inputPollCb != nullptr) g_inputPollCb();

    if(!g_gameLoaded) {
        if(g_videoCb != nullptr) {
            g_videoCb(nullptr, PPU::SCREEN_WIDTH, PPU::SCREEN_HEIGHT, 0);
        }
        return;
    }

    updateControllerState(0);
    updateControllerState(1);

    g_emu.updateUntilFrame(g_frameTimeMs);

    convertVideoFrame();

    if(g_videoCb != nullptr) {
        g_videoCb(g_videoFrame.data(), PPU::SCREEN_WIDTH, PPU::SCREEN_HEIGHT, PPU::SCREEN_WIDTH * sizeof(uint32_t));
    }

    g_audio.submit();
}

RETRO_API size_t retro_serialize_size(void)
{
    if(!g_gameLoaded) return 0;

    SerializationSize s;
    g_emu.serialization(s);
    return s.size();
}

RETRO_API bool retro_serialize(void* data, size_t size)
{
    if(!g_gameLoaded || data == nullptr) return false;

    Serialize s;
    g_emu.serialization(s);
    const auto& serialized = s.getData();

    if(size < serialized.size()) return false;

    std::memcpy(data, serialized.data(), serialized.size());
    return true;
}

RETRO_API bool retro_unserialize(const void* data, size_t size)
{
    if(!g_gameLoaded || data == nullptr || size == 0) return false;

    std::vector<uint8_t> state(size);
    std::memcpy(state.data(), data, size);
    g_emu.loadStateFromMemory(state);
    return true;
}

RETRO_API void retro_cheat_reset(void)
{
}

RETRO_API void retro_cheat_set(unsigned, bool, const char*)
{
}

RETRO_API bool retro_load_game(const struct retro_game_info* game)
{
    if(game == nullptr) {
        frontendMessage("Failed to load content: null game info.", 300);
        return false;
    }

    if(g_gameLoaded) {
        g_emu.close();
        g_gameLoaded = false;
    }

    g_audio.reset();

    bool loaded = false;

    if(game->path != nullptr && std::strlen(game->path) > 0) {
        loaded = g_emu.open(game->path);
    }
    else if(game->data != nullptr && game->size > 0) {
        std::string tempPath;
        if(writeTempRom(game->data, game->size, tempPath)) {
            g_tempRomPath = tempPath;
            loaded = g_emu.open(g_tempRomPath);
        }
        else {
            frontendMessage("Failed to load content: could not create temporary ROM file.", 300);
        }
    }
    else {
        frontendMessage("Failed to load content: no path or in-memory data provided.", 300);
    }

    g_gameLoaded = loaded;

    if(g_gameLoaded) updateTimingFromRegion();

    return g_gameLoaded;
}

RETRO_API bool retro_load_game_special(unsigned, const struct retro_game_info*, size_t)
{
    return false;
}

RETRO_API void retro_unload_game(void)
{
    if(g_gameLoaded) {
        g_emu.close();
    }

    g_gameLoaded = false;
    g_audio.reset();

    if(!g_tempRomPath.empty()) {
        std::error_code ec;
        std::filesystem::remove(g_tempRomPath, ec);
        g_tempRomPath.clear();
    }
}

RETRO_API unsigned retro_get_region(void)
{
    if(!g_gameLoaded) return RETRO_REGION_NTSC;

    return g_emu.region() == Settings::Region::PAL ? RETRO_REGION_PAL : RETRO_REGION_NTSC;
}

RETRO_API void* retro_get_memory_data(unsigned id)
{
    if(id == RETRO_MEMORY_SAVE_RAM) return nullptr;
    return nullptr;
}

RETRO_API size_t retro_get_memory_size(unsigned id)
{
    if(id == RETRO_MEMORY_SAVE_RAM) return 0;
    return 0;

}
