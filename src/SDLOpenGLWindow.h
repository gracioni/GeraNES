#ifndef INCLUDE_SDLOpenGLWindow
#define INCLUDE_SDLOpenGLWindow

#include <SDL.h>
#include <GL/gl.h>

#ifdef __MINGW32__
#include <dwmapi.h>
#endif

#include "GeraNes/Logger.h"


class SDLOpenGLWindow {

private:

    SDL_Window* m_window = NULL;
    SDL_GLContext m_context = NULL;

    bool m_quit = false;

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

        #ifdef __MINGW32__
        HRESULT hr = DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
        if (!SUCCEEDED(hr)) {
        // log message or react in a different way
        }

        #endif

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
    
            SDL_GL_SwapWindow(m_window);     
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

    

};

#endif