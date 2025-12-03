#ifndef SDL_OPENGL_WINDOW_H
#define SDL_OPENGL_WINDOW_H

#include <SDL.h>

#include "CppGL/GLHeaders.h"

#include "GeraNES/Logger.h"

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/html5.h>
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

    void swapBuffers() {
        SDL_GL_SwapWindow(m_window);
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

                            int windowWidth = event.window.data1;
                            int windowHeight = event.window.data2;
                            glViewport(0, 0, windowWidth, windowHeight);

                            break;
                    }
                    
                    break;                    
                
            }
        }

        if(m_quit) return; 

        paintGL();

        swapBuffers(); 

    }

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

        #ifdef __EMSCRIPTEN__

            EmscriptenFullscreenChangeEvent fullscreenStatus;
            EMSCRIPTEN_RESULT result = emscripten_get_fullscreen_status(&fullscreenStatus);

            if (result == EMSCRIPTEN_RESULT_SUCCESS) {
                return fullscreenStatus.isFullscreen;
            }

            return false;

        #else

            auto flags = SDL_GetWindowFlags(m_window);
            return flags & SDL_WINDOW_FULLSCREEN_DESKTOP;

        #endif
    }

    bool setFullScreen(bool state) {

        #ifdef __EMSCRIPTEN__

            if(state) {

                EmscriptenFullscreenStrategy strategy;
                strategy.scaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF;
                strategy.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
                strategy.canvasResizedCallback = nullptr;
                strategy.canvasResizedCallbackUserData = nullptr;

                // Solicita o modo de tela cheia "proper fullscreen"
                EMSCRIPTEN_RESULT result = emscripten_request_fullscreen_strategy("canvas", EM_TRUE, &strategy);

                return result == EMSCRIPTEN_RESULT_SUCCESS;
            }
            else {

                EMSCRIPTEN_RESULT result = emscripten_exit_fullscreen();

                return result == EMSCRIPTEN_RESULT_SUCCESS;    
            }       

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

#endif