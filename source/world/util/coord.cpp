#include "coord.hpp"
#include <world/chunk.hpp>

Vec3i Coord::vec() const
{
    if (!chunk)
        return Vec3i(coords.x, coords.y, coords.z);
    return Vec3i((chunk->x << 4) | coords.x, coords.y, (chunk->z << 4) | coords.z);
}