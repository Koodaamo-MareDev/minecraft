#include "entity.hpp"
#include "chunk_new.hpp"
#include "blocks.hpp"
#include "sound.hpp"

extern sound_system_t *sound_system;
extern float wiimote_x;
extern float wiimote_z;
extern u32 wiimote_held;

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

void aabb_entity_t::set_position(vec3f pos)
{
    position = pos;
    prev_position = pos;
    aabb.min = vec3f(pos.x - width * 0.5, pos.y - y_offset + y_size, pos.z - width * 0.5);
    aabb.max = vec3f(pos.x + width * 0.5, pos.y - y_offset + y_size + height, pos.z + width * 0.5);
}

void aabb_entity_t::tick()
{
    vec3f fluid_velocity = vec3f(0, 0, 0);
    bool water_movement = false;
    bool lava_movement = false;
    aabb_t fluid_aabb = aabb;
    fluid_aabb.min.y += 0.4;
    fluid_aabb.max.y -= 0.4;
    vec3i min = vec3i(std::floor(fluid_aabb.min.x), std::floor(fluid_aabb.min.y - 1), std::floor(fluid_aabb.min.z));
    vec3i max = vec3i(std::floor(fluid_aabb.max.x + 1), std::floor(fluid_aabb.max.y + 1), std::floor(fluid_aabb.max.z + 1));
    for (int x = min.x; x < max.x; x++)
    {
        for (int y = min.y; y < max.y; y++)
        {
            for (int z = min.z; z < max.z; z++)
            {
                vec3i block_pos = vec3i(x, y, z);
                block_t *block = get_block_at(block_pos, chunk);
                if (!block || !is_fluid(block->get_blockid()))
                    continue;
                if (y + 1 - get_fluid_height(block_pos, block->get_blockid(), chunk) >= max.y)
                    continue;
                if (!block->intersects(fluid_aabb, block_pos))
                    continue;
                fluid_velocity = fluid_velocity + get_fluid_direction(block, block_pos);
                BlockID base_fluid = basefluid(block->get_blockid());
                if (base_fluid == BlockID::water)
                    water_movement = true;
                if (base_fluid == BlockID::lava)
                    lava_movement = true;
            }
        }
    }
    if (water_movement || lava_movement)
    {
        float old_y = position.y;

        velocity.x += wiimote_x * 0.02;
        velocity.z += wiimote_z * 0.02;

        if ((wiimote_held & WPAD_CLASSIC_BUTTON_B))
            velocity.y += 0.04;
        velocity = velocity + (fluid_velocity.normalize() * 0.004);

        check_collisions();

        if (water_movement)
            velocity = velocity * 0.8;
        else
            velocity = velocity * 0.5;
        velocity.y -= 0.02;

        aabb_t fluid_aabb = aabb;
        fluid_aabb.translate(vec3f(velocity.x, velocity.y + 0.6 - position.y + old_y, velocity.z));
        if (horizontal_collision && !is_colliding_fluid(fluid_aabb))
        {
            velocity.y = 0.3;
        }
    }
    else
    {
        BlockID block_at_feet = get_block_at(vec3i(std::floor(position.x), std::floor(aabb.min.y) - 1, std::floor(position.z)), chunk)->get_blockid();
        vfloat_t h_friction = 0.91;
        if (on_ground)
        {
            h_friction = 0.546;
            if (block_at_feet != BlockID::air)
            {
                h_friction = properties(block_at_feet).m_slipperiness * 0.91;
            }
        }

        if (local)
        {
            // If the entity is on the ground, allow them to jump
            jumping = on_ground && (wiimote_held & WPAD_CLASSIC_BUTTON_B);

            vfloat_t speed = on_ground ? 0.1 * (0.16277136 / (h_friction * h_friction * h_friction)) : 0.02;
            velocity.x += wiimote_x * speed;
            velocity.z += wiimote_z * speed;
        }
        if (jumping)
        {
            velocity.y = 0.42;
        }
        check_collisions();

        velocity.y -= 0.08;
        velocity.y *= 0.98;
        velocity.x *= h_friction;
        velocity.z *= h_friction;
    }

    // Play step sound if the entity is moving on the ground or when it jumps
    vec3f player_horizontal_velocity = position - prev_position;
    last_step_distance += player_horizontal_velocity.magnitude();
    if (last_step_distance > 1.5f || jumping)
    {
        vec3f feet_pos(position.x, aabb.min.y - 0.001, position.z);
        vec3i feet_block_pos = vec3i(std::floor(feet_pos.x), std::floor(feet_pos.y), std::floor(feet_pos.z));
        block_t *block_at_feet = get_block_at(feet_block_pos, chunk);
        feet_block_pos.y--;
        block_t *block_below_feet = get_block_at(feet_block_pos, chunk);
        if (block_below_feet && block_below_feet->get_blockid() != BlockID::air)
        {
            if (!block_at_feet || !properties(block_at_feet->get_blockid()).m_fluid)
            {
                sound_t sound = get_step_sound(block_at_feet->get_blockid());
                sound.position = feet_pos;
                sound_system->play_sound(sound);
                last_step_distance = 0;
            }
        }
    }
}

