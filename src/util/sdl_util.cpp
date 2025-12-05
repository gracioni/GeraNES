#include "sdl_util.h"

std::shared_ptr<GLTexture> loadImageFromMemory(const unsigned char* buffer, size_t size, bool flipVertically) {
    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load_from_memory(buffer, static_cast<int>(size), &w, &h, &ch, 0);
    if (!data)
        return nullptr;

    GLenum format = GL_RGB;
    GLenum internalFormat = GL_RGB;
    if (ch == 1)
        format = internalFormat = GL_RED;
    else if (ch == 3)
        format = internalFormat = GL_RGB;
    else if (ch == 4)
        format = internalFormat = GL_RGBA;
    else
        format = internalFormat = GL_RGBA; // fallback

    auto tex = std::make_shared<GLTexture>();
    tex->upload2D(w, h, data, internalFormat, format, GL_UNSIGNED_BYTE, true);

    stbi_image_free(data);

    return tex;
}

std::shared_ptr<GLTexture> loadImageFromStream(std::istream& in, bool flipVertically) 
{
    if (!in.good()) 
        return nullptr;

    if (flipVertically)
        stbi_set_flip_vertically_on_load(1);

    // Lê toda a stream para um buffer
    std::vector<unsigned char> buffer;

    in.seekg(0, std::ios::end);
    std::streampos endPos = in.tellg();
    if (endPos != -1) {
        std::size_t size = static_cast<std::size_t>(endPos);
        in.seekg(0, std::ios::beg);

        buffer.resize(size);
        in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size));

        std::streamsize got = in.gcount();
        if (got <= 0)
            return nullptr;

        if (static_cast<std::size_t>(got) != size)
            buffer.resize(static_cast<std::size_t>(got));

    } else {
        // stream não-seekable
        buffer.assign(
            std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()
        );
    }

    if (buffer.empty())
        return nullptr;

    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load_from_memory(buffer.data(), (int)buffer.size(), &w, &h, &ch, 0);
    if (!data)
        return nullptr;

    GLenum format = GL_RGB;
    GLenum internalFormat = GL_RGB;
    if (ch == 1)
        format = internalFormat = GL_RED;
    else if (ch == 3)
        format = internalFormat = GL_RGB;
    else if (ch == 4)
        format = internalFormat = GL_RGBA;
    else
        format = internalFormat = GL_RGBA; // fallback

    auto tex = std::make_shared<GLTexture>();
    tex->upload2D(w, h, data, internalFormat, format, GL_UNSIGNED_BYTE, true);

    stbi_image_free(data);

    return tex;
}