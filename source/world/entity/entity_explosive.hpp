#pragma once
#include <world/entity/entity_physical.hpp>

class EntityExplosive : virtual public EntityPhysical
{
public:
    uint16_t fuse = 80;
    float power = 4.0;
    EntityExplosive() {}

    virtual void tick();

    virtual void explode();
};