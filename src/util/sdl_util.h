#pragma once

#include <SDL.h>
#include "CppGL/GLTexture.h"
#include <string>
#include <memory>
#include <stdexcept>
#include <istream>
#include <fstream>
#include <vector>
#include "stb_image.h"

static double_t getTime()
{
    return (double_t)SDL_GetTicks64()/1000.0;
    //return (float)SDL_GetTicks()/1000.0f; 
}

std::shared_ptr<GLTexture> loadImageFromMemory(const unsigned char* data, size_t size, bool flipVertically = true);

// Carrega a imagem a partir de uma std::istream (qualquer stream) e retorna shared_ptr<GLTexture>.
// A stream pode ser qualquer std::istream (std::ifstream, std::istringstream, etc).
std::shared_ptr<GLTexture> loadImageFromStream(std::istream& in, bool flipVertically = true);

// Conveniência: abre o arquivo e chama loadImageFromStreamShared
static std::shared_ptr<GLTexture> loadImageFromFile(const std::string& path, bool flipVertically = true)
{
    std::ifstream fin(path, std::ios::binary);
    if (!fin.is_open())
        return nullptr;

    return loadImageFromStream(fin, flipVertically);
}
