#include "GeraNESApp.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

extern "C" {

    void EMSCRIPTEN_KEEPALIVE processFile(int handler, const char* fileName, size_t fileSize, const uint8_t* fileContent) {
        reinterpret_cast<GeraNESApp*>(handler)->processFile(fileName, fileSize, fileContent);
    }

}

#endif
