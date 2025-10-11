#include "chunkprovider.hpp"

#include <world/util/raycast.hpp>
#include <world/chunk.hpp>
#include <world/world.hpp>
#include <ported/NoiseSynthesizer.hpp>
#include <ported/MapGenTerrain.hpp>
#include <ported/MapGenCaves.hpp>
#include <ported/MapGenSurface.hpp>
#include <ported/WorldGenLiquids.hpp>
#include <ported/WorldGenLakes.hpp>

ChunkProviderOverworld::ChunkProviderOverworld(World *world)
{
    // Initialize the noise synthesizer with the seed
    this->world = world;
    noiser.Init(world->seed);
}

void ChunkProviderOverworld::provide_chunk(Chunk *chunk)
{
    // Prioritize loading the chunk from disk if it exists
    if (chunk->generation_stage == ChunkGenStage::loading)
    {
        chunk->read();
        return;
    }

    int index;
    javaport::Random rng(chunk->x * 0x4F9939F508L + chunk->z * 0x1F38D3E7L + world->seed);

    // Reset the blocks array
    std::fill_n(blocks, 16 * 16 * WORLD_HEIGHT, BlockID::air);

    // Generate the base terrain
    javaport::MapGenTerrain terrain_gen(noiser);
    terrain_gen.generate(chunk, world->seed, blocks);

    // Generate the caves
    javaport::MapGenCaves cavegen;
    cavegen.generate(chunk, world->seed, blocks);

    // Add the surface layer
    javaport::MapGenSurface surface_gen(noiser);
    surface_gen.generate(chunk, world->seed, blocks);

    // Generate the bedrock layer
    for (index = 0; index < 256; index++)
    {
        blocks[index] = BlockID::bedrock;
    }

    // Randomize the additional bedrock layers (1-4)
    for (; index < 1280; index++)
    {
        if (rng.next(1))
            blocks[index] = BlockID::bedrock;
    }

    // Apply the generated blocks to the chunk
    Block *block = chunk->blockstates;
    for (int32_t i = 0; i < 16 * 16 * WORLD_HEIGHT; i++, block++)
    {
        block->set_blockid(blocks[i]);
    }

    // Fix snowy grass
    for (index = 16 * 16 * 63; index < 16 * 16 * MAX_WORLD_Y; index++)
    {
        if (blocks[index] == BlockID::grass && blocks[index + 256] == BlockID::snow_layer)
        {
            chunk->blockstates[index].meta = 1;
        }
    }

    // The chunk is now ready for features generation
    chunk->generation_stage = ChunkGenStage::features;
}

void ChunkProviderOverworld::populate_chunk(Chunk *chunk)
{
    // Populate the base height map for generating surface features
    for (int z = 0, index = 0; z < 16; z++)
        for (int x = 0; x < 16; x++, index++)
            chunk->terrain_map[index] = skycast(Vec3i(x, 0, z), chunk);

    for (int32_t x = chunk->x - 1; x <= chunk->x + 1; x++)
    {
        for (int32_t z = chunk->z - 1; z <= chunk->z + 1; z++)
        {
            // Skip corners
            if ((x - chunk->x) && (z - chunk->z))
                continue;

            // Attempt to generate features for the current chunk and its neighbors
            Chunk *neighbor = world->get_chunk(x, z);
            if (neighbor)
            {
                generate_features(neighbor);

                // Update the VBOs of the neighboring chunks
                for (int i = 0; i < VERTICAL_SECTION_COUNT; i++)
                {
                    neighbor->sections[i].dirty = true;
                }
            }
        }
    }

    chunk->generation_stage = ChunkGenStage::done;
}

