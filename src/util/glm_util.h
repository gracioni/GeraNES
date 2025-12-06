#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

glm::vec2 static moveTowards(const glm::vec2& current,
                      const glm::vec2& target,
                      float maxDistanceDelta)
{
    glm::vec2 toVector = target - current;
    float distSq = glm::length2(toVector);

    if (distSq == 0.0f || distSq <= maxDistanceDelta * maxDistanceDelta)
        return target;

    float dist = glm::sqrt(distSq);
    return current + toVector / dist * maxDistanceDelta;
}
