#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "GeraNES/defines.h"
#include "GeraNESApp/GeraNESApp.h"
#include "Test.h"

int main(int argc, char* argv[]) {
    const std::filesystem::path originalCwd = std::filesystem::current_path();
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
            }
        }
    }
    catch(...) {
        // Keep startup resilient; fallback to current process working directory.
    }

    if(argc >= 2 && std::string(argv[1]) == "--version") {
        std::cout << GERANES_VERSION << std::endl;
        return EXIT_SUCCESS;
    }

    if(argc >= 3 && std::string(argv[1]) == "--test") {
        return Test::runHeadless(argv[2]);
    }

    GeraNESApp app;

    app.create(GERANES_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_RESIZABLE);

    app.run();

    return EXIT_SUCCESS;
}
