#include "entity_physical.hpp"
#include <world/chunk.hpp>
#include <world/world.hpp>
#include <block/blocks.hpp>
#include <render/render_fluids.hpp>
#include <nbt/serializers.hpp>

bool EntityPhysical::collides(EntityPhysical *other)
{
    return other && aabb.intersects(other->aabb);
}

bool EntityPhysical::can_remove()
{
    if (!chunk)
        return true;
    Vec3f entity_pos = get_position(0);
    if (entity_pos.y < 0)
        return true;
    if (entity_pos.y > 255)
        return false;
    Vec3i int_pos = entity_pos.round();
    Chunk *curr_chunk = world->get_chunk_from_pos(int_pos);
    if (!curr_chunk)
        return true;
    if (int_pos.y < 0 || int_pos.y >= 256)
        return false;
    Section &vbo = curr_chunk->sections[int_pos.y >> 4];
    if (vbo.dirty || curr_chunk->light_pending || !vbo.stable())
        return false;
    return dead;
}

void EntityPhysical::resolve_collision(EntityPhysical *b)
{
    Vec3f push = aabb.push_out_horizontal(b->aabb);

    // Add velocity to separate the entities
    this->velocity = this->velocity + push.fast_normalize() * 0.025;
}

void EntityPhysical::teleport(Vec3f pos)
{
    position = pos;
    prev_position = pos;
    vfloat_t half_width = width * 0.5;
    aabb.min = Vec3f(pos.x - half_width, pos.y - y_offset + y_size, pos.z - half_width);
    aabb.max = Vec3f(pos.x + half_width, pos.y - y_offset + y_size + height, pos.z + half_width);
}

void EntityPhysical::set_server_position(Vec3i pos, uint8_t ticks)
{
    server_pos = pos;
    animation_pos = Vec3f(pos.x, pos.y, pos.z) / 32.0;
    animation_tick = ticks;
}

void EntityPhysical::set_server_pos_rot(Vec3i pos, Vec3f rot, uint8_t ticks)
{
    server_pos = pos;
    animation_pos = Vec3f(pos.x, pos.y, pos.z) / 32.0;
    animation_rot = rot;
    animation_tick = ticks;
}

