#pragma once

#include <world/entity/entity_pathfinder.hpp>
#include <world/entity/entity_explosive.hpp>
#include <world/entity/entity_living.hpp>

class EntityCreeper : public EntityPathfinder, public EntityExplosive, public EntityLiving
{
public:
    EntityCreeper(const Vec3f &position);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);

    virtual bool should_jump();
};