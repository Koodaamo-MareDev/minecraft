#pragma once

#include <world/entity/entity_physical.hpp>
#include <item/item_stack.hpp>

class EntityItem : virtual public EntityPhysical
{
public:
    item::ItemStack item_stack;
    EntityItem(const Vec3f &position, const item::ItemStack &item_stack);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);

    virtual void resolve_collision(EntityPhysical *b);

    virtual bool collides(EntityPhysical *other);

    virtual void pickup(Vec3f pos);

private:
    bool picked_up = false;
    Vec3f pickup_pos;
};
