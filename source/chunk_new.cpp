#include "chunk_new.hpp"
#include "blocks.hpp"
#include "texturedefs.h"
#include "vec2i.hpp"
#include "vec3i.hpp"
#include "blocks.hpp"
#include "timers.hpp"
#include "light.hpp"
#include "fastnoise/fastnoiselite.h"
#include "improvednoise/improvednoise.hpp"
#include "base3d.hpp"
#include "render.hpp"
#include "raycast.hpp"
#include "lock.hpp"
#include "asynclib.hpp"
#include "ported/MapGenCaves.hpp"
#include "ported/WorldGenLiquids.hpp"
#include "ported/WorldGenLakes.hpp"
#include "model.hpp"
#include <tuple>
#include <map>
#include <vector>
#include <deque>
#include <algorithm>
#include <gccore.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
extern bool isExiting;
const vec3i face_offsets[] = {
    vec3i{-1, +0, +0},
    vec3i{+1, +0, +0},
    vec3i{+0, -1, +0},
    vec3i{+0, +1, +0},
    vec3i{+0, +0, -1},
    vec3i{+0, +0, +1}};
void *get_aligned_pointer_32(void *ptr)
{
    return (void *)((u64(ptr) + 0x1F) & (~0x1F));
}
int world_seed = 0;
uint32_t world_tick = 0;
ImprovedNoise improved_noise;
mutex_t chunk_mutex;
lwp_t __chunk_generator_thread_handle = (lwp_t)NULL;
bool __chunk_generator_init_done = false;
void *__chunk_generator_init_internal(void *);

std::deque<chunk_t *> chunks;
std::deque<chunk_t *> pending_chunks;

bool has_pending_chunks()
{
    return pending_chunks.size() > 0;
}

bool chunk_sorter(chunk_t *&a, chunk_t *&b)
{
    return (*a < *b);
}

bool chunk_sorter_reverse(chunk_t *&a, chunk_t *&b)
{
    return (*b < *a);
}

std::deque<chunk_t *> &get_chunks()
{
    return chunks;
}

chunk_t *get_chunk_from_pos(const vec3i &pos)
{
    return get_chunk(block_to_chunk_pos(pos));
}

chunk_t *get_chunk(const vec2i &pos)
{
    return get_chunk(pos.x, pos.y);
}

chunk_t *get_chunk(int32_t x, int32_t z)
{
    std::deque<chunk_t *>::iterator it = std::find_if(chunks.begin(), chunks.end(), [x, z](chunk_t *chunk)
                                                      { return chunk && chunk->x == x && chunk->z == z; });
    if (it == chunks.end())
        return nullptr;
    return *it;
}

bool add_chunk(int32_t x, int32_t z)
{
    if (pending_chunks.size() + chunks.size() >= CHUNK_COUNT)
        return false;
    lock_t chunk_lock(chunk_mutex);
    for (chunk_t *&m_chunk : pending_chunks)
    {
        if (m_chunk && m_chunk->x == x && m_chunk->z == z)
        {
            return false;
        }
    }
    for (chunk_t *&m_chunk : chunks)
    {
        if (m_chunk && m_chunk->x == x && m_chunk->z == z)
        {
            return false;
        }
    }

    chunk_t *chunk = new chunk_t;
    if (chunk)
    {
        chunk->x = x;
        chunk->z = z;
        chunk->generation_stage = ChunkGenStage::empty;
        pending_chunks.push_back(chunk);
        std::sort(pending_chunks.begin(), pending_chunks.end(), chunk_sorter);
        return true;
    }

    printf("Failed to allocate memory for chunk\n");

    return false;
}

inline bool in_range(int x, int min, int max)
{
    return x >= min && x <= max;
}

/* The state must be initialized to non-zero */
inline uint32_t fastrand()
{
    /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
    static uint32_t x = 1;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

/* The state must be initialized to non-zero */
inline void treerand(int x, int z, uint32_t *output, uint32_t count)
{
    float v = float(z * (x + 327) + z) + 0.19f;
    uint32_t a;
    memcpy(&a, &v, 4);
    /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
    while (count-- > 0)
    {
        a ^= a << 13;
        a ^= a >> 17;
        a ^= a << 5;
        *(output++) = a;
    }
}

inline void plant_tree(vec3i position, int height, chunk_t *chunk)
{
    block_t *base_block = get_block_at(position - vec3i(0, 1, 0), chunk);
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
                vec3i leaves_pos = position + vec3i(x, y, z);
                replace_air_at(leaves_pos, BlockID::leaves, chunk);
            }
    }
    // Place narrow part of leaves
    for (int x = -1; x <= 1; x++)
    {
        for (int y = height - 1; y <= height; y++)
        {
            for (int z = -1; z <= 1; z++)
            {
                vec3i leaves_off = vec3i(x, y, z);
                // Place the leaves in a "+" pattern at the top.
                if (y == height && (x | z) && std::abs(x) == std::abs(z))
                    continue;
                replace_air_at(position + leaves_off, BlockID::leaves, chunk);
            }
        }
    }
    // Place tree logs
    for (int y = 0; y < height; y++)
    {
        set_block_at(position, BlockID::wood, chunk);
        ++position.y;
    }
}

inline void generate_trees(vec3i pos, chunk_t *chunk, javaport::Random &rng)
{
    static uint32_t tree_positions[WORLDGEN_TREE_ATTEMPTS];
    treerand(pos.x, pos.z, tree_positions, WORLDGEN_TREE_ATTEMPTS);
    for (uint32_t c = 0; c < WORLDGEN_TREE_ATTEMPTS; c++)
    {
        uint32_t tree_value = tree_positions[c];
        vec3i tree_pos((tree_value & 15), 0, ((tree_value >> 24) & 15));

        // Place the trees on above ground.
        tree_pos.y = chunk->terrain_map[tree_pos.x | (tree_pos.z << 4)];
        tree_pos += vec3i(pos.x, 1, pos.z);

        // Trees should only grow on top of the sea level
        if (tree_pos.y >= 64)
        {
            int tree_height = (tree_value >> 28) & 3;
            plant_tree(tree_pos, 3 + tree_height, chunk);
        }
    }
}

void generate_vein(vec3i pos, BlockID id, chunk_t *chunk, javaport::Random &rng)
{
    for (int x = pos.x; x < pos.x + 2; x++)
    {
        for (int y = pos.y; y < pos.y + 2; y++)
        {
            for (int z = pos.z; z < pos.z + 2; z++)
            {
                if (rng.nextInt(2) == 0)
                {
                    vec3i pos(x, y, z);
                    block_t *block = get_block_at(pos, chunk);
                    if (block && block->get_blockid() == BlockID::stone)
                    {
                        block->set_blockid(id);
                    }
                }
            }
        }
    }
}

void generate_ore_type(vec3i neighbor_pos, BlockID id, int count, int max_height, chunk_t *chunk, javaport::Random &rng)
{
    for (int i = 0; i < count; i++)
    {
        int x = rng.nextInt(16);
        int y = rng.nextInt(80);
        int z = rng.nextInt(16);
        vec3i pos(x + neighbor_pos.x, y, z + neighbor_pos.z);
        if (y > max_height)
            continue;
        generate_vein(pos, id, chunk, rng);
        block_t *block = get_block_at(pos, chunk);
        x = rng.nextInt(3) - 1;
        y = rng.nextInt(3) - 1;
        z = rng.nextInt(3) - 1;
        if (block && block->get_blockid() == id && (x | y | z) != 0)
        {
            pos = vec3i(x + pos.x, y + pos.y, z + pos.z);
            generate_vein(pos, id, chunk, rng);
        }
    }
}

