#ifndef GL_HEADERS_H
#define GL_HEADERS_H

#ifdef __EMSCRIPTEN__
    #include <GLES3/gl3.h>
#else
    #include <GL/glew.h>
    #include <GL/gl.h>
    #include <GL/glext.h>
#endif

#endif