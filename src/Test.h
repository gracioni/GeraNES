#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "GeraNES/GeraNESEmu.h"
#include "logger/logger.h"
#include "signal/signal.h"

class Test
{
private:
    class ErrorLogForwarder : public SigSlot::SigSlotBase
    {
    public:
        void onLog(const std::string& msg, Logger::Type type)
        {
            if(type == Logger::Type::ERROR) {
                std::cerr << msg << std::endl;
            }
        }
    };

    static std::string readOutputText(GeraNESEmu& emu)
    {
        std::string out;
        out.reserve(1024);

        for(int i = 0; i < 0x1FFC; ++i) {
            const uint8_t c = emu.read(0x6004 + i);
            if(c == 0) break;
            out.push_back(static_cast<char>(c));
        }

        return out;
    }

public:
    static int runHeadless(const std::string& romPath)
    {
        ErrorLogForwarder errorLogForwarder;
        Logger::instance().signalLog.bind(&ErrorLogForwarder::onLog, &errorLogForwarder);

        GeraNESEmu emu;

        if(!emu.open(romPath) || !emu.valid()) {
            return 2;
        }

        // Do not use player speed-boost (3x). In test mode we run uncapped headless.
        emu.setSpeedBoost(false);
        emu.setPaused(false);

        constexpr uint32_t STEP_MS = 100; // max accepted by emulator update() path

        bool resetArmed = false;
        int resetCountdownMs = 0;

        constexpr int MAX_STEPS = 2'000'000;
        for(int step = 0; step < MAX_STEPS; ++step) {
            emu.update(STEP_MS);

            const uint8_t m1 = emu.read(0x6001);
            const uint8_t m2 = emu.read(0x6002);
            const uint8_t m3 = emu.read(0x6003);
            if(!(m1 == 0xDE && m2 == 0xB0 && m3 == 0x61)) {
                continue;
            }

            const uint8_t status = emu.read(0x6000);
            if(status == 0x80) {
                continue;
            }

            if(status == 0x81) {
                if(!resetArmed) {
                    resetArmed = true;
                    resetCountdownMs = 100;
                } else {
                    resetCountdownMs -= static_cast<int>(STEP_MS);
                }

                if(resetCountdownMs <= 0) {
                    emu.reset();
                    resetArmed = false;
                }
                continue;
            }

            std::cout << readOutputText(emu);
            return static_cast<int>(status);
        }

        return 3;
    }
};

