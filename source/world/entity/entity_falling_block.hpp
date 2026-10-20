#pragma once
#include <world/entity/entity_physical.hpp>
#include <block/block_properties.hpp>

class EntityFallingBlock : virtual public EntityPhysical
{
public:
    bool placed = false;
    uint16_t fall_time = 0;
    BlockState block_state;
    EntityFallingBlock(BlockState block_state, const Vec3i &position);

    size_t size()
    {
        return sizeof(*this);
    }

    virtual void resolve_collision(EntityPhysical *b) {}

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);

    virtual bool can_remove();

    virtual bool use_fluid_physics()
    {
        return false;
    }
};
