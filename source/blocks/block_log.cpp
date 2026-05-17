#include "block_log.hpp"

#include <world/world.hpp>
#include <world/chunk_cache.hpp>

BlockLog::BlockLog(uint16_t id) : BlockBase(id, 20, Materials::WOOD)
{
    data.sound_type = BlockSoundType::wood;
}

uint8_t BlockLog::face_texture_index(uint8_t face, uint8_t meta)
{
    if (face == +BlockFace::NY || face == +BlockFace::PY)
        return 21;
    if (meta == 1)
        return 116;
    else if (meta == 2)
        return 117;
    return 20;
}

uint16_t BlockLog::drop_meta(uint16_t meta)
{
    return meta;
}

void BlockLog::on_removed(World *world, const Vec3i &pos)
{
    // Ensure surrounding chunks are loaded
    ChunkCache cache = build_chunk_cache(world, pos.x >> 4, pos.z >> 4);

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (!cache.chunks[i][j])
                return;
    for (int x = -4; x <= 4; x++)
        for (int z = -4; z <= 4; z++)
            for (int y = -4; y <= 4; y++)
            {
                Chunk *c;
                BlockState *block = get_block_cached(cache, pos.x + x, pos.y + y, pos.z + z, c);
                if (block && block->blockid == BlockID::leaves)
                    block->meta |= 4;
            }
}
