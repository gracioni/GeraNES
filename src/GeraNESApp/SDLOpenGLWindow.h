#pragma once

#include <SDL.h>
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <SDL_syswm.h>
    #include <windows.h>
    #ifdef ERROR
        #undef ERROR
    #endif
#endif

#include <string>

#include <glm/vec2.hpp>

#include "CppGL/GLHeaders.h"

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#endif

#ifdef __EMSCRIPTEN__
class SDLOpenGLWindow;
extern SDLOpenGLWindow* instance;
#endif

class SDLOpenGLWindow {
private:
    SDL_Window* m_window = NULL;
    SDL_GLContext m_context = NULL;

    bool m_quit = false;
    int m_lastDrawableW = 0;
    int m_lastDrawableH = 0;
    int m_lastDisplayIndex = -1;
#ifdef _WIN32
    HWND m_hwnd = nullptr;
    WNDPROC m_prevWndProc = nullptr;
    bool m_windowsNativePumpEnabled = true;
    bool m_inNativeMoveSizeLoop = false;
    bool m_inWindowsCaptionInteraction = false;
    Uint64 m_lastNativeMoveSizePumpTick = 0;
    static constexpr Uint64 NATIVE_MOVE_SIZE_PUMP_INTERVAL_MS = 16;
    static constexpr UINT_PTR NATIVE_MOVE_SIZE_TIMER_ID = 0x474E4553; // 'GNES'
#endif

    void swapBuffers();
    void syncDrawableSize(bool emitResizeEvent);
    void syncDisplayIndex();
    void notifyWindowsTitleBarInteractionChanged();
    void mainLoop();

#ifdef _WIN32
    static constexpr const wchar_t* WINDOW_PROP_NAME = L"GeraNES.SDLOpenGLWindow.Instance";

    static LRESULT CALLBACK windowsSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleWindowsMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void nativeMoveSizeLoopStep(bool force);
    void installWindowsSubclass();
    void removeWindowsSubclass();
    void stopWindowsNativePump();
#endif

public:
    SDLOpenGLWindow();

    bool create(const std::string& title, int x, int y, int w, int h, Uint32 flags);
    void setTitle(const std::string& title);

    virtual bool initGL();

    int width();
    int height();

    void run();

    virtual bool onEvent(SDL_Event& e);
    virtual void onWindowsTitleBarInteractionChanged(bool active);
    virtual void onWindowDisplayChanged(int displayIndex);
    virtual void paintGL();

    virtual ~SDLOpenGLWindow();

    void* glContext();
    SDL_Window* sdlWindow();

    void* nativeWindowHandle() const;

    void quit();

#ifdef _WIN32
    void setWindowsNativePumpEnabled(bool enabled);
    bool isWindowsTitleBarInteractionActive() const;
#else
    void setWindowsNativePumpEnabled(bool enabled);
    bool isWindowsTitleBarInteractionActive() const;
#endif

    bool isFullScreen();
    bool setFullScreen(bool state, bool exclusive = false);

    void minimizeWindow();
    void restoreWindow();
    void maximizeWindow();

    bool isMinimized() const;

    int getVSync() const;
    void setVSync(int vsync) const;

    int getDisplayFrameRate();

    glm::vec2 GetWindowDPI();
};
