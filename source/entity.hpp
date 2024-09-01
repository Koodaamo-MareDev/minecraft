#ifndef _ENTITY_HPP_
#define _ENTITY_HPP_

#include <vector>
#include <wiiuse/wpad.h>
#include "vec3f.hpp"
#include "aabb.hpp"
#include "block.hpp"

constexpr vfloat_t ENTITY_GRAVITY = 9.8f;

class chunk_t;

class entity_t
{
public:
    vec3f position;
    vec3f velocity;

    entity_t() : position(0, 0, 0), velocity(0, 0, 0) {}
    entity_t(vec3f position, vec3f velocity) : position(position), velocity(velocity) {}
    entity_t(vec3f position) : position(position), velocity(0, 0, 0) {}
    virtual ~entity_t() {}
};

class aabb_entity_t : public entity_t
{
public:
    enum class drag_phase_t : bool
    {
        before_friction = false,
        after_friction = true,
    };
    aabb_t aabb;
    vec3f prev_position = vec3f(0, 0, 0);
    vfloat_t width = 0;
    vfloat_t height = 0;
    vfloat_t y_offset = 0;
    vfloat_t y_size = 0;
    vfloat_t last_step_distance = 0;
    vfloat_t gravity = 0.08;
    chunk_t *chunk = nullptr;
    bool local = false;
    bool on_ground = false;
    bool jumping = false;
    bool horizontal_collision = false;
    bool walk_sound = true;
    bool dead = false;
    drag_phase_t drag_phase = drag_phase_t::before_friction;
    uint8_t light_level = 0;

    aabb_entity_t(float width, float height) : entity_t(), width(width), height(height) {}

    ~aabb_entity_t() {}

    bool collides(aabb_entity_t *other);

    bool can_remove();

    virtual void resolve_collision(aabb_entity_t *b);

    void set_position(vec3f pos);

    virtual void tick();

    virtual void render(float partial_ticks) {}

    virtual size_t size()
    {
        return sizeof(*this);
    }

    std::vector<aabb_t> get_colliding_aabbs(const aabb_t &aabb);

    bool is_colliding_fluid(const aabb_t &aabb);

    void move_and_check_collisions();

    vec3f get_position(vfloat_t partial_ticks)
    {
        return this->prev_position + (this->position - this->prev_position) * partial_ticks;
    }
};

class falling_block_entity_t : public aabb_entity_t
{
public:
    uint16_t fall_time = 0;
    block_t block_state;
    falling_block_entity_t(block_t block_state, const vec3i &position);

    size_t size()
    {
        return sizeof(*this);
    }

    virtual void resolve_collision(aabb_entity_t *b) {}

    virtual void tick();

    virtual void render(float partial_ticks);
};

class exploding_block_entity_t : public falling_block_entity_t
{
public:
    uint16_t fuse = 80;
    exploding_block_entity_t(block_t block_state, const vec3i &position, uint16_t fuse);

    virtual void tick();

    virtual void render(float partial_ticks);
};

#endif