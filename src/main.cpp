#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

#include "GeraNES/defines.h"
#include "GeraNESApp/GeraNESApp.h"
#include "HealthCheck.h"
#include "NetplayTest.h"
#include "StateReplayTest.h"
#include "Test.h"

namespace
{
    std::filesystem::path resolveInputPath(const std::filesystem::path& originalCwd, const char* rawPath)
    {
        if(rawPath == nullptr || rawPath[0] == '\0') {
            return {};
        }

        const std::filesystem::path inputPath(rawPath);
        const std::filesystem::path resolved = inputPath.is_absolute()
            ? inputPath
            : (originalCwd / inputPath);
        return resolved.lexically_normal();
    }

    void printHelp()
    {
        std::cout
            << GERANES_NAME << " " << GERANES_VERSION << "\n\n"
            << "Usage:\n"
            << "  GeraNES\n"
            << "  GeraNES --help\n"
            << "  GeraNES --version\n"
            << "  GeraNES --test <rom_path>\n"
            << "  GeraNES --test-netplay <rom_path> [--frames <n>] [--input-delay <n>] [--rollback-window <n>] [--crc-interval <n>] [--report <path>] [--force-desync-frame <n>]\n"
            << "  GeraNES --test-state-replay <rom_path> [--frames <n>] [--replay-horizon <n>] [--extra-horizon <n>] [--seed <n>] [--extra-seed <n>] [--probe-stride <n>] [--from-frame <n>] [--robust] [--report <path>]\n"
            << "  GeraNES --healthcheck <rom_path> <out_dir> [--seed <n>] [--sim-seconds <n>] [--shot-interval <n>]\n\n"
            << "Commands:\n"
            << "  --help         Show this help text.\n"
            << "  --version      Print emulator version.\n"
            << "  --test         Run the existing headless test mode for one ROM.\n"
            << "  --test-netplay Run a deterministic in-process host/client netplay validation.\n"
            << "  --test-state-replay  Validate save/load + replay determinism from frame snapshots.\n"
            << "  --healthcheck  Run deterministic headless health-check mode and export artifacts.\n\n"
            << "Netplay test options:\n"
            << "  --frames <n>              Frames to simulate. Default: 600\n"
            << "  --input-delay <n>         Session input delay in frames. Default: 2\n"
            << "  --rollback-window <n>     Snapshot/rollback history size. Default: 600\n"
            << "  --crc-interval <n>        CRC report interval in frames. Default: 10\n"
            << "  --report <path>           Write the JSON report to a file instead of stdout.\n"
            << "  --force-desync-frame <n>  Intentionally corrupt the client state on/after this frame.\n\n"
            << "State replay test options:\n"
            << "  --frames <n>              Frames to simulate in the baseline run. Default: 180\n"
            << "  --replay-horizon <n>      Frames to replay after each loaded snapshot. Default: 3\n"
            << "  --seed <n>                Deterministic input seed. Default: 324478056\n"
            << "  --extra-horizon <n>       Add another replay horizon to the same invocation.\n"
            << "  --extra-seed <n>          Add another deterministic input seed to the same invocation.\n"
            << "  --probe-stride <n>        Test every Nth snapshot instead of every snapshot. Default: 1\n"
            << "  --from-frame <n>          Test only a single snapshot starting at frame N.\n"
            << "  --robust                  Add a small built-in matrix of extra seeds/horizons.\n"
            << "  --report <path>           Write the JSON report to a file instead of stdout.\n\n"
            << "Healthcheck options:\n"
            << "  <out_dir>            Parent output folder. A subfolder with the ROM name is created automatically.\n"
            << "  --seed <n>           Deterministic input seed. Default: 12648430\n"
            << "  --sim-seconds <n>    Emulated duration in seconds. Default: 120\n"
            << "  --shot-interval <n>  Screenshot interval in emulated seconds. Default: 10\n";
    }

    void printHealthCheckUsage()
    {
        std::cerr
            << "Usage:\n"
            << "  GeraNES --healthcheck <rom_path> <out_dir> [--seed <n>] [--sim-seconds <n>] [--shot-interval <n>]\n"
            << "  Note: artifacts are written to <out_dir>/<rom_name>/\n";
    }

    void printNetplayTestUsage()
    {
        std::cerr
            << "Usage:\n"
            << "  GeraNES --test-netplay <rom_path> [--frames <n>] [--input-delay <n>] [--rollback-window <n>] [--crc-interval <n>] [--report <path>] [--force-desync-frame <n>]\n";
    }

    void printStateReplayTestUsage()
    {
        std::cerr
            << "Usage:\n"
            << "  GeraNES --test-state-replay <rom_path> [--frames <n>] [--replay-horizon <n>] [--extra-horizon <n>] [--seed <n>] [--extra-seed <n>] [--probe-stride <n>] [--from-frame <n>] [--robust] [--report <path>]\n";
    }

    bool parseUintArg(const char* value, uint32_t& outValue)
    {
        if(value == nullptr || value[0] == '\0') return false;

        char* end = nullptr;
        const unsigned long parsed = std::strtoul(value, &end, 10);
        if(end == value || (end != nullptr && *end != '\0')) return false;
        if(parsed > std::numeric_limits<uint32_t>::max()) return false;

        outValue = static_cast<uint32_t>(parsed);
        return true;
    }
}

