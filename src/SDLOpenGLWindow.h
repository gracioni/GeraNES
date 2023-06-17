#ifndef INCLUDE_SDLOpenGLWindow
#define INCLUDE_SDLOpenGLWindow

#include <SDL.h>
#include <GL/gl.h>

#ifdef __MINGW32__
#include <windows.h>
#include <dwmapi.h>
#include <VersionHelpers.h>
#endif

#include "GeraNes/Logger.h"


class SDLOpenGLWindow {

private:

    SDL_Window* m_window = NULL;
    SDL_GLContext m_context = NULL;

    bool m_quit = false;


    //code from love2d
    void swapBuffers() {

    #ifdef __MINGW32__
        bool useDwmFlush = false;
        int swapInterval = getVSync();

        // https://github.com/love2d/love/issues/1628
        // VSync can interact badly with Windows desktop composition (DWM) in windowed mode. DwmFlush can be used instead
        // of vsync, but it's much less flexible so we're very conservative here with where it's used:
        // - It won't work with exclusive or desktop fullscreen.
        // - DWM refreshes don't always match the refresh rate of the monitor the window is in (or the requested swap
        //   interval), so we only use it when they do match.
        // - The user may force GL vsync, and DwmFlush shouldn't be used together with GL vsync.
        if (m_context != nullptr && !isFullScreen() && swapInterval == 1)
        {
            // Desktop composition is always enabled in Windows 8+. But DwmIsCompositionEnabled won't always return true...
            // (see DwmIsCompositionEnabled docs).
            BOOL compositionEnabled = IsWindows8OrGreater();
            if (compositionEnabled || (SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)) && compositionEnabled))
            {
                DWM_TIMING_INFO info = {};
                info.cbSize = sizeof(DWM_TIMING_INFO);
                double dwmRefreshRate = 0;
                if (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &info)))
                    dwmRefreshRate = (double)info.rateRefresh.uiNumerator / (double)info.rateRefresh.uiDenominator;

                SDL_DisplayMode dmode = {};
                int displayindex = SDL_GetWindowDisplayIndex(m_window);

                if (displayindex >= 0)
                    SDL_GetCurrentDisplayMode(displayindex, &dmode);

                if (dmode.refresh_rate > 0 && dwmRefreshRate > 0 && (fabs(dmode.refresh_rate - dwmRefreshRate) < 2))
                {
                    SDL_GL_SetSwapInterval(0);
                    if (SDL_GL_GetSwapInterval() == 0)
                        useDwmFlush = true;
                    else
                        SDL_GL_SetSwapInterval(swapInterval);
                }
            }
        }
    #endif

        SDL_GL_SwapWindow(m_window);

    #ifdef __MINGW32__
        if (useDwmFlush)
        {
            DwmFlush();
            SDL_GL_SetSwapInterval(swapInterval);
        }
    #endif
    }

public:

    bool create(const std::string& title, int x, int y, int w, int h, Uint32 flags) {

        Logger::instance().log("Initializing SDL window...", Logger::INFO);

        m_window = SDL_CreateWindow(title.c_str(), x, y, w, h, flags | SDL_WINDOW_OPENGL);

        if(m_window == NULL) {
            Logger::instance().log(std::string("SDL_CreateWindow error: ") + SDL_GetError(), Logger::ERROR2);
            return false;
        }          
        
        Logger::instance().log("Initializing GL context..", Logger::INFO);

        m_context = SDL_GL_CreateContext(m_window);

        if(m_context == NULL) {
            Logger::instance().log(std::string("SDL_GL_CreateContext error: ") + SDL_GetError(), Logger::ERROR2);
            return false;
        }

        SDL_GL_MakeCurrent(m_window, m_context);

        initGL();

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

        while(!m_quit)
        {
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

                            //case SDL_WINDOWEVENT_RESIZED:
                            case SDL_WINDOWEVENT_SIZE_CHANGED:

                                int windowWidth = event.window.data1;
                                int windowHeight = event.window.data2;
                                // std::cout << "[INFO] Window size: "
                                //           << windowWidth
                                //           << "x"
                                //           << windowHeight
                                //           << std::endl;
                                glViewport(0, 0, windowWidth, windowHeight);

                                break;
                        }
                        
                        break;                    
                    
                }
            }

            if(m_quit) continue; 

            paintGL();

            swapBuffers();  
        }
    }

    virtual bool onEvent(SDL_Event& e) {
        return false;
    }

    virtual void paintGL() {
    }

    virtual ~SDLOpenGLWindow() {
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

    void quit() {
        m_quit = true;
    }

    bool isFullScreen() {
        auto flags = SDL_GetWindowFlags(m_window);
        return flags & SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    bool setFullScreen(bool state) {
        int flags = state ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
        return SDL_SetWindowFullscreen(m_window, flags) == 0;   
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
            Logger::instance().log(std::string("SDL_CreateWindow error: ") + SDL_GetError(), Logger::ERROR2);
        }

        return mode.refresh_rate;
    }

    

};

#endif