void generate_ores(vec3i neighbor_pos, chunk_t *chunk, javaport::Random &rng)
{
    int coal_count = rng.nextInt(8) + 16;
    int iron_count = rng.nextInt(4) + 8;
    int gold_count = rng.nextInt(4) + 8;
    int diamond_count = rng.nextInt(4) + 8;

    generate_ore_type(neighbor_pos, BlockID::coal_ore, coal_count, 80, chunk, rng);
    generate_ore_type(neighbor_pos, BlockID::iron_ore, iron_count, 56, chunk, rng);
    generate_ore_type(neighbor_pos, BlockID::gold_ore, gold_count, 32, chunk, rng);
    generate_ore_type(neighbor_pos, BlockID::diamond_ore, diamond_count, 12, chunk, rng);
}
extern aabb_entity_t *player;
void generate_features(chunk_t *chunk)
{
    javaport::Random rng(chunk->x * 0x4F9939F508L + chunk->z * 0x1F38D3E7L + world_seed);
    vec3i block_pos(chunk->x * 16, 0, chunk->z * 16);
    generate_ores(block_pos, chunk, rng);
    generate_trees(block_pos, chunk, rng);

    if (rng.nextInt(4) == 0)
    {
        javaport::WorldGenLakes lake_gen(BlockID::water);
        vec3i pos(rng.nextInt(16) + block_pos.x, rng.nextInt(128), rng.nextInt(16) + block_pos.z);
        lake_gen.generate(rng, pos);
    }

    if (rng.nextInt(8) == 0)
    {
        javaport::WorldGenLakes lava_lake_gen(BlockID::lava);
        vec3i pos(rng.nextInt(16) + block_pos.x, rng.nextInt(128), rng.nextInt(16) + block_pos.z);
        if (pos.y < 64 || rng.nextInt(10) == 0)
            lava_lake_gen.generate(rng, pos);
    }

    javaport::WorldGenLiquids water_liquid_gen(BlockID::water);
    for (int i = 0; i < 50; i++)
    {
        vec3i pos(rng.nextInt(16) + 8, rng.nextInt(120) + 8, rng.nextInt(16) + 8);
        water_liquid_gen.generate(rng, pos + block_pos);
    }

    javaport::WorldGenLiquids lava_gen(BlockID::lava);
    for (int i = 0; i < 20; i++)
    {
        vec3i pos(rng.nextInt(16) + 8, rng.nextInt(rng.nextInt(rng.nextInt(112) + 8) + 8), rng.nextInt(16) + 8);
        lava_gen.generate(rng, pos + block_pos);
    }
}

void generate_chunk()
{
    if (pending_chunks.size() == 0)
        return;
    chunk_t *chunk = pending_chunks.back();
    if (!chunk || chunk->generation_stage != ChunkGenStage::empty)
        return;
    printf("Start generation of chunk %d, %d\n", chunk->x, chunk->z);
    chunk->generation_stage = ChunkGenStage::heightmap;

    int x_offset = (chunk->x * 16);
    int z_offset = (chunk->z * 16);
    int index = 0;

    // This is used to limit the height of cave noise to minimize
    // the amount of unnecessary calls to the noise generator.
    uint8_t max_height = 0;

    static float terrain_noise[256];
    static float sand_noise[256];

    vec3i noise_block_pos(x_offset, 512, z_offset);
    vec3f noise_pos(x_offset - 1, 384, z_offset - 1);
    noise_pos.x++;
    noise_pos.z++;
    noise_pos.y = 512;
    // The chunk's heightmap is used differently here. In particular,
    // it's used for storing the noise value for later parts of the
    // terrain generation, like the altitude of trees and caves.
    vec3i noise_size(16, 1, 16);
    javaport::Random rng(world_seed);
    int32_t off_x = rng.nextInt(0xFFFFF);
    int32_t off_z = rng.nextInt(0xFFFFF);
    vec3i sand_off(off_x, 0, off_z);

    improved_noise.GetNoiseSet(noise_pos, noise_size, 32, 2, terrain_noise);
    improved_noise.GetNoiseSet(noise_pos + sand_off, noise_size, 24, 1, sand_noise);

    index = 0;
    for (int z = 0; z < 16; z++)
    {
        for (int x = 0; x < 16; x++, index++)
        {
            vfloat_t dist = 0;
            uint8_t height = uint8_t((dist + terrain_noise[index]) * 32) + 48;
            chunk->terrain_map[index] = height;
            max_height = std::max(height, max_height);
            bool generate_sand = sand_noise[index] > 0.5;
            for (int y = std::max(63, int(height)); y >= 0; y--)
            {
                BlockID id = BlockID::air;
                // Coat with grass (or sand if near sea level)
                if (y == height)
                {
                    id = height < 63 ? BlockID::dirt : BlockID::grass;
                    if (generate_sand && in_range(y, 59, 64))
                        id = BlockID::sand;
                    else
                        generate_sand = false;
                }
                // Add some dirt (or sand if near sea level)
                else if (in_range(y, height - 2, height - 1))
                {
                    id = BlockID::dirt;
                    if (generate_sand && in_range(height, 59, 64))
                        id = BlockID::sand;
                }
                // Place water
                else if (height < 63 && in_range(y, height + 1, 63))
                    id = BlockID::water;
                // Place stone or sandstone
                else if (in_range(y, 5, height - 3))
                {
                    id = BlockID::stone;
                    if (generate_sand && in_range(y, height - 4, height - 3))
                        id = BlockID::sandstone;
                }
                // Randomize bedrock
                else if (in_range(y, 1, 4))
                    id = (fastrand() & 1) ? BlockID::bedrock : BlockID::stone;
                // Bottom layer is bedrock
                else if (!y)
                    id = BlockID::bedrock;
                chunk->set_block(vec3i(x, y, z), id);
            }
        }
    }
    chunk->generation_stage = ChunkGenStage::cavegen;

    // Carve caves
    javaport::MapGenCaves cavegen;

    static BlockID carved[16 * 16 * 256];

    block_t *block = chunk->blockstates;
    for (int32_t i = 0; i < 16 * 16 * 256; i++, block++)
    {
        carved[i] = block->get_blockid();
    }
    cavegen.generate(chunk, world_seed, carved);

    block = chunk->blockstates;
    for (int32_t i = 0; i < 16 * 16 * 256; i++, block++)
    {
        block->set_blockid(carved[i]);
    }

    chunk->generation_stage = ChunkGenStage::features;

    usleep(100);

    index = 0;
    for (int z = 0; z < 16; z++)
        for (int x = 0; x < 16; x++, index++)
            chunk->terrain_map[index] = skycast(vec3i(x, chunk->terrain_map[index], z), chunk);

    // If the chunk generator has already been de-initialized
    // during generation, clean up the chunk and return
    if (!__chunk_generator_init_done)
    {
        delete chunk;
        return;
    }

    lock_t chunk_lock(chunk_mutex);
    chunks.push_back(chunk);
    pending_chunks.erase(std::find(pending_chunks.begin(), pending_chunks.end(), chunk));
    chunk_lock.unlock();
    for (int32_t x = chunk->x - 1; x <= chunk->x + 1; x++)
    {
        for (int32_t z = chunk->z - 1; z <= chunk->z + 1; z++)
        {
            // Skip corners
            if ((x - chunk->x) && (z - chunk->z))
                continue;
            // Attempt to generate features for the neighboring chunks
            chunk_t *neighbor = get_chunk(x, z);
            if (neighbor)
            {
                generate_features(neighbor);
                usleep(100);

                // Update the VBOs of the neighboring chunks
                for (int i = 0; i < 16; i++)
                {
                    neighbor->vbos[i].dirty = true;
                }
            }
        }
    }

    chunk->generation_stage = ChunkGenStage::done;
    printf("End generation of chunk %d, %d\n", chunk->x, chunk->z);
}

