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
uniform int Scanlines;
uniform bool GrayScale;

COMPAT_VARYING vec2 uv;

vec4 toGray(vec4 color)
{
    float c = (color.r+color.g+color.b)/3.0;
    return vec4(c,c,c,color.a);
}

void main() {

    if(Scanlines != 0) {

        gl_FragColor = vec4(0.0,0.0,0.0,1.0);

        float freq = float(Scanlines);
        float phase = freq*0.25;
        const float lineIntensity = 0.33;
        const float bright = 1.0 + 0.707/2.0 * 0.25;

        float value = sin(2.0 * M_PI * freq * uv.y + phase);
        value = clamp(value, 0.0, 1.0);
        value = 1.0 - lineIntensity *value;

        gl_FragColor.xyz += value * bright * COMPAT_TEXTURE(Texture,uv).xyz;
    }
    else gl_FragColor = COMPAT_TEXTURE(Texture,uv);

    if(GrayScale) gl_FragColor = toGray(gl_FragColor);

}

#endif


