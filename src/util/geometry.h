#pragma once

#include <glm/glm.hpp>

static bool pointInRect(const glm::vec2& p, const glm::vec2& min, const glm::vec2& max)
{
    return (p.x >= min.x && p.x <= max.x &&
            p.y >= min.y && p.y <= max.y);
}


static bool metersToPixels(float meters, glm::vec2& outPixels, glm::vec2 dpi = glm::vec2(96.0f))
{
    if (meters < 0.0f) {
        return false;
    }

    if (dpi.x <= 0.0f) return false;
    if (dpi.y <= 0.0f) return false;

    // 1 m = 100 cm ; 1 inch = 2.54 cm → metros * 100 / 2.54 = inches
    const float inches = meters * 100.0f / 2.54f;

    // pixels = inches * dpi
    outPixels.x = inches * dpi.x;
    outPixels.y = inches * dpi.y;

    return true;
}