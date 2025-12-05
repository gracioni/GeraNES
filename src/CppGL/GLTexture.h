#pragma once

#include "GLHeaders.h"
#include <cstdio>
#include <cstddef>
#include <utility>
#include <cmath>
#include <algorithm>

class GLTexture {
private:
    GLuint textureID = 0;

    // metadata
    int width_ = 0;
    int height_ = 0;
    int levels_ = 1; // mipmap levels
    GLenum internalFormat_ = GL_RGBA8;
    GLenum format_ = GL_RGBA;
    GLenum type_ = GL_UNSIGNED_BYTE;
    bool hasMipmaps_ = false;

    // last set parameters (for convenience/debug)
    GLenum minFilter_ = GL_LINEAR;
    GLenum magFilter_ = GL_LINEAR;
    GLenum wrapS_ = GL_CLAMP_TO_EDGE;
    GLenum wrapT_ = GL_CLAMP_TO_EDGE;

public:
    GLTexture() = default;

    // Non-copyable, movable
    GLTexture(const GLTexture&) = delete;
    GLTexture& operator=(const GLTexture&) = delete;

    GLTexture(GLTexture&& other) noexcept {
        moveFrom(std::move(other));
    }
    GLTexture& operator=(GLTexture&& other) noexcept {
        if (this != &other) {
            release();
            moveFrom(std::move(other));
        }
        return *this;
    }

    ~GLTexture() {
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
            textureID = 0;
        }
    }

    void create() {
        if (textureID == 0) glGenTextures(1, &textureID);
    }

    void bind(GLenum target = GL_TEXTURE_2D) const {
        glBindTexture(target, textureID);
    }

    void release(GLenum target = GL_TEXTURE_2D) const {
        if (textureID != 0) glBindTexture(target, 0);
    }

    GLuint id() const { return textureID; }
    bool isValid() const { return textureID != 0; }

    // metadata getters
    int width() const { return width_; }
    int height() const { return height_; }
    int levels() const { return levels_; }
    bool hasMipmaps() const { return hasMipmaps_; }
    GLenum internalFormat() const { return internalFormat_; }
    GLenum format() const { return format_; }
    GLenum type() const { return type_; }

    // set basic params (updates cached values too)
    void setFilters(GLenum minFilter, GLenum magFilter, GLenum target = GL_TEXTURE_2D) {
        bind(target);
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, magFilter);
        minFilter_ = minFilter;
        magFilter_ = magFilter;
        release(target);
    }

    void setClampMode(GLenum wrapS, GLenum wrapT, GLenum target = GL_TEXTURE_2D) {
        bind(target);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, wrapS);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, wrapT);
        wrapS_ = wrapS;
        wrapT_ = wrapT;
        release(target);
    }

    // Upload pixels (assume pixels point to correct layout matching format_/type_)
    bool upload2D(int w, int h, const void* pixels,
              GLenum internalFormat = GL_RGBA8, GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE,
              bool generateMipmaps = false, GLenum target = GL_TEXTURE_2D)
    {
        create();
        bind(target);

        // store basic metadata
        width_ = w;
        height_ = h;
        type_ = type;

        // helper: is power of two
        auto isPowerOfTwo = [](int v)->bool { return v > 0 && ( (v & (v - 1)) == 0 ); };
        bool pot = isPowerOfTwo(w) && isPowerOfTwo(h);

        // Decide internalFormat handling depending on platform.
        // On Emscripten/WebGL1 internalFormat must equal format (GL_RGBA etc).
        #ifdef __EMSCRIPTEN__
            internalFormat_ = format; // WebGL1-safe
        #else
            internalFormat_ = internalFormat; // Desktop: allow GL_RGBA8 etc.
        #endif
        format_ = format;

        // Decide whether we'll actually generate mipmaps.
        // - Desktop: respect generateMipmaps (GL supports NPOT mipmaps in modern GL)
        // - Emscripten/WebGL1: only generate mipmaps if POT
        bool willGenerateMipmaps = generateMipmaps;
        #ifdef __EMSCRIPTEN__
            willGenerateMipmaps = generateMipmaps && pot;
        #endif

        hasMipmaps_ = willGenerateMipmaps;
        levels_ = willGenerateMipmaps ? (1 + static_cast<int>(std::floor(std::log2(static_cast<float>(std::max(w,h)))))) : 1;

        // alignment safe (caller may want to set elsewhere)
        GLint oldAlign = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldAlign);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Set explicit texture parameters (avoid driver defaults that bite on WebGL).
        // Wrap: for portability use CLAMP_TO_EDGE (safe for NPOT); desktop can handle REPEAT if you want,
        // but CLAMP_TO_EDGE is harmless and simpler.
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // MAG filter always set
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // MIN filter: choose depending on whether we will create mipmaps.
        if (willGenerateMipmaps) {
            // If generating mipmaps use trilinear as default; desktop and WebGL2 support this.
            // On WebGL1 we've ensured willGenerateMipmaps==false for NPOT, so this is safe.
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        } else {
            // No mipmaps -> use a non-mipmap min filter (required for NPOT in WebGL1)
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }

        // Upload image data. On Emscripten we ensured internalFormat_ == format_ above.
        glTexImage2D(target, 0, internalFormat_, w, h, 0, format_, type_, pixels);

        // Generate mipmaps when allowed (desktop or POT on WebGL1)
        if (willGenerateMipmaps) {
            glGenerateMipmap(target);
        }

        // restore alignment
        glPixelStorei(GL_UNPACK_ALIGNMENT, oldAlign);

        release(target);

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            // Use your project's logging if available. fprintf goes to stdout/stderr visible in browser console.
            fprintf(stderr, "GL error after upload2D: 0x%X (w=%d h=%d pot=%d requestedMip=%d willGen=%d)\n",
                    static_cast<unsigned int>(err), w, h, (int)pot, (int)generateMipmaps, (int)willGenerateMipmaps);
        
            return false;
        }

        return true;
    }


    // convenience: estimate GPU size in bytes (very rough)
    std::size_t gpuSizeBytes() const {
        if (!isValid() || width_ <= 0 || height_ <= 0) return 0;
        int bpp = 4; // assume 4 bytes per pixel for RGBA8
        // compressed formats would need special handling
        return static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * bpp * static_cast<std::size_t>(levels_);
    }

private:
    void moveFrom(GLTexture&& o) noexcept {
        textureID = o.textureID;
        width_ = o.width_;
        height_ = o.height_;
        levels_ = o.levels_;
        internalFormat_ = o.internalFormat_;
        format_ = o.format_;
        type_ = o.type_;
        hasMipmaps_ = o.hasMipmaps_;
        minFilter_ = o.minFilter_;
        magFilter_ = o.magFilter_;
        wrapS_ = o.wrapS_;
        wrapT_ = o.wrapT_;

        o.textureID = 0;
        o.width_ = o.height_ = 0;
        o.levels_ = 1;
    }

    void release() noexcept {
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
            textureID = 0;
        }
    }
};
