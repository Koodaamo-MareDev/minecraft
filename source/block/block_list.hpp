#pragma once

#include "block_base.hpp"
#include <math/vec3i.hpp>

class World;

extern BlockBase *block_list[256];

void init_blocks();
BlockBase *block_at(World *world, const Vec3i &pos);