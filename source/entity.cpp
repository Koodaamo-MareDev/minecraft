#include "entity.hpp"

void entity_t::update(float dt)
{
    // Apply gravity
    velocity.y -= ENTITY_GRAVITY * dt;

    // Set terminal velocity on y axis
    const float terminal_velocity = 20.0f;
    if (velocity.y > terminal_velocity)
    {
        velocity.y = terminal_velocity;
    }
    else if (velocity.y < -terminal_velocity)
    {
        velocity.y = -terminal_velocity;
    }

    // Set maximum horizontal velocity on x and z axis
    const float max_velocity = 20.0f;
    if (velocity.x > max_velocity)
    {
        velocity.x = max_velocity;
    }
    else if (velocity.x < -max_velocity)
    {
        velocity.x = -max_velocity;
    }
    if (velocity.z > max_velocity)
    {
        velocity.z = max_velocity;
    }
    else if (velocity.z < -max_velocity)
    {
        velocity.z = -max_velocity;
    }
}

void aabb_entity_t::update_aabb()
{
    aabb.min = position + bounds.min;
    aabb.max = position + bounds.max;
}

void aabb_entity_t::set_bounds(aabb_t bounds)
{
    this->bounds = bounds;
    update_aabb();
}

void aabb_entity_t::update(float dt)
{
    // Reset on_ground flag
    on_ground = false;

    // Update entity physics
    entity_t::update(dt);
}

bool aabb_entity_t::collides(aabb_entity_t *other)
{
    return aabb.intersects(other->aabb);
}

void aabb_entity_t::resolve_collision(aabb_entity_t *b)
{
    vec3f push = aabb.push_out(b->aabb);

    // Add velocity to separate the entities
    this->velocity = this->velocity + push * 10.0f;
    b->velocity = b->velocity - push * 10.0f;
}

void aabb_entity_t::move(vec3f delta)
{
    // Update position
    position = position + delta;

    // Update AABB
    update_aabb();
}

void aabb_entity_t::apply_friction(float friction)
{
    // Apply friction on X and Z axis
    velocity.x *= friction;
    velocity.z *= friction;
}