void ChunkProviderOverworld::plant_tree(Vec3i pos, int height)
{
    Block *base_block = world->get_block_at(pos - Vec3i(0, 1, 0));
    if (!base_block)
        return;
    BlockID base_id = base_block->get_blockid();
    if (base_id != BlockID::grass && base_id != BlockID::dirt)
        return;

    // Place dirt below tree
    base_block->set_blockid(BlockID::dirt);

    // Place wide part of leaves
    for (int x = -2; x <= 2; x++)
    {
        for (int y = height - 3; y < height - 1; y++)
            for (int z = -2; z <= 2; z++)
            {
                Vec3i leaves_pos = pos + Vec3i(x, y, z);
                world->replace_air_at(leaves_pos, BlockID::leaves);
            }
    }
    // Place narrow part of leaves
    for (int x = -1; x <= 1; x++)
    {
        for (int y = height - 1; y <= height; y++)
        {
            for (int z = -1; z <= 1; z++)
            {
                Vec3i leaves_off = Vec3i(x, y, z);
                // Place the leaves in a "+" pattern at the top.
                if (y == height && (x | z) && std::abs(x) == std::abs(z))
                    continue;
                world->replace_air_at(pos + leaves_off, BlockID::leaves);
            }
        }
    }
    // Place tree logs
    for (int y = 0; y < height; y++)
    {
        world->set_block_at(pos, BlockID::wood);
        ++pos.y;
    }
}

void ChunkProviderOverworld::generate_trees(Vec3i pos, javaport::Random &rng)
{
    Chunk *chunk = world->get_chunk_from_pos(pos);
    constexpr size_t tree_count = 8;
    uint32_t tree_positions[tree_count];
    for (size_t i = 0; i < tree_count; i++)
        tree_positions[i] = rng.nextInt();

    // Generate the trees based on the noise value
    uint32_t max_trees = 8 * std::clamp(noiser.GetNoise(pos.x / 128.0f, pos.z / 128.0f) * 0.625f + 0.5f, 0.0, 1.0);
    for (uint32_t c = 0; c < max_trees; c++)
    {
        uint32_t tree_value = tree_positions[c];
        Vec3i tree_pos((tree_value & 15), 0, ((tree_value >> 24) & 15));

        // Place the trees on above ground.
        tree_pos.y = chunk->terrain_map[tree_pos.x | (tree_pos.z << 4)];
        tree_pos += Vec3i(pos.x, 1, pos.z);

        // Trees should only grow on top of the sea level
        if (tree_pos.y >= 64)
        {
            int tree_height = (tree_value >> 28) & 3;
            plant_tree(tree_pos, 3 + tree_height);
        }
    }
}

void ChunkProviderOverworld::generate_flowers(Vec3i pos, javaport::Random &rng)
{
    Chunk *chunk = world->get_chunk_from_pos(pos);
    constexpr size_t flower_count = 16;
    uint32_t flower_positions[flower_count];
    for (size_t i = 0; i < flower_count; i++)
        flower_positions[i] = rng.nextInt();

    // Generate flowers based on the noise value
    uint32_t max_trees = 16 * std::clamp(noiser.GetNoise((pos.x - 121.47f) / 128.0f, (pos.z + 121.47f) / 128.0f) * 0.625f + 0.5f, 0.0, 1.0);
    for (uint32_t c = 0; c < max_trees; c++)
    {
        uint32_t flower_value = flower_positions[c];
        Vec3i flower_pos((flower_value & 15), 0, ((flower_value >> 24) & 15));

        // Place the flowers above ground.
        flower_pos.y = chunk->terrain_map[flower_pos.x | (flower_pos.z << 4)];
        flower_pos += Vec3i(pos.x, 1, pos.z);

        // Flowers should only grow on top of the sea level
        if (flower_pos.y >= 64)
        {
            Block *block = chunk->get_block(flower_pos - Vec3i(0, 1, 0));
            if (block->get_blockid() != BlockID::grass)
                continue;
            block = chunk->get_block(flower_pos);
            if (block->get_blockid() != BlockID::air)
                continue;
            block->set_blockid((flower_value & 1) ? BlockID::dandelion : BlockID::rose);
        }
    }
}

