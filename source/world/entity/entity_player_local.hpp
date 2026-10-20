#pragma once
#include <world/entity/entity_player.hpp>

class EntityPlayerLocal : public EntityPlayer
{
public:
    BlockID in_fluid = BlockID::air;

    float mining_progress = 0.0f;
    int mining_tick = 0;

    int16_t health_update_tick = 0;

    Vec3f view_bob_offset = Vec3f(0, 0, 0);
    Vec3f view_bob_screen_offset = Vec3f(0, 0, 0);

    bool raycast_target_found = false;
    Vec3i raycast_target_pos = Vec3i(0, 0, 0);
    Vec3i raycast_target_face = Vec3i(0, 0, 0);
    AABB raycast_target_bounds;

    inventory::PlayerInventory items = inventory::PlayerInventory(45); // 9 special slots, 4 rows of 9 slots
    item::ItemStack *selected_item = nullptr;

    EntityPlayerLocal(const Vec3f &position);

    virtual void serialize(NBTTagCompound *result);

    virtual void deserialize(NBTTagCompound *result);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);

    virtual void animate();

    virtual bool should_jump();

    virtual bool can_remove();

    virtual void hurt(int16_t damage);
};
