#include "block_chest.hpp"

#include <world/world.hpp>
#include <world/tile_entity/tile_entity_chest.hpp>
#include <world/chunk.hpp>
#include <registry/block_list.hpp>
#include <block/block_properties.hpp>
#include <gui/gui_container.hpp>

BlockChest::BlockChest(uint16_t id) : BlockContainer(id, Materials::WOOD)
{
    data.texture_index = 26;
    data.sound_type = BlockSoundType::wood;
    data.render_type = BlockRenderType::full_special;
}

uint8_t BlockChest::texture_index(World *world, const Vec3i &pos, uint8_t face)
{
    // Bottom and top faces are always the same
    if (face == +BlockFace::NY || face == +BlockFace::PY)
        return 25;

    BlockBase *neighbors[4];
    {
        neighbors[0] = block_at(world, pos + Vec3i{-1, 0, 0});
        neighbors[1] = block_at(world, pos + Vec3i{+1, 0, 0});
        neighbors[2] = block_at(world, pos + Vec3i{0, 0, -1});
        neighbors[3] = block_at(world, pos + Vec3i{0, 0, +1});
    }

    if (!has_neighbor_chest(world, pos))
    {
        // Single chest
        uint8_t direction = +BlockFace::PZ;
        if (!should_render_side(world, pos, +BlockFace::PZ) && should_render_side(world, pos, +BlockFace::NZ))
            direction = +BlockFace::NZ;
        if (!should_render_side(world, pos, +BlockFace::NX) && should_render_side(world, pos, +BlockFace::PX))
            direction = +BlockFace::PX;
        if (!should_render_side(world, pos, +BlockFace::PX) && should_render_side(world, pos, +BlockFace::NX))
            direction = +BlockFace::NX;

        return 26 + (face == direction);
    }

    // Double chest

    if ((face != +BlockFace::NZ && face != +BlockFace::PZ) && neighbors[0]->block_id() != BlockID::chest && neighbors[1]->block_id() != BlockID::chest)
    {
        // X axis
        bool half = neighbors[2]->block_id() == BlockID::chest;
        uint8_t direction = +BlockFace::PX;
        BlockBase *n = neighbors[half ? 2 : 3];
        if ((!should_render_side(world, pos, +BlockFace::PX) || !n->should_render_side(world, pos, +BlockFace::PX)) &&
            should_render_side(world, pos, +BlockFace::NX) && n->should_render_side(world, pos, +BlockFace::NX))
            direction = +BlockFace::NX;

        if (face == +BlockFace::NX)
            half = !half;

        return 26 + (face == direction ? 16 : 32) - half;
    }
    else if ((face != +BlockFace::NX && face != +BlockFace::PX) && neighbors[2]->block_id() != BlockID::chest && neighbors[3]->block_id() != BlockID::chest)
    {
        // Z axis
        bool half = neighbors[0]->block_id() == BlockID::chest;
        uint8_t direction = +BlockFace::PZ;
        BlockBase *n = neighbors[half ? 0 : 1];
        if ((!should_render_side(world, pos, +BlockFace::PZ) || !n->should_render_side(world, pos, +BlockFace::PZ)) &&
            should_render_side(world, pos, +BlockFace::NZ) && n->should_render_side(world, pos, +BlockFace::NZ))
            direction = +BlockFace::NZ;

        if (face == +BlockFace::PZ)
            half = !half;

        return 26 + (face == direction ? 16 : 32) - half;
    }
    return 26;
}

bool BlockChest::has_neighbor_chest(World *world, const Vec3i &pos)
{
    return (world->get_block_id_at({pos.x - 1, pos.y, pos.z}) == data.block_id ||
            world->get_block_id_at({pos.x + 1, pos.y, pos.z}) == data.block_id ||
            world->get_block_id_at({pos.x, pos.y, pos.z - 1}) == data.block_id ||
            world->get_block_id_at({pos.x, pos.y, pos.z + 1}) == data.block_id);
}

bool BlockChest::can_place(World *world, const Vec3i &pos)
{
    int surrounding = 0;
    // Prevent placement between two chests
    if (world->get_block_id_at({pos.x - 1, pos.y, pos.z}) == data.block_id)
        surrounding++;
    if (world->get_block_id_at({pos.x + 1, pos.y, pos.z}) == data.block_id)
        surrounding++;
    if (world->get_block_id_at({pos.x, pos.y, pos.z - 1}) == data.block_id)
        surrounding++;
    if (world->get_block_id_at({pos.x, pos.y, pos.z + 1}) == data.block_id)
        surrounding++;

    if (surrounding > 1)
        return false;

    // Prevent placing next to double chest.
    if (has_neighbor_chest(world, {pos.x - 1, pos.y, pos.z}))
        return false;
    if (has_neighbor_chest(world, {pos.x + 1, pos.y, pos.z}))
        return false;
    if (has_neighbor_chest(world, {pos.x, pos.y, pos.z - 1}))
        return false;
    if (has_neighbor_chest(world, {pos.x, pos.y, pos.z + 1}))
        return false;
    return true;
}

void BlockChest::on_removed(World *world, const Vec3i &pos)
{
    if (!world->is_remote())
    {
        TileEntityChest *te = dynamic_cast<TileEntityChest *>(world->get_tile_entity(pos));

        for (size_t i = 0; i < te->items.size(); i++)
        {
            item::ItemStack &stack = te->items[i];
            while (!stack.empty())
            {
                int count = world->random.nextInt(21) + 10;
                if (count > stack.count)
                    count = stack.count;
                item::ItemStack drop = stack;
                stack.count -= count;
                drop.count = count;
                if (!drop.empty())
                    world->spawn_drop(pos, drop);
            }
        }
    }
    BlockContainer::on_removed(world, pos);
}

bool BlockChest::on_use(EntityPhysical *entity, const Vec3i &pos)
{
    World *world = entity->world;
    if (world->get_block_id_at({pos.x, pos.y + 1, pos.z}))
    {
        return true;
    }
    else if (block_at(world, {pos.x - 1, pos.y, pos.z})->block_id() == data.block_id &&
             block_at(world, {pos.x - 1, pos.y + 1, pos.z})->is_opaque())
    {
        return true;
    }
    else if (block_at(world, {pos.x + 1, pos.y, pos.z})->block_id() == data.block_id &&
             block_at(world, {pos.x + 1, pos.y + 1, pos.z})->is_opaque())
    {
        return true;
    }
    else if (block_at(world, {pos.x, pos.y, pos.z - 1})->block_id() == data.block_id &&
             block_at(world, {pos.x, pos.y + 1, pos.z - 1})->is_opaque())
    {
        return true;
    }
    else if (block_at(world, {pos.x, pos.y, pos.z + 1})->block_id() == data.block_id &&
             block_at(world, {pos.x, pos.y + 1, pos.z + 1})->is_opaque())
    {
        return true;
    }

    if (!world->is_remote())
    {
        TileEntityChest *te = dynamic_cast<TileEntityChest *>(world->get_tile_entity(pos));
        if (!te)
            return true;

        // TODO: Handle Large Chest Inventory

        Gui::set_gui(new GuiContainer(entity, &te->items));
    }

    return true;
}

TileEntity *BlockChest::get_tile_entity(World *world, const Vec3i &pos)
{
    return new TileEntityChest;
}
