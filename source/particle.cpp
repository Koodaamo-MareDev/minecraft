#include "particle.hpp"
#include "chunk_new.hpp"
#include "block.hpp"

void particle::update(float dt)
{
    if (!life_time)
        return;
    if (physics & PPHYSIC_FLAG_GRAVITY)
    {
        // Apply gravity
        velocity.y -= ENTITY_GRAVITY * dt;
    }

    if (physics & PPHYSIC_FLAG_FRICTION)
    {
        // Apply friction on X and Z axis
        velocity.x *= 0.85f;
        velocity.z *= 0.85f;

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
    }

    // Used for collision detection
    vec3i old_pos = vec3i(std::round(position.x), std::round(position.y), std::round(position.z));

    // Update position
    position = position + velocity * dt;

    // Check if the particle has moved
    vec3i new_pos = vec3i(std::round(position.x), std::round(position.y), std::round(position.z));

    // Get the block at the particle's position
    block_t *block = get_block_at(new_pos);
    if (block)
    {
        if (old_pos != new_pos)
        {
            // Check if the block is solid
            if ((physics & PPHYSIC_FLAG_COLLIDE) && block_properties[block->id].m_opacity > 1)
            {
                // Place the particle on the surface of the block
                vec3f old_vel = velocity;
                velocity = vec3f(0, 0, 0);

                if (old_pos.x > new_pos.x)
                    position.x = old_pos.x + .5f;
                else if (old_pos.x < new_pos.x)
                    position.x = old_pos.x - .5f;
                else
                    velocity.x = old_vel.x;

                if (old_pos.y > new_pos.y)
                    position.y = old_pos.y - .5f;
                else if (old_pos.y < new_pos.y)
                    position.y = old_pos.y - 1.5f;
                else
                    velocity.y = old_vel.y;

                if (old_pos.z > new_pos.z)
                    position.z = old_pos.z + .5f;
                else if (old_pos.z < new_pos.z)
                    position.z = old_pos.z - .5f;
                else
                    velocity.z = old_vel.z;
            }
        }
    }
    new_pos = vec3i(std::round(position.x), std::round(position.y), std::round(position.z));
    block = get_block_at(new_pos);
    if (block)
    {
        brightness = block->light;
        if (block_properties[block->id].m_opacity > 1)
        {
            life_time = 0;
        }
    }

    // Update lifetime
    if (life_time > 0)
        life_time--;
}

void particle_system::update(float dt)
{
    for (int i = 0; i < 256; i++)
    {
        particles[i].update(dt);
    }
}

void particle_system::add_particle(particle part)
{
    for (int i = 0; i < 256; i++)
    {
        if (particles[i].life_time == 0)
        {
            particles[i] = part;
            return;
        }
    }
}