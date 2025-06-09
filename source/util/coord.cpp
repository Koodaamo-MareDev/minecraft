#include "coord.hpp"
#include "chunk.hpp"

coord::operator vec3i() const
{
    if (!chunk)
        return vec3i(coords.x, coords.y, coords.z);
    return vec3i((chunk->x << 4) | coords.x, coords.y, (chunk->z << 4) | coords.z);
}