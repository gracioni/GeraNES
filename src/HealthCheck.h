#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "GeraNES/GeraNESEmu.h"
#include "GeraNES/PPU.h"
#include "logger/logger.h"
#include "signal/signal.h"

extern "C" {
    void* tdefl_write_image_to_png_file_in_memory(const void* pImage, int w, int h, int num_chans, size_t* pLen_out);
    void mz_free(void* p);
}

class HealthCheck
{
public:
    // Runs a deterministic headless ROM healthcheck, captures periodic screenshots,
    // and writes analysis artifacts for a later offline validation step.
    struct Options
    {
        std::string romPath;
        std::string outDir;
        uint32_t seed = 0xC0FFEEu;
        uint32_t simSeconds = 120;
        uint32_t screenshotIntervalSeconds = 10;
    };

private:
    struct Buttons
    {
        bool a = false;
        bool b = false;
        bool select = false;
        bool start = false;
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
    };

    class LogCollector : public SigSlot::SigSlotBase
    {
    public:
        std::vector<std::string> entries;

        void onLog(const std::string& msg, Logger::Type type)
        {
            std::ostringstream ss;
            ss << static_cast<int>(type) << "|" << msg;
            entries.push_back(ss.str());
        }
    };

    class DeterministicInputGenerator
    {
    private:
        uint32_t m_state = 0;
        Buttons m_buttons;
        int m_framesRemaining = 0;
        bool m_startupMode = true;

        uint32_t nextU32()
        {
            m_state ^= m_state << 13;
            m_state ^= m_state >> 17;
            m_state ^= m_state << 5;
            return m_state;
        }

        int nextRange(int minValue, int maxValue)
        {
            const uint32_t span = static_cast<uint32_t>(maxValue - minValue + 1);
            return minValue + static_cast<int>(nextU32() % span);
        }

        void clearButtons()
        {
            m_buttons = {};
        }

        void generateStartupAction()
        {
            clearButtons();

            const uint32_t roll = nextU32() % 100;
            if(roll < 25) {
                m_buttons.start = true;
            } else {
                if(roll < 70) m_buttons.a = true;
                if(roll >= 35) m_buttons.b = true;
            }

            m_framesRemaining = nextRange(2, 12);
        }

        void generateNextAction()
        {
            clearButtons();

            const uint32_t roll = nextU32() % 100;
            if(roll < 12) {
                m_buttons.start = true;
                m_framesRemaining = nextRange(2, 6);
                return;
            }

            if(roll < 18) {
                m_buttons.select = true;
                m_framesRemaining = nextRange(2, 6);
                return;
            }

            const uint32_t directionRoll = nextU32() % 8;
            switch(directionRoll) {
            case 0: m_buttons.up = true; break;
            case 1: m_buttons.down = true; break;
            case 2: m_buttons.left = true; break;
            case 3: m_buttons.right = true; break;
            case 4: m_buttons.up = true; m_buttons.right = true; break;
            case 5: m_buttons.up = true; m_buttons.left = true; break;
            case 6: m_buttons.down = true; m_buttons.right = true; break;
            case 7: m_buttons.down = true; m_buttons.left = true; break;
            }

            const uint32_t actionRoll = nextU32() % 100;
            if(actionRoll < 40) m_buttons.a = true;
            if(actionRoll >= 20 && actionRoll < 55) m_buttons.b = true;

            m_framesRemaining = nextRange(8, 90);
        }

    public:
        explicit DeterministicInputGenerator(uint32_t seed)
            : m_state(seed == 0 ? 0x6D2B79F5u : seed)
        {
        }

        const Buttons& buttonsForFrame(uint32_t frame, uint32_t fps)
        {
            const bool startupMode = frame <= (30u * std::max<uint32_t>(1, fps));
            if(startupMode != m_startupMode) {
                m_startupMode = startupMode;
                m_framesRemaining = 0;
            }

            if(m_framesRemaining <= 0) {
                if(m_startupMode) {
                    generateStartupAction();
                } else {
                    generateNextAction();
                }
            }
            --m_framesRemaining;
            return m_buttons;
        }
    };

