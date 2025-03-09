#include "entity.hpp"

#include <math/math_utils.h>

#include "raycast.hpp"
#include "chunk.hpp"
#include "render.hpp"
#include "render_gui.hpp"
#include "blocks.hpp"
#include "light.hpp"
#include "particle.hpp"
#include "pathfinding.hpp"
#include "sounds.hpp"
#include "inventory.hpp"
#include "gui.hpp"
#include "world.hpp"
extern float wiimote_x;
extern float wiimote_z;
extern u32 wiimote_held;

pathfinding_t pathfinder;

constexpr int item_pickup_ticks = 2;
constexpr int item_lifetime = 6000;

creeper_model_t creeper_model = creeper_model_t();
player_model_t player_model = player_model_t();

bool entity_physical::collides(entity_physical *other)
{
    return other && aabb.intersects(other->aabb);
}

bool entity_physical::can_remove()
{
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

void entity_physical::resolve_collision(entity_physical *b)
{
    vec3f push = aabb.push_out_horizontal(b->aabb);

    // Add velocity to separate the entities
    this->velocity = this->velocity + push.normalize() * 0.025;
}

void entity_physical::teleport(vec3f pos)
{
    position = pos;
    prev_position = pos;
    vfloat_t half_width = width * 0.5;
    aabb.min = vec3f(pos.x - half_width, pos.y - y_offset + y_size, pos.z - half_width);
    aabb.max = vec3f(pos.x + half_width, pos.y - y_offset + y_size + height, pos.z + half_width);
}

void entity_physical::set_server_position(vec3i pos, uint8_t ticks)
{
    server_pos = pos;
    animation_pos = vec3f(pos.x, pos.y, pos.z) / 32.0;
    animation_tick = ticks;
}

void entity_physical::set_server_pos_rot(vec3i pos, vec3f rot, uint8_t ticks)
{
    server_pos = pos;
    animation_pos = vec3f(pos.x, pos.y, pos.z) / 32.0;
    animation_rot = rot;
    animation_tick = ticks;
}

void entity_physical::tick()
{
    prev_rotation = rotation;
    prev_position = position;
    if (current_world->is_remote() && !simulate_offline)
    {
        return;
    }
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
    if (use_fluid_physics() && water_movement)
    {
        if (!in_water)
        {
            vfloat_t impact = 0.2 / Q_rsqrt_d(velocity.x * velocity.x * 0.2 + velocity.y * velocity.y + velocity.z * velocity.z * 0.2);
            if (impact > 1.0)
                impact = 1.0;
            javaport::Random rng;
            sound sound = get_sound("splash");
            sound.position = position;
            sound.pitch = 0.6 + rng.nextFloat() * 0.8;
            sound.volume = impact;
            current_world->play_sound(sound);

            particle particle;
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

                current_world->add_particle(particle);
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

                current_world->add_particle(particle);
            }
        }
        in_water = true;
    }
    else
    {
        in_water = false;
    }
    if (use_fluid_physics() && (water_movement || lava_movement))
    {
        float old_y = position.y;

        // Allow swimming in water
        jumping = should_jump();

        // Move the entity horizontally
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

        // If the entity is on the ground, allow them to jump
        jumping = on_ground && should_jump();

        // Move the entity horizontally
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
}
void entity_physical::animate()
{
}
void entity_physical::render(float partial_ticks, bool transparency)
{
#ifdef DEBUG_ENTITIES
    if (chunk)
    {
        vec3f entity_position = get_position(partial_ticks);
        entity_position.y = (aabb.max.y + aabb.min.y) * 0.5;
        vec3i block_pos = vec3i(std::floor(entity_position.x), std::floor(entity_position.y + 0.5), std::floor(entity_position.z));
        block_t *block = get_block_at(block_pos, chunk);
        if (block && !properties(block->id).m_solid)
        {
            light_level = block->light;
        }
        // Render a mob spawner at the entity's position
        block_t block_state = {uint8_t(BlockID::bricks), 0x7F, 0, light_level};
        transform_view(gertex::get_view_matrix(), entity_position - vec3f(0.5, 0.5, 0.5), (aabb.max - aabb.min));

        // Restore default texture
        use_texture(terrain_texture);
        render_single_block(block_state, transparency);
    }
#endif
}

NBTTagList *serialize_vec3d(vec3f vec)
{
    NBTTagList *result = new NBTTagList;
    result->addTag(new NBTTagDouble(vec.x));
    result->addTag(new NBTTagDouble(vec.y));
    result->addTag(new NBTTagDouble(vec.z));
    return result;
}

NBTTagList *serialize_vec2f(vec3f vec)
{
    NBTTagList *result = new NBTTagList;
    result->addTag(new NBTTagFloat(vec.x));
    result->addTag(new NBTTagFloat(vec.y));
    return result;
}

vec3f deserialize_vec3d(NBTTagList *list)
{
    if (!list || list->tagType != NBTBase::TAG_Double || list->value.size() < 3)
        return vec3f(0, 0, 0);
    vec3f result;
    result.x = ((NBTTagDouble *)list->getTag(0))->value;
    result.y = ((NBTTagDouble *)list->getTag(1))->value;
    result.z = ((NBTTagDouble *)list->getTag(2))->value;
    return result;
}

vec3f deserialize_vec2f(NBTTagList *list)
{
    if (!list || list->tagType != NBTBase::TAG_Float || list->value.size() < 2)
        return vec3f(0, 0, 0);
    vec3f result;
    result.x = ((NBTTagFloat *)list->getTag(0))->value;
    result.y = ((NBTTagFloat *)list->getTag(1))->value;
    return result;
}

void entity_physical::serialize(NBTTagCompound *result)
{
    vec3f pos(position.x, aabb.min.y + y_offset - y_size + 0.0001, position.z);

    result->setTag("Motion", serialize_vec3d(velocity));
    result->setTag("Pos", serialize_vec3d(pos));
    result->setTag("Rotation", serialize_vec2f(rotation));

    result->setTag("Air", new NBTTagShort(300));
    result->setTag("AttackTime", new NBTTagShort(0));
    result->setTag("DeathTime", new NBTTagShort(0));
    result->setTag("FallDistance", new NBTTagFloat(0));
    result->setTag("Fire", new NBTTagShort(-20));
    result->setTag("OnGround", new NBTTagByte(on_ground));
    result->setTag("HurtTime", new NBTTagShort(0));
}

void entity_physical::deserialize(NBTTagCompound *nbt)
{
    vec3f pos = deserialize_vec3d(nbt->getList("Pos"));
    vec3f motion = deserialize_vec3d(nbt->getList("Motion"));
    vec3f rotation = deserialize_vec2f(nbt->getList("Rotation"));

    teleport(pos);
    velocity = motion;
    this->rotation = rotation;

    on_ground = nbt->getByte("OnGround");
}

std::vector<aabb_t> entity_physical::get_colliding_aabbs(const aabb_t &aabb)
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

bool entity_physical::is_colliding_fluid(const aabb_t &aabb)
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

void entity_physical::move_and_check_collisions()
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

vec3f entity_pathfinder::simple_pathfind(vec3f target)
{
    return pathfinder.simple_pathfind(vec3f(position.x, aabb.min.y, position.z), target, path);
}

entity_falling_block::entity_falling_block(block_t block_state, const vec3i &position) : entity_physical(), block_state(block_state)
{
    teleport(vec3f(position.x, position.y, position.z) + vec3f(0.5, 0, 0.5));
    this->walk_sound = false;
    this->drag_phase = drag_phase_t::after_friction;
    this->gravity = 0.04;
    this->simulate_offline = true;
    this->width = 0.999;
    this->height = 0.999;
}
void entity_falling_block::tick()
{
    fall_time++;
    entity_physical::tick();
    if (on_ground && !current_world->is_remote())
    {
        dead = true; // Mark for removal
        vec3i int_pos = get_foot_blockpos();
        block_t *block = get_block_at(int_pos, chunk);
        if (block)
        {
            // Update the block
            *block = this->block_state;
            block->set_blockid(block->get_blockid());
            update_block_at(int_pos);
            update_neighbors(int_pos);
        }
    }
}

void entity_falling_block::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;

    // Prepare the block state
    vec3i int_pos = vec3i(std::floor(position.x), std::floor(position.y + 0.5), std::floor(position.z));
    block_t *block_at_pos = chunk->get_block(int_pos);
    if (!chunk->light_update_count && fall_time && block_at_pos && !properties(block_at_pos->id).m_solid)
        block_state.light = block_at_pos->light;
    block_state.visibility_flags = 0x7F;

    use_texture(terrain_texture);

    // Draw the selected block
    if (on_ground || fall_time <= 1)
    {
        // Prepare the transformation matrix
        vec3f chunk_pos = vec3f(int_pos.x & ~0xF, int_pos.y & ~0xF, int_pos.z & ~0xF);
        transform_view(gertex::get_view_matrix(), chunk_pos);
        render_single_block_at(block_state, int_pos, false);
        render_single_block_at(block_state, int_pos, true);
    }
    else
    {
        // Prepare the transformation matrix
        transform_view(gertex::get_view_matrix(), get_position(partial_ticks) - vec3f(0.5, 0, 0.5));
        render_single_block(block_state, false);
        render_single_block(block_state, true);
    }
}

