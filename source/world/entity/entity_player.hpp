#pragma once
#include <world/entity/entity_living.hpp>
#include <item/inventory.hpp>

class EntityPlayer : public EntityLiving
{
public:
    std::string player_name = "";
    int selected_hotbar_slot = 0;
    bool in_bed = false;
    Vec3i bed_pos = Vec3i(0, 0, 0);
    item::ItemStack equipment[5] = {};

    EntityPlayer(const Vec3f &position);

    virtual void hurt(int16_t damage);
};
