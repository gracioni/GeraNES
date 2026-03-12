#include "GeraNESApp.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <cstdint>

extern "C" {

    void EMSCRIPTEN_KEEPALIVE processUploadedFile(intptr_t handler, const char* fileName, size_t fileSize, const uint8_t* fileContent) {
        reinterpret_cast<GeraNESApp*>(handler)->processUploadedFile(fileName, fileSize, fileContent);
    }

    void EMSCRIPTEN_KEEPALIVE restartAudioModule(intptr_t handler) {
        reinterpret_cast<GeraNESApp*>(handler)->restartAudioModule();
    }

    void EMSCRIPTEN_KEEPALIVE onSessionImportComplete(intptr_t handler) {
        reinterpret_cast<GeraNESApp*>(handler)->onSessionImportComplete();
    }   

}

#endif