entity_explosive_block::entity_explosive_block(block_t block_state, const vec3i &position, uint16_t fuse) : entity_falling_block(block_state, position), entity_explosive()
{
    // Randomize a tiny "jump" to the block
    javaport::Random rng;
    velocity = vec3f(rng.nextDouble() * 0.04 - 0.02, 0.2, rng.nextDouble() * 0.04 - 0.02);

    this->fuse = fuse;

    // Play the fuse sound if there is a long enough fuse
    if (fuse > 45)
    {
        sound sound = get_sound("fuse");
        sound.position = get_position(0);
        sound.volume = 0.5;
        sound.pitch = 1.0;
        current_world->play_sound(sound);
    }
}

void entity_explosive_block::tick()
{

    entity_physical::tick();
    entity_explosive::tick();
}

void entity_explosive_block::render(float partial_ticks, bool transparency)
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
    transform_view(gertex::get_view_matrix(), get_position(partial_ticks) - vec3f(0.5, 0, 0.5));

    if ((fuse / 5) % 2 == 1)
        use_texture(white_texture);

    render_single_block(block_state, false);
    render_single_block(block_state, true);

    use_texture(terrain_texture);
}

entity_creeper::entity_creeper(const vec3f &position) : entity_explosive(), entity_living()
{
    this->fuse = 80;
    this->power = 3;
    this->health = 20;
    this->width = 0.6;
    this->height = 1.7;
    this->walk_sound = false;
    this->gravity = 0.08;
    this->y_offset = 1.445;
    memcpy(&creeper_model.texture, &creeper_texture, sizeof(GXTexObj));
    teleport(position);
}

