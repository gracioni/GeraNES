#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying
#define COMPAT_ATTRIBUTE attribute
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

#define M_PI 3.1415926535897932384626433832795


#ifdef VERTEX

COMPAT_ATTRIBUTE vec2 VertexCoord;
COMPAT_ATTRIBUTE vec2 TexCoord;

uniform mat4 MVPMatrix;

COMPAT_VARYING vec2 uv;

void main() {
    gl_Position = MVPMatrix * vec4(VertexCoord,0.0,1.0);
    uv = TexCoord;
}

#endif

#ifdef FRAGMENT

#ifdef GL_ES
    #ifdef GL_FRAGMENT_PRECISION_HIGH
    precision highp float;
    #else
    precision mediump float;
    #endif
#endif

uniform sampler2D Texture;

COMPAT_VARYING vec2 uv;

void main() {
    gl_FragColor = COMPAT_TEXTURE(Texture,uv);
}

#endif