void print_chunk_status()
{
    printf("Chunks: %d, Pending: %d", chunks.size(), pending_chunks.size());
}

void deinit_chunks()
{
    if (!__chunk_generator_init_done)
        return;
    __chunk_generator_init_done = false;
    pending_chunks.clear();
    LWP_MutexDestroy(chunk_mutex);
}

// create chunk
void init_chunks()
{
    if (__chunk_generator_init_done)
        return;
    __chunk_generator_init_done = true;
    LWP_MutexInit(&chunk_mutex, false);
    LWP_CreateThread(&__chunk_generator_thread_handle, /* thread handle */
                     __chunk_generator_init_internal,  /* code */
                     NULL,                             /* arg pointer for thread */
                     NULL,                             /* stack base */
                     128 * 1024,                       /* stack size */
                     50 /* thread priority */);
}

void *__chunk_generator_init_internal(void *)
{
    improved_noise.Init(world_seed = fastrand());

    while (__chunk_generator_init_done)
    {
        while (pending_chunks.size() == 0)
        {
            usleep(100);
            if (!__chunk_generator_init_done)
                break;
        }
        generate_chunk();
    }
    chunks.clear();
    return NULL;
}

BlockID get_block_id_at(const vec3i &position, BlockID default_id, chunk_t *near)
{
    block_t *block = get_block_at(position, near);
    if (!block)
        return default_id;
    return block->get_blockid();
}
block_t *get_block_at(const vec3i &position, chunk_t *near)
{
    if (position.y < 0 || position.y > 255)
        return nullptr;
    if (near && near->x == (position.x >> 4) && near->z == (position.z >> 4))
        return near->get_block(position);
    chunk_t *chunk = get_chunk_from_pos(position);
    if (!chunk)
        return nullptr;
    return chunk->get_block(position);
}

void set_block_at(const vec3i &pos, BlockID id, chunk_t *near)
{
    block_t *block = get_block_at(pos, near);
    if (block)
        block->set_blockid(id);
}

void replace_air_at(vec3i pos, BlockID id, chunk_t *near)
{
    block_t *block = get_block_at(pos, near);
    if (block && block->get_blockid() == BlockID::air)
        block->set_blockid(id);
}

void chunk_t::update_height_map(vec3i pos)
{
    pos.x &= 15;
    pos.z &= 15;
    uint8_t *height = &height_map[((pos.x << 4) | pos.z)];
    BlockID id = get_block(pos)->get_blockid();
    if (get_block_opacity(id))
    {
        if (pos.y >= *height)
            *height = pos.y + 1;
    }
    else
    {
        while (*height > 0)
        {
            pos.y = *height - 1;
            id = get_block(pos)->get_blockid();
            if (get_block_opacity(id))
                break;
            (*height)--;
        }
    }
}

void update_block_at(const vec3i &pos)
{
    if (pos.y > 255 || pos.y < 0)
        return;
    chunk_t *chunk = get_chunk_from_pos(pos);
    if (!chunk)
        return;
    block_t *block = chunk->get_block(pos);
    blockproperties_t prop = properties(block->id);
    if (prop.m_fluid)
    {
        block->meta |= FLUID_UPDATE_REQUIRED_FLAG;
        chunk->has_fluid_updates[pos.y >> 4] = 1;
    }
    if (prop.m_fall)
    {
        BlockID block_below = get_block_id_at(pos + vec3i(0, -1, 0), BlockID::stone, chunk);
        // Hopefully this prevents crashes caused by fluids colliding with falling blocks far away
        if (chunk->player_taxicab_distance() < SIMULATION_DISTANCE * 16)
        {
            if (block_below == BlockID::air || properties(block_below).m_fluid)
            {
                chunk->entities.push_back(new falling_block_entity_t(*block, pos));
                block->set_blockid(BlockID::air);
                block->meta = 0;
            }
        }
    }
    chunk->update_height_map(pos);
    update_light(pos, chunk);
}

void update_neighbors(const vec3i &pos)
{
    for (int i = 0; i < 6; i++)
    {
        vec3i neighbour = pos + face_offsets[i];
        update_block_at(neighbour);
    }
}

void chunk_t::light_up()
{
    if (!this->lit_state)
    {
        int chunkX = this->x * 16;
        int chunkZ = this->z * 16;
        for (int x = 0; x < 16; x++)
        {
            for (int z = 0; z < 16; z++)
            {
                int end_y = skycast(vec3i(x, 0, z), this);
                if (end_y >= 255 || end_y <= 0)
                    return;
                this->height_map[(x << 4) | z] = end_y + 1;
                for (int y = 255; y > end_y; y--)
                    this->get_block(vec3i(x, y, z))->sky_light = 15;
                update_light(vec3i(chunkX + x, end_y + 1, chunkZ + z), this);

                // Update block lights
                for (int y = 0; y < end_y; y++)
                {
                    if (get_block_luminance(this->get_block(vec3i(x, y, z))->get_blockid()))
                    {
                        update_light(vec3i(chunkX + x, y, chunkZ + z), this);
                    }
                }
            }
        }
        lit_state = 1;
    }
}

void chunk_t::recalculate_visibility(block_t *block, vec3i pos)
{
    block_t *neighbors[6];
    get_neighbors(pos, neighbors, this);
    uint8_t visibility = 0x40;
    if (block->get_blockid() == BlockID::leaves)
    {
        block->visibility_flags = 0x7F;
        return;
    }
    for (int i = 0; i < 6; i++)
    {
        block_t *other_block = neighbors[i];
        if (!other_block || !other_block->get_visibility())
        {
            visibility |= 1 << i;
            continue;
        }

        if (other_block->id != block->id || !is_face_transparent(get_face_texture_index(block, i)))
        {
            RenderType other_rt = properties(other_block->id).m_render_type;
            visibility |= ((other_rt != RenderType::full && other_rt != RenderType::full_special) || is_face_transparent(get_face_texture_index(other_block, i ^ 1))) << i;
        }
    }
    block->visibility_flags = visibility;
}

// recalculates the blockstates of a section
void chunk_t::recalculate_section_visibility(int section)
{
    vec3i chunk_pos(this->x * 16, section * 16, this->z * 16);

    block_t *block = this->get_block(chunk_pos); // Gets the first block of the section
    for (int y = 0; y < 16; y++)
    {
        for (int z = 0; z < 16; z++)
        {
            for (int x = 0; x < 16; x++, block++)
            {
                if (!block->get_visibility())
                    continue;
                vec3i pos = vec3i(x, y, z);
                this->recalculate_visibility(block, chunk_pos + pos);
            }
        }
    }
}

