#pragma once

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    #include <GLES3/gl3.h>
#else
    #include <GL/glew.h>
    #include <GL/gl.h>
    #include <GL/glext.h>
#endif
