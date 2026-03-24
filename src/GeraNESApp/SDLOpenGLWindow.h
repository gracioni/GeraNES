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

#include "CppGL/GLHeaders.h"

#include "logger/logger.h"

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

    void swapBuffers() {
        SDL_GL_SwapWindow(m_window);
    }

    void syncDrawableSize(bool emitResizeEvent) {
        if (m_window == NULL) return;

        int drawableW = 0;
        int drawableH = 0;
        SDL_GL_GetDrawableSize(m_window, &drawableW, &drawableH);
        if (drawableW <= 0 || drawableH <= 0) return;

        if (drawableW == m_lastDrawableW && drawableH == m_lastDrawableH) return;

        m_lastDrawableW = drawableW;
        m_lastDrawableH = drawableH;

        glViewport(0, 0, drawableW, drawableH);

        if (!emitResizeEvent) return;

        int windowW = 0;
        int windowH = 0;
        SDL_GetWindowSize(m_window, &windowW, &windowH);

        SDL_Event resizeEvent;
        SDL_zero(resizeEvent);
        resizeEvent.type = SDL_WINDOWEVENT;
        resizeEvent.window.windowID = SDL_GetWindowID(m_window);
        resizeEvent.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
        resizeEvent.window.data1 = windowW;
        resizeEvent.window.data2 = windowH;
        SDL_PushEvent(&resizeEvent);
    }

    void notifyWindowsTitleBarInteractionChanged()
    {
#ifdef _WIN32
        onWindowsTitleBarInteractionChanged(m_inWindowsCaptionInteraction || m_inNativeMoveSizeLoop);
#endif
    }

    void mainLoop() {

        SDL_Event event;

        while(SDL_PollEvent(&event))
        {
            onEvent(event);                

            switch(event.type)
            {
                case SDL_QUIT:
                    quit();
                    break;                    

                case SDL_WINDOWEVENT:

                    switch(event.window.event) {

                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                            syncDrawableSize(false);
                            break;
                    }
                    
                    break;                    
                
            }
        }

        if(m_quit) return; 

        // Fullscreen transitions don't always arrive as SIZE_CHANGED in SDL web.
        syncDrawableSize(true);

        paintGL();

        swapBuffers(); 

    }

#ifdef _WIN32
    static constexpr const wchar_t* WINDOW_PROP_NAME = L"GeraNES.SDLOpenGLWindow.Instance";

    static LRESULT CALLBACK windowsSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto* self = reinterpret_cast<SDLOpenGLWindow*>(GetPropW(hwnd, WINDOW_PROP_NAME));
        if(self == nullptr || self->m_prevWndProc == nullptr) {
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        return self->handleWindowsMessage(hwnd, msg, wParam, lParam);
    }

    LRESULT handleWindowsMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if(!m_windowsNativePumpEnabled) {
            return CallWindowProcW(m_prevWndProc, hwnd, msg, wParam, lParam);
        }

        switch(msg) {
            case WM_NCLBUTTONDOWN:
                if(wParam == HTCAPTION) {
                    m_inWindowsCaptionInteraction = true;
                    notifyWindowsTitleBarInteractionChanged();
                }
                break;

            case WM_NCLBUTTONUP:
            case WM_CAPTURECHANGED:
                m_inWindowsCaptionInteraction = false;
                notifyWindowsTitleBarInteractionChanged();
                break;

            case WM_ENTERSIZEMOVE:
                m_inNativeMoveSizeLoop = true;
                notifyWindowsTitleBarInteractionChanged();
                m_lastNativeMoveSizePumpTick = 0;
                SetTimer(hwnd, NATIVE_MOVE_SIZE_TIMER_ID, static_cast<UINT>(NATIVE_MOVE_SIZE_PUMP_INTERVAL_MS), nullptr);
                nativeMoveSizeLoopStep(true);
                break;

            case WM_EXITSIZEMOVE:
                m_inNativeMoveSizeLoop = false;
                m_inWindowsCaptionInteraction = false;
                notifyWindowsTitleBarInteractionChanged();
                m_lastNativeMoveSizePumpTick = 0;
                KillTimer(hwnd, NATIVE_MOVE_SIZE_TIMER_ID);
                nativeMoveSizeLoopStep(true);
                break;

            case WM_MOVING:
            case WM_SIZING:
            case WM_PAINT:
                if(m_inNativeMoveSizeLoop) {
                    nativeMoveSizeLoopStep(false);
                }
                break;

            case WM_TIMER:
                if(m_inNativeMoveSizeLoop && wParam == NATIVE_MOVE_SIZE_TIMER_ID) {
                    nativeMoveSizeLoopStep(false);
                    return 0;
                }
                break;

            case WM_CLOSE:
            case WM_DESTROY:
            case WM_NCDESTROY:
                m_inNativeMoveSizeLoop = false;
                m_inWindowsCaptionInteraction = false;
                notifyWindowsTitleBarInteractionChanged();
                m_lastNativeMoveSizePumpTick = 0;
                KillTimer(hwnd, NATIVE_MOVE_SIZE_TIMER_ID);
                break;
        }

        return CallWindowProcW(m_prevWndProc, hwnd, msg, wParam, lParam);
    }

    void nativeMoveSizeLoopStep(bool force)
    {
        if(m_window == NULL || m_context == NULL) return;

        const Uint64 now = SDL_GetTicks64();
        if(!force && m_lastNativeMoveSizePumpTick != 0 &&
           (now - m_lastNativeMoveSizePumpTick) < NATIVE_MOVE_SIZE_PUMP_INTERVAL_MS) {
            return;
        }

        m_lastNativeMoveSizePumpTick = now;
        SDL_GL_MakeCurrent(m_window, m_context);
        syncDrawableSize(true);
        paintGL();
        swapBuffers();
    }

    void installWindowsSubclass()
    {
        if(m_window == NULL) return;

        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if(!SDL_GetWindowWMInfo(m_window, &wmInfo)) return;
        if(wmInfo.subsystem != SDL_SYSWM_WINDOWS) return;

        m_hwnd = wmInfo.info.win.window;
        if(m_hwnd == nullptr) return;

        SetPropW(m_hwnd, WINDOW_PROP_NAME, this);
        m_prevWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&windowsSubclassProc))
        );
    }

    void removeWindowsSubclass()
    {
        if(m_hwnd != nullptr) {
            KillTimer(m_hwnd, NATIVE_MOVE_SIZE_TIMER_ID);
            if(m_prevWndProc != nullptr) {
                SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_prevWndProc));
            }
            RemovePropW(m_hwnd, WINDOW_PROP_NAME);
        }

        m_hwnd = nullptr;
        m_prevWndProc = nullptr;
        m_inNativeMoveSizeLoop = false;
        m_inWindowsCaptionInteraction = false;
        m_lastNativeMoveSizePumpTick = 0;
    }

    void stopWindowsNativePump()
    {
        if(m_hwnd != nullptr) {
            KillTimer(m_hwnd, NATIVE_MOVE_SIZE_TIMER_ID);
        }
        m_inNativeMoveSizeLoop = false;
        m_inWindowsCaptionInteraction = false;
        m_lastNativeMoveSizePumpTick = 0;
    }
