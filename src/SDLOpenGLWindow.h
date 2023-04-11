#ifndef INCLUDE_SDLOpenGLWindow
#define INCLUDE_SDLOpenGLWindow

#include <SDL.h>
#include <GL/gl.h>

class SDLOpenGLWindow {

private:

    SDL_Window* m_window;
    SDL_GLContext m_context;

    bool m_quit = false;

public:

    int create(const char *title, int x, int y, int w, int h, Uint32 flags) {

        m_window = SDL_CreateWindow(title, x, y, w, h, flags | SDL_WINDOW_OPENGL);
        
        m_context = SDL_GL_CreateContext(m_window);
        SDL_GL_MakeCurrent(m_window, m_context);        

        initGL();
    }

    virtual void initGL() {        
    }

    int width() {
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        return w;
    }

    int height() {
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        return h;
    }

    void run() {

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
        SDL_GL_DeleteContext(m_context);                   
        SDL_DestroyWindow(m_window);
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

};

#endif