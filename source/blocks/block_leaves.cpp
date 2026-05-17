#include "block_leaves.hpp"

#include <block/blocks.hpp>
#include <world/world.hpp>
#include <world/chunk_cache.hpp>

BlockLeaves::BlockLeaves(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::LEAVES)
{
    data.sound_type = BlockSoundType::grass;
}

bool BlockLeaves::is_opaque()
{
    return false;
}

bool BlockLeaves::should_render_side(World *world, const Vec3i &pos, uint8_t face)
{
    return render_fast_leaves ? BlockBase::should_render_side(world, pos, face) : (world->get_block_id_at(pos) == data.block_id);
}

uint8_t BlockLeaves::face_texture_index(uint8_t face, uint8_t meta)
{
    if (meta == 1)
        return 219 + render_fast_leaves;
    if (meta == 2)
        return 52 + render_fast_leaves;
    return 186 + render_fast_leaves;
}

void BlockLeaves::on_removed(World *world, const Vec3i &pos)
{
    ChunkCache cache = build_chunk_cache(world, pos.x >> 4, pos.z >> 4);

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (!cache.chunks[i][j])
                return;
    for (int x = -1; x <= 1; x++)
        for (int z = -1; z <= 1; z++)
            for (int y = -1; y <= 1; y++)
            {
                Chunk *c;
                BlockState *block = get_block_cached(cache, pos.x + x, pos.y + y, pos.z + z, c);
                if (block && block->blockid == BlockID::leaves)
                    block->meta |= 4;
            }
}

void BlockLeaves::on_random_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    // Server-side only
    if (world->is_remote())
        return;

    int metadata = world->get_meta_at(pos);

    constexpr int decay_flag = 0x4;

    // Leaf does not need decay check
    if ((metadata & decay_flag) == 0)
    {
        return;
    }

    constexpr int search_radius = 4;

    constexpr int buffer_size = 32;
    constexpr int buffer_center = buffer_size / 2;
    constexpr int buffer_plane = buffer_size * buffer_size;

    // Allocate reusable buffer
    std::vector<int> adjacent_tree_blocks;
    adjacent_tree_blocks.resize(
        buffer_size * buffer_size * buffer_size);

    // Ensure surrounding chunks are loaded
    ChunkCache cache = build_chunk_cache(world, pos.x >> 4, pos.z >> 4);

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (!cache.chunks[i][j])
                return;

    /**
     * Buffer values:
     *  0  = wood
     * -1  = other block
     * -2  = leaves
     *  1+ = distance from wood
     */

    auto get_index = [&](int dx, int dy, int dz)
    {
        return (dx + buffer_center) * buffer_plane +
               (dy + buffer_center) * buffer_size +
               (dz + buffer_center);
    };

    // ---------------------------------------------------------------------
    // Step 1: Scan nearby blocks
    // ---------------------------------------------------------------------

    for (int dx = -search_radius; dx <= search_radius; ++dx)
    {
        for (int dy = -search_radius; dy <= search_radius; ++dy)
        {
            for (int dz = -search_radius; dz <= search_radius; ++dz)
            {

                int block_id =
                    world->get_block_id_at(pos + Vec3i{dx, dy, dz});

                int index = get_index(dx, dy, dz);

                if (block_id == BlockID::wood)
                {
                    adjacent_tree_blocks[index] = 0;
                }
                else if (block_id == BlockID::leaves)
                {
                    adjacent_tree_blocks[index] = -2;
                }
                else
                {
                    adjacent_tree_blocks[index] = -1;
                }
            }
        }
    }

    // ---------------------------------------------------------------------
    // Step 2: Flood-fill outward from wood blocks
    // ---------------------------------------------------------------------

    auto mark_leaf_if_needed =
        [&](int dx, int dy, int dz, int distance)
    {
        int index = get_index(dx, dy, dz);

        // -2 means unvisited leaves
        if (adjacent_tree_blocks[index] == -2)
        {
            adjacent_tree_blocks[index] = distance;
        }
    };

    for (int distance = 1; distance <= 4; ++distance)
    {

        for (int dx = -search_radius; dx <= search_radius; ++dx)
        {
            for (int dy = -search_radius; dy <= search_radius; ++dy)
            {
                for (int dz = -search_radius; dz <= search_radius; ++dz)
                {

                    int index = get_index(dx, dy, dz);

                    // Found previous distance level
                    if (adjacent_tree_blocks[index] == distance - 1)
                    {

                        mark_leaf_if_needed(
                            dx - 1, dy, dz, distance);

                        mark_leaf_if_needed(
                            dx + 1, dy, dz, distance);

                        mark_leaf_if_needed(
                            dx, dy - 1, dz, distance);

                        mark_leaf_if_needed(
                            dx, dy + 1, dz, distance);

                        mark_leaf_if_needed(
                            dx, dy, dz - 1, distance);

                        mark_leaf_if_needed(
                            dx, dy, dz + 1, distance);
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------------------
    // Step 3: Check center leaf
    // ---------------------------------------------------------------------

    int center_index = get_index(0, 0, 0);

    bool connected_to_wood =
        adjacent_tree_blocks[center_index] >= 0;

    if (connected_to_wood)
    {

        // Clear decay flag
        world->set_meta_at(pos, metadata & ~decay_flag);
    }
    else
    {
        // Leaf is no longer connected to wood
        drop_item(world, pos, metadata);
        world->set_block_at(pos, BlockID::air);
    }
}

uint16_t BlockLeaves::drop_count(javaport::Random &random)
{
    return random.nextInt(16) == 0 ? 1 : 0;
}

uint16_t BlockLeaves::drop_id(uint16_t meta, javaport::Random &random)
{
    return +BlockID::sapling;
}
