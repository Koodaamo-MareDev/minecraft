#pragma once

#include <math/vec3i.hpp>

class World;

struct RedstoneUpdate
{
    World *world;
    Vec3i pos;
    uint32_t ticks;
};