    static uint64_t framebufferHash(const uint32_t* framebuffer)
    {
        constexpr uint64_t FNV_OFFSET = 1469598103934665603ull;
        constexpr uint64_t FNV_PRIME = 1099511628211ull;

        uint64_t hash = FNV_OFFSET;
        for(int i = 0; i < PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT; ++i) {
            uint32_t value = framebuffer[i];
            for(int b = 0; b < 4; ++b) {
                hash ^= static_cast<uint8_t>((value >> (b * 8)) & 0xFF);
                hash *= FNV_PRIME;
            }
        }
        return hash;
    }

    static uint32_t uniqueColorCount(const uint32_t* framebuffer)
    {
        std::vector<uint32_t> colors(framebuffer, framebuffer + PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT);
        std::sort(colors.begin(), colors.end());
        const auto it = std::unique(colors.begin(), colors.end());
        return static_cast<uint32_t>(std::distance(colors.begin(), it));
    }

    static uint32_t changedPixels(const uint32_t* prev, const uint32_t* current)
    {
        if(prev == nullptr || current == nullptr) return 0;

        uint32_t changed = 0;
        for(int i = 0; i < PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT; ++i) {
            if(prev[i] != current[i]) ++changed;
        }
        return changed;
    }

    static bool writePng(const std::filesystem::path& path, const uint32_t* framebuffer)
    {
        std::vector<uint8_t> rgb(PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT * 3);
        for(int i = 0; i < PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT; ++i) {
            const uint32_t pixel = framebuffer[i];
            rgb[(i * 3) + 0] = static_cast<uint8_t>(pixel & 0xFF);
            rgb[(i * 3) + 1] = static_cast<uint8_t>((pixel >> 8) & 0xFF);
            rgb[(i * 3) + 2] = static_cast<uint8_t>((pixel >> 16) & 0xFF);
        }

        size_t pngSize = 0;
        void* pngData = tdefl_write_image_to_png_file_in_memory(
            rgb.data(),
            PPU::SCREEN_WIDTH,
            PPU::SCREEN_HEIGHT,
            3,
            &pngSize
        );
        if(pngData == nullptr || pngSize == 0) return false;

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if(!out.is_open()) {
            mz_free(pngData);
            return false;
        }

        out.write(static_cast<const char*>(pngData), static_cast<std::streamsize>(pngSize));
        mz_free(pngData);
        return out.good();
    }

