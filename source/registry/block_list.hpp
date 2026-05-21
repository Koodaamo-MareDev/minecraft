#pragma once

#include <block/block_base.hpp>
#include <math/vec3i.hpp>

namespace registry {
    void register_blocks();
}

class World;
extern BlockBase *block_list[256];
BlockBase *block_at(World *world, const Vec3i &pos);