int chunk_t::build_vbo(int section, bool transparent)
{
    chunkvbo_t &vbo = this->vbos[section];
// #define OLD_VBOSYSTEM
#ifdef OLD_VBOSYSTEM
    int quadVertexCount = pre_render_block_mesh(section, transparent);
    int triaVertexCount = pre_render_fluid_mesh(section, transparent);
    if (!quadVertexCount && !triaVertexCount)
    {
        if (transparent)
        {
            vbo.transparent.detach();
        }
        else
        {
            vbo.solid.detach();
        }
        return (0);
    }

    // Calculate potential memory usage
    int estimatedMemory = (triaVertexCount + quadVertexCount) * VERTEX_ATTR_LENGTH;
    if (quadVertexCount)
        estimatedMemory += 3;
    if (triaVertexCount)
        estimatedMemory += 3;
    estimatedMemory += 32;
    estimatedMemory = (estimatedMemory + 31) & ~31;

    void *displist_vbo = memalign(32, estimatedMemory); // 32 bytes for alignment

    if (displist_vbo == nullptr)
    {
        printf("Failed to allocate %d bytes for section %d VBO at (%d, %d)\n", estimatedMemory, section, this->x, this->z);
        return (1);
    }
    DCInvalidateRange(displist_vbo, estimatedMemory);

    GX_BeginDispList(displist_vbo, estimatedMemory);
    if (quadVertexCount)
    {
        int newQuadVertexCount;
        if ((newQuadVertexCount = render_block_mesh(section, transparent, quadVertexCount)) != quadVertexCount)
        {
            printf("Invalid vtx count while rendering blocks: expected %d, got %d", quadVertexCount, newQuadVertexCount);
            isExiting = true;
        }
    }
    if (triaVertexCount)
    {
        int newTriaVertexCount;
        if ((newTriaVertexCount = render_fluid_mesh(section, transparent, triaVertexCount)) != triaVertexCount)
        {
            printf("Invalid vtx count while rendering fluid: expected %d, got %d", triaVertexCount, newTriaVertexCount);
            isExiting = true;
        }
    }
    // GX_EndDispList() returns the size of the display list, so store that value and use it with GX_CallDispList().
    int preciseMemory = GX_EndDispList();
    if (!preciseMemory)
    {
        printf("Failed to create display list for section %d at (%d, %d)\n", section, this->x, this->z);
        printf("Size should be %d\n", estimatedMemory);
        return (2);
    }
    if (preciseMemory != estimatedMemory)
    {
        if (abs(preciseMemory - estimatedMemory) > 64)
        {
            printf("Sizes differ too much: %d != %d, diff=%d\n", preciseMemory, estimatedMemory, preciseMemory - estimatedMemory);
            return (2);
        }
    }

    if (transparent)
    {
        vbo.transparent.buffer = displist_vbo;
        vbo.transparent.length = preciseMemory;
    }
    else
    {
        vbo.solid.buffer = displist_vbo;
        vbo.solid.length = preciseMemory;
    }
#else
    static uint8_t vbo_buffer[64000 * VERTEX_ATTR_LENGTH] __attribute__((aligned(32)));
    DCInvalidateRange(vbo_buffer, sizeof(vbo_buffer));

    GX_BeginDispList(vbo_buffer, sizeof(vbo_buffer));

    // The first byte (GX_Begin command) is skipped, and after that the vertex count is stored as a uint16_t
    uint16_t *quadVtxCountPtr = (uint16_t *)(&vbo_buffer[1]);

    // Render the block mesh
    int quadVtxCount = render_block_mesh(section, transparent, 64000U);

    // 3 bytes for the GX_Begin command, with 1 byte offset to access the vertex count (uint16_t)
    int offset = (quadVtxCount * VERTEX_ATTR_LENGTH) + 1 + 3;

    // The vertex count for the fluid mesh is stored as a uint16_t after the block mesh vertex data
    uint16_t *triVtxCountPtr = (uint16_t *)(&vbo_buffer[offset]);

    int triVtxCount = render_fluid_mesh(section, transparent, 64000U);

    // End the display list - we don't need to store the size, as we can calculate it from the vertex counts
    uint32_t success = GX_EndDispList();

    if (!success)
    {
        printf("Failed to create display list for section %d at (%d, %d)\n", section, this->x, this->z);
        return (2);
    }
    if (transparent)
    {
        vbo.transparent.detach();
    }
    else
    {
        vbo.solid.detach();
    }

    if (!quadVtxCount && !triVtxCount)
    {
        return (0);
    }

    if (quadVtxCount)
    {
        // Store the vertex count for the block mesh
        quadVtxCountPtr[0] = quadVtxCount;
    }

    if (triVtxCount)
    {
        // Store the vertex count for the fluid mesh
        triVtxCountPtr[0] = triVtxCount;
    }
    uint32_t displist_size = (triVtxCount + quadVtxCount) * VERTEX_ATTR_LENGTH;
    if (quadVtxCount)
        displist_size += 3;
    if (triVtxCount)
        displist_size += 3;
    displist_size += 32;

    displist_size = (displist_size + 31) & ~31;

    void *displist_vbo = memalign(32, displist_size);
    if (displist_vbo == nullptr)
    {
        printf("Failed to allocate %d bytes for section %d VBO at (%d, %d)\n", displist_size, section, this->x, this->z);
        return (1);
    }

    uint32_t index = 0;
    // Copy the quad data
    if (quadVtxCount)
    {
        uint32_t quadSize = quadVtxCount * VERTEX_ATTR_LENGTH + 3;
        memcpy(displist_vbo, vbo_buffer, quadSize);
        index += quadSize;
    }

    // Copy the triangle data
    if (triVtxCount)
    {
        uint32_t triSize = triVtxCount * VERTEX_ATTR_LENGTH + 3;
        memcpy((void *)((u32)displist_vbo + index), (void *)((u32)vbo_buffer + (quadVtxCount * VERTEX_ATTR_LENGTH + 3)), triSize);
        index += triSize;
    }

    // Set the rest of the buffer to 0
    memset((void *)((u32)displist_vbo + index), 0, displist_size - index);

    if (transparent)
    {
        vbo.transparent.buffer = displist_vbo;
        vbo.transparent.length = displist_size;
    }
    else
    {
        vbo.solid.buffer = displist_vbo;
        vbo.solid.length = displist_size;
    }
    DCFlushRange(displist_vbo, displist_size);
#endif
    return (0);
}

int chunk_t::pre_render_fluid_mesh(int section, bool transparent)
{
    int vertexCount = 0;
    vec3i chunk_offset = vec3i(this->x * 16, section * 16, this->z * 16);

    GX_BeginGroup(GX_TRIANGLES, 0);
    block_t *block = get_block(chunk_offset);
    for (int _y = 0; _y < 16; _y++)
    {
        for (int _z = 0; _z < 16; _z++)
        {
            for (int _x = 0; _x < 16; _x++, block++)
            {
                vec3i blockpos = vec3i(_x, _y, _z) + chunk_offset;
                if (properties(block->id).m_fluid && transparent == properties(block->id).m_transparent)
                    vertexCount += render_fluid(block, blockpos);
            }
        }
    }
    GX_EndGroup();
    return vertexCount;
}
int chunk_t::render_fluid_mesh(int section, bool transparent, int vertexCount)
{
    vec3i chunk_offset = vec3i(this->x * 16, section * 16, this->z * 16);

    GX_BeginGroup(GX_TRIANGLES, vertexCount);
    vertexCount = 0;
    block_t *block = get_block(chunk_offset);
    for (int _y = 0; _y < 16; _y++)
    {
        for (int _z = 0; _z < 16; _z++)
        {
            for (int _x = 0; _x < 16; _x++, block++)
            {
                vec3i blockpos = vec3i(_x, _y, _z) + chunk_offset;
                if (properties(block->id).m_fluid && transparent == properties(block->id).m_transparent)
                    vertexCount += render_fluid(block, blockpos);
            }
        }
    }
    GX_EndGroup();
    return vertexCount;
}