int main(int argc, char* argv[]) {
    const std::filesystem::path originalCwd = std::filesystem::current_path();
    std::filesystem::path testRomPath;
    std::filesystem::path testNetplayRomPath;
    std::filesystem::path stateReplayRomPath;
    std::filesystem::path healthcheckRomPath;
    std::filesystem::path healthcheckOutDir;
    if(argc >= 2) {
        const std::string command = argv[1];
        if(command == "--test" && argc >= 3) {
            testRomPath = resolveInputPath(originalCwd, argv[2]);
        }
        else if(command == "--test-netplay" && argc >= 3) {
            testNetplayRomPath = resolveInputPath(originalCwd, argv[2]);
        }
        else if(command == "--test-state-replay" && argc >= 3) {
            stateReplayRomPath = resolveInputPath(originalCwd, argv[2]);
        }
        else if(command == "--healthcheck" && argc >= 4) {
            healthcheckRomPath = resolveInputPath(originalCwd, argv[2]);
            healthcheckOutDir = resolveInputPath(originalCwd, argv[3]);
        }
    }

    struct CwdRestoreGuard {
        std::filesystem::path path;
        ~CwdRestoreGuard() {
            try {
                if(!path.empty()) std::filesystem::current_path(path);
            }
            catch(...) {
            }
        }
    } cwdRestoreGuard{originalCwd};

    try {
        if(argc > 0 && argv[0] && argv[0][0] != '\0') {
            const std::filesystem::path exePath = std::filesystem::absolute(argv[0]);
            const std::filesystem::path exeDir = exePath.parent_path();
            if(!exeDir.empty()) {
                std::filesystem::current_path(exeDir);
                AppSettings::setStorageDirectory(exeDir);
            }
        }
    }
    catch(...) {
        // Keep startup resilient; fallback to current process working directory.
    }

    AppSettings::setStorageDirectory(std::filesystem::current_path());

    if(argc >= 2 && std::string(argv[1]) == "--help") {
        printHelp();
        return EXIT_SUCCESS;
    }

    if(argc >= 2 && std::string(argv[1]) == "--version") {
        std::cout << GERANES_VERSION << std::endl;
        return EXIT_SUCCESS;
    }

    if(argc >= 3 && std::string(argv[1]) == "--test") {
        return Test::runHeadless(testRomPath.empty() ? std::string(argv[2]) : testRomPath.string());
    }

    if(argc >= 3 && std::string(argv[1]) == "--test-netplay") {
        NetplayTest::Options options;
        options.romPath = testNetplayRomPath.empty() ? std::string(argv[2]) : testNetplayRomPath.string();

        for(int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            auto nextValue = [&](uint32_t& target) -> bool {
                if(i + 1 >= argc) return false;
                uint32_t parsed = 0;
                if(!parseUintArg(argv[i + 1], parsed)) return false;
                target = parsed;
                ++i;
                return true;
            };

            if(arg == "--frames") {
                if(!nextValue(options.frames) || options.frames == 0) {
                    std::cerr << "Invalid value for --frames.\n";
                    printNetplayTestUsage();
                    return EXIT_FAILURE;
                }
            }
            else if(arg == "--input-delay") {
                if(!nextValue(options.inputDelayFrames) || options.inputDelayFrames > 8) {
                    std::cerr << "Invalid value for --input-delay.\n";
                    printNetplayTestUsage();
                    return EXIT_FAILURE;
                }
            }
            else if(arg == "--rollback-window") {
                if(!nextValue(options.rollbackWindowFrames) || options.rollbackWindowFrames == 0) {
                    std::cerr << "Invalid value for --rollback-window.\n";
                    printNetplayTestUsage();
                    return EXIT_FAILURE;
                }
            }
            else if(arg == "--crc-interval") {
                if(!nextValue(options.crcIntervalFrames) || options.crcIntervalFrames == 0) {
                    std::cerr << "Invalid value for --crc-interval.\n";
                    printNetplayTestUsage();
                    return EXIT_FAILURE;
                }
            }
            else if(arg == "--force-desync-frame") {
                if(!nextValue(options.forceDesyncFrame) || options.forceDesyncFrame == 0) {
                    std::cerr << "Invalid value for --force-desync-frame.\n";
                    printNetplayTestUsage();
                    return EXIT_FAILURE;
                }
            }
            else if(arg == "--report") {
                if(i + 1 >= argc) {
                    std::cerr << "Missing value for --report.\n";
                    printNetplayTestUsage();
                    return EXIT_FAILURE;
                }
                options.reportPath = resolveInputPath(originalCwd, argv[i + 1]).string();
                ++i;
            }
            else {
                std::cerr << "Unknown --test-netplay argument: " << arg << "\n";
                printNetplayTestUsage();
                return EXIT_FAILURE;
            }
        }

        return NetplayTest::runHeadless(options);
    }

    if(argc >= 3 && std::string(argv[1]) == "--test-state-replay") {
        StateReplayTest::Options options;
        options.romPath = stateReplayRomPath.empty() ? std::string(argv[2]) : stateReplayRomPath.string();

        for(int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            auto nextValue = [&](uint32_t& target) -> bool {
                if(i + 1 >= argc) return false;
                uint32_t parsed = 0;
                if(!parseUintArg(argv[i + 1], parsed)) return false;
                target = parsed;
                ++i;
                return true;
            };

            if(arg == "--frames") {
                if(!nextValue(options.frames) || options.frames == 0) {
                    std::cerr << "Invalid value for --frames.\n";
                    printStateReplayTestUsage();
                    return EXIT_FAILURE;
                }
            }
            else if(arg == "--replay-horizon") {
                if(!nextValue(options.replayHorizon) || options.replayHorizon == 0) {
                    std::cerr << "Invalid value for --replay-horizon.\n";
                    printStateReplayTestUsage();
                    return EXIT_FAILURE;
                }
            }
            else if(arg == "--seed") {
                if(!nextValue(options.seed)) {
                    std::cerr << "Invalid value for --seed.\n";
                    printStateReplayTestUsage();
                    return EXIT_FAILURE;
                }
            }
            else if(arg == "--extra-seed") {
                uint32_t extraSeed = 0;
                if(!nextValue(extraSeed)) {
                    std::cerr << "Invalid value for --extra-seed.\n";
                    printStateReplayTestUsage();
                    return EXIT_FAILURE;
                }
                options.extraSeeds.push_back(extraSeed);
            }
            else if(arg == "--extra-horizon") {
                uint32_t extraHorizon = 0;
                if(!nextValue(extraHorizon) || extraHorizon == 0) {
                    std::cerr << "Invalid value for --extra-horizon.\n";
                    printStateReplayTestUsage();
                    return EXIT_FAILURE;
                }
                options.extraReplayHorizons.push_back(extraHorizon);
            }
            else if(arg == "--probe-stride") {
                if(!nextValue(options.probeStride) || options.probeStride == 0) {
                    std::cerr << "Invalid value for --probe-stride.\n";
                    printStateReplayTestUsage();
                    return EXIT_FAILURE;
                }
            }
            else if(arg == "--from-frame") {
                uint32_t fromFrame = 0;
                if(!nextValue(fromFrame)) {
                    std::cerr << "Invalid value for --from-frame.\n";
                    printStateReplayTestUsage();
                    return EXIT_FAILURE;
                }
                options.fromFrame = fromFrame;
            }
            else if(arg == "--robust") {
                options.robust = true;
            }
            else if(arg == "--report") {
                if(i + 1 >= argc) {
                    std::cerr << "Missing value for --report.\n";
                    printStateReplayTestUsage();
                    return EXIT_FAILURE;
                }
                options.reportPath = resolveInputPath(originalCwd, argv[i + 1]).string();
                ++i;
            }
            else {
                std::cerr << "Unknown --test-state-replay argument: " << arg << "\n";
                printStateReplayTestUsage();
                return EXIT_FAILURE;
            }
        }

        return StateReplayTest::runHeadless(options);
    }

    if(argc >= 2 && std::string(argv[1]) == "--healthcheck") {
        if(argc < 4) {
            printHealthCheckUsage();
            return EXIT_FAILURE;
        }

        HealthCheck::Options options;
        options.romPath = healthcheckRomPath.empty() ? std::string(argv[2]) : healthcheckRomPath.string();
        options.outDir = healthcheckOutDir.empty() ? std::string(argv[3]) : healthcheckOutDir.string();

        for(int i = 4; i < argc; ++i) {
            const std::string arg = argv[i];
            auto nextValue = [&](uint32_t& target) -> bool {
                if(i + 1 >= argc) return false;
                uint32_t parsed = 0;
                if(!parseUintArg(argv[i + 1], parsed)) return false;
                target = parsed;
                ++i;
                return true;
            };

            if(arg == "--seed") {
                if(!nextValue(options.seed)) {
                    std::cerr << "Invalid value for --seed.\n";
                    printHealthCheckUsage();
                    return EXIT_FAILURE;
                }
            }
            else if(arg == "--sim-seconds") {
                if(!nextValue(options.simSeconds) || options.simSeconds == 0) {
                    std::cerr << "Invalid value for --sim-seconds.\n";
                    printHealthCheckUsage();
                    return EXIT_FAILURE;
                }
            }
            else if(arg == "--shot-interval") {
                if(!nextValue(options.screenshotIntervalSeconds) || options.screenshotIntervalSeconds == 0) {
                    std::cerr << "Invalid value for --shot-interval.\n";
                    printHealthCheckUsage();
                    return EXIT_FAILURE;
                }
            }
            else {
                std::cerr << "Unknown --healthcheck argument: " << arg << "\n";
                printHealthCheckUsage();
                return EXIT_FAILURE;
            }
        }

        return HealthCheck::runHeadless(options);
    }

    GeraNESApp app;

    app.create(GERANES_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_RESIZABLE);

    app.run();

    return EXIT_SUCCESS;
}