void entity_creeper::tick()
{
    if (current_world->is_remote())
    {
        entity_physical::tick();
        return;
    }

    if (!follow_entity)
    {
        for (int x = position.x - 16; x <= position.x + 16 && !follow_entity; x += 16)
        {
            for (int z = position.z - 16; z <= position.z + 16 && !follow_entity; z += 16)
            {
                chunk_t *curr_chunk = get_chunk_from_pos(vec3i(x, 0, z));
                if (!curr_chunk)
                    continue;
                for (entity_physical *entity : curr_chunk->entities)
                {
                    if (entity && !entity->dead && entity == current_world->player.m_entity)
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
                sound sound = randomize_sound("cave", 13);
                sound.position = position;
                sound.volume = 1.0;
                sound.pitch = 1.0;
                current_world->play_sound(sound);
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
                sound sound = get_sound("fuse");
                sound.position = position;
                sound.volume = 0.5;
                sound.pitch = 1.0;
                current_world->play_sound(sound);
            }

            entity_explosive::tick();
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
    entity_physical::tick();
}

bool entity_creeper::should_jump()
{
    if (!follow_entity)
        return false;

    if (in_water || movement.y > 0)
        return true;

    return false;
}

void entity_creeper::render(float partial_ticks, bool transparency)
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

    while (body_rotation_y < 0)
        body_rotation_y += 360;
    body_rotation_y = std::fmod(body_rotation_y, 360);

    vfloat_t diff = entity_rotation.y - body_rotation_y;
    // Get the shortest angle
    if (diff > 180)
        diff -= 360;
    if (diff < -180)
        diff += 360;

    // Rotate the body to stay in bounds of the head rotation
    if (diff > 45)
        body_rotation_y = entity_rotation.y - 45;
    if (diff < -45)
        body_rotation_y = entity_rotation.y + 45;

    vec3f h_velocity = animation_tick ? (animation_pos - position) : (position - prev_position);
    h_velocity.y = 0;

    body_rotation_y = lerpd(body_rotation_y, entity_rotation.y, h_velocity.magnitude());

    creeper_model.speed = h_velocity.magnitude() * 30;

    creeper_model.pos = entity_position - vec3f(0.5, 0.5, 0.5);
    creeper_model.pos.y += y_offset;
    creeper_model.rot.y = body_rotation_y;
    creeper_model.head_rot = vec3f(entity_rotation.x, entity_rotation.y, 0);

    // Draw the creeper
    gertex::set_color_mul(get_lightmap_color(light_level));

    if (fuse < creeper_fuse)
    {
        gertex::set_color_add(std::sin(fuse + partial_ticks * 0.1) > 0 ? GXColor{0, 0, 0, 0xFF} : GXColor{0xFF, 0xFF, 0xFF, 0xFF});
    }

    creeper_model.render(accumulated_walk_distance, partial_ticks);
#ifdef DEBUG
    if (follow_entity)
    {
        block_t block_state;
        block_state.id = 50; // Fire
        block_state.visibility_flags = 0x7F;
        block_state.meta = 0;
        block_state.light = 0xFF;
        use_texture(terrain_texture);
        for (size_t i = 0; i < path.size(); i++)
        {
            vec3i path_pos = path[i];
            vec3f chunk_pos = vec3f(path_pos.x & ~0xF, path_pos.y & ~0xF, path_pos.z & ~0xF);
            transform_view(gertex::get_view_matrix(), chunk_pos);
            render_single_block_at(block_state, path_pos, false);
            render_single_block_at(block_state, path_pos, true);
        }
        vec3i path_pos = vec3i(std::floor(follow_entity->position.x), std::floor(follow_entity->aabb.min.y), std::floor(follow_entity->position.z));
        int below = checkbelow(path_pos) + 1;
        path_pos.y = below;
        block_state.id = 56; // Diamond block
        vec3f chunk_pos = vec3f(path_pos.x & ~0xF, path_pos.y & ~0xF, path_pos.z & ~0xF);
        transform_view(gertex::get_view_matrix(), chunk_pos);
        render_single_block_at(block_state, path_pos, false);
        render_single_block_at(block_state, path_pos, true);
    }
#endif
}

entity_item::entity_item(const vec3f &position, const inventory::item_stack &item_stack) : entity_physical()
{
    this->walk_sound = false;
    this->gravity = 0.04;
    this->y_offset = 0.125;
    this->item_stack = item_stack;
    this->pickup_pos = position;
    this->type = 1;
    this->simulate_offline = true;
    this->width = 0.25;
    this->height = 0.25;
    teleport(position);
}

void entity_item::tick()
{
    entity_physical::tick();

    if (current_world->is_remote())
        return;

    if (ticks_existed >= 6000)
        dead = true;
}

void entity_item::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;

    vec3f entity_position = get_position(partial_ticks);

    // Adjust position for pickup animation
    if (picked_up)
    {
        // Lerp the position towards the target in <item_pickup_ticks> ticks
        vec3f target = pickup_pos;
        vfloat_t factor = (ticks_existed + partial_ticks - (item_lifetime - item_pickup_ticks)) / vfloat_t(item_pickup_ticks); // The lifetime is adjusted to line up with the pickup animation
        if (factor > 1)
            factor = 1;
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
    if (current_world->player.m_entity)
    {
        item_rot.y = current_world->player.m_entity->rotation.y + 180;
    }

    // Lock the item rotation to the y-axis
    item_rot.x = 0;
    item_rot.z = 0;

    int multi = item_stack.count > 1 ? 1 : 0;

    inventory::item item = item_stack.as_item();

    RenderType render_type = properties(item.id).m_render_type;
    block_t block = {uint8_t(item.id & 0xFF), 0x7F, uint8_t(item_stack.meta & 0xFF), 0xF, 0xF};
    block.light = light_level;

    vec3f anim_offset = vec3f(-0.5, std::sin((ticks_existed + partial_ticks) * M_1_PI * 0.25) * 0.125 - 0.375, -0.5);
    vec3f dupe_offset = vec3f(0.0625);
    // Draw the item (twice if there are multiple items)
    for (int i = 0; i <= multi; i++)
    {
        if (item.is_block())
        {
            use_texture(terrain_texture);

            if (!properties(item.id).m_fluid && (render_type == RenderType::full || render_type == RenderType::full_special || render_type == RenderType::slab))
            {
                // Draw the block
                item_rot.y = (ticks_existed + partial_ticks) * 4;
                transform_view(gertex::get_view_matrix(), entity_position + anim_offset + dupe_offset * i, vec3f(0.25), item_rot, true);
                blockproperties_t props = properties(block.id);
                if (bool(props.m_transparent) == transparency)
                    render_single_block(block, props.m_transparent);
            }
            else
            {
                // Draw the item
                item_rot.z = 180;
                transform_view(gertex::get_view_matrix(), entity_position + anim_offset + dupe_offset * i + vec3f(0, 0.125, 0), vec3f(0.5), item_rot, true);

                if (transparency)
                    render_single_item(get_default_texture_index(BlockID(block.id)), true, light_level);
            }
        }
        else
        {
            use_texture(items_texture);

            // Draw the item
            item_rot.z = 180;
            transform_view(gertex::get_view_matrix(), entity_position + anim_offset + dupe_offset * i + vec3f(0, 0.125, 0), vec3f(0.5), item_rot, true);

            if (transparency)
                render_single_item(item.texture_index, true, light_level);
        }
    }
}

void entity_item::resolve_collision(entity_physical *b)
{
    if (current_world->is_remote())
        return;
    if (dead || picked_up || ticks_existed < 20)
        return;

    entity_player_local *player = dynamic_cast<entity_player_local *>(b);
    if (!player)
        return;

    inventory::item_stack left_over = current_world->player.m_inventory.add(item_stack);
    if (left_over.count)
    {
        if (left_over.count != item_stack.count)
        {
            // Play the pop sound if the player picks up at least one item
            javaport::Random rng;
            sound sound = get_sound("pop");
            sound.position = position;
            sound.volume = 0.5;
            sound.pitch = rng.nextFloat() * 0.8 + 0.6;
            current_world->play_sound(sound);
            if (gui::get_gui())
                gui::get_gui()->refresh();
        }
        item_stack = left_over;
    }
    else
    {
        // Play the pop sound if the player picks up the stack
        javaport::Random rng;
        sound sound = get_sound("pop");
        sound.position = position;
        sound.volume = 0.5;
        sound.pitch = rng.nextFloat() * 0.8 + 0.6;
        current_world->play_sound(sound);
        picked_up = true;
        pickup_pos = b->get_position(0) - vec3f(0, 0.5, 0);
        ticks_existed = item_lifetime - item_pickup_ticks;
        if (gui::get_gui())
            gui::get_gui()->refresh();
    }
}

bool entity_item::collides(entity_physical *other)
{
    return aabb.inflate(0.7).intersects(other->aabb);
}

void entity_item::pickup(vec3f pos)
{
    // Play the pop sound if the player picks up the stack
    javaport::Random rng;
    sound sound = get_sound("pop");
    sound.position = position;
    sound.volume = 0.5;
    sound.pitch = rng.nextFloat() * 0.8 + 0.6;
    current_world->play_sound(sound);

    // Mark the item as picked up
    picked_up = true;

    // Start the pickup animation
    pickup_pos = pos;
    ticks_existed = item_lifetime - item_pickup_ticks;
}

void entity_explosive::tick()
{
    if (!fuse)
        explode();
    else
        fuse--;
}

void entity_explosive::explode()
{
    dead = true;
    current_world->create_explosion(position + vec3i(0, y_offset - y_size, 0), power, chunk);
}

void entity_living::hurt(uint16_t damage)
{
    health -= damage;
    if (health <= 0 && !current_world->is_remote())
        dead = true;
}

void entity_living::tick()
{
    entity_physical::tick();
    if (walk_sound)
    {
        // Play step sound if the entity is moving on the ground or when it jumps
        vec3f player_horizontal_velocity = position - prev_position;
        last_step_distance += player_horizontal_velocity.magnitude() * 0.6;
        if ((on_ground && jumping) || last_step_distance > 1)
        {
            vec3f feet_pos(position.x, aabb.min.y - 0.5, position.z);
            vec3i feet_block_pos = vec3i(std::floor(feet_pos.x), std::floor(feet_pos.y), std::floor(feet_pos.z));
            BlockID block_at_feet = get_block_id_at(feet_block_pos, BlockID::air, chunk);
            feet_block_pos.y--;
            BlockID block_below_feet = get_block_id_at(feet_block_pos, BlockID::air, chunk);
            if (block_below_feet != BlockID::air)
            {
                if (!properties(block_at_feet).m_fluid)
                {
                    sound sound = get_step_sound(block_at_feet);
                    sound.position = feet_pos;
                    current_world->play_sound(sound);
                    last_step_distance = 0;
                }
            }
        }
    }
}

void entity_living::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;
    if (!dynamic_cast<entity_player *>(this))
    {
        entity_physical::render(partial_ticks, transparency);
        return;
    }
    vec3f entity_position = get_position(partial_ticks);
    vec3f entity_rotation = get_rotation(partial_ticks);
    vec3i block_pos = vec3i(std::floor(entity_position.x), std::floor(entity_position.y), std::floor(entity_position.z));
    block_t *block = get_block_at(block_pos, nullptr);
    if (block && !properties(block->id).m_solid)
    {
        light_level = block->light;
    }
    entity_position.y += y_offset;

    while (body_rotation_y >= 180)
        body_rotation_y -= 360;
    while (body_rotation_y < -180)
        body_rotation_y += 360;
    vec3f h_velocity = vec3f(velocity.x, 0, velocity.z);
    vfloat_t h_speed = h_velocity.magnitude();

    vfloat_t diff;
    // Rotate body to face the direction of movement
    if (h_speed > 0.1)
    {
        vfloat_t y_angle = vector_to_angles(h_velocity).y;
        diff = y_angle - body_rotation_y;
        while (diff >= 180)
            diff -= 360;
        while (diff < -180)
            diff += 360;
        body_rotation_y = lerpd(body_rotation_y, body_rotation_y + diff, h_speed);
    }

    diff = entity_rotation.y - body_rotation_y;

    // Get the shortest angle
    while (diff >= 180)
        diff -= 360;
    while (diff < -180)
        diff += 360;

    // Rotate the body to stay in bounds of the head rotation
    if (diff > 45)
        body_rotation_y = entity_rotation.y - 45;
    if (diff < -45)
        body_rotation_y = entity_rotation.y + 45;
}

void entity_living::serialize(NBTTagCompound *result)
{
    entity_physical::serialize(result);
    result->setTag("Health", new NBTTagShort(health));
}

void entity_living::deserialize(NBTTagCompound *nbt)
{
    entity_physical::deserialize(nbt);
    health = nbt->getShort("Health");
}

void entity_living::animate()
{
    vec3f old_position = position;

    // Animate the entity's position and rotation
    if (animation_tick > 0)
    {
        vec3f diff = animation_pos - position;
        vec3f new_position = position + diff / animation_tick;
        vfloat_t diff_rot = animation_rot.y - rotation.y;
        while (diff_rot < -180.0)
            diff_rot += 360.0;
        while (diff_rot >= 180.0)
            diff_rot -= 360.0;
        vec3f new_rotation = rotation + vec3f((animation_rot.x - rotation.x) / animation_tick, diff_rot / animation_tick, 0);
        if (std::abs(new_rotation.y - body_rotation_y) > 45)
            body_rotation_y += diff_rot / animation_tick;
        animation_tick--;
        teleport(new_position);
        rotation = new_rotation;
    }
    else if (!simulate_offline)
    {
        teleport(animation_pos);
        rotation = animation_rot;
    }

    velocity = vec3f::lerp(velocity, position - old_position, 0.25);
}

entity_player::entity_player(const vec3f &position) : entity_living()
{
    this->width = 0.6;
    this->height = 1.8;
    this->health = 20;
    this->walk_sound = true;
    this->gravity = 0.08;
    this->y_offset = 1.62;
    this->y_size = 0;
    this->drag_phase = drag_phase_t::before_friction;
    this->type = 48; // Mob - Not sure if this is correct as there is no player entity "type". Java uses instanceof to check if an entity is a player which is not possible in C++
    memcpy(&player_model.texture, &player_texture, sizeof(GXTexObj));
    teleport(position);
}

void entity_player::hurt(uint16_t damage)
{
    entity_living::hurt(damage);
    javaport::Random rng;
    sound sound = get_sound("hurt");
    sound.position = current_world->player.m_entity->position;
    sound.volume = 0.5;
    sound.pitch = rng.nextFloat() * 0.4 + 0.8;
    current_world->play_sound(sound);
}

entity_player_local::entity_player_local(const vec3f &position) : entity_player(position)
{
    simulate_offline = true;
    teleport(position);
}

void entity_player_local::serialize(NBTTagCompound *result)
{
    entity_living::serialize(result);

    result->setTag("Inventory", new NBTTagList);
    result->setTag("Dimension", new NBTTagInt(-int(current_world->hell)));
    result->setTag("Score", new NBTTagInt(0));
    result->setTag("Sleeping", new NBTTagByte(in_bed));
    result->setTag("SleepTimer", new NBTTagShort(0));

    // TODO: Serialize inventory
}

void entity_player_local::deserialize(NBTTagCompound *result)
{
    entity_living::deserialize(result);

    set_world_hell(result->getInt("Dimension") == -1);
    in_bed = result->getByte("Sleeping");

    // TODO: Deserialize inventory
}

void entity_player_local::tick()
{
    movement.x = wiimote_x;
    movement.z = wiimote_z;
    entity_living::tick();
    if (!current_world->is_remote() && aabb.min.y < -750)
        teleport(vec3f(position.x, 256, position.z));
}

void entity_player_local::render(float partial_ticks, bool transparency)
{
    // For now, we're not going to render the local player
}

void entity_player_local::animate()
{
    // Stub as we don't want to animate the local player
}

bool entity_player_local::should_jump()
{
    return (wiimote_held & WPAD_CLASSIC_BUTTON_B);
}

bool entity_player_local::can_remove()
{
    return false;
}

entity_player_mp::entity_player_mp(const vec3f &position, std::string player_name) : entity_player(position)
{
    this->player_name = player_name;
}

void entity_player_mp::tick()
{
    entity_physical::tick();
}

void entity_player_mp::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;

    // Get the player's position and rotation
    vec3f entity_position = get_position(partial_ticks);
    vec3f entity_rotation = get_rotation(partial_ticks);

    // The name tag should render in the transparent pass
    if (transparency)
    {
        transform_view(gertex::get_view_matrix(), entity_position, vec3f(1), vec3f(0, 0, 0), true);

        gertex::use_fog(false);

        // Enable direct colors
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

        // Render through walls
        GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);

        // Use floats for vertex positions (this will later be restored to fixed point by draw_text_3d)
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);

        // Use solid white as the texture
        use_texture(white_texture);

        // Render the background for the name tag
        vec3f bg_size = vec3f(0.25);
        bg_size.x *= (text_width_3d(player_name) + 2) * 0.125;
        bg_size.y += 0.03125;
        vec3f right_vec = -angles_to_vector(0, yrot + 90);
        vec3f up_vec = -angles_to_vector(xrot + 90, yrot);
        draw_colored_sprite_3d(white_texture, vec3f(-0.5, 1.875, -0.5), bg_size, vec3f(0, -0.03125 * 0.5, 0), right_vec, up_vec, 0, 0, 1, 1, GXColor{0, 0, 0, 0x3F});

        // Render partially transparent name tag (as seen behind walls)
        draw_text_3d(vec3f(-0.5, 1.875, -0.5), player_name, GXColor{0xFF, 0xFF, 0xFF, 0x3F});

        // Render the name tag normally
        gertex::use_fog(true);
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        draw_text_3d(vec3f(-0.5, 1.875, -0.5), player_name, GXColor{0xFF, 0xFF, 0xFF, 0xFF});

        // The player should not render in the transparent pass
        return;
    }
    entity_living::render(partial_ticks, transparency);

    vfloat_t h_speed = velocity.magnitude();

    if (h_speed > 0.05)
    {
        accumulated_walk_distance += h_speed * 1.8;
        player_model.speed = lerpd(player_model.speed, 32, 0.15);
    }
    else
    {
        player_model.speed = lerpd(player_model.speed, 0, 0.15);
    }

    player_model.pos = entity_position - vec3f(0.5);
    player_model.pos.y += y_offset;
    player_model.rot = vec3f(0, body_rotation_y, 0);
    player_model.head_rot = vec3f(entity_rotation.x, entity_rotation.y, 0);

    gertex::set_color_mul(get_lightmap_color(light_level));
    player_model.render(accumulated_walk_distance, partial_ticks);
}