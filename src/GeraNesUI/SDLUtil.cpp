#include "SDLUtil.h"

double_t GetTime()
{
    return (double_t)SDL_GetTicks64()/1000.0;
    //return (float)SDL_GetTicks()/1000.0f; 
}

/*
GLuint loadTextureFromFile(const char* path) {

    GLuint ret = 0;

    IMG_Init(IMG_INIT_PNG);

    SDL_Surface* surface = IMG_Load(path);

    glGenTextures(1, &ret);
    glBindTexture(GL_TEXTURE_2D, ret);    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    GLuint externalFormat, internalFormat;
    SDL_PixelFormat *format = surface->format;
    if (surface->format->BytesPerPixel == 4) {

        if (surface->format->Rmask == 0x000000ff)
            externalFormat = GL_RGBA;
        else
            externalFormat = GL_BGRA;
    }
    else {

        // no alpha
        if (surface->format->Rmask == 0x000000ff)
            externalFormat = GL_RGB;
        else
            externalFormat = GL_BGR;
    }
    internalFormat = (surface->format->BytesPerPixel == 4) ? GL_RGBA : GL_RGB;
	
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, surface->w, surface->h, 0, externalFormat, GL_UNSIGNED_BYTE, surface->pixels);

    glBindTexture(GL_TEXTURE_2D, 0);

    SDL_FreeSurface(surface);

    return ret;
}
*/