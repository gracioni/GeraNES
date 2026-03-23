#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

#include "GeraNES/defines.h"
#include "GeraNESApp/GeraNESApp.h"
#include "HealthCheck.h"
#include "Test.h"

namespace
{
    void printHelp()
    {
        std::cout
            << GERANES_NAME << " " << GERANES_VERSION << "\n\n"
            << "Usage:\n"
            << "  GeraNES\n"
            << "  GeraNES --help\n"
            << "  GeraNES --version\n"
            << "  GeraNES --test <rom_path>\n"
            << "  GeraNES --healthcheck <rom_path> <out_dir> [--seed <n>] [--sim-seconds <n>] [--shot-interval <n>]\n\n"
            << "Commands:\n"
            << "  --help         Show this help text.\n"
            << "  --version      Print emulator version.\n"
            << "  --test         Run the existing headless test mode for one ROM.\n"
            << "  --healthcheck  Run deterministic headless health-check mode and export artifacts.\n\n"
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
    if(argc >= 3 && std::string(argv[1]) == "--test" && argv[2] && argv[2][0] != '\0') {
        const std::filesystem::path inputTestRomPath = argv[2];
        testRomPath = inputTestRomPath.is_absolute() ? inputTestRomPath : (originalCwd / inputTestRomPath).lexically_normal();
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

    if(argc >= 2 && std::string(argv[1]) == "--healthcheck") {
        if(argc < 4) {
            printHealthCheckUsage();
            return EXIT_FAILURE;
        }

        HealthCheck::Options options;
        options.romPath = argv[2];
        options.outDir = argv[3];

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
