#ifndef _ENTITY_HPP_
#define _ENTITY_HPP_

#define ENTITY_GRAVITY (9.8f * 2.0f)

#include "vec3f.hpp"
#include "aabb.hpp"

class entity_t
{
public:
    vec3f position;
    vec3f velocity;

    entity_t() : position(0, 0, 0), velocity(0, 0, 0) {}
    entity_t(vec3f position, vec3f velocity) : position(position), velocity(velocity) {}
    entity_t(vec3f position) : position(position), velocity(0, 0, 0) {}
    virtual ~entity_t() {}

    virtual void update(float dt);
};

class aabb_entity_t : public entity_t
{
public:
    aabb_t aabb;
    bool on_ground = false;
    bool jumping = false;
    vec3f prev_position;

    aabb_entity_t() : entity_t(), aabb() {}
    aabb_entity_t(vec3f position, vec3f velocity, aabb_t aabb) : entity_t(position, velocity), aabb(aabb) {}
    aabb_entity_t(vec3f position, aabb_t aabb) : entity_t(position), aabb(aabb) {}

    ~aabb_entity_t() {}

    void update_aabb();

    void set_bounds(aabb_t bounds);

    void update(float dt) override;

    bool collides(aabb_entity_t *other);

    void resolve_collision(aabb_entity_t *b);

    void move(vec3f delta);

    void apply_friction(float friction);

private:
    aabb_t bounds;
};

#endif