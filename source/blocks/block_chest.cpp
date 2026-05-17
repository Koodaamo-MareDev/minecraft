#include "block_chest.hpp"

#include <world/world.hpp>
#include <world/tile_entity/tile_entity_chest.hpp>
#include <world/chunk.hpp>
#include <block/block_list.hpp>
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
    BlockState *block = world->get_block_at(pos);

    // Bottom and top faces are always the same
    if (face == +BlockFace::NY || face == +BlockFace::PY)
        return 25;

    BlockState *neighbors[4];
    {
        BlockState *tmp_neighbors[6];
        if (world)
            world->get_neighbors(pos, tmp_neighbors);
        neighbors[0] = tmp_neighbors[0];
        neighbors[1] = tmp_neighbors[1];
        neighbors[2] = tmp_neighbors[4];
        neighbors[3] = tmp_neighbors[5];
    }

    if (std::none_of(neighbors, neighbors + 4, [](BlockState *block)
                     { return block && block->blockid == BlockID::chest; }))
    {
        // Single chest
        uint8_t direction = FACE_PZ;
        if (!(block->visibility_flags & (VIS_PZ)) && (block->visibility_flags & (VIS_NZ)))
            direction = FACE_NZ;
        if (!(block->visibility_flags & (VIS_NX)) && (block->visibility_flags & (VIS_PX)))
            direction = FACE_PX;
        if (!(block->visibility_flags & (VIS_PX)) && (block->visibility_flags & (VIS_NX)))
            direction = FACE_NX;

        return 26 + (face == direction);
    }

    // Double chest

    if ((face != FACE_NZ && face != FACE_PZ) && neighbors[0]->blockid != BlockID::chest && neighbors[1]->blockid != BlockID::chest)
    {
        // X axis
        bool half = neighbors[2]->blockid == BlockID::chest;
        uint8_t other_flags = neighbors[half ? 2 : 3]->visibility_flags;
        uint8_t direction = FACE_PX;
        if ((!(block->visibility_flags & (VIS_PX)) || !(other_flags & (VIS_PX))) && (block->visibility_flags & (VIS_NX)) && (other_flags & (VIS_NX)))
            direction = FACE_NX;

        if (face == FACE_NX)
            half = !half;

        return 26 + (face == direction ? 16 : 32) - half;
    }
    else if ((face != FACE_NX && face != FACE_PX) && neighbors[2]->blockid != BlockID::chest && neighbors[3]->blockid != BlockID::chest)
    {
        // Z axis
        bool half = neighbors[0]->blockid == BlockID::chest;

        uint8_t other_flags = neighbors[half ? 0 : 1]->visibility_flags;
        uint8_t direction = FACE_PZ;
        if ((!(block->visibility_flags & (VIS_PZ)) || !(other_flags & (VIS_PZ))) && (block->visibility_flags & (VIS_NZ)) && (other_flags & (VIS_NZ)))
            direction = FACE_NZ;

        if (face == FACE_PZ)
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