int chunk_t::pre_render_block_mesh(int section, bool transparent)
{
    int vertexCount = 0;
    vec3i chunk_offset = vec3i(this->x * 16, section * 16, this->z * 16);
    GX_BeginGroup(GX_QUADS, 0);
    // Build the mesh from the blockstates
    block_t *block = get_block(chunk_offset);
    for (int _y = 0; _y < 16; _y++)
    {
        for (int _z = 0; _z < 16; _z++)
        {
            for (int _x = 0; _x < 16; _x++, block++)
            {
                vec3i blockpos = vec3i(_x, _y, _z) + chunk_offset;
                vertexCount += pre_render_block(block, blockpos, transparent);
            }
        }
    }
    GX_EndGroup();
    return vertexCount;
}

int chunk_t::render_block_mesh(int section, bool transparent, int vertexCount)
{
    vec3i chunk_offset = vec3i(this->x * 16, section * 16, this->z * 16);
    GX_BeginGroup(GX_QUADS, vertexCount);
    vertexCount = 0;
    // Build the mesh from the blockstates
    block_t *block = get_block(chunk_offset);
    for (int _y = 0; _y < 16; _y++)
    {
        for (int _z = 0; _z < 16; _z++)
        {
            for (int _x = 0; _x < 16; _x++, block++)
            {
                vec3i blockpos = vec3i(_x, _y, _z) + chunk_offset;
                vertexCount += render_block(block, blockpos, transparent);
            }
        }
    }
    uint16_t vtxCount = GX_EndGroup();
    if (vtxCount != vertexCount)
        printf("VTXCOUNT: %d != %d", vertexCount, vtxCount);
    return vertexCount;
}

inline int DrawHorizontalQuad(vertex_property_t p0, vertex_property_t p1, vertex_property_t p2, vertex_property_t p3, uint8_t light)
{
    vertex_property_t vertices[4] = {p0, p1, p2, p3};
    uint8_t curr = 0;
    // Find the vertex with smallest y position and start there.
    vertex_property_t min_vertex = vertices[0];
    for (int i = 1; i < 4; i++)
    {
        if (vertices[i].pos.y < min_vertex.pos.y)
            min_vertex = vertices[i];
    }
    curr = min_vertex.index;
    for (int i = 0; i < 3; i++)
    {
        GX_VertexLit(vertices[curr], light, 3);
        curr = (curr + 1) & 3;
    }
    curr = (min_vertex.index + 2) & 3;
    for (int i = 0; i < 3; i++)
    {
        GX_VertexLit(vertices[curr], light, 3);
        curr = (curr + 1) & 3;
    }
    return 2;
}

inline int DrawVerticalQuad(vertex_property_t p0, vertex_property_t p1, vertex_property_t p2, vertex_property_t p3, uint8_t light)
{
    vertex_property_t vertices[4] = {p0, p1, p2, p3};
    int faceCount = 0;
    uint8_t curr = 0;
    if (p0.y_uv != p1.y_uv)
    {
        for (int i = 0; i < 3; i++)
        {
            GX_VertexLit(vertices[curr], light, 3);
            curr = (curr + 1) & 3;
        }
        faceCount++;
    }
    curr = 2;
    if (p2.y_uv != p3.y_uv)
    {
        for (int i = 0; i < 3; i++)
        {
            GX_VertexLit(vertices[curr], light, 3);
            curr = (curr + 1) & 3;
        }
        faceCount++;
    }
    return faceCount;
}

void get_neighbors(const vec3i &pos, block_t **neighbors, chunk_t *near)
{

    if (((pos.x + 1) & 15) <= 1 || ((pos.z + 1) & 15) <= 1 || ((pos.y + 1) & 255) <= 1 || !near)
        for (int x = 0; x < 6; x++)
            neighbors[x] = get_block_at(pos + face_offsets[x], near);
    else
        for (int x = 0; x < 6; x++)
            neighbors[x] = near->get_block(pos + face_offsets[x]);
}

int chunk_t::pre_render_block(block_t *block, const vec3i &pos, bool transparent)
{
    if (!block->get_visibility() || properties(block->id).m_fluid || transparent != properties(block->id).m_transparent)
        return 0;
    if (transparent)
    {
        switch (properties(block->id).m_render_type)
        {
        case RenderType::cross:
            return render_cross(block, pos);
        case RenderType::special:
            return render_special(block, pos);
        case RenderType::flat_ground:
            return render_flat_ground(block, pos);
        case RenderType::slab:
            return render_slab(block, pos);
        default:
            break;
        }
    }
    int vertexCount = 0;
    for (uint8_t face = 0; face < 6; face++)
    {
        if (block->get_opacity(face))
            vertexCount += 4;
    }
    return vertexCount;
}

int chunk_t::render_block(block_t *block, const vec3i &pos, bool transparent)
{
    if (!block->get_visibility() || properties(block->id).m_fluid || transparent != properties(block->id).m_transparent)
        return 0;
    if (transparent)
    {
        switch (properties(block->id).m_render_type)
        {
        case RenderType::cross:
            return render_cross(block, pos);
        case RenderType::special:
            return render_special(block, pos);
        case RenderType::flat_ground:
            return render_flat_ground(block, pos);
        case RenderType::slab:
            return render_slab(block, pos);
        default:
            break;
        }
    }
    int vertexCount = 0;
    if (properties(block->id).m_render_type == RenderType::full_special)
    {
        for (uint8_t face = 0; face < 6; face++)
        {
            if (block->get_opacity(face))
                vertexCount += render_face(pos, face, get_face_texture_index(block, face), this, block);
        }
    }
    else
    {
        for (uint8_t face = 0; face < 6; face++)
        {
            if (block->get_opacity(face))
                vertexCount += render_face(pos, face, properties(block->id).m_texture_index, this, block);
        }
    }
    return vertexCount;
}

int chunk_t::render_special(block_t *block, const vec3i &pos)
{
    switch (block->get_blockid())
    {
    case BlockID::torch:
        return render_torch(block, pos);
    default:
        break;
    }
    return 0;
}

int chunk_t::render_flat_ground(block_t *block, const vec3i &pos)
{
    uint8_t lighting = block->light;
    vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_default_texture_index(block->get_blockid());
    GX_VertexLit({vertex_pos + vec3f{0.5, -.4375, -.5}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f{0.5, -.4375, 0.5}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f{-.5, -.4375, 0.5}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f{-.5, -.4375, -.5}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PY);
    return 4;
}

int chunk_t::render_cross(block_t *block, const vec3i &pos)
{
    uint8_t lighting = block->light;
    vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_default_texture_index(block->get_blockid());

    // Front face

    // NX, NZ -> PX, PZ
    GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    // PX, NZ -> NX, PZ
    GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);

    // Back face

    // NX, PZ -> PX, NZ
    GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PZ);

    // PX, PZ -> NX, NZ
    GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NZ);

    return 16;
}

