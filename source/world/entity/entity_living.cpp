#include "entity_living.hpp"
#include <world/world.hpp>
#include <block/blocks.hpp>
#include <render/render.hpp>
#include <sounds.hpp>

void EntityLiving::fall(vfloat_t distance)
{
    if (world->is_remote())
        return;
    int damage = std::ceil(distance - 3);
    if (damage > 0)
    {
        hurt(damage);

        Vec3f feet_pos(position.x, aabb.min.y - 0.5, position.z);
        Vec3i feet_block_pos = Vec3i(std::floor(feet_pos.x), std::floor(feet_pos.y), std::floor(feet_pos.z));
        BlockID block_at_feet = world->get_block_id_at(feet_block_pos, BlockID::air);
        feet_block_pos.y--;
        BlockID block_below_feet = world->get_block_id_at(feet_block_pos, BlockID::air);
        if (block_below_feet != BlockID::air)
        {
            Sound sound = get_step_sound(block_at_feet);
            sound.position = feet_pos;
            world->play_sound(sound);
        }
    }
}

void EntityLiving::hurt(int16_t damage)
{
    health -= damage;
    hurt_ticks = 10;
    if (health < 0)
        health = 0;
    if (health == 0 && !world->is_remote())
    {
        dead = true;
    }
}

void EntityLiving::tick()
{
    EntityPhysical::tick();
    last_walk_speed = walk_speed;
    walk_speed += (std::min(1.0, Vec3f(velocity.x, 0, velocity.z).magnitude() * 4) - walk_speed) * 0.4;
    accumulated_walk_distance += walk_speed;
    if (hurt_ticks > 0)
    {
        hurt_ticks--;
    }
    if (in_water)
        fall_distance = 0;
    if (on_ground)
    {
        if (fall_distance > 0)
        {
            fall(fall_distance);
            fall_distance = 0;
        }
    }
    else if ((position - prev_position).y < 0)
    {
        fall_distance -= (position - prev_position).y;
    }
    if (walk_sound)
    {
        // Play step sound if the entity is moving on the ground or when it jumps
        Vec3f player_horizontal_velocity = position - prev_position;
        last_step_distance += player_horizontal_velocity.magnitude() * 0.6;
        if ((on_ground && jumping) || last_step_distance > 1)
        {
            Vec3f feet_pos(position.x, aabb.min.y - 0.5, position.z);
            Vec3i feet_block_pos = Vec3i(std::floor(feet_pos.x), std::floor(feet_pos.y), std::floor(feet_pos.z));
            BlockID block_at_feet = world->get_block_id_at(feet_block_pos, BlockID::air);
            feet_block_pos.y--;
            BlockID block_below_feet = world->get_block_id_at(feet_block_pos, BlockID::air);
            if (block_below_feet != BlockID::air)
            {
                if (!properties(block_at_feet).m_fluid)
                {
                    Sound sound = get_step_sound(block_at_feet);
                    sound.position = feet_pos;
                    world->play_sound(sound);
                    last_step_distance = 0;
                }
            }
        }
    }
}

void EntityLiving::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;
    Vec3f entity_position = get_position(partial_ticks);
    Vec3f entity_rotation = get_rotation(partial_ticks);
    Vec3i block_pos = entity_position.round();
    BlockState *block = world->get_block_at(block_pos);
    if (block && !properties(block->id).m_solid)
    {
        light_level = block->light;
    }
    entity_position.y += y_offset;

    while (body_rotation_y >= 180)
        body_rotation_y -= 360;
    while (body_rotation_y < -180)
        body_rotation_y += 360;
    Vec3f h_velocity = Vec3f(velocity.x, 0, velocity.z);
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
        body_rotation_y = lerpd(body_rotation_y, body_rotation_y + diff, 0.05);
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

void EntityLiving::serialize(NBTTagCompound *result)
{
    EntityPhysical::serialize(result);
    result->setTag("Health", new NBTTagShort(health));
}

void EntityLiving::deserialize(NBTTagCompound *nbt)
{
    EntityPhysical::deserialize(nbt);
    health = nbt->getShort("Health");
    if (health <= 0)
    {
        dead = true;
    }
}

void EntityLiving::animate()
{
    Vec3f old_position = position;

    // Animate the entity's position and rotation
    if (animation_tick > 0)
    {
        Vec3f diff = animation_pos - position;
        Vec3f new_position = position + diff / animation_tick;
        vfloat_t diff_rot = animation_rot.y - rotation.y;
        while (diff_rot < -180.0)
            diff_rot += 360.0;
        while (diff_rot >= 180.0)
            diff_rot -= 360.0;
        Vec3f new_rotation = rotation + Vec3f((animation_rot.x - rotation.x) / animation_tick, diff_rot / animation_tick, 0);
        animation_tick--;
        teleport(new_position);
        prev_rotation = rotation;
        rotation = new_rotation;
    }
    else if (!simulate_offline)
    {
        teleport(animation_pos);
        prev_rotation = rotation;
        rotation = animation_rot;
    }

    velocity = position - old_position;
    prev_position = old_position;
}
