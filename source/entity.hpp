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

class chunk_t;

class entity_base
{
public:
    vec3f position;
    vec3f velocity;

    entity_base() : position(0, 0, 0), velocity(0, 0, 0) {}
    entity_base(vec3f position, vec3f velocity) : position(position), velocity(velocity) {}
    entity_base(vec3f position) : position(position), velocity(0, 0, 0) {}
    virtual ~entity_base() {}
};

class entity_physical : public entity_base
{
public:
    enum class drag_phase_t : bool
    {
        before_friction = false,
        after_friction = true,
    };
    int32_t entity_id = 0;
    uint8_t type = 0;
    uint32_t last_world_tick = 0;
    uint32_t ticks_existed = 0;
    aabb_t aabb;
    vec3f rotation = vec3f(0, 0, 0);
    vec3f prev_position = vec3f(0, 0, 0);
    vec3f prev_rotation = vec3f(0, 0, 0);
    vec3f movement = vec3f(0, 0, 0);
    vfloat_t width = 0;
    vfloat_t height = 0;
    vfloat_t y_offset = 0;
    vfloat_t y_size = 0;
    vfloat_t gravity = 0.08;
    chunk_t *chunk = nullptr;
    bool on_ground = false;
    bool in_water = false;
    bool jumping = false;
    bool horizontal_collision = false;
    bool walk_sound = true;
    bool dead = false;
    bool simulate_offline = false;
    vec3i server_pos = vec3i(0, 0, 0);
    vec3f animation_pos = vec3f(0, 0, 0);
    vec3f animation_rot = vec3f(0, 0, 0);
    uint8_t animation_tick = 0;
    drag_phase_t drag_phase = drag_phase_t::before_friction;
    uint8_t light_level = 0;

    entity_physical() : entity_base()
    {
        width = 1;
        height = 1;
    }

    ~entity_physical() {}

    virtual bool collides(entity_physical *other);

    virtual bool can_remove();

    virtual void resolve_collision(entity_physical *b);

    void teleport(vec3f pos);

    // NOTE: The position should be provided in 1/32ths of a block
    void set_server_position(vec3i pos, uint8_t ticks = 0);

    // NOTE: The position should be provided in 1/32ths of a block
    void set_server_pos_rot(vec3i pos, vec3f rot, uint8_t ticks);

    virtual void tick();

    virtual void animate();

    virtual void render(float partial_ticks, bool transparency);

    virtual size_t size()
    {
        return sizeof(*this);
    }

    virtual void serialize(NBTTagCompound *);

    virtual void deserialize(NBTTagCompound *nbt);

    std::vector<aabb_t> get_colliding_aabbs(const aabb_t &aabb);

    bool is_colliding_fluid(const aabb_t &aabb);

    void move_and_check_collisions();

    vec3f get_position(vfloat_t partial_ticks)
    {
        return this->prev_position + (this->position - this->prev_position) * partial_ticks;
    }

    vec3f get_rotation(vfloat_t partial_ticks)
    {
        return this->prev_rotation + (this->rotation - this->prev_rotation) * partial_ticks;
    }

    virtual bool should_jump()
    {
        return false;
    }

    vec3i get_head_blockpos()
    {
        return vec3i(std::floor(position.x), std::floor(aabb.min.y + y_offset), std::floor(position.z));
    }

    vec3i get_foot_blockpos()
    {
        return vec3i(std::floor(position.x), std::floor(aabb.min.y - y_size), std::floor(position.z));
    }
};

class entity_living : virtual public entity_physical
{
public:
    uint16_t health = 20;
    uint16_t max_health = 20;
    uint16_t holding_item = 0;
    vfloat_t last_step_distance = 0;
    vfloat_t accumulated_walk_distance = 0;
    vfloat_t body_rotation_y = 0.0;

    entity_living() : entity_physical() {}

    virtual void hurt(uint16_t damage);

    virtual void tick();

    virtual void animate();

    virtual void render(float partial_ticks, bool transparency);

    virtual void serialize(NBTTagCompound *);
    virtual void deserialize(NBTTagCompound *nbt);
};

class entity_pathfinder : virtual public entity_physical
{
public:
    entity_pathfinder() {}
    entity_physical *follow_entity = nullptr;
    std::deque<vec3i> path;
    vec3i last_goal = vec3i(0, 0, 0);
    vec3f simple_pathfind(vec3f target);
};

class entity_explosive : virtual public entity_physical
{
public:
    uint16_t fuse = 80;
    float power = 4.0;
    entity_explosive() {}

    virtual void tick();

    virtual void explode();
};

class entity_falling_block : virtual public entity_physical
{
public:
    uint16_t fall_time = 0;
    block_t block_state;
    entity_falling_block(block_t block_state, const vec3i &position);

    size_t size()
    {
        return sizeof(*this);
    }

    virtual void resolve_collision(entity_physical *b) {}

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);
};

class entity_explosive_block : public entity_falling_block, public entity_explosive
{
public:
    entity_explosive_block(block_t block_state, const vec3i &position, uint16_t fuse);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);
};

class entity_creeper : public entity_pathfinder, public entity_explosive, public entity_living
{
public:
    entity_creeper(const vec3f &position);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);

    virtual bool should_jump();
};

class entity_item : public entity_physical
{
public:
    inventory::item_stack item_stack;
    entity_item(const vec3f &position, const inventory::item_stack &item_stack);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);

    virtual void resolve_collision(entity_physical *b);

    virtual bool collides(entity_physical *other);

    virtual void pickup(vec3f pos);

private:
    bool picked_up = false;
    vec3f pickup_pos;
};

class entity_player : public entity_living
{
public:
    int selected_hotbar_slot = 0;
    bool in_bed = false;
    vec3i bed_pos = vec3i(0, 0, 0);

    entity_player(const vec3f &position);

    virtual void hurt(uint16_t damage);
};

class entity_player_local : public entity_player
{
public:
    entity_player_local(const vec3f &position);

    virtual void serialize(NBTTagCompound *result);

    virtual void deserialize(NBTTagCompound *result);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);

    virtual void animate();

    virtual bool should_jump();

    virtual bool can_remove();
};

class entity_player_mp : public entity_player
{
public:
    std::string player_name = "";
    entity_player_mp(const vec3f &position, std::string player_name);

    virtual void tick();

    virtual void render(float partial_ticks, bool transparency);
};

#endif