#endif

public:

    #ifdef __EMSCRIPTEN__
    SDLOpenGLWindow() {
        instance = this;
    }
    #endif

    bool create(const std::string& title, int x, int y, int w, int h, Uint32 flags) {

        Logger::instance().log("Initializing SDL window...", Logger::Type::INFO);

        m_window = SDL_CreateWindow(title.c_str(), x, y, w, h, flags | SDL_WINDOW_OPENGL);

        if(m_window == NULL) {
            Logger::instance().log(std::string("SDL_CreateWindow error: ") + SDL_GetError(), Logger::Type::ERROR);
            return false;
        }          
        
        Logger::instance().log("Initializing GL context...", Logger::Type::INFO);

        m_context = SDL_GL_CreateContext(m_window);

        if(m_context == NULL) {
            Logger::instance().log(std::string("SDL_GL_CreateContext error: ") + SDL_GetError(), Logger::Type::ERROR);
            return false;
        }

        SDL_GL_MakeCurrent(m_window, m_context);

        syncDrawableSize(false);

        initGL();

        #ifdef _WIN32
            installWindowsSubclass();
        #endif

        return true;
    }

    void setTitle(const std::string& title) { 
        if(m_window == NULL) return;
        SDL_SetWindowTitle(m_window, title.c_str());
    }

    virtual bool initGL() {
        return true;     
    }

    int width() {

        if(m_window == NULL) return 0;

        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        return w;
    }

    int height() {

        if(m_window == NULL) return 0;

        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        return h;
    }

    void run() {

        if(m_window == NULL) return;

        #ifdef __EMSCRIPTEN__

            // 0 fps means to use requestAnimationFrame; non-0 means to use setTimeout.
            emscripten_set_main_loop([]() {
                instance->mainLoop();
            }, 0, 1);           

        #else
            while(!m_quit)
            {
                mainLoop();
            }
        #endif
    }

    virtual bool onEvent(SDL_Event& e) {
        return false;
    }

    virtual void onWindowsTitleBarInteractionChanged(bool active) {
        (void)active;
    }

    virtual void paintGL() {
    }

    virtual ~SDLOpenGLWindow() {
        #ifdef _WIN32
            removeWindowsSubclass();
        #endif
        if(m_context != NULL) SDL_GL_DeleteContext(m_context);                   
        if(m_window != NULL) SDL_DestroyWindow(m_window);
        SDL_Quit();
    }

    void* glContext() {
        return m_context;
    }

    SDL_Window* sdlWindow() {
        return m_window;
    }

    void* nativeWindowHandle() const
    {
#ifdef _WIN32
        return m_hwnd;
#else
        return nullptr;
#endif
    }

    void quit() {
#ifdef _WIN32
        stopWindowsNativePump();
#endif
        m_quit = true;
    }

