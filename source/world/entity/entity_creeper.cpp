#include "entity_creeper.hpp"
#include <world/entity/entity_player.hpp>
#include <world/world.hpp>
#include <render/render.hpp>
#include <render/model.hpp>
#include <sounds.hpp>

static CreeperModel creeper_model = CreeperModel();
constexpr static uint16_t creeper_fuse = 30;

EntityCreeper::EntityCreeper(const Vec3f &position) : EntityExplosive(), EntityLiving()
{
    this->fuse = creeper_fuse;
    this->power = 3;
    this->health = 20;
    this->width = 0.6;
    this->height = 1.7;
    this->walk_sound = false;
    this->gravity = 0.08;
    creeper_model.texture = &creeper_texture;
    teleport(position);
}

void EntityCreeper::tick()
{
    if (world->is_remote())
    {
        EntityLiving::tick();
        return;
    }

    if (!follow_entity)
    {
        for (int x = position.x - 16; x <= position.x + 16 && !follow_entity; x += 16)
        {
            for (int z = position.z - 16; z <= position.z + 16 && !follow_entity; z += 16)
            {
                Chunk *curr_chunk = world->get_chunk_from_pos(Vec3i(x, 0, z));
                if (!curr_chunk)
                    continue;
                for (EntityPhysical *entity : curr_chunk->entities)
                {
                    if (entity && !entity->dead && dynamic_cast<EntityPlayer *>(entity))
                    {
                        Vec3f entity_position = entity->get_position(0);
                        Vec3f direction = entity_position - position;
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
        Vec3f direction = follow_entity->get_head_pos() - get_head_pos();
        vfloat_t sqrdistance = direction.sqr_magnitude();
        if (sqrdistance < 512 && sqrdistance > 0.5)
        {
            rotation = vector_to_angles(direction.fast_normalize());
            body_rotation_y = lerpd(body_rotation_y, std::atan2(movement.x, movement.z) / M_DTOR, world->partial_ticks * 0.5f);
            EntityPathfinder::tick();
        }
        // Explode if the player is too close
        if (sqrdistance < 6.25)
        {
            // Stop movement
            movement = Vec3f(0, 0, 0);

            // Play the fuse sound
            if (fuse == creeper_fuse)
            {
                Sound sound = get_sound("random/fuse");
                sound.position = position;
                sound.volume = 0.5;
                sound.pitch = 0.5;
                world->play_sound(sound);
            }

            EntityExplosive::tick();
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
    EntityLiving::tick();
}

bool EntityCreeper::should_jump()
{
    if (!follow_entity)
        return false;

    if (in_water || movement.y > 0)
        return true;

    return false;
}

void EntityCreeper::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;

    // The creeper should not render in the transparent pass
    if (transparency)
        return;

    EntityLiving::render(partial_ticks, transparency);

    Vec3f entity_position = get_position(partial_ticks);
    Vec3f entity_rotation = get_rotation(partial_ticks);

    creeper_model.speed = std::min(1.0f, last_walk_speed + (walk_speed - last_walk_speed) * partial_ticks);
    creeper_model.pos = entity_position;
    creeper_model.rot.y = body_rotation_y;
    creeper_model.head_rot = Vec3f(entity_rotation.x, entity_rotation.y, 0);

    // Draw the creeper
    gertex::set_color_mul(get_lightmap_color(light_level) * (14 / 16.));

    if (fuse < creeper_fuse)
    {
        gertex::set_color_add(std::sin(fuse + partial_ticks * 0.1) > 0 ? GXColor{0, 0, 0, 0xFF} : GXColor{0xFF, 0xFF, 0xFF, 0xFF});
    }
    creeper_model.pose(accumulated_walk_distance - walk_speed * (1 - partial_ticks));
    creeper_model.render(partial_ticks, transparency);
}
