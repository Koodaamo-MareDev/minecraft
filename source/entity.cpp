#include "entity.hpp"
#include "raycast.hpp"
#include "chunk_new.hpp"
#include "render.hpp"
#include "blocks.hpp"
extern sound_system_t *sound_system;
extern float wiimote_x;
extern float wiimote_z;
extern u32 wiimote_held;

bool aabb_entity_t::collides(aabb_entity_t *other)
{
    return aabb.intersects(other->aabb);
}

bool aabb_entity_t::can_remove()
{
    if (local)
        return false;
    if (!chunk)
        return true;
    vec3f entity_pos = get_position(0);
    if (entity_pos.y < 0)
        return true;
    if (entity_pos.y > 255)
        return false;
    vec3i int_pos = vec3i(std::floor(entity_pos.x), std::floor(entity_pos.y), std::floor(entity_pos.z));
    chunk_t *curr_chunk = get_chunk_from_pos(int_pos, false);
    chunkvbo_t &vbo = curr_chunk->vbos[int_pos.y >> 4];
    if (vbo.dirty || curr_chunk->light_update_count || vbo.cached_solid_buffer != vbo.solid_buffer || vbo.cached_transparent_buffer != vbo.transparent_buffer)
        return false;
    return dead;
}

void aabb_entity_t::resolve_collision(aabb_entity_t *b)
{
    vec3f push = aabb.push_out(b->aabb);

    // Add velocity to separate the entities
    this->velocity = this->velocity + push * 0.5;
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

        if (local)
        {
            velocity.x += wiimote_x * 0.02;
            velocity.z += wiimote_z * 0.02;

            if ((wiimote_held & WPAD_CLASSIC_BUTTON_B))
                velocity.y += 0.04;
        }
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
        BlockID block_at_feet = get_block_id_at(vec3i(std::floor(position.x), std::floor(aabb.min.y) - 1, std::floor(position.z)), BlockID::air, chunk);
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

        if (!drag_phase)
            velocity.y -= gravity;

        velocity.y *= 0.98;

        if (drag_phase)
            velocity.y -= gravity;

        velocity.x *= h_friction;
        velocity.z *= h_friction;
    }
    if (walk_sound)
    {
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

falling_block_entity_t::falling_block_entity_t(block_t block_state, const vec3i &position) : aabb_entity_t(0.999f, 0.999f), block_state(block_state)
{
    aabb_entity_t(0.999f, 0.999f);
    chunk = get_chunk_from_pos(position, false);
    set_position(vec3f(position.x, position.y, position.z) + vec3f(0.5, 0, 0.5));
    this->walk_sound = false;
    this->drag_phase = true;
    this->gravity = 0.04;
    if (!chunk)
        dead = true;
}
void falling_block_entity_t::tick()
{
    if (dead)
        return;
    aabb_entity_t::tick();
    if (on_ground)
    {
        dead = true; // Mark for removal
        vec3f current_pos = get_position(0);
        vec3i int_pos = vec3i(std::floor(current_pos.x), std::floor(current_pos.y), std::floor(current_pos.z));
        block_t *block = get_block_at(int_pos, chunk);
        if (block)
        {
            *block = this->block_state;
            block->set_blockid(block->get_blockid()); // Update the block
            update_block_at(int_pos);
        }
        set_position(vec3f(int_pos.x, int_pos.y, int_pos.z) + vec3f(0.5, 0, 0.5));
    }
}

void falling_block_entity_t::render(float partial_ticks)
{
    if (!chunk)
        return;

    // Prepare the block state
    vec3i int_pos = vec3i(std::floor(position.x), std::floor(position.y), std::floor(position.z));
    block_t *block_at_pos = chunk->get_block(int_pos);
    if (block_at_pos && !properties(block_at_pos->id).m_solid)
        block_state.light = block_at_pos->light;
    block_state.visibility_flags = 0x7F;

    // Draw the selected block
    if (on_ground)
    {
        // Prepare the transformation matrix
        vec3f chunk_pos = vec3f(int_pos.x & ~0xF, int_pos.y & ~0xF, int_pos.z & ~0xF);
        transform_view(get_view_matrix(), chunk_pos);
        render_single_block_at(block_state, int_pos, false);
        render_single_block_at(block_state, int_pos, true);
    }
    else
    {
        // Prepare the transformation matrix
        transform_view(get_view_matrix(), get_position(partial_ticks) - vec3f(0.5, 0, 0.5));
        render_single_block(block_state, false);
        render_single_block(block_state, true);
    }
}

void PlaySound(sound_t sound);                               // in minecraft.cpp
void CreateExplosion(vec3f pos, float power, chunk_t *near); // in minecraft.cpp

exploding_block_entity_t::exploding_block_entity_t(block_t block_state, const vec3i &position, uint16_t fuse) : falling_block_entity_t(block_state, position), fuse(fuse)
{
    falling_block_entity_t(block_state, position);
    this->velocity = vec3f(JavaLCGDouble() * 0.04 - 0.02, 0.2, JavaLCGDouble() * 0.04 - 0.02);

    // Play the fuse sound if there is a long enough fuse
    if (fuse > 45)
    {
        sound_t sound(fuse_sound);
        sound.position = get_position(0);
        sound.volume = 0.5;
        sound.pitch = 1.0;
        PlaySound(sound);
    }
}

void exploding_block_entity_t::tick()
{
    if (dead)
        return;

    aabb_entity_t::tick();
    if (on_ground)
    {
    }
    --fuse;
    if (!fuse)
    {
        dead = true;
        CreateExplosion(position + vec3i(0, 0.5625, 0), 3, chunk);
    }
}

void exploding_block_entity_t::render(float partial_ticks)
{
    if (!chunk)
        return;

    // Prepare the block state
    vec3i int_pos = vec3i(std::floor(position.x), std::floor(position.y), std::floor(position.z));
    block_t *block_at_pos = chunk->get_block(int_pos);
    if (block_at_pos && !properties(block_at_pos->id).m_solid)
        block_state.light = block_at_pos->light;
    block_state.visibility_flags = 0x7F;

    // Draw the selected block
    transform_view(get_view_matrix(), get_position(partial_ticks) - vec3f(0.5, 0, 0.5));

    if ((fuse / 5) % 2 == 1)
        use_texture(white_texture);

    render_single_block(block_state, false);
    render_single_block(block_state, true);

    use_texture(blockmap_texture);
}