#ifdef _WIN32
    void setWindowsNativePumpEnabled(bool enabled)
    {
        if(m_windowsNativePumpEnabled == enabled) return;
        m_windowsNativePumpEnabled = enabled;
        if(!enabled) {
            stopWindowsNativePump();
        }
    }

    bool isWindowsTitleBarInteractionActive() const
    {
        return m_inWindowsCaptionInteraction || m_inNativeMoveSizeLoop;
    }
#else
    void setWindowsNativePumpEnabled(bool enabled)
    {
        (void)enabled;
    }

    bool isWindowsTitleBarInteractionActive() const
    {
        return false;
    }
#endif

    bool isFullScreen() {

        #ifdef __EMSCRIPTEN__
            int isFullscreen = EM_ASM_INT({
                if (typeof window !== 'undefined' && typeof window.geranesIsFullscreen === 'function') {
                    return window.geranesIsFullscreen() ? 1 : 0;
                }
                return !!(document.fullscreenElement || document.webkitFullscreenElement || document.msFullscreenElement);
            });
            return isFullscreen != 0;

        #else

            auto flags = SDL_GetWindowFlags(m_window);
            return flags & SDL_WINDOW_FULLSCREEN_DESKTOP;

        #endif
    }

    bool setFullScreen(bool state) {

        #ifdef __EMSCRIPTEN__
            int requested = EM_ASM_INT({
                var desired = !!$0;

                if (typeof window !== 'undefined' && typeof window.geranesSetFullscreen === 'function') {
                    try {
                        window.geranesSetFullscreen(desired);
                        return 1;
                    } catch (e) {
                        console.error("geranesSetFullscreen failed:", e);
                        return 0;
                    }
                }

                var canvas = (typeof Module !== 'undefined' && Module.canvas) ? Module.canvas : document.getElementById('canvas');
                if (!canvas) return 0;

                var current = !!(document.fullscreenElement || document.webkitFullscreenElement || document.msFullscreenElement);
                if (desired === current) return 1;

                if (desired) {
                    var req = canvas.requestFullscreen || canvas.webkitRequestFullscreen || canvas.msRequestFullscreen;
                    if (!req) return 0;
                    req.call(canvas);
                    return 1;
                }

                var exitFs = document.exitFullscreen || document.webkitExitFullscreen || document.msExitFullscreen;
                if (!exitFs) return 0;
                exitFs.call(document);
                return 1;
            }, state ? 1 : 0);

            return requested != 0;

        #else

            int flags = state ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
            return SDL_SetWindowFullscreen(m_window, flags) == 0;

        #endif
        
    }

    void minimizeWindow() {
        SDL_MinimizeWindow(m_window);
    }

    void restoreWindow() {
        SDL_RestoreWindow(m_window);
    }

    void maximizeWindow() {
        SDL_MaximizeWindow(m_window);
    }

    int getVSync() const
    {
        return m_context != nullptr ? SDL_GL_GetSwapInterval() : 0;
    }

    void setVSync(int vsync) const
    {
        if (m_context == nullptr) return;

	    SDL_GL_SetSwapInterval(vsync);

        // Check if adaptive vsync was requested but not supported, and fall back
        // to regular vsync if so.
        if (vsync == -1 && SDL_GL_GetSwapInterval() != -1)
            SDL_GL_SetSwapInterval(1);  
    
    }

    int getDisplayFrameRate() {

        SDL_DisplayMode mode;
        if(SDL_GetCurrentDisplayMode(0,&mode) != 0) {
            Logger::instance().log(std::string("SDL_CreateWindow error: ") + SDL_GetError(), Logger::Type::ERROR);
        }

        return mode.refresh_rate;
    }

    glm::vec2 GetWindowDPI()
    {
        const float baseDpi = 96.0f;

        int winW, winH;
        SDL_GetWindowSize(m_window, &winW, &winH);

        int drawW, drawH;
        SDL_GL_GetDrawableSize(m_window, &drawW, &drawH);
 
        if (winW == 0 || winH == 0)
            return glm::vec2(baseDpi, baseDpi);

        float scaleX = (float)drawW / (float)winW;
        float scaleY = (float)drawH / (float)winH;

        float dpiX = scaleX * baseDpi;
        float dpiY = scaleY * baseDpi;

        return glm::vec2(dpiX, dpiY);
    }  

};
