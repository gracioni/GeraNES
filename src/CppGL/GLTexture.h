#ifndef GL_TEXTURE_H
#define GL_TEXTURE_H

#include "GLHeaders.h"

class GLTexture {

private:

    GLuint textureID;

public:

    GLTexture() : textureID(0) {}

    void create() {
        glGenTextures(1, &textureID); 
    }

    // Método para bind a textura
    void bind(GLenum target = GL_TEXTURE_2D) const {    
        glBindTexture(target, textureID); 
    }

    void release(GLenum target = GL_TEXTURE_2D) const {
        if (textureID != 0) {
            glBindTexture(target, 0);
        }
    }

    ~GLTexture() {
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
        }
    }

    GLuint id() const {
        return textureID;
    }

    bool isValid() const {
        return textureID != 0;
    }

    void setFilters(GLenum minFilter, GLenum magFilter, GLenum target = GL_TEXTURE_2D) const {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, magFilter);     
    }

    void setClampMode(GLenum wrapS, GLenum wrapT, GLenum target = GL_TEXTURE_2D) const {
        glTexParameteri(target, GL_TEXTURE_WRAP_S, wrapS);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, wrapT);      
    }

};


#endif