#include "coord.hpp"
#include <world/chunk.hpp>

Coord::operator Vec3i() const
{
    if (!chunk)
        return Vec3i(coords.x, coords.y, coords.z);
    return Vec3i((chunk->x << 4) | coords.x, coords.y, (chunk->z << 4) | coords.z);
}