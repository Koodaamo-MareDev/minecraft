#include "entity.hpp"
#include "raycast.hpp"
#include "chunk_new.hpp"
#include "render.hpp"
#include "blocks.hpp"
#include "light.hpp"
#include "particle.hpp"
#include "maths.hpp"
#include "pathfinding.hpp"
#include "sounds.hpp"
#include "inventory.hpp"
#include "gui.hpp"
extern float wiimote_x;
extern float wiimote_z;
extern u32 wiimote_held;
extern aabb_entity_t *player;
extern inventory::container player_inventory;
extern gui *current_gui;

pathfinding_t pathfinder;

void PlaySound(sound_t sound);                               // in minecraft.cpp
void AddParticle(const particle_t &particle);                // in minecraft.cpp
void CreateExplosion(vec3f pos, float power, chunk_t *near); // in minecraft.cpp

constexpr int item_pickup_ticks = 2;
constexpr int item_lifetime = 6000;

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
    chunk_t *curr_chunk = get_chunk_from_pos(int_pos);
    if (!curr_chunk)
        return true;
    if (int_pos.y < 0 || int_pos.y >= 256)
        return false;
    chunkvbo_t &vbo = curr_chunk->vbos[int_pos.y >> 4];
    if (vbo.dirty || curr_chunk->light_update_count || vbo.cached_solid != vbo.solid || vbo.cached_transparent != vbo.transparent)
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
    ticks_existed++;
    prev_rotation = rotation;
    vec3f fluid_velocity = vec3f(0, 0, 0);
    bool water_movement = false;
    bool lava_movement = false;
    bool cobweb_movement = false;
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
                if (!block)
                    continue;
                if (!block->intersects(fluid_aabb, block_pos))
                    continue;
                if (block->get_blockid() == BlockID::cobweb)
                    cobweb_movement = true;
                if (!is_fluid(block->get_blockid()))
                    continue;
                if (y + 1 - get_fluid_height(block_pos, block->get_blockid(), chunk) >= max.y)
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
    if (water_movement)
    {
        if (!in_water)
        {
            float impact = 0.2 / Q_rsqrt_d(velocity.x * velocity.x * 0.2 + velocity.y * velocity.y + velocity.z * velocity.z * 0.2);
            if (impact > 1.0)
                impact = 1.0;
            javaport::Random rng;
            sound_t sound = get_sound("splash");
            sound.position = position;
            sound.pitch = 0.6 + rng.nextFloat() * 0.8;
            sound.volume = impact;
            PlaySound(sound);

            particle_t particle;
            particle.life_time = particle.max_life_time = 80;
            particle.physics = PPHYSIC_FLAG_COLLIDE;
            particle.type = PTYPE_GENERIC;
            particle.brightness = 0xFF;

            particle.size = 8;

            // Set the particle texture coordinates
            particle.u = 0;
            particle.v = 32;
            for (int i = 0; i < 1 + width * 20; i++)
            {
                // Randomize the particle position and velocity
                particle.position = position + vec3f(rng.nextFloat() * 2 - 1, 0, rng.nextFloat() * 2 - 1) * width - vec3f(0.5, 0.5, 0.5);
                particle.position.y = std::floor(aabb.min.y + 1);
                particle.velocity = velocity;
                particle.velocity.y -= 0.2 * rng.nextFloat();

                // Randomize life time
                particle.life_time = particle.max_life_time - (rand() % 20);

                AddParticle(particle);
            }
            particle.physics |= PPHYSIC_FLAG_GRAVITY;
            particle.v = 16;

            for (int i = 0; i < 1 + width * 20; i++)
            {
                particle.u = rng.nextInt(4) + 3;
                // Randomize the particle position and velocity
                particle.position = position + vec3f(rng.nextFloat() * 2 - 1, 0, rng.nextFloat() * 2 - 1) * width - vec3f(0.5, 0.5, 0.5);
                particle.position.y = std::floor(aabb.min.y + 1);
                particle.velocity = velocity;

                // Randomize life time
                particle.life_time = particle.max_life_time - (rand() % 20);

                AddParticle(particle);
            }
        }
        in_water = true;
    }
    else
    {
        in_water = false;
    }
    if (water_movement || lava_movement)
    {
        float old_y = position.y;

        if (local)
        {
            movement.x = wiimote_x;
            movement.z = wiimote_z;
            jumping = (wiimote_held & WPAD_CLASSIC_BUTTON_B);
        }
        else
        {
            jumping = should_jump();
        }
        velocity.x += movement.x * 0.02;
        velocity.z += movement.z * 0.02;

        if (jumping)
            velocity.y += 0.04;

        velocity = velocity + (fluid_velocity.normalize() * 0.004);

        if (cobweb_movement)
            velocity = velocity * 0.25;

        move_and_check_collisions();

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

        jumping = false;
        vfloat_t speed = on_ground ? 0.1 * (0.16277136 / (h_friction * h_friction * h_friction)) : 0.02;
        if (local)
        {
            // If the entity is on the ground, allow them to jump
            jumping = on_ground && (wiimote_held & WPAD_CLASSIC_BUTTON_B);
            movement.x = wiimote_x;
            movement.z = wiimote_z;
        }
        else
        {
            jumping = on_ground && should_jump();
        }
        velocity.x += movement.x * speed;
        velocity.z += movement.z * speed;
        if (jumping)
        {
            velocity.y = 0.42;
        }
        if (cobweb_movement)
            velocity = velocity * 0.25;
        move_and_check_collisions();

        // Gravity can be applied in different phases - before or after applying friction
        if (drag_phase == drag_phase_t::before_friction)
            velocity.y -= gravity;

        velocity.y *= 0.98;

        if (drag_phase == drag_phase_t::after_friction)
            velocity.y -= gravity;

        velocity.x *= h_friction;
        velocity.z *= h_friction;
    }
    if (walk_sound)
    {
        // Play step sound if the entity is moving on the ground or when it jumps
        vec3f player_horizontal_velocity = position - prev_position;
        last_step_distance += player_horizontal_velocity.magnitude() * 0.6;
        if ((on_ground && jumping) || last_step_distance > 1)
        {
            vec3f feet_pos(position.x, aabb.min.y - 0.5, position.z);
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
                    PlaySound(sound);
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

void aabb_entity_t::move_and_check_collisions()
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

vec3f aabb_entity_t::simple_pathfind(vec3f target)
{
    return pathfinder.simple_pathfind(vec3f(position.x, aabb.min.y, position.z), target, path);
}

falling_block_entity_t::falling_block_entity_t(block_t block_state, const vec3i &position) : aabb_entity_t(0.999f, 0.999f), block_state(block_state)
{
    aabb_entity_t(0.999f, 0.999f);
    chunk = get_chunk_from_pos(position);
    set_position(vec3f(position.x, position.y, position.z) + vec3f(0.5, 0, 0.5));
    this->walk_sound = false;
    this->drag_phase = drag_phase_t::after_friction;
    this->gravity = 0.04;
    if (!chunk)
        dead = true;
}
void falling_block_entity_t::tick()
{
    if (dead)
        return;
    if (!fall_time)
    {
        update_neighbors(vec3i(std::floor(position.x), std::floor(position.y), std::floor(position.z)));
    }
    fall_time++;
    aabb_entity_t::tick();
    if (on_ground)
    {
        dead = true; // Mark for removal
        vec3f current_pos = get_position(0);
        vec3i int_pos = vec3i(std::floor(current_pos.x), std::floor(current_pos.y), std::floor(current_pos.z));
        block_t *block = get_block_at(int_pos, chunk);
        if (block)
        {
            // Update the block
            *block = this->block_state;
            block->set_blockid(block->get_blockid());
            update_block_at(int_pos);
            update_neighbors(int_pos);
        }
        set_position(vec3f(int_pos.x, int_pos.y, int_pos.z) + vec3f(0.5, 0, 0.5));
    }
}

void falling_block_entity_t::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;

    // Prepare the block state
    vec3i int_pos = vec3i(std::floor(position.x), std::floor(position.y + 0.5), std::floor(position.z));
    block_t *block_at_pos = chunk->get_block(int_pos);
    if (!chunk->light_update_count && fall_time && block_at_pos && !properties(block_at_pos->id).m_solid)
        block_state.light = block_at_pos->light;
    block_state.visibility_flags = 0x7F;

    use_texture(blockmap_texture);

    // Draw the selected block
    if (on_ground || fall_time <= 1)
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

exploding_block_entity_t::exploding_block_entity_t(block_t block_state, const vec3i &position, uint16_t fuse) : falling_block_entity_t(block_state, position), fuse(fuse)
{
    falling_block_entity_t(block_state, position);
    javaport::Random rng;
    this->velocity = vec3f(rng.nextDouble() * 0.04 - 0.02, 0.2, rng.nextDouble() * 0.04 - 0.02);

    // Play the fuse sound if there is a long enough fuse
    if (fuse > 45)
    {
        sound_t sound = get_sound("fuse");
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

void exploding_block_entity_t::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;

    // Prepare the block state
    vec3i int_pos = vec3i(std::floor(position.x), std::floor(position.y + 0.5f), std::floor(position.z));
    block_t *block_at_pos = chunk->get_block(int_pos);
    if (!chunk->light_update_count && block_at_pos && !properties(block_at_pos->id).m_solid)
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

creeper_model_t creeper_model = creeper_model_t();

creeper_entity_t::creeper_entity_t(const vec3f &position) : aabb_entity_t(0.6f, 1.7f)
{
    aabb_entity_t(0.6f, 1.7f);
    set_position(position);
    this->walk_sound = false;
    this->gravity = 0.08;
    this->y_offset = 1.445;
    memcpy(&creeper_model.texture, &creeper_texture, sizeof(GXTexObj));
}

void creeper_entity_t::tick()
{
    if (dead)
        return;

    if (!follow_entity)
    {
        for (int x = position.x - 16; x <= position.x + 16 && !follow_entity; x += 16)
        {
            for (int z = position.z - 16; z <= position.z + 16 && !follow_entity; z += 16)
            {
                chunk_t *chunk = get_chunk_from_pos(vec3i(x, 0, z));
                if (!chunk)
                    continue;
                for (aabb_entity_t *entity : chunk->entities)
                {
                    if (entity && !entity->dead && entity == player)
                    {
                        vec3f entity_position = entity->get_position(0);
                        vec3f direction = entity_position - position;
                        vfloat_t sqrdistance = direction.sqr_magnitude();
                        if (sqrdistance < 512 && sqrdistance > 0.5)
                        {
                            follow_entity = entity;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (follow_entity)
    {
        vec3f direction = follow_entity->position - position;
        vfloat_t sqrdistance = direction.sqr_magnitude();
        if (sqrdistance < 512 && sqrdistance > 0.5)
        {
            rotation = vector_to_angles(direction.normalize());
            if (ticks_existed % 4 == 0)
            {
                vec3f target = vec3f(std::floor(follow_entity->position.x), std::floor(follow_entity->aabb.min.y), std::floor(follow_entity->position.z));

                vec3i target_i = vec3i(target.x, target.y, target.z);
                int below = checkbelow(target_i) + 1;
                if (target.y - below < 3)
                {
                    target.y = below;
                }
                vec3f move = simple_pathfind(target);
                movement = vec3f(move.x, 0, move.z).normalize() * 0.5 + vec3f(0, move.y > 0.25, 0);
            }
            if (ticks_existed % 1200 == 300)
            {
                sound_t sound = randomize_sound("cave", 13);
                sound.position = position;
                sound.volume = 1.0;
                sound.pitch = 1.0;
                PlaySound(sound);
            }
        }
        // Explode if the player is too close
        if (sqrdistance < 6.25)
        {
            // Stop movement
            movement = vec3f(0, 0, 0);

            // Play the fuse sound
            if (fuse == creeper_fuse)
            {
                sound_t sound = get_sound("fuse");
                sound.position = position;
                sound.volume = 0.5;
                sound.pitch = 1.0;
                PlaySound(sound);
            }

            if (!fuse)
            {
                dead = true;
                CreateExplosion(get_position(0), 3, nullptr);
            }
            else
            {
                fuse--;
            }
        }
        else if (fuse < creeper_fuse)
        {
            // Reset the fuse slowly when out of range
            fuse++;
        }
        if (sqrdistance > 256)
        {
            follow_entity = nullptr;
        }
    }
    aabb_entity_t::tick();
}

bool creeper_entity_t::should_jump()
{
    if (!follow_entity)
        return false;

    if (in_water || movement.y > 0)
        return true;

    return false;
}

void creeper_entity_t::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;

    // The creeper should not render in the transparent pass
    if (transparency)
        return;

    vec3f entity_position = get_position(partial_ticks);
    vec3f entity_rotation = get_rotation(partial_ticks);

    vec3i block_pos = vec3i(std::floor(entity_position.x), std::floor(entity_position.y), std::floor(entity_position.z));
    block_t *block = get_block_at(block_pos, chunk);
    if (block && !properties(block->id).m_solid)
    {
        light_level = block->light;
    }

    while (entity_rotation.y < 0)
        entity_rotation.y += 360;
    entity_rotation.y = std::fmod(entity_rotation.y, 360);

    while (body_rotation.y < 0)
        body_rotation.y += 360;
    body_rotation.y = std::fmod(body_rotation.y, 360);

    vfloat_t diff = entity_rotation.y - body_rotation.y;
    // Get the shortest angle
    if (diff > 180)
        diff -= 360;
    if (diff < -180)
        diff += 360;

    // Rotate the body to stay in bounds of the head rotation
    if (diff > 45)
        body_rotation.y = entity_rotation.y - 45;
    if (diff < -45)
        body_rotation.y = entity_rotation.y + 45;

    vec3f h_velocity = vec3f(velocity.x, 0, velocity.z);
    h_velocity.y = 0;

    body_rotation.y = lerpd(body_rotation.y, entity_rotation.y, h_velocity.magnitude());

    creeper_model.speed = h_velocity.magnitude() * 30;

    creeper_model.pos = entity_position - vec3f(0.5, 0.5, 0.5);
    creeper_model.rot = body_rotation;
    creeper_model.head_rot = vec3f(entity_rotation.x, entity_rotation.y, 0);

    // Draw the creeper
    set_color_multiply(get_lightmap_color(light_level));

    if (fuse < creeper_fuse)
    {
        set_color_add(std::sin(fuse + partial_ticks * 0.1) > 0 ? GXColor{0, 0, 0, 0xFF} : GXColor{0xFF, 0xFF, 0xFF, 0xFF});
    }

    creeper_model.render(partial_ticks);
#ifdef DEBUG
    if (follow_entity)
    {
        block_t block_state;
        block_state.id = 50; // Fire
        block_state.visibility_flags = 0x7F;
        block_state.meta = 0;
        block_state.light = 0xFF;
        use_texture(blockmap_texture);
        for (size_t i = 0; i < path.size(); i++)
        {
            vec3i path_pos = path[i];
            vec3f chunk_pos = vec3f(path_pos.x & ~0xF, path_pos.y & ~0xF, path_pos.z & ~0xF);
            transform_view(get_view_matrix(), chunk_pos);
            render_single_block_at(block_state, path_pos, false);
            render_single_block_at(block_state, path_pos, true);
        }
        vec3i path_pos = vec3i(std::floor(follow_entity->position.x), std::floor(follow_entity->aabb.min.y), std::floor(follow_entity->position.z));
        int below = checkbelow(path_pos) + 1;
        path_pos.y = below;
        block_state.id = 56; // Diamond block
        vec3f chunk_pos = vec3f(path_pos.x & ~0xF, path_pos.y & ~0xF, path_pos.z & ~0xF);
        transform_view(get_view_matrix(), chunk_pos);
        render_single_block_at(block_state, path_pos, false);
        render_single_block_at(block_state, path_pos, true);
    }
#endif
}

item_entity_t::item_entity_t(const vec3f &position, const inventory::item_stack &item_stack) : aabb_entity_t(.2f, .2f)
{
    set_position(position);
    chunk = get_chunk_from_pos(vec3i(std::floor(position.x), std::floor(position.y), std::floor(position.z)));
    this->walk_sound = false;
    this->gravity = 0.04;
    this->y_offset = 0.125;
    this->item_stack = item_stack;
    this->pickup_pos = position;
}

void item_entity_t::tick()
{
    if (dead)
        return;
    aabb_entity_t::tick();

    if (ticks_existed >= 6000)
        dead = true;
}

void item_entity_t::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;

    if (dead)
        return;

    vec3f entity_position = get_position(partial_ticks);

    // Adjust position for pickup animation
    if (picked_up)
    {
        // Lerp the position towards the target in <item_pickup_ticks> ticks
        vec3f target = pickup_pos;
        vfloat_t factor = (ticks_existed + partial_ticks - (item_lifetime - item_pickup_ticks)) / vfloat_t(item_pickup_ticks); // The lifetime is adjusted to line up with the pickup animation
        entity_position = vec3f::lerp(entity_position, target, factor);
    }

    vec3i block_pos = vec3i(std::floor(entity_position.x), std::floor(entity_position.y), std::floor(entity_position.z));
    block_t *light_block = get_block_at(block_pos, chunk);
    if (light_block && !properties(light_block->id).m_solid)
    {
        light_level = light_block->light;
    }

    // Default item rotation
    vec3f item_rot = vec3f(0, 0, 0);

    // Get the direction towards the player
    if (player)
    {
        item_rot.y = player->rotation.y + 180;
    }

    // Lock the item rotation to the y-axis
    item_rot.x = 0;
    item_rot.z = 0;

    int multi = item_stack.count > 1 ? 1 : 0;

    inventory::item item = item_stack.as_item();

    RenderType render_type = properties(item.id).m_render_type;
    block_t block = {uint8_t(item.id & 0xFF), 0x7F, item_stack.meta, 0xF, 0xF};
    block.light = light_level;

    vec3f anim_offset = vec3f(-0.5, std::sin((ticks_existed + partial_ticks) * M_1_PI * 0.25) * 0.125 - 0.375, -0.5);
    vec3f dupe_offset = vec3f(0.0625);
    // Draw the item (twice if there are multiple items)
    for (int i = 0; i <= multi; i++)
    {
        if (item.is_block())
        {
            use_texture(blockmap_texture);

            if (!properties(item.id).m_fluid && (render_type == RenderType::full || render_type == RenderType::full_special || render_type == RenderType::slab))
            {
                // Draw the block
                item_rot.y = (ticks_existed + partial_ticks) * 4;
                transform_view(get_view_matrix(), entity_position + anim_offset + dupe_offset * i, vec3f(4), item_rot, true);
                blockproperties_t props = properties(block.id);
                if (bool(props.m_transparent) == transparency)
                    render_single_block(block, props.m_transparent);
            }
            else
            {
                // Draw the item
                item_rot.z = 180;
                transform_view(get_view_matrix(), entity_position + anim_offset + dupe_offset * i + vec3f(0, 0.125, 0), vec3f(2), item_rot, true);

                if (transparency)
                    render_single_item(get_default_texture_index(BlockID(block.id)), true, light_level);
            }
        }
        else
        {
            use_texture(items_texture);

            // Draw the item
            item_rot.z = 180;
            transform_view(get_view_matrix(), entity_position + anim_offset + dupe_offset * i + vec3f(0, 0.125, 0), vec3f(2), item_rot, true);

            if (transparency)
                render_single_item(item.texture_index, true, light_level);
        }
    }
}

void item_entity_t::resolve_collision(aabb_entity_t *b)
{
    if (dead || picked_up || ticks_existed < 20)
        return;

    if (b == player)
    {
        inventory::item_stack left_over = player_inventory.add(item_stack);
        if (left_over.count)
        {
            if (left_over.count != item_stack.count)
            {
                // Play the pop sound if the player picks up at least one item
                javaport::Random rng;
                sound_t sound = get_sound("pop");
                sound.position = position;
                sound.volume = 0.5;
                sound.pitch = rng.nextFloat() * 0.8 + 0.6;
                PlaySound(sound);
                if (current_gui)
                    current_gui->refresh();
            }
            item_stack = left_over;
        }
        else
        {
            // Play the pop sound if the player picks up the stack
            javaport::Random rng;
            sound_t sound = get_sound("pop");
            sound.position = position;
            sound.volume = 0.5;
            sound.pitch = rng.nextFloat() * 0.8 + 0.6;
            PlaySound(sound);
            picked_up = true;
            pickup_pos = player->get_position(0) - vec3f(0, 0.5, 0);
            ticks_existed = item_lifetime - item_pickup_ticks;
            if (current_gui)
                current_gui->refresh();
        }
    }
}

bool item_entity_t::collides(aabb_entity_t *other)
{
    return aabb.inflate(0.7).intersects(other->aabb);
}