std::vector<aabb_t> aabb_entity_t::get_colliding_aabbs(const aabb_t &aabb)
{
    std::vector<aabb_t> nearby_blocks;
    vec3i min = vec3i(std::floor(aabb.min.x), std::floor(aabb.min.y - 1), std::floor(aabb.min.z));
    vec3i max = vec3i(std::floor(aabb.max.x + 1), std::floor(aabb.max.y + 1), std::floor(aabb.max.z + 1));
    for (int x = min.x; x < max.x; x++)
    {
        for (int y = min.y; y < max.y; y++)
        {
            for (int z = min.z; z < max.z; z++)
            {
                vec3i block_pos = vec3i(x, y, z);
                block_t *block = get_block_at(block_pos, chunk);
                if (!block)
                    continue;
                if (properties(block->id).m_collision == CollisionType::solid)
                    block->get_aabb(block_pos, aabb, nearby_blocks);
            }
        }
    }
    return nearby_blocks;
}

bool aabb_entity_t::is_colliding_fluid(const aabb_t &aabb)
{
    vec3i min = vec3i(std::floor(aabb.min.x), std::floor(aabb.min.y), std::floor(aabb.min.z));
    vec3i max = vec3i(std::floor(aabb.max.x + 1), std::floor(aabb.max.y + 1), std::floor(aabb.max.z + 1));
    if (aabb.min.x < 0)
        min.y--;
    if (aabb.min.y < 0)
        min.y--;
    if (aabb.min.z < 0)
        min.z--;
    for (int x = min.x; x < max.x; x++)
    {
        for (int y = min.y; y < max.y; y++)
        {
            for (int z = min.z; z < max.z; z++)
            {
                vec3i block_pos = vec3i(x, y, z);
                block_t *block = get_block_at(block_pos, chunk);
                if (block && properties(block->id).m_collision == CollisionType::fluid && block->intersects(aabb, block_pos))
                    return true;
            }
        }
    }
    return false;
}

void aabb_entity_t::check_collisions()
{
    horizontal_collision = false;
    vec3f old_velocity = velocity;
    vec3f new_velocity = velocity;

    std::vector<aabb_t> nearby_blocks = get_colliding_aabbs(aabb.extend_by(velocity));

    for (aabb_t &block_aabb : nearby_blocks)
    {
        new_velocity.y = block_aabb.calculate_y_offset(aabb, new_velocity.y);
    }
    aabb.translate(vec3f(0, new_velocity.y, 0));

    on_ground = old_velocity.y != new_velocity.y && old_velocity.y < 0;

    for (aabb_t &block_aabb : nearby_blocks)
    {
        new_velocity.x = block_aabb.calculate_x_offset(aabb, new_velocity.x);
    }
    aabb.translate(vec3f(new_velocity.x, 0, 0));

    horizontal_collision = old_velocity.x != new_velocity.x;

    for (aabb_t &block_aabb : nearby_blocks)
    {
        new_velocity.z = block_aabb.calculate_z_offset(aabb, new_velocity.z);
    }
    aabb.translate(vec3f(0, 0, new_velocity.z));

    horizontal_collision |= old_velocity.z != new_velocity.z;

    position.x = (aabb.min.x + aabb.max.x) * 0.5;
    position.y = aabb.min.y + y_offset - y_size;
    position.z = (aabb.min.z + aabb.max.z) * 0.5;

    if (new_velocity.x != old_velocity.x)
        velocity.x = 0;
    if (new_velocity.y != old_velocity.y)
        velocity.y = 0;
    if (new_velocity.z != old_velocity.z)
        velocity.z = 0;
}