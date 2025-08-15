#ifndef _ENTITY_HPP_
#define _ENTITY_HPP_

#include <vector>
#include <wiiuse/wpad.h>
#include <math/vec3f.hpp>
#include <math/aabb.hpp>
#include <deque>

#include "block.hpp"
#include "model.hpp"
#include "inventory.hpp"
#include "nbt/nbt.hpp"

constexpr vfloat_t ENTITY_GRAVITY = 9.8f;

constexpr uint8_t creeper_fuse = 20;

class Chunk;

class EntityBase
{
public:
    Vec3f position;
    Vec3f velocity;

    EntityBase() : position(0, 0, 0), velocity(0, 0, 0) {}
    EntityBase(Vec3f position, Vec3f velocity) : position(position), velocity(velocity) {}
    EntityBase(Vec3f position) : position(position), velocity(0, 0, 0) {}
    virtual ~EntityBase() {}
};

class EntityPhysical : public EntityBase
{
public:
    enum class DragPhase : bool
    {
        before_friction = false,
        after_friction = true,
    };
    int32_t entity_id = 0;
    uint8_t type = 0;
    uint32_t ticks_existed = 0;
    AABB aabb;
    Vec3f rotation = Vec3f(0, 0, 0);
    Vec3f prev_position = Vec3f(0, 0, 0);
    Vec3f prev_rotation = Vec3f(0, 0, 0);
    Vec3f movement = Vec3f(0, 0, 0);
    vfloat_t width = 0;
    vfloat_t height = 0;
    vfloat_t y_offset = 0;
    vfloat_t y_size = 0;
    vfloat_t gravity = 0.08;
    Chunk *chunk = nullptr;
    bool on_ground = false;
    bool in_water = false;
    bool jumping = false;
    bool horizontal_collision = false;
    bool walk_sound = true;
    bool dead = false;
    bool simulate_offline = false;
    Vec3i server_pos = Vec3i(0, 0, 0);
    Vec3f animation_pos = Vec3f(0, 0, 0);
    Vec3f animation_rot = Vec3f(0, 0, 0);
    uint8_t animation_tick = 0;
    DragPhase drag_phase = DragPhase::before_friction;
    uint8_t light_level = 0;

    EntityPhysical() : EntityBase()
    {
        width = 1;
        height = 1;
    }

    ~EntityPhysical() {}

    virtual bool collides(EntityPhysical *other);

    virtual bool can_remove();

    virtual void resolve_collision(EntityPhysical *b);

    void teleport(Vec3f pos);

    // NOTE: The position should be provided in 1/32ths of a block
    void set_server_position(Vec3i pos, uint8_t ticks = 0);

    // NOTE: The position should be provided in 1/32ths of a block
    void set_server_pos_rot(Vec3i pos, Vec3f rot, uint8_t ticks);

    virtual void tick();

    virtual void animate();

    virtual void render(float partial_ticks, bool transparency);

    virtual size_t size()
    {
        return sizeof(*this);
    }

    virtual void serialize(NBTTagCompound *);

    virtual void deserialize(NBTTagCompound *nbt);

    std::vector<AABB> get_colliding_aabbs(const AABB &aabb);

    bool is_colliding_fluid(const AABB &aabb);

    void move_and_check_collisions();

    Vec3f get_position(vfloat_t partial_ticks)
    {
        return this->prev_position + (this->position - this->prev_position) * partial_ticks;
    }

    Vec3f get_rotation(vfloat_t partial_ticks)
    {
        return this->prev_rotation + (this->rotation - this->prev_rotation) * partial_ticks;
    }

    virtual bool should_jump()
    {
        return false;
    }

    virtual bool use_fluid_physics()
    {
        return true;
    }

    Vec3i get_head_blockpos()
    {
        return Vec3i(std::floor(position.x), std::floor(aabb.min.y + y_offset), std::floor(position.z));
    }

    Vec3i get_foot_blockpos()
    {
        return Vec3i(std::floor(position.x), std::floor(aabb.min.y - y_size), std::floor(position.z));
    }
};

class EntityLiving : virtual public EntityPhysical
{
public:
    uint16_t health = 20;
    uint16_t max_health = 20;
    uint16_t holding_item = 0;
    vfloat_t last_step_distance = 0;
    vfloat_t accumulated_walk_distance = 0;
    vfloat_t body_rotation_y = 0.0;

    EntityLiving() : EntityPhysical() {}

    virtual void hurt(uint16_t damage);

    virtual void tick();

    virtual void animate();

    virtual void render(float partial_ticks, bool transparency);

    virtual void serialize(NBTTagCompound *);
    virtual void deserialize(NBTTagCompound *nbt);
};

class EntityPathfinder : virtual public EntityPhysical
{
public:
    EntityPathfinder() {}
    EntityPhysical *follow_entity = nullptr;
    std::deque<Vec3i> path;
    Vec3i last_goal = Vec3i(0, 0, 0);
    Vec3f simple_pathfind(Vec3f target);
};

class EntityExplosive : virtual public EntityPhysical
{
public:
    uint16_t fuse = 80;
    float power = 4.0;
    EntityExplosive() {}

    virtual void tick();

    virtual void explode();
};

class EntityFallingBlock : virtual public EntityPhysical
{
public:
    uint16_t fall_time = 0;
    Block block_state;
    EntityFallingBlock(Block block_state, const Vec3i &position);

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

class EntityExplosiveBlock : public EntityFallingBlock, public EntityExplosive
{
public:
    EntityExplosiveBlock(Block block_state, const Vec3i &position, uint16_t fuse);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);
};

class EntityCreeper : public EntityPathfinder, public EntityExplosive, public EntityLiving
{
public:
    EntityCreeper(const Vec3f &position);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);

    virtual bool should_jump();
};

class EntityItem : virtual public EntityPhysical
{
public:
    inventory::ItemStack item_stack;
    EntityItem(const Vec3f &position, const inventory::ItemStack &item_stack);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);

    virtual void resolve_collision(EntityPhysical *b);

    virtual bool collides(EntityPhysical *other);

    virtual void pickup(Vec3f pos);

private:
    bool picked_up = false;
    Vec3f pickup_pos;
};

class EntityPlayer : public EntityLiving
{
public:
    std::string player_name = "";
    int selected_hotbar_slot = 0;
    bool in_bed = false;
    Vec3i bed_pos = Vec3i(0, 0, 0);
    inventory::ItemStack equipment[5] = {};

    EntityPlayer(const Vec3f &position);

    virtual void hurt(uint16_t damage);
};

class EntityPlayerLocal : public EntityPlayer
{
public:
    BlockID in_fluid = BlockID::air;

    float mining_progress = 0.0f;
    int mining_tick = 0;

    Vec3f view_bob_offset = Vec3f(0, 0, 0);
    Vec3f view_bob_screen_offset = Vec3f(0, 0, 0);

    bool raycast_target_found = false;
    Vec3i raycast_target_pos = Vec3i(0, 0, 0);
    Vec3i raycast_target_face = Vec3i(0, 0, 0);
    AABB raycast_target_bounds;

    inventory::PlayerInventory items = inventory::PlayerInventory(45); // 9 special slots, 4 rows of 9 slots
    inventory::ItemStack *selected_item = nullptr;

    EntityPlayerLocal(const Vec3f &position);

    virtual void serialize(NBTTagCompound *result);

    virtual void deserialize(NBTTagCompound *result);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);

    virtual void animate();

    virtual bool should_jump();

    virtual bool can_remove();
};

class EntityPlayerMp : public EntityPlayer
{
public:
    EntityPlayerMp(const Vec3f &position, std::string player_name);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);
};

#endif