void EntityPhysical::tick()
{
    prev_rotation = rotation;
    prev_position = position;
    if (world->is_remote() && !simulate_offline)
    {
        return;
    }
    Vec3f fluid_velocity = Vec3f(0, 0, 0);
    bool water_movement = false;
    bool lava_movement = false;
    bool cobweb_movement = false;
    AABB fluid_aabb = aabb;
    fluid_aabb.min.y += 0.4;
    fluid_aabb.max.y -= 0.4;
    Vec3i min = Vec3i(std::floor(fluid_aabb.min.x), std::floor(fluid_aabb.min.y - 1), std::floor(fluid_aabb.min.z));
    Vec3i max = Vec3i(std::floor(fluid_aabb.max.x + 1), std::floor(fluid_aabb.max.y + 1), std::floor(fluid_aabb.max.z + 1));
    for (int x = min.x; x < max.x; x++)
    {
        for (int y = min.y; y < max.y; y++)
        {
            for (int z = min.z; z < max.z; z++)
            {
                Vec3i block_pos = Vec3i(x, y, z);
                BlockState *block = world->get_block_at(block_pos);
                if (!block)
                    continue;
                if (!block->intersects(fluid_aabb, block_pos))
                    continue;
                if (block->blockid == BlockID::cobweb)
                    cobweb_movement = true;
                if (!is_fluid(block->blockid))
                    continue;
                if (y + 1 - get_fluid_height(world, block_pos, block->blockid) >= max.y)
                    continue;
                fluid_velocity = fluid_velocity + get_fluid_direction(world, block, block_pos);
                BlockID base_fluid = basefluid(block->blockid);
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
            Sound sound = get_sound("random/splash");
            sound.position = position;
            sound.pitch = 0.6 + rng.nextFloat() * 0.8;
            sound.volume = impact;
            world->play_sound(sound);

            Particle particle;
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
                particle.position = position + Vec3f(rng.nextFloat() * 2 - 1, 0, rng.nextFloat() * 2 - 1) * width;
                particle.position.y = std::floor(aabb.min.y + 1);
                particle.velocity = velocity;
                particle.velocity.y -= 0.2 * rng.nextFloat();

                // Randomize life time
                particle.life_time = particle.max_life_time - (rand() % 20);

                world->add_particle(particle);
            }
            particle.physics |= PPHYSIC_FLAG_GRAVITY;
            particle.v = 16;

            for (int i = 0; i < 1 + width * 20; i++)
            {
                particle.u = rng.nextInt(4) + 3;
                // Randomize the particle position and velocity
                particle.position = position + Vec3f(rng.nextFloat() * 2 - 1, 0, rng.nextFloat() * 2 - 1) * width;
                particle.position.y = std::floor(aabb.min.y + 1);
                particle.velocity = velocity;

                // Randomize life time
                particle.life_time = particle.max_life_time - (rand() % 20);

                world->add_particle(particle);
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

        velocity = velocity + (fluid_velocity.fast_normalize() * 0.004);

        if (cobweb_movement)
            velocity = velocity * 0.25;

        move_and_check_collisions();

        if (water_movement)
            velocity = velocity * 0.8;
        else
            velocity = velocity * 0.5;
        velocity.y -= 0.02;

        AABB fluid_aabb = aabb;
        fluid_aabb.translate(Vec3f(velocity.x, velocity.y + 0.6 - position.y + old_y, velocity.z));
        if (horizontal_collision && !is_colliding_fluid(fluid_aabb))
        {
            velocity.y = 0.3;
        }
    }
    else
    {
        BlockID block_at_feet = world->get_block_id_at(Vec3i(std::floor(position.x), std::floor(aabb.min.y) - 1, std::floor(position.z)), BlockID::air);
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
        if (drag_phase == DragPhase::before_friction)
            velocity.y -= gravity;

        velocity.y *= 0.98;

        if (drag_phase == DragPhase::after_friction)
            velocity.y -= gravity;

        velocity.x *= h_friction;
        velocity.z *= h_friction;
    }
}
void EntityPhysical::animate()
{
}
void EntityPhysical::render(float partial_ticks, bool transparency)
{
#ifdef DEBUG_ENTITIES
    if (chunk)
    {
        Vec3f entity_position = get_position(partial_ticks);
        entity_position.y = (aabb.max.y + aabb.min.y) * 0.5;
        Vec3i block_pos = (entity_position + Vec3f(0, 0.5, 0)).round();
        Block *block = world->get_block_at(block_pos);
        if (block && !properties(block->id).m_solid)
        {
            light_level = block->light;
        }
        // Render a mob spawner at the entity's position
        Block block_state = {uint8_t(BlockID::bricks), 0x7F, 0, light_level};
        transform_view(gertex::get_view_matrix(), entity_position, (aabb.max - aabb.min));

        // Restore default texture
        use_texture(terrain_texture);
        render_single_block(block_state, transparency);
    }
#endif
}

void EntityPhysical::serialize(NBTTagCompound *result)
{
    Vec3f pos(position.x, aabb.min.y + y_offset - y_size + 0.0001, position.z);

    result->setTag("Motion", serialize_Vec3f(velocity));
    result->setTag("Pos", serialize_Vec3f(pos));
    result->setTag("Rotation", serialize_Vec2f(rotation));

    result->setTag("Air", new NBTTagShort(300));
    result->setTag("AttackTime", new NBTTagShort(0));
    result->setTag("DeathTime", new NBTTagShort(0));
    result->setTag("FallDistance", new NBTTagFloat(0));
    result->setTag("Fire", new NBTTagShort(-20));
    result->setTag("OnGround", new NBTTagByte(on_ground));
    result->setTag("HurtTime", new NBTTagShort(0));
}

void EntityPhysical::deserialize(NBTTagCompound *nbt)
{
    Vec3f pos = deserialize_Vec3f(nbt->getList("Pos"));
    Vec3f motion = deserialize_Vec3f(nbt->getList("Motion"));
    Vec3f rotation = deserialize_Vec2f(nbt->getList("Rotation"));

    teleport(pos);
    velocity = motion;
    this->rotation = rotation;

    on_ground = nbt->getByte("OnGround");
}

std::vector<AABB> EntityPhysical::get_colliding_aabbs(const AABB &aabb)
{
    std::vector<AABB> nearby_blocks;
    Vec3i min = Vec3i(std::floor(aabb.min.x), std::floor(aabb.min.y - 1), std::floor(aabb.min.z));
    Vec3i max = Vec3i(std::floor(aabb.max.x + 1), std::floor(aabb.max.y + 1), std::floor(aabb.max.z + 1));
    for (int x = min.x; x < max.x; x++)
    {
        for (int y = min.y; y < max.y; y++)
        {
            for (int z = min.z; z < max.z; z++)
            {
                Vec3i block_pos = Vec3i(x, y, z);
                BlockState *block = world->get_block_at(block_pos);
                if (!block)
                    continue;
                if (properties(block->id).m_collision == CollisionType::solid)
                    block->get_aabb(block_pos, aabb, nearby_blocks);
            }
        }
    }
    return nearby_blocks;
}

bool EntityPhysical::is_colliding_fluid(const AABB &aabb)
{
    Vec3i min = Vec3i(std::floor(aabb.min.x), std::floor(aabb.min.y), std::floor(aabb.min.z));
    Vec3i max = Vec3i(std::floor(aabb.max.x + 1), std::floor(aabb.max.y + 1), std::floor(aabb.max.z + 1));
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
                Vec3i block_pos = Vec3i(x, y, z);
                BlockState *block = world->get_block_at(block_pos);
                if (block && properties(block->id).m_collision == CollisionType::fluid && block->intersects(aabb, block_pos))
                    return true;
            }
        }
    }
    return false;
}

void EntityPhysical::move_and_check_collisions()
{
    horizontal_collision = false;
    Vec3f old_velocity = velocity;
    Vec3f new_velocity = velocity;

    std::vector<AABB> nearby_blocks = get_colliding_aabbs(aabb.extend_by(velocity));

    for (AABB &block_aabb : nearby_blocks)
    {
        new_velocity.y = block_aabb.calculate_y_offset(aabb, new_velocity.y);
    }
    aabb.translate(Vec3f(0, new_velocity.y, 0));

    on_ground = old_velocity.y != new_velocity.y && old_velocity.y < 0;

    for (AABB &block_aabb : nearby_blocks)
    {
        new_velocity.x = block_aabb.calculate_x_offset(aabb, new_velocity.x);
    }
    aabb.translate(Vec3f(new_velocity.x, 0, 0));

    horizontal_collision = old_velocity.x != new_velocity.x;

    for (AABB &block_aabb : nearby_blocks)
    {
        new_velocity.z = block_aabb.calculate_z_offset(aabb, new_velocity.z);
    }
    aabb.translate(Vec3f(0, 0, new_velocity.z));

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