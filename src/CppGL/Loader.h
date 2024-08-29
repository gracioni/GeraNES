#ifndef INCLUDE_Loader
#define INCLUDE_Loader

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#endif