int chunk_t::render_slab(block_t *block, const vec3i &pos)
{
    uint8_t lighting = block->light;
    vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t top_index = get_default_texture_index(block->get_blockid());
    uint32_t side_index = top_index - 1;
    bool render_top = true;
    bool render_bottom = true;
    bool top_half = block->meta & 1;
    int vertexCount = 0;
    if (block->meta & 2)
    {
        for (uint8_t face = 0; face < 6; face++)
        {
            if (block->get_opacity(face))
                vertexCount += render_face(pos, face, get_face_texture_index(block, face), this, block);
        }
        return vertexCount;
    }
    else if (top_half)
    {
        vertex_pos = vertex_pos + vec3f(0, 0.5, 0);
        if (!block->get_opacity(FACE_PY))
            render_top = false;
    }
    else
    {
        if (!block->get_opacity(FACE_NY))
            render_bottom = false;
    }

    if (block->get_opacity(FACE_NX))
    {
        lighting = get_face_light_index(pos, FACE_NX, this, block);
        GX_VertexLit({vertex_pos + vec3f(-.5, -.5, -.5), TEXTURE_NX(side_index), TEXTURE_PY(side_index)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + vec3f(-.5, 0.0, -.5), TEXTURE_NX(side_index), TEXTURE_NY(side_index) + 8}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + vec3f(-.5, 0.0, 0.5), TEXTURE_PX(side_index), TEXTURE_NY(side_index) + 8}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + vec3f(-.5, -.5, 0.5), TEXTURE_PX(side_index), TEXTURE_PY(side_index)}, lighting, FACE_NX);
        vertexCount += 4;
    }

    if (block->get_opacity(FACE_PX))
    {
        lighting = get_face_light_index(pos, FACE_PX, this, block);
        GX_VertexLit({vertex_pos + vec3f(0.5, -.5, 0.5), TEXTURE_NX(side_index), TEXTURE_PY(side_index)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + vec3f(0.5, 0.0, 0.5), TEXTURE_NX(side_index), TEXTURE_NY(side_index) + 8}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + vec3f(0.5, 0.0, -.5), TEXTURE_PX(side_index), TEXTURE_NY(side_index) + 8}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + vec3f(0.5, -.5, -.5), TEXTURE_PX(side_index), TEXTURE_PY(side_index)}, lighting, FACE_PX);
        vertexCount += 4;
    }

    if (render_bottom)
    {
        vec3i offset(0, (!top_half), 0);
        lighting = get_face_light_index(pos + offset, FACE_NY, this, block);
        GX_VertexLit({vertex_pos + vec3f(-.5, -.5, -.5), TEXTURE_NX(top_index), TEXTURE_PY(top_index)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + vec3f(-.5, -.5, 0.5), TEXTURE_NX(top_index), TEXTURE_NY(top_index)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + vec3f(0.5, -.5, 0.5), TEXTURE_PX(top_index), TEXTURE_NY(top_index)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + vec3f(0.5, -.5, -.5), TEXTURE_PX(top_index), TEXTURE_PY(top_index)}, lighting, FACE_NY);
        vertexCount += 4;
    }

    if (render_top)
    {
        vec3i offset(0, -(!top_half), 0);
        lighting = get_face_light_index(pos + offset, FACE_PY, this, block);
        GX_VertexLit({vertex_pos + vec3f(0.5, 0.0, -.5), TEXTURE_NX(top_index), TEXTURE_PY(top_index)}, lighting, FACE_PY);
        GX_VertexLit({vertex_pos + vec3f(0.5, 0.0, 0.5), TEXTURE_NX(top_index), TEXTURE_NY(top_index)}, lighting, FACE_PY);
        GX_VertexLit({vertex_pos + vec3f(-.5, 0.0, 0.5), TEXTURE_PX(top_index), TEXTURE_NY(top_index)}, lighting, FACE_PY);
        GX_VertexLit({vertex_pos + vec3f(-.5, 0.0, -.5), TEXTURE_PX(top_index), TEXTURE_PY(top_index)}, lighting, FACE_PY);
        vertexCount += 4;
    }

    if (block->get_opacity(FACE_NZ))
    {
        lighting = get_face_light_index(pos, FACE_NZ, this, block);
        GX_VertexLit({vertex_pos + vec3f(0.5, -.5, -.5), TEXTURE_NX(side_index), TEXTURE_PY(side_index)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + vec3f(0.5, 0.0, -.5), TEXTURE_NX(side_index), TEXTURE_NY(side_index) + 8}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + vec3f(-.5, 0.0, -.5), TEXTURE_PX(side_index), TEXTURE_NY(side_index) + 8}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + vec3f(-.5, -.5, -.5), TEXTURE_PX(side_index), TEXTURE_PY(side_index)}, lighting, FACE_NZ);
        vertexCount += 4;
    }

    if (block->get_opacity(FACE_PZ))
    {
        lighting = get_face_light_index(pos, FACE_PZ, this, block);
        GX_VertexLit({vertex_pos + vec3f(-.5, -.5, 0.5), TEXTURE_NX(side_index), TEXTURE_PY(side_index)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + vec3f(-.5, 0.0, 0.5), TEXTURE_NX(side_index), TEXTURE_NY(side_index) + 8}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + vec3f(0.5, 0.0, 0.5), TEXTURE_PX(side_index), TEXTURE_NY(side_index) + 8}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + vec3f(0.5, -.5, 0.5), TEXTURE_PX(side_index), TEXTURE_PY(side_index)}, lighting, FACE_PZ);
        vertexCount += 4;
    }
    return vertexCount;
}

