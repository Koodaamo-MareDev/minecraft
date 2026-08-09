#include "block_redstone_torch.hpp"

#include <world/world.hpp>
#include <item/item_id.hpp>
#include <sounds.hpp>

BlockRedstoneTorch::BlockRedstoneTorch(uint16_t id, uint8_t texture_index, bool active) : BlockTorch(id, texture_index), active(active)
{
    data.tick_on_load = true;
}

uint16_t BlockRedstoneTorch::drop_id(uint16_t meta, javaport::Random &random)
{
    return BlockID::redstone_torch;
}

void BlockRedstoneTorch::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_id)
{
    BlockTorch::on_neighbor_changed(world, pos, neighbor_id);
    world->schedule_block_update(pos, data.block_id, delay_ticks);
}

void BlockRedstoneTorch::on_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    bool powered = gets_signal(world, pos);

        printf("ticked");
    while (!updates.empty() && world->ticks - updates.front().ticks > 100)
        updates.pop_front();

    if (this->active)
    {
        if (powered)
        {
            world->set_block_and_meta_at(pos, BlockID::unlit_redstone_torch, world->get_meta_at(pos));
            world->notify_at(pos);
            if (check_burnout(world, pos, true))
            {
                Sound fizz_sound = get_sound("random/fizz");
                fizz_sound.position = pos + Vec3f(0.5);
                fizz_sound.volume = 0.5f;
                fizz_sound.pitch = 2.6f + (random.nextFloat() - random.nextFloat()) * 0.8f;
                world->play_sound(fizz_sound);
                for (int i = 0; i < 5; i++)
                {
                    Particle p(world);
                    p.type = PTYPE_TINY_SMOKE;
                    p.position = fizz_sound.position + Vec3f(random.nextDouble() * 0.6 + 0.2, random.nextDouble() * 0.6 + 0.2, random.nextDouble() * 0.6 + 0.2);
                    world->add_particle(p);
                }
            }
        }
    }
    else if (!powered && !check_burnout(world, pos, false))
    {
        world->set_block_and_meta_at(pos, BlockID::redstone_torch, world->get_meta_at(pos));
        world->notify_at(pos);
    }
}

void BlockRedstoneTorch::on_added(World *world, const Vec3i &pos)
{
    if (world->get_meta_at(pos) == 0)
        BlockTorch::on_added(world, pos);

    if (this->active)
        for (uint8_t i = 0; i < 6; i++)
        {
            world->notify_at(block_face[(i + 2) % 6], data.block_id);
        }
}

void BlockRedstoneTorch::on_removed(World *world, const Vec3i &pos)
{
    if (this->active)
        for (uint8_t i = 0; i < 6; i++)
        {
            world->notify_at(block_face[(i + 2) % 6], data.block_id);
        }
}

bool BlockRedstoneTorch::provides_power(World *world, const Vec3i &pos, uint8_t face)
{
    if (!this->active)
        return false;

    uint8_t meta = world->get_meta_at(pos);
    return !((meta == 5 && face == +BlockFace::PY) ||
             (meta == 4 && face == +BlockFace::PZ) ||
             (meta == 3 && face == +BlockFace::NZ) ||
             (meta == 2 && face == +BlockFace::PX) ||
             (meta == 1 && face == +BlockFace::NX));
}

bool BlockRedstoneTorch::provides_indirect_power(World *world, const Vec3i &pos, uint8_t face)
{
    return face == +BlockFace::NY && provides_power(world, pos, face);
}

bool BlockRedstoneTorch::is_power_source()
{
    return true;
}

bool BlockRedstoneTorch::check_burnout(World *world, const Vec3i &pos, bool add)
{
    if (add)
        updates.emplace_back(RedstoneUpdate{.world = world, .pos = pos, .ticks = world->ticks});

    int same_count = 0;

    for (RedstoneUpdate &e : updates)
    {
        if (e.world == world && e.pos == pos)
        {
            same_count++;
            if (same_count >= 8)
                return true;
        }
    }
    return false;
}

bool BlockRedstoneTorch::gets_signal(World *world, const Vec3i &pos)
{
    uint8_t meta = world->get_meta_at(pos);
    switch (meta)
    {
    case 1:
        return provides_indirect_power(world, pos + block_face[+BlockFace::NX], +BlockFace::NX);
    case 2:
        return provides_indirect_power(world, pos + block_face[+BlockFace::PX], +BlockFace::PX);
    case 3:
        return provides_indirect_power(world, pos + block_face[+BlockFace::NZ], +BlockFace::NZ);
    case 4:
        return provides_indirect_power(world, pos + block_face[+BlockFace::PZ], +BlockFace::PZ);
    case 5:
        return provides_indirect_power(world, pos + block_face[+BlockFace::NY], +BlockFace::NY);

    default:
        return false;
    }
}
