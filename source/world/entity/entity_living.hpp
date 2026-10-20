#pragma once
#include <world/entity/entity_physical.hpp>
#include <nbt/nbt.hpp>

class EntityLiving : virtual public EntityPhysical
{
public:
    int16_t hurt_ticks = 0;
    int16_t health = 20;
    int16_t max_health = 20;
    uint16_t holding_item = 0;
    vfloat_t fall_distance = 0;
    vfloat_t last_step_distance = 0;
    vfloat_t accumulated_walk_distance = 0;
    vfloat_t body_rotation_y = 0.0;
    float walk_speed = 0;
    float last_walk_speed = 0;

    EntityLiving() : EntityPhysical() {}

    virtual void fall(vfloat_t distance);

    virtual void hurt(int16_t damage);

    virtual void tick();

    virtual void animate();

    virtual void render(float partial_ticks, bool transparency);

    virtual void serialize(NBTTagCompound *);
    virtual void deserialize(NBTTagCompound *nbt);
};