    static std::string buttonsToString(const Buttons& b)
    {
        std::string out;
        if(b.up) out += "U";
        if(b.down) out += "D";
        if(b.left) out += "L";
        if(b.right) out += "R";
        if(b.a) out += "A";
        if(b.b) out += "B";
        if(b.select) out += "S";
        if(b.start) out += "T";
        if(out.empty()) out = "-";
        return out;
    }

public:
    static int runHeadless(const Options& options)
    {
        namespace fs = std::filesystem;

        if(options.romPath.empty() || options.outDir.empty()) {
            std::cerr << "HealthCheck requires romPath and outDir." << std::endl;
            return 2;
        }

        const fs::path romPath = fs::absolute(fs::path(options.romPath)).lexically_normal();
        const std::string romFolderName = romPath.stem().string().empty() ? "rom" : romPath.stem().string();
        const fs::path outputRoot = fs::path(options.outDir) / romFolderName;

        fs::create_directories(outputRoot);
        fs::create_directories(outputRoot / "frames");

        LogCollector logCollector;
        Logger::instance().signalLog.bind(&LogCollector::onLog, &logCollector);

        GeraNESEmu emu(DummyAudioOutput::instance());
        if(!emu.open(options.romPath) || !emu.valid()) {
            return 2;
        }

        emu.setSpeedBoost(false);
        emu.setPaused(false);
        emu.enableOverclock(false);
        emu.disableSpriteLimit(false);

        DeterministicInputGenerator input(options.seed);

        const uint32_t fps = std::max<uint32_t>(1, emu.getFPS());
        const uint32_t totalFrames = options.simSeconds * fps;
        const uint32_t shotEveryFrames = std::max<uint32_t>(1, options.screenshotIntervalSeconds * fps);

        std::vector<uint32_t> prevFrame;
        std::vector<nlohmann::json> shots;
        std::vector<nlohmann::json> events;

        for(uint32_t frame = 1; frame <= totalFrames; ++frame) {
            const Buttons& buttons = input.buttonsForFrame(frame, fps);
            const std::string buttonsLabel = buttonsToString(buttons);
            emu.setController1Buttons(
                buttons.a,
                buttons.b,
                buttons.select,
                buttons.start,
                buttons.up,
                buttons.down,
                buttons.left,
                buttons.right
            );
            emu.setController2Buttons(false, false, false, false, false, false, false, false);
            emu.updateUntilFrame(0);

            if(frame == 1 || frame == totalFrames || (frame % shotEveryFrames) == 0) {
                const uint32_t* framebuffer = emu.getFramebuffer();
                std::ostringstream fileName;
                fileName << "frame_" << std::setw(6) << std::setfill('0') << frame << ".png";
                const fs::path screenshotPath = outputRoot / "frames" / fileName.str();
                writePng(screenshotPath, framebuffer);

                const uint64_t hash = framebufferHash(framebuffer);
                const uint32_t colors = uniqueColorCount(framebuffer);
                const uint32_t changed = prevFrame.empty() ? 0 : changedPixels(prevFrame.data(), framebuffer);

                shots.push_back({
                    {"frame", frame},
                    {"emuSeconds", static_cast<double>(frame) / static_cast<double>(fps)},
                    {"file", (fs::path("frames") / fileName.str()).generic_string()},
                    {"hashFnv64", hash},
                    {"uniqueColors", colors},
                    {"changedPixelsSinceLastShot", changed},
                    {"buttons", buttonsLabel}
                });

                prevFrame.assign(framebuffer, framebuffer + PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT);
            }

            if(frame == 1 || (frame % fps) == 0) {
                events.push_back({
                    {"frame", frame},
                    {"emuSeconds", static_cast<double>(frame) / static_cast<double>(fps)},
                    {"buttons", buttonsLabel}
                });
            }
        }

        nlohmann::json run = {
            {"romPath", romPath.string()},
            {"seed", options.seed},
            {"simSeconds", options.simSeconds},
            {"screenshotIntervalSeconds", options.screenshotIntervalSeconds},
            {"fps", fps},
            {"totalFrames", totalFrames},
            {"emulatorVersion", GERANES_VERSION},
            {"shots", shots},
            {"logLineCount", logCollector.entries.size()}
        };

        {
            std::ofstream out(outputRoot / "run.json", std::ios::binary | std::ios::trunc);
            out << run.dump(2);
        }

        {
            std::ofstream out(outputRoot / "events.jsonl", std::ios::binary | std::ios::trunc);
            for(const auto& event : events) {
                out << event.dump() << "\n";
            }
        }

        {
            std::ofstream out(outputRoot / "metrics.csv", std::ios::binary | std::ios::trunc);
            out << "frame,emu_seconds,file,hash_fnv64,unique_colors,changed_pixels_since_last_shot,buttons\n";
            for(const auto& shot : shots) {
                out << shot["frame"].get<uint32_t>() << ","
                    << shot["emuSeconds"].get<double>() << ","
                    << shot["file"].get<std::string>() << ","
                    << shot["hashFnv64"].get<uint64_t>() << ","
                    << shot["uniqueColors"].get<uint32_t>() << ","
                    << shot["changedPixelsSinceLastShot"].get<uint32_t>() << ","
                    << shot["buttons"].get<std::string>() << "\n";
            }
        }

        {
            std::ofstream out(outputRoot / "log.txt", std::ios::binary | std::ios::trunc);
            for(const std::string& line : logCollector.entries) {
                out << line << "\n";
            }
        }

        std::cout << (outputRoot / "run.json").string() << std::endl;
        return 0;
    }
};
