#include "entity_explosive.hpp"
#include <world/world.hpp>

void EntityExplosive::tick()
{
    if (!fuse)
        explode();
    else
        fuse--;
}

void EntityExplosive::explode()
{
    dead = true;
    world->create_explosion(position + Vec3i(0, y_offset - y_size, 0), power);
}
