#include "GeraNesUI/GeraNESApp.h"

int main(int argc, char* argv[]) {

    GeraNESApp app;

    app.create("GeraNES", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_RESIZABLE);

    app.run();

}
