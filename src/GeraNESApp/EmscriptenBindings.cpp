#include "GeraNESApp.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace {

    ImGuiKey mapWebKeyToImGuiKey(const char* key) {
        if(key == nullptr || key[0] == '\0') {
            return ImGuiKey_None;
        }

        const std::string_view keyView(key);

        if(keyView == "Backspace") return ImGuiKey_Backspace;
        if(keyView == "Tab") return ImGuiKey_Tab;
        if(keyView == "Enter") return ImGuiKey_Enter;
        if(keyView == "Escape") return ImGuiKey_Escape;
        if(keyView == "Delete") return ImGuiKey_Delete;
        if(keyView == "Insert") return ImGuiKey_Insert;
        if(keyView == "Home") return ImGuiKey_Home;
        if(keyView == "End") return ImGuiKey_End;
        if(keyView == "PageUp") return ImGuiKey_PageUp;
        if(keyView == "PageDown") return ImGuiKey_PageDown;
        if(keyView == "ArrowLeft") return ImGuiKey_LeftArrow;
        if(keyView == "ArrowRight") return ImGuiKey_RightArrow;
        if(keyView == "ArrowUp") return ImGuiKey_UpArrow;
        if(keyView == "ArrowDown") return ImGuiKey_DownArrow;

        if(keyView.size() == 1) {
            const char c = keyView[0];

            if(c >= '0' && c <= '9') {
                return static_cast<ImGuiKey>(ImGuiKey_0 + (c - '0'));
            }

            if(c >= 'a' && c <= 'z') {
                return static_cast<ImGuiKey>(ImGuiKey_A + (c - 'a'));
            }

            if(c >= 'A' && c <= 'Z') {
                return static_cast<ImGuiKey>(ImGuiKey_A + (c - 'A'));
            }
        }

        return ImGuiKey_None;
    }

}

extern "C" {

    void EMSCRIPTEN_KEEPALIVE processUploadedFile(intptr_t handler, const char* fileName, size_t fileSize, const uint8_t* fileContent) {
        reinterpret_cast<GeraNESApp*>(handler)->processUploadedFile(fileName, fileSize, fileContent);
    }

    void EMSCRIPTEN_KEEPALIVE restartAudioModule(intptr_t handler) {
        reinterpret_cast<GeraNESApp*>(handler)->restartAudioModule();
    }

    void EMSCRIPTEN_KEEPALIVE onWebVisibilityChanged(intptr_t handler, int visible) {
        reinterpret_cast<GeraNESApp*>(handler)->onWebVisibilityChanged(visible != 0);
    }

    void EMSCRIPTEN_KEEPALIVE onWebAppUnload(intptr_t handler) {
        reinterpret_cast<GeraNESApp*>(handler)->onWebAppUnload();
    }

    void EMSCRIPTEN_KEEPALIVE onSessionImportComplete(intptr_t handler) {
        reinterpret_cast<GeraNESApp*>(handler)->onSessionImportComplete();
    }

    void EMSCRIPTEN_KEEPALIVE onModImportComplete(intptr_t handler) {
        reinterpret_cast<GeraNESApp*>(handler)->onModImportComplete();
    }

    void EMSCRIPTEN_KEEPALIVE injectWebTextUtf8(const char* text) {
        if(text == nullptr || text[0] == '\0') {
            return;
        }

        ImGui::GetIO().AddInputCharactersUTF8(text);
    }

    void EMSCRIPTEN_KEEPALIVE injectWebKeyEvent(const char* key, int down, int ctrl, int shift, int alt, int super) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl, ctrl != 0);
        io.AddKeyEvent(ImGuiMod_Shift, shift != 0);
        io.AddKeyEvent(ImGuiMod_Alt, alt != 0);
        io.AddKeyEvent(ImGuiMod_Super, super != 0);

        const ImGuiKey imguiKey = mapWebKeyToImGuiKey(key);
        if(imguiKey != ImGuiKey_None) {
            io.AddKeyEvent(imguiKey, down != 0);
        }
    }

}

#endif
