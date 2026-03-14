#include <cstdlib>
#include <iostream>
#include <string>

#include "GeraNES/defines.h"
#include "GeraNESApp/GeraNESApp.h"
#include "Test.h"

int main(int argc, char* argv[]) {
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
