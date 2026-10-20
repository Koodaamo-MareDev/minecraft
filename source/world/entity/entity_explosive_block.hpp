#pragma once
#include <world/entity/entity_falling_block.hpp>
#include <world/entity/entity_explosive.hpp>

class EntityExplosiveBlock : public EntityFallingBlock, public EntityExplosive
{
public:
    EntityExplosiveBlock(BlockState block_state, const Vec3i &position, uint16_t fuse);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);
};