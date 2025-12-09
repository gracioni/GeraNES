#include "GeraNESApp.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

extern "C" {

    void EMSCRIPTEN_KEEPALIVE processUploadedFile(int handler, const char* fileName, size_t fileSize, const uint8_t* fileContent) {
        reinterpret_cast<GeraNESApp*>(handler)->processUploadedFile(fileName, fileSize, fileContent);
    }

    void EMSCRIPTEN_KEEPALIVE restartAudioModule(int handler) {
        reinterpret_cast<GeraNESApp*>(handler)->restartAudioModule();
    }

    void EMSCRIPTEN_KEEPALIVE onSessionImportComplete(int handler) {
        reinterpret_cast<GeraNESApp*>(handler)->onSessionImportComplete();
    }   

}

#endif