int chunk_t::render_torch(block_t *block, const vec3i &pos)
{
    uint8_t lighting = block->light;
    vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_default_texture_index(block->get_blockid());
    // Negative X side
    GX_VertexLit({vertex_pos + vec3f{-.0625f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + vec3f{-.0625f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + vec3f{-.0625f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + vec3f{-.0625f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    // Positive X side
    GX_VertexLit({vertex_pos + vec3f{0.0625f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PX);
    GX_VertexLit({vertex_pos + vec3f{0.0625f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PX);
    GX_VertexLit({vertex_pos + vec3f{0.0625f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PX);
    GX_VertexLit({vertex_pos + vec3f{0.0625f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PX);
    // Negative Z side
    GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.0625f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.0625f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.0625f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.0625f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NZ);
    // Positive Z side
    GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.0625f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.0625f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.0625f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.0625f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PZ);
    // Top side
    GX_VertexLit({vertex_pos + vec3f{0.0625f, 0.125f, -.0625f}, TEXTURE_X(texture_index) + 9 * BASE3D_UV_FRAC_LO, TEXTURE_Y(texture_index) + 8 * BASE3D_UV_FRAC_LO}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f{0.0625f, 0.125f, 0.0625f}, TEXTURE_X(texture_index) + 9 * BASE3D_UV_FRAC_LO, TEXTURE_Y(texture_index) + 6 * BASE3D_UV_FRAC_LO}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f{-.0625f, 0.125f, 0.0625f}, TEXTURE_X(texture_index) + 7 * BASE3D_UV_FRAC_LO, TEXTURE_Y(texture_index) + 6 * BASE3D_UV_FRAC_LO}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f{-.0625f, 0.125f, -.0625f}, TEXTURE_X(texture_index) + 7 * BASE3D_UV_FRAC_LO, TEXTURE_Y(texture_index) + 8 * BASE3D_UV_FRAC_LO}, lighting, FACE_PY);

    return 20;
}

vec3f get_fluid_direction(block_t *block, vec3i pos, chunk_t *chunk)
{

    BlockID block_id = block->get_blockid();

    if (is_still_fluid(block_id))
        return vec3f(0.0, -1.0, 0.0);

    // Used to check block types around the fluid
    block_t *neighbors[6];
    get_neighbors(pos, neighbors);

    vec3f direction = vec3f(0.0, 0.0, 0.0);

    bool direction_set = false;

    for (int i = 0; i < 6; i++)
    {
        if (i == FACE_NY || i == FACE_PY)
            continue;
        if (neighbors[i] && is_same_fluid(neighbors[i]->get_blockid(), block_id))
        {
            direction_set = true;
            if (get_fluid_meta_level(neighbors[i]) < get_fluid_meta_level(block))
                direction = direction - vec3f(face_offsets[i].x, 0, face_offsets[i].z);
            else if (get_fluid_meta_level(neighbors[i]) > get_fluid_meta_level(block))
                direction = direction + vec3f(face_offsets[i].x, 0, face_offsets[i].z);
            else
                direction_set = false;
        }
    }
    if (!direction_set)
        direction.y = -1.0;
    return direction.normalize();
}

int chunk_t::render_fluid(block_t *block, const vec3i &pos)
{
    BlockID block_id = block->get_blockid();

    int faceCount = 0;
    vec3i local_pos = {pos.x & 0xF, pos.y & 0xF, pos.z & 0xF};

    // Used to check block types around the fluid
    block_t *neighbors[6];
    BlockID neighbor_ids[6];

    // Sorted maximum and minimum values of the corners above
    int corner_min[4];
    int corner_max[4];

    // These determine the placement of the quad
    float corner_bottoms[4];
    float corner_tops[4];

    get_neighbors(pos, neighbors, this);
    for (int x = 0; x < 6; x++)
    {
        neighbor_ids[x] = neighbors[x] ? neighbors[x]->get_blockid() : BlockID::air;
    }

    if (!base3d_is_drawing)
    {
        if (!is_same_fluid(block_id, neighbor_ids[FACE_NY]) && (!is_solid(neighbor_ids[FACE_NY]) || properties(neighbor_ids[FACE_NY]).m_transparent))
            faceCount += 2;
        if (!is_same_fluid(block_id, neighbor_ids[FACE_PY]))
            faceCount += 2;
        if (!is_same_fluid(block_id, neighbor_ids[FACE_NZ]) && (!is_solid(neighbor_ids[FACE_NZ]) || properties(neighbor_ids[FACE_NZ]).m_transparent))
            faceCount += 2;
        if (!is_same_fluid(block_id, neighbor_ids[FACE_PZ]) && (!is_solid(neighbor_ids[FACE_PZ]) || properties(neighbor_ids[FACE_PZ]).m_transparent))
            faceCount += 2;
        if (!is_same_fluid(block_id, neighbor_ids[FACE_NX]) && (!is_solid(neighbor_ids[FACE_NX]) || properties(neighbor_ids[FACE_NX]).m_transparent))
            faceCount += 2;
        if (!is_same_fluid(block_id, neighbor_ids[FACE_PX]) && (!is_solid(neighbor_ids[FACE_PX]) || properties(neighbor_ids[FACE_PX]).m_transparent))
            faceCount += 2;
        return faceCount * 3;
    }

    vec3i corner_offsets[4] = {
        {0, 0, 0},
        {0, 0, 1},
        {1, 0, 1},
        {1, 0, 0},
    };
    for (int i = 0; i < 4; i++)
    {
        corner_max[i] = (get_fluid_visual_level(pos + corner_offsets[i], block_id, this) << 1);
        if (!corner_max[i])
            corner_max[i] = 1;
        corner_min[i] = 0;
        corner_tops[i] = float(corner_max[i]) * 0.0625f;
        corner_bottoms[i] = 0.0f;
    }

    uint8_t light = block->light;

    // TEXTURE HANDLING: Check surrounding water levels > this:
    // If surrounded by 1, the water texture is flowing to the opposite direction
    // If surrounded by 2 in I-shaped pattern, the water texture is still
    // If surrounded by 2 diagonally, the water texture is flowing diagonally to the other direction
    // If surrounded by 3, the water texture is flowing to the 1 other direction
    // If surrounded by 4, the water texture is still

    uint8_t texture_offset = block_properties[uint8_t(flowfluid(block_id))].m_texture_index;

    uint32_t texture_start_x = TEXTURE_X(texture_offset);
    uint32_t texture_start_y = TEXTURE_Y(texture_offset);
    uint32_t texture_end_x = texture_start_x + UV_SCALE;
    uint32_t texture_end_y = texture_start_y + UV_SCALE;

    vertex_property_t bottomPlaneCoords[4] = {
        {(local_pos + vec3f{-.5f, -.5f, -.5f}), texture_start_x, texture_end_y},
        {(local_pos + vec3f{-.5f, -.5f, +.5f}), texture_start_x, texture_start_y},
        {(local_pos + vec3f{+.5f, -.5f, +.5f}), texture_end_x, texture_start_y},
        {(local_pos + vec3f{+.5f, -.5f, -.5f}), texture_end_x, texture_end_y},
    };
    if (!is_same_fluid(block_id, neighbor_ids[FACE_NY]) && (!is_solid(neighbor_ids[FACE_NY]) || properties(neighbor_ids[FACE_NY]).m_transparent))
        faceCount += DrawHorizontalQuad(bottomPlaneCoords[0], bottomPlaneCoords[1], bottomPlaneCoords[2], bottomPlaneCoords[3], neighbors[FACE_NY] ? neighbors[FACE_NY]->light : light);

    vec3f direction = get_fluid_direction(block, pos, this);
    float angle = -1000;
    float cos_angle = 8;
    float sin_angle = 0;

    if (direction.x != 0 || direction.z != 0)
    {
        angle = std::atan2(direction.x, direction.z) + M_PI;
        cos_angle = std::cos(angle) * 8;
        sin_angle = std::sin(angle) * 8;
    }

    uint32_t tex_off_x = TEXTURE_X(texture_offset) + 16;
    uint32_t tex_off_y = TEXTURE_Y(texture_offset) + 16;
    if (angle < -999)
    {
        int basefluid_offset = texture_offset = block_properties[uint8_t(basefluid(block_id))].m_texture_index;
        tex_off_x = TEXTURE_X(basefluid_offset) + 8;
        tex_off_y = TEXTURE_Y(basefluid_offset) + 8;
    }

    vertex_property_t topPlaneCoords[4] = {
        {(local_pos + vec3f{+.5f, -.5f + corner_tops[3], -.5f}), uint32_t(tex_off_x - cos_angle - sin_angle), uint32_t(tex_off_y - cos_angle + sin_angle)},
        {(local_pos + vec3f{+.5f, -.5f + corner_tops[2], +.5f}), uint32_t(tex_off_x - cos_angle + sin_angle), uint32_t(tex_off_y + cos_angle + sin_angle)},
        {(local_pos + vec3f{-.5f, -.5f + corner_tops[1], +.5f}), uint32_t(tex_off_x + cos_angle + sin_angle), uint32_t(tex_off_y + cos_angle - sin_angle)},
        {(local_pos + vec3f{-.5f, -.5f + corner_tops[0], -.5f}), uint32_t(tex_off_x + cos_angle - sin_angle), uint32_t(tex_off_y - cos_angle - sin_angle)},
    };
    if (!is_same_fluid(block_id, neighbor_ids[FACE_PY]))
        faceCount += DrawHorizontalQuad(topPlaneCoords[0], topPlaneCoords[1], topPlaneCoords[2], topPlaneCoords[3], is_solid(neighbor_ids[FACE_PY]) ? light : neighbors[FACE_PY]->light);

    texture_offset = get_default_texture_index(flowfluid(block_id));

    vertex_property_t sideCoords[4] = {0};

    texture_start_x = TEXTURE_X(texture_offset);
    texture_end_x = texture_start_x + UV_SCALE;
    texture_start_y = TEXTURE_Y(texture_offset);

    sideCoords[0].x_uv = sideCoords[1].x_uv = texture_start_x;
    sideCoords[2].x_uv = sideCoords[3].x_uv = texture_end_x;
    sideCoords[3].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[0], -.5f};
    sideCoords[2].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[0], -.5f};
    sideCoords[1].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[3], -.5f};
    sideCoords[0].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[3], -.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[0];
    sideCoords[2].y_uv = texture_start_y + corner_max[0];
    sideCoords[1].y_uv = texture_start_y + corner_max[3];
    sideCoords[0].y_uv = texture_start_y + corner_min[3];
    if (!is_same_fluid(block_id, neighbor_ids[FACE_NZ]) && (!is_solid(neighbor_ids[FACE_NZ]) || properties(neighbor_ids[FACE_NZ]).m_transparent))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_NZ]->light);

    sideCoords[3].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[2], +.5f};
    sideCoords[2].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[2], +.5f};
    sideCoords[1].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[1], +.5f};
    sideCoords[0].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[1], +.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[2];
    sideCoords[2].y_uv = texture_start_y + corner_max[2];
    sideCoords[1].y_uv = texture_start_y + corner_max[1];
    sideCoords[0].y_uv = texture_start_y + corner_min[1];
    if (!is_same_fluid(block_id, neighbor_ids[FACE_PZ]) && (!is_solid(neighbor_ids[FACE_PZ]) || properties(neighbor_ids[FACE_PZ]).m_transparent))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_PZ]->light);

    sideCoords[3].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[1], +.5f};
    sideCoords[2].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[1], +.5f};
    sideCoords[1].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[0], -.5f};
    sideCoords[0].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[0], -.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[1];
    sideCoords[2].y_uv = texture_start_y + corner_max[1];
    sideCoords[1].y_uv = texture_start_y + corner_max[0];
    sideCoords[0].y_uv = texture_start_y + corner_min[0];
    if (!is_same_fluid(block_id, neighbor_ids[FACE_NX]) && (!is_solid(neighbor_ids[FACE_NX]) || properties(neighbor_ids[FACE_NX]).m_transparent))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_NX]->light);

    sideCoords[3].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[3], -.5f};
    sideCoords[2].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[3], -.5f};
    sideCoords[1].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[2], +.5f};
    sideCoords[0].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[2], +.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[3];
    sideCoords[2].y_uv = texture_start_y + corner_max[3];
    sideCoords[1].y_uv = texture_start_y + corner_max[2];
    sideCoords[0].y_uv = texture_start_y + corner_min[2];
    if (!is_same_fluid(block_id, neighbor_ids[FACE_PX]) && (!is_solid(neighbor_ids[FACE_PX]) || properties(neighbor_ids[FACE_PX]).m_transparent))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_PX]->light);
    return faceCount * 3;
}