void ChunkProviderOverworld::generate_vein(Vec3i pos, BlockID id, javaport::Random &rng)
{
    for (int x = pos.x; x < pos.x + 2; x++)
    {
        for (int y = pos.y; y < pos.y + 2; y++)
        {
            for (int z = pos.z; z < pos.z + 2; z++)
            {
                if (rng.nextInt(2) == 0)
                {
                    Vec3i pos(x, y, z);
                    Block *block = world->get_block_at(pos);
                    if (block && block->get_blockid() == BlockID::stone)
                    {
                        block->set_blockid(id);
                    }
                }
            }
        }
    }
}

void ChunkProviderOverworld::generate_ore_type(Vec3i pos, BlockID id, int count, int max_height, javaport::Random &rng)
{
    for (int i = 0; i < count; i++)
    {
        int x = rng.nextInt(16);
        int y = rng.nextInt(80);
        int z = rng.nextInt(16);
        Vec3i offset_pos(x + pos.x, y, z + pos.z);
        if (y > max_height)
            continue;
        generate_vein(offset_pos, id, rng);
        Block *block = world->get_block_at(offset_pos);
        x = rng.nextInt(3) - 1;
        y = rng.nextInt(3) - 1;
        z = rng.nextInt(3) - 1;
        if (block && block->get_blockid() == id && (x | y | z) != 0)
        {
            offset_pos = Vec3i(x + offset_pos.x, y + offset_pos.y, z + offset_pos.z);
            generate_vein(offset_pos, id, rng);
        }
    }
}

void ChunkProviderOverworld::generate_ores(Vec3i pos, javaport::Random &rng)
{
    int coal_count = rng.nextInt(8) + 16;
    int iron_count = rng.nextInt(4) + 8;
    int gold_count = rng.nextInt(4) + 8;
    int diamond_count = rng.nextInt(4) + 8;

    generate_ore_type(pos, BlockID::coal_ore, coal_count, 80, rng);
    generate_ore_type(pos, BlockID::iron_ore, iron_count, 56, rng);
    generate_ore_type(pos, BlockID::gold_ore, gold_count, 32, rng);
    generate_ore_type(pos, BlockID::diamond_ore, diamond_count, 12, rng);
}

void ChunkProviderOverworld::generate_features(Chunk *chunk)
{
    javaport::Random rng(chunk->x * 0x4F9939F508L + chunk->z * 0x1F38D3E7L + world->seed);
    Vec3i block_pos(chunk->x * 16, 0, chunk->z * 16);
    generate_ores(block_pos, rng);
    generate_trees(block_pos, rng);
    generate_flowers(block_pos, rng);
    if (rng.nextInt(4) == 0)
    {
        javaport::WorldGenLakes lake_gen(world, BlockID::water);
        Vec3i pos(rng.nextInt(16) + block_pos.x, rng.nextInt(128), rng.nextInt(16) + block_pos.z);
        lake_gen.generate(rng, pos);
    }

    if (rng.nextInt(8) == 0)
    {
        javaport::WorldGenLakes lava_lake_gen(world, BlockID::lava);
        Vec3i pos(rng.nextInt(16) + block_pos.x, rng.nextInt(128), rng.nextInt(16) + block_pos.z);
        if (pos.y < 64 || rng.nextInt(10) == 0)
            lava_lake_gen.generate(rng, pos);
    }

    javaport::WorldGenLiquids water_liquid_gen(world, BlockID::water);
    for (int i = 0; i < 50; i++)
    {
        Vec3i pos(rng.nextInt(16) + 8, rng.nextInt(120) + 8, rng.nextInt(16) + 8);
        water_liquid_gen.generate(rng, pos + block_pos);
    }

    javaport::WorldGenLiquids lava_gen(world, BlockID::lava);
    for (int i = 0; i < 20; i++)
    {
        Vec3i pos(rng.nextInt(16) + 8, rng.nextInt(rng.nextInt(rng.nextInt(112) + 8) + 8), rng.nextInt(16) + 8);
        lava_gen.generate(rng, pos + block_pos);
    }
}
