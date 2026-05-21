#include "block_door.hpp"
#include <render/render_blocks.hpp>
#include <world/world.hpp>
#include <registry/block_list.hpp>
#include <item/item_id.hpp>
#include <util/constants.hpp>
#include <sound.hpp>
#include <sounds.hpp>

BlockDoor::BlockDoor(uint16_t id, Materials material) : BlockBase(id, material)
{
    data.sound_type = material == Materials::IRON ? BlockSoundType::metal : BlockSoundType::wood;
    data.render_type = BlockRenderType::special;
    data.render_func = render_door;
}

bool BlockDoor::is_opaque()
{
    return false;
}

bool BlockDoor::can_place(World *world, const Vec3i &pos)
{
    if (pos.y >= MAX_WORLD_Y)
        return false;
    return block_at(world, {pos.x, pos.y - 1, pos.z})->is_opaque() &&
           BlockBase::can_place(world, pos) &&
           BlockBase::can_place(world, {pos.x, pos.y + 1, pos.z});
}

uint8_t BlockDoor::face_texture_index(uint8_t face, uint8_t meta)
{
    return data.material == Materials::IRON ? 97 : 98;
}

void BlockDoor::get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB aabb;
    aabb.min = Vec3f(pos.x, pos.y, pos.z);
    aabb.max = aabb.min + Vec3f(1, 1, 1);
    uint8_t meta = world->get_meta_at(pos);
    bool open = meta & 4;
    uint8_t direction = meta & 3;
    if (!open)
        direction = (direction + 3) & 3;

    constexpr vfloat_t door_thickness = 0.1875;

    if (direction == 0)
        aabb.max.z -= 1 - door_thickness;
    else if (direction == 1)
        aabb.min.x += 1 - door_thickness;
    else if (direction == 2)
        aabb.min.z += 1 - door_thickness;
    else if (direction == 3)
        aabb.max.x -= 1 - door_thickness;

    if (aabb.intersects(other))
        aabb_list.push_back(aabb);
}

void BlockDoor::on_placed(World *world, const Vec3i &pos, uint8_t face)
{
}

void BlockDoor::set_door_rotation(World *world, const Vec3i &pos, bool open)
{
    uint8_t meta = world->get_meta_at(pos);
    if ((meta & 8) != 0)
    {
        if (world->get_block_id_at({pos.x, pos.y - 1, pos.z}) == data.block_id)
        {
            set_door_rotation(world, {pos.x, pos.y - 1, pos.z}, open);
        }
    }
    else
    {
        bool old_open = (meta & 4) != 0;
        if (old_open != open)
        {
            if (world->get_block_id_at({pos.x, pos.y + 1, pos.z}) == data.block_id)
            {
                world->set_meta_at({pos.x, pos.y + 1, pos.z}, (meta ^ 4) | 8);
            }
            world->set_meta_at(pos, meta ^ 4);
            Sound flick_sound = get_sound(javaport::Random().nextDouble() < 0.5 ? "random/door_open" : "random/door_close");
            flick_sound.position = Vec3f(pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f);
            flick_sound.pitch = world->random.nextFloat() * 0.1f + 0.9f;
            world->play_sound(flick_sound);
        }
    }
}

void BlockDoor::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
    uint8_t meta = world->get_meta_at(pos);
    if ((meta & 8) != 0)
    {
        world->set_block_at(pos, BlockID::air);
        if (block_at(world, pos + block_face[neighbor_face])->is_power_source())
            on_neighbor_changed(world, {pos.x, pos.y - 1, pos.z}, neighbor_face);
    }
    else
    {
        bool should_drop = false;
        if (world->get_block_id_at({pos.x, pos.y + 1, pos.z}) == data.block_id)
        {
            world->set_block_at({pos.x, pos.y, pos.z}, BlockID::air);
            should_drop = true;
        }
        if (!block_at(world, {pos.x, pos.y - 1, pos.z})->is_opaque())
        {
            world->set_block_at({pos.x, pos.y, pos.z}, BlockID::air);
            should_drop = true;
            if (world->get_block_id_at({pos.x, pos.y + 1, pos.z}) == data.block_id)
                world->set_block_at({pos.x, pos.y + 1, pos.z}, BlockID::air);
        }
        if (should_drop)
        {
            if (!world->is_remote())
                drop_item(world, pos, meta);
        }
        else
        {
            if (block_at(world, pos + block_face[neighbor_face])->is_power_source())
            {
                bool is_powered = block_at(world, pos)->has_indirect_power(world, pos) ||
                                  block_at(world, pos)->has_indirect_power(world, {pos.x, pos.y + 1, pos.z});
                set_door_rotation(world, pos, is_powered);
            }
        }
    }
}

bool BlockDoor::on_use(EntityPhysical *entity, const Vec3i &pos)
{
    if (data.material == Materials::IRON)
        return true;

    World *world = entity->world;
    uint8_t meta = world->get_meta_at(pos);
    bool open = (meta & 4) != 0;
    set_door_rotation(world, pos, !open);

    return true;
}

void BlockDoor::on_click(EntityPhysical *entity, const Vec3i &pos)
{
    on_use(entity, pos);
}

uint16_t BlockDoor::drop_id(uint16_t meta, javaport::Random &random)
{
    return data.material == Materials::IRON ? +ItemID::iron_door : +ItemID::door;
}