void chunk_t::update_entities()
{
    // Update entities
    for (aabb_entity_t *&entity : entities)
    {
        // Workaround to prevent duplicate updates for entities that moved to a different chunk
        if (entity->last_world_tick == world_tick)
            continue;

        // Update the entities' positions
        entity->prev_position = entity->position;

        // Tick the entity
        entity->tick();

        entity->last_world_tick = world_tick;
    }

    // Resolve collisions with current chunk and neighboring chunks' entities
    for (int i = 0; i <= 6; i++)
    {
        if (i == FACE_NY || i == FACE_PY)
            continue;
        chunk_t *neighbor = (i == 6 ? this : get_chunk(this->x + face_offsets[i].x, this->z + face_offsets[i].z));
        if (neighbor)
        {
            for (aabb_entity_t *&entity : neighbor->entities)
            {
                for (aabb_entity_t *&this_entity : this->entities)
                {
                    // Prevent entities from colliding with themselves
                    if (entity != this_entity && entity->collides(this_entity))
                    {
                        // Resolve the collision - always resolve from the perspective of the non-player entity
                        if (this_entity == player)
                            entity->resolve_collision(this_entity);
                        else
                            this_entity->resolve_collision(entity);
                    }
                }
            }
        }
    }

    // Get a list of entities that are out of bounds while iterating
    std::vector<aabb_entity_t *> out_of_bounds;
    for (aabb_entity_t *&entity : entities)
    {
        vec3i entity_pos = vec3i(int(std::floor(entity->position.x)), int(std::floor(entity->position.y)), int(std::floor(entity->position.z)));
        if (entity->can_remove() || entity_pos.x < this->x * 16 || entity_pos.x >= (this->x + 1) * 16 || entity_pos.z < this->z * 16 || entity_pos.z >= (this->z + 1) * 16)
        {
            out_of_bounds.push_back(entity);
        }
    }

    // Remove out of bounds entities from this chunk, we'll handle them later
    for (aabb_entity_t *&entity : out_of_bounds)
    {
        entities.erase(std::remove(entities.begin(), entities.end(), entity), entities.end());
    }

    // Place out of bounds entities in the correct chunk
    for (aabb_entity_t *&entity : out_of_bounds)
    {
        vec3i entity_pos = vec3i(int(entity->position.x), int(entity->position.y), int(entity->position.z));
        chunk_t *new_chunk = get_chunk_from_pos(entity_pos);

        // Check if the chunk is loaded
        if (!entity->can_remove() && new_chunk)
        {
            entity->chunk = new_chunk;
            new_chunk->entities.push_back(entity);
        }
        else
        {
            // If the chunk is not loaded or entity is dead, delete the entity
            delete entity;
        }
    }
}

void chunk_t::render_entities(float partial_ticks, bool transparency)
{
    for (aabb_entity_t *&entity : entities)
    {
        entity->render(partial_ticks, transparency);
    }

    // Restore default texture
    use_texture(blockmap_texture);

    // Restore default colors
    GX_SetTevKColor(GX_KCOLOR0, (GXColor){0, 0, 0, 255});
    GX_SetTevKColor(GX_KCOLOR1, (GXColor){255, 255, 255, 255});
}

uint32_t chunk_t::size()
{
    uint32_t base_size = sizeof(chunk_t);
    for (int i = 0; i < VERTICAL_SECTION_COUNT; i++)
        base_size += this->vbos[i].cached_solid.length + this->vbos[i].cached_transparent.length + sizeof(chunkvbo_t);
    for (aabb_entity_t *&entity : entities)
        base_size += entity->size();
    return base_size;
}