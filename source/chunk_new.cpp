#include "cpugx.hpp"
#include "chunk_new.hpp"
#include "block.hpp"
#include "texturedefs.h"
#include "vec3i.hpp"
#include "blocks.hpp"
#include "timers.hpp"
#include "light.hpp"
#include "fastnoise/fastnoiselite.h"
#include "improvednoise/improvednoise.hpp"
#include "base3d.hpp"
#include "render.hpp"
#include "raycast.hpp"
#include "threadhandler.hpp"
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
extern guVector player_pos;
int heightmap_seed = 0, cavegen_seed = 0;
FastNoiseLite noise;
FastNoiseLite cave_noise;
ImprovedNoise improved_noise;
mutex_t __chunks_mutex;
lwp_t __chunk_generator_thread_handle = (lwp_t)NULL;
bool __chunk_generator_init_done = false;
void *__chunk_generator_init_internal(void *);

bool lock_chunks()
{
    return LWP_MutexLock(__chunks_mutex);
}
bool unlock_chunks()
{
    return LWP_MutexUnlock(__chunks_mutex);
}

std::deque<chunk_t *> chunks;
std::deque<chunk_t *> pending_chunks;

bool has_pending_chunks()
{
    return pending_chunks.size() > 0;
}

std::deque<chunk_t *> &get_chunks()
{
    return chunks;
}

vec3i block_to_chunk_pos(vec3i pos)
{
    pos.x -= 15 * (pos.x < 0);
    pos.z -= 15 * (pos.z < 0);
    pos.x /= 16;
    pos.z /= 16;
    return pos;
}

chunk_t *get_chunk_from_pos(vec3i pos, bool load, bool write_cache)
{
    return get_chunk(block_to_chunk_pos(pos), load, write_cache);
}

std::map<lwp_t, chunk_t *> chunk_cache;
chunk_t *get_chunk(vec3i pos, bool load, bool write_cache)
{
    chunk_t *retval = nullptr;
    // Chunk lookup based on x and z position
    for (chunk_t *&chunk : chunks)
    {
        if (chunk)
            if (chunk->x == pos.x && chunk->z == pos.z)
            {
                retval = chunk;
                break;
            }
    }
    if (!retval && load)
    {
        if (chunks.size() < CHUNK_COUNT)
            add_chunk(pos);
    }

    return retval;
}

void add_chunk(vec3i pos)
{
    if (pending_chunks.size() >= RENDER_DISTANCE)
        return;

    while (lock_chunks())
        threadqueue_yield();
    for (chunk_t *&m_chunk : pending_chunks)
    {
        if (m_chunk && m_chunk->x == pos.x && m_chunk->z == pos.z)
        {
            unlock_chunks();
            return;
        }
    }

    for (chunk_t *&m_chunk : chunks)
    {
        if (m_chunk && m_chunk->x == pos.x && m_chunk->z == pos.z)
        {
            unlock_chunks();
            return;
        }
    }
    chunk_t *chunk = new chunk_t;
    if (chunk)
    {
        chunk->x = pos.x;
        chunk->z = pos.z;
        pending_chunks.push_back(chunk);
    }
    unlock_chunks();
}

inline bool in_range(int x, int min, int max)
{
    return std::clamp(x, min, max) == x;
}

int fastrand_a = 1;
/* The state must be initialized to non-zero */
uint32_t fastrand()
{
    /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
    uint32_t x = fastrand_a;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return fastrand_a = x;
}

/* The state must be initialized to non-zero */
void treerand(int x, int z, uint32_t *output, uint32_t count)
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

void plant_tree(vec3i position, chunk_t *chunk, int height)
{
    block_t *base_block = chunk->get_block(position - vec3i(0, 1, 0));
    if (base_block->get_blockid() != BlockID::grass)
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
                chunk->replace_air(leaves_pos, BlockID::leaves);
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
                chunk->replace_air(position + leaves_off, BlockID::leaves);
            }
        }
    }
    // Place tree logs
    for (int y = 0; y < height; y++)
    {
        chunk->set_block(position, BlockID::wood);
        ++position.y;
    }
}

void generate_trees(chunk_t *chunk)
{
    static uint32_t tree_positions[WORLDGEN_TREE_ATTEMPTS];
    treerand(chunk->x, chunk->z, tree_positions, WORLDGEN_TREE_ATTEMPTS);
    for (uint32_t c = 0; c < WORLDGEN_TREE_ATTEMPTS; c++)
    {
        uint32_t tree_value = tree_positions[c];
        vec3i tree_pos((tree_value & 15), 0, ((tree_value >> 24) & 15));

        // Dirty fix: Just don't place trees at chunk borders
        if (tree_pos.x < 2 || tree_pos.x > 13 || tree_pos.z < 2 || tree_pos.z > 13)
            continue;

        // Place the trees on above ground.
        tree_pos.y = chunk->height_map[tree_pos.x | (tree_pos.z << 4)] + 1;

        // Trees should only grow on top of the sea level
        if (tree_pos.y >= 64)
        {
            int tree_height = (tree_value >> 28) & 3;
            plant_tree(tree_pos, chunk, 3 + tree_height);
        }
    }
}

void generate_chunk()
{
    while (pending_chunks.size() == 0)
    {
        threadqueue_sleep();
        if (!__chunk_generator_init_done)
            return;
    }
    chunk_t *chunk = pending_chunks.back();
    if (!chunk)
        return;
    int x_offset = (chunk->x * 16);
    int z_offset = (chunk->z * 16);
    int index = 0;

    for (int z = 0; z < 16; z++)
    {
        for (int x = 0; x < 16; x++)
        {
            // The chunk's heightmap is used differently here. In particular,
            // it's used for storing the noise value for later parts of the
            // terrain generation, like the altitude of trees and caves.
            chunk->height_map[index++] = uint8_t(noise.GetNoise(float(x + x_offset), float(z + z_offset)) * 16.0f + 64.0f);
        }
    }
    index = 0;
    for (int z = 0; z < 16; z++)
    {
        for (int x = 0; x < 16; x++)
        {
            uint8_t value = chunk->height_map[index++];
            for (int y = 0; y < 64 || y <= value; y++)
            {
                BlockID id = BlockID::air;
                // Coat with grass
                if (y == value)
                    id = BlockID::grass;
                // Add some dirt
                else if (in_range(y, value - 2, value - 1))
                    id = BlockID::dirt;
                // Place water
                else if (value < 63 && in_range(y, value + 1, 63))
                    id = BlockID::water;
                // Place stone
                else if (in_range(y, 5, value - 3))
                    id = BlockID::stone;
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
    // Carve caves
    index = 0;
    for (int z = 0; z < 16; z++)
    {
        for (int x = 0; x < 16; x++)
        {

            while (lock_chunks())
                threadqueue_yield();
            uint8_t height = chunk->height_map[index++];
            for (int y = height; y > 0; y--)
            {
                block_t *block = chunk->get_block(vec3i(x, y, z));
                if (block->get_blockid() != BlockID::stone)
                    continue;
                float noise_value = (cave_noise.GetNoise(float(x + x_offset), float(y), float(z + z_offset)));
                if (noise_value < -.5f && (height > 63 || abs(height - y) > 2))
                    block->set_blockid(y >= 10 ? BlockID::air : BlockID::lava);
            }
            unlock_chunks();
        }
    }
    generate_trees(chunk);

    // Reset chunk heightmap for further use.
    memset(chunk->height_map, 0, 256);

    // If the chunk generator has already been de-initialized
    // during generation, clean up the chunk and return
    if (!__chunk_generator_init_done)
    {
        delete chunk;
        return;
    }
    chunk->valid = true;
    while (lock_chunks())
        threadqueue_yield();
    chunks.push_back(chunk);
    pending_chunks.erase(
        std::remove_if(pending_chunks.begin(), pending_chunks.end(),
                       [](chunk_t *&c)
                       { return !c || c->valid; }),
        pending_chunks.end());
    unlock_chunks();
    chunk_t *neighbor_chunks[4];
    neighbor_chunks[0] = get_chunk(vec3i(chunk->x + 1, 0, chunk->z), false, false);
    neighbor_chunks[1] = get_chunk(vec3i(chunk->x - 1, 0, chunk->z), false, false);
    neighbor_chunks[2] = get_chunk(vec3i(chunk->x, 0, chunk->z + 1), false, false);
    neighbor_chunks[3] = get_chunk(vec3i(chunk->x, 0, chunk->z - 1), false, false);
    chunk_t **c = neighbor_chunks;
    for (int i = 0; i < 4; i++, c++)
        for (int vbo_y = 0; vbo_y < 16; vbo_y++)
            if (*c)
                (*c)->vbos[vbo_y].dirty = true;
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
    LWP_MutexDestroy(__chunks_mutex);
}

// create chunk
void init_chunks()
{
    if (__chunk_generator_init_done)
        return;
    __chunk_generator_init_done = true;
    LWP_MutexInit(&__chunks_mutex, false);
    LWP_CreateThread(&__chunk_generator_thread_handle, /* thread handle */
                     __chunk_generator_init_internal,  /* code */
                     NULL,                             /* arg pointer for thread */
                     NULL,                             /* stack base */
                     128 * 1024,                       /* stack size */
                     50 /* thread priority */);
}

void *__chunk_generator_init_internal(void *)
{
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFrequency(1 / 64.0f);
    noise.SetSeed(heightmap_seed = fastrand());
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(2);

    cave_noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    cave_noise.SetFrequency(1 / 12.0f);
    cave_noise.SetSeed(cavegen_seed = fastrand());
    cave_noise.SetFractalType(FastNoiseLite::FractalType_None);
    cave_noise.SetFractalOctaves(1);
    while (__chunk_generator_init_done)
    {
        generate_chunk();
        threadqueue_sleep();
    }
    chunks.clear();
    return NULL;
}

BlockID get_block_id_at(vec3i position, BlockID default_id)
{
    block_t *block = get_block_at(position);
    if (!block)
        return default_id;
    return block->get_blockid();
}
block_t *get_block_at(vec3i position)
{
    if (position.y < 0 || position.y > 255)
        return nullptr;
    chunk_t *chunk = get_chunk_from_pos(position, false, false);
    if (!chunk)
        return nullptr;
    position.x &= 0x0F;
    position.y &= 0xFF;
    position.z &= 0x0F;
    return &chunk->blockstates[position.x | (position.y << 4) | (position.z << 12)];
}

void chunk_t::recalculate()
{
    for (int i = 0; i < VERTICAL_SECTION_COUNT; i++)
    {
        this->recalculate_section(i);
    }
}

void chunk_t::update_height_map(vec3i pos)
{
    pos.x &= 15;
    pos.z &= 15;
    uint8_t *height = height_map + ((pos.x << 4) + pos.z);
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

void update_block_at(vec3i pos)
{
    if (pos.y > 255 || pos.y < 0)
        return;
    chunk_t *chunk = get_chunk_from_pos(pos, false, false);
    if (!chunk)
        return;
    block_t *block = chunk->get_block(pos);
    if (!block)
        return;
    chunk->update_height_map(pos);
    update_light(pos);
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
                update_light(vec3i(chunkX + x, end_y, chunkZ + z));

                // Update block lights
                for (int y = 0; y < end_y; y++)
                {
                    if (get_block_luminance(this->get_block(vec3i(x, y, z))->get_blockid()))
                    {
                        update_light(vec3i(chunkX + x, y, chunkZ + z));
                    }
                }
            }
        }
        lit_state = 1;
    }
}

// recalculates the blockstates of a section
void chunk_t::recalculate_section(int section)
{

    int chunkX = this->x * 16;
    int chunkZ = this->z * 16;
    int chunkY = section * 16;

    vec3i chunk_pos = {chunkX, chunkY, chunkZ};
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            for (int z = 0; z < 16; z++)
            {

                block_t *block = this->get_block(vec3i(x, y + chunkY, z));
                BlockID block_id = block->get_blockid();
                vec3i local_pos = {x, y, z};
                block->visibility_flags = 0x40 * (block_id != BlockID::air);
                if (block->get_visibility())
                {
                    for (int i = 0; i < 6; i++)
                    {
                        vec3i other = chunk_pos + local_pos + face_offsets[i];
                        block_t *other_block = get_block_at(other);
                        BlockID other_id = !other_block ? BlockID::air : other_block->get_blockid();
                        bool face_transparent = is_face_transparent(get_face_texture_index(block, i));

                        bool other_face_transparent = false;
                        bool other_visibility = false;
                        if (other_block)
                        {
                            other_face_transparent = is_fluid(other_id) || is_face_transparent(get_face_texture_index(other_block, i ^ 1));
                            other_visibility = other_block->get_visibility();
                        }
                        else if (other.y >= 0 && other.y <= 255)
                        {
                            continue;
                        }

                        if ((!face_transparent && other_face_transparent) || !other_visibility)
                        {
                            block->set_opacity(i, true);
                        }
                        else if (face_transparent && other_block && other_id != block_id)
                        {
                            block->set_opacity(i, true);
                        }
                    }
                }
            }
        }
    }
}

void chunk_t::destroy_vbo(int section, unsigned char which)
{
    if (!which)
        return;
    if ((which & VBO_SOLID))
    {
        if (this->vbos[section].solid_buffer)
            free(this->vbos[section].solid_buffer);
        this->vbos[section].solid_buffer = nullptr;
        this->vbos[section].solid_buffer_length = 0;
    }
    if ((which & VBO_TRANSPARENT))
    {
        if (this->vbos[section].transparent_buffer)
            free(this->vbos[section].transparent_buffer);
        this->vbos[section].transparent_buffer = nullptr;
        this->vbos[section].transparent_buffer_length = 0;
    }
}

void chunk_t::build_all_vbos()
{
    for (int i = 0; i < VERTICAL_SECTION_COUNT; i++)
    {
        this->destroy_vbo(i, VBO_ALL);
        if (this->build_vbo(i, false) || this->build_vbo(i, true))
            printf("BuildVBO failed.\n");
    }
}
void chunk_t::rebuild_vbo(int section, bool transparent)
{
    destroy_vbo(section, transparent ? VBO_TRANSPARENT : VBO_SOLID);
    if (this->build_vbo(section, transparent))
        printf("BuildVBO failed.\n");
}
int chunk_t::build_vbo(int section, bool transparent)
{
    int chunkX = this->x * 16;
    int chunkY = section * 16;
    int chunkZ = this->z * 16;
    this->vbos[section].x = chunkX;
    this->vbos[section].y = chunkY;
    this->vbos[section].z = chunkZ;
    int quadVertexCount = pre_render_block_mesh(section, transparent);
    int triaVertexCount = pre_render_fluid_mesh(section, transparent);
    if (!quadVertexCount && !triaVertexCount)
    {
        if (transparent)
        {
            this->vbos[section].transparent_buffer = nullptr;
            this->vbos[section].transparent_buffer_length = 0;
        }
        else
        {
            this->vbos[section].solid_buffer = nullptr;
            this->vbos[section].solid_buffer_length = 0;
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
    DCInvalidateRange(ALIGNPTR(displist_vbo), estimatedMemory);
    // memset(ALIGNPTR(displist_vbo), 0, estimatedMemory);

    GX_BeginDispList(ALIGNPTR(displist_vbo), estimatedMemory);
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
        this->vbos[section].transparent_buffer = displist_vbo;
        this->vbos[section].transparent_buffer_length = preciseMemory;
    }
    else
    {
        this->vbos[section].solid_buffer = displist_vbo;
        this->vbos[section].solid_buffer_length = preciseMemory;
    }
    return (0);
}

int chunk_t::pre_render_fluid_mesh(int section, bool transparent)
{
    int vertexCount = 0;
    vec3i chunk_offset = vec3i{this->x * 16, section * 16, this->z * 16};

    for (int _x = 0; _x < 16; _x++)
    {
        for (int _z = 0; _z < 16; _z++)
        {
            for (int _y = 0; _y < 16; _y++)
            {
                vec3i blockpos = vec3i(_x, _y, _z) + chunk_offset;
                block_t *block = get_block(blockpos);
                if (block)
                {
                    BlockID block_id = block->get_blockid();
                    if (is_fluid(block_id) && transparent == (basefluid(block_id) == BlockID::water))
                        vertexCount += render_fluid(block, blockpos);
                }
            }
        }
    }
    return vertexCount;
}
int chunk_t::render_fluid_mesh(int section, bool transparent, int vertexCount)
{
    vec3i chunk_offset = vec3i(this->x * 16, section * 16, this->z * 16);

    GX_BeginGroup(GX_TRIANGLES, vertexCount);
    vertexCount = 0;
    for (int _x = 0; _x < 16; _x++)
    {
        for (int _z = 0; _z < 16; _z++)
        {
            for (int _y = 0; _y < 16; _y++)
            {
                vec3i blockpos = vec3i(_x, _y, _z) + chunk_offset;
                block_t *block = get_block(blockpos);
                if (block)
                {
                    BlockID block_id = block->get_blockid();
                    if (is_fluid(block_id) && transparent == (basefluid(block_id) == BlockID::water))
                        vertexCount += render_fluid(block, blockpos);
                }
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
    for (int _x = 0; _x < 16; _x++)
    {
        for (int _y = 0; _y < 16; _y++)
        {
            for (int _z = 0; _z < 16; _z++)
            {
                vec3i blockpos = vec3i(_x, _y, _z) + chunk_offset;
                block_t *block = this->get_block(blockpos);
                vertexCount += render_block(block, blockpos, transparent);
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
    for (int _x = 0; _x < 16; _x++)
    {
        for (int _y = 0; _y < 16; _y++)
        {
            for (int _z = 0; _z < 16; _z++)
            {
                vec3i blockpos = vec3i(_x, _y, _z) + chunk_offset;
                block_t *block = this->get_block(blockpos);
                vertexCount += render_block(block, blockpos, transparent);
            }
        }
    }
    uint16_t vtxCount = GX_EndGroup();
    if (vtxCount != vertexCount)
        printf("VTXCOUNT: %d != %d", vertexCount, vtxCount);
    return vertexCount;
}

bool CompareVertices(const vertex_property_t &a, const vertex_property_t &b)
{
    return a.pos.y < b.pos.y;
}

int DrawHorizontalQuad(vertex_property_t p0, vertex_property_t p1, vertex_property_t p2, vertex_property_t p3, uint8_t light)
{
    static std::vector<vertex_property_t> vertices(4);

    (vertices[0] = p0).index = 0;
    (vertices[1] = p1).index = 1;
    (vertices[2] = p2).index = 2;
    (vertices[3] = p3).index = 3;
    vertex_property_t min_vertex = *std::min_element(vertices.begin(), vertices.end(), CompareVertices);
    uint8_t curr = min_vertex.index;
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

int DrawVerticalQuad(vertex_property_t p0, vertex_property_t p1, vertex_property_t p2, vertex_property_t p3, uint8_t light)
{
    static std::vector<vertex_property_t> vertices(4);
    vertices[0] = p0;
    vertices[1] = p1;
    vertices[2] = p2;
    vertices[3] = p3;
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

void get_neighbors(vec3i pos, block_t **neighbors)
{
    for (int x = 0; x < 6; x++)
        neighbors[x] = get_block_at(pos + face_offsets[x]);
}

int chunk_t::render_block(block_t *block, vec3i pos, bool transparent)
{
    if (!block->get_visibility() || is_fluid(block->get_blockid()))
        return 0;
    if (transparent)
    {
        switch (block->get_blockid())
        {
        case BlockID::torch:
            return render_torch(block, pos);

        default:
            break;
        }
    }
    vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = 0;
    int vertexCount = 0;
    for (uint8_t face = 0; face < 6; face++)
    {
        if (block->get_opacity(face) && transparent == is_face_transparent(texture_index = get_face_texture_index(block, face)))
            vertexCount += render_face(pos, face, texture_index);
    }
    return vertexCount;
}
int chunk_t::render_torch(block_t *block, vec3i pos)
{
    int lighting = get_block_luminance(BlockID::torch);
    vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_default_texture_index(block->get_blockid());
    // Positive X side
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
    GX_VertexLit({vertex_pos + vec3f{0.0625f, 0.125f, -.0625f}, TEXTURE_X(texture_index) + 9, TEXTURE_Y(texture_index) + 8}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f{0.0625f, 0.125f, 0.0625f}, TEXTURE_X(texture_index) + 9, TEXTURE_Y(texture_index) + 6}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f{-.0625f, 0.125f, 0.0625f}, TEXTURE_X(texture_index) + 7, TEXTURE_Y(texture_index) + 6}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f{-.0625f, 0.125f, -.0625f}, TEXTURE_X(texture_index) + 7, TEXTURE_Y(texture_index) + 8}, lighting, FACE_PY);

    return 20;
}

int chunk_t::render_fluid(block_t *block, vec3i pos)
{
    BlockID block_id = block->get_blockid();

    int faceCount = 0;
    vec3i local_pos = {pos.x & 0xF, pos.y & 0xF, pos.z & 0xF};

    // Used to check block types around the fluid
    block_t *neighbors[6];
    BlockID neighbor_ids[6];

    // Sorted maximum and minimum values of the corners above
    uint8_t corner_min[4];
    uint8_t corner_max[4];

    // These determine the placement of the quad
    float corner_bottoms[4];
    float corner_tops[4];

    get_neighbors(pos, neighbors);
    for (int x = 0; x < 6; x++)
    {
        neighbor_ids[x] = neighbors[x] ? neighbors[x]->get_blockid() : BlockID::air;
    }

    vec3i corner_offsets[4] = {
        {0, 0, 0},
        {0, 0, 1},
        {1, 0, 1},
        {1, 0, 0},
    };
    for (int i = 0; i < 4; i++)
    {
        corner_max[i] = get_fluid_visual_level(pos + corner_offsets[i], block_id);
        corner_min[i] = 0;
        corner_tops[i] = float(corner_max[i]) * 0.125f;
        corner_bottoms[i] = 0.0f;
    }

    uint8_t light = block->light;

    // TEXTURE HANDLING: Check surrounding water levels > this:
    // If surrounded by 1, the water texture is flowing to the opposite direction
    // If surrounded by 2 in I-shaped pattern, the water texture is still
    // If surrounded by 2 diagonally, the water texture is flowing diagonally to the other direction
    // If surrounded by 3, the water texture is flowing to the 1 other direction
    // If surrounded by 4, the water texture is still

    uint8_t texture_offset = get_default_texture_index(block_id);
    /*
        uint8_t count = 0;
        uint8_t bitmask = 0;

        for (int i = 0, j = 0; i < 6; i++)
        {
            if (i == 2 || i == 3)
                continue;
            if (get_fluid_meta_level(neighbors[i]) < fluid_level)
            {
                bitmask |= (1 << j);
                count++;
                switch (count)
                {
                case 1:
                    texture_offset = texture_offsets[1 + (j >> 1)];
                    break;
                case 2:
                    texture_offset = texture_offsets[1 + (((!(bitmask & 3)) != (!(bitmask & 12))) << 1)];
                    break;
                case 3:
                    texture_offset = texture_offsets[1 + ((!(bitmask & 1)) != (!(bitmask & 2)))];
                    break;
                case 4:
                    texture_offset = texture_offsets[0];
                    break;

                default:
                    break;
                }
            }
            j++;
        }
    */
    uint8_t texture_start_x = TEXTURE_X(texture_offset);
    uint8_t texture_start_y = TEXTURE_Y(texture_offset);
    uint8_t texture_end_x = texture_start_x + UV_SCALE;
    uint8_t texture_end_y = texture_start_y + UV_SCALE;

    vertex_property_t bottomPlaneCoords[4] = {
        {(local_pos + vec3f{-.5f, -.5f, -.5f}), texture_start_x, texture_end_y},
        {(local_pos + vec3f{-.5f, -.5f, +.5f}), texture_start_x, texture_start_y},
        {(local_pos + vec3f{+.5f, -.5f, +.5f}), texture_end_x, texture_start_y},
        {(local_pos + vec3f{+.5f, -.5f, -.5f}), texture_end_x, texture_end_y},
    };
    if (!is_same_fluid(block_id, neighbor_ids[FACE_NY]) && !is_solid(neighbor_ids[FACE_NY]))
        faceCount += DrawHorizontalQuad(bottomPlaneCoords[0], bottomPlaneCoords[1], bottomPlaneCoords[2], bottomPlaneCoords[3], neighbors[FACE_NY] ? neighbors[FACE_NY]->light : light);

    vertex_property_t topPlaneCoords[4] = {
        {(local_pos + vec3f{+.5f, -.5f + corner_tops[3], -.5f}), texture_end_x, texture_end_y},
        {(local_pos + vec3f{+.5f, -.5f + corner_tops[2], +.5f}), texture_end_x, texture_start_y},
        {(local_pos + vec3f{-.5f, -.5f + corner_tops[1], +.5f}), texture_start_x, texture_start_y},
        {(local_pos + vec3f{-.5f, -.5f + corner_tops[0], -.5f}), texture_start_x, texture_end_y},
    };
    if (!is_same_fluid(block_id, neighbor_ids[FACE_PY]))
        faceCount += DrawHorizontalQuad(topPlaneCoords[0], topPlaneCoords[1], topPlaneCoords[2], topPlaneCoords[3], is_solid(neighbor_ids[FACE_PY]) ? light : neighbors[FACE_PY]->light);

    texture_offset = get_default_texture_index(block_id);

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
    if (neighbors[FACE_NZ] && !is_same_fluid(block_id, neighbor_ids[FACE_NZ]) && !is_solid(neighbor_ids[FACE_NZ]))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_NZ]->light);

    sideCoords[3].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[2], +.5f};
    sideCoords[2].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[2], +.5f};
    sideCoords[1].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[1], +.5f};
    sideCoords[0].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[1], +.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[2];
    sideCoords[2].y_uv = texture_start_y + corner_max[2];
    sideCoords[1].y_uv = texture_start_y + corner_max[1];
    sideCoords[0].y_uv = texture_start_y + corner_min[1];
    if (neighbors[FACE_PZ] && !is_same_fluid(block_id, neighbor_ids[FACE_PZ]) && !is_solid(neighbor_ids[FACE_PZ]))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_PZ]->light);

    sideCoords[3].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[1], +.5f};
    sideCoords[2].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[1], +.5f};
    sideCoords[1].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[0], -.5f};
    sideCoords[0].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[0], -.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[1];
    sideCoords[2].y_uv = texture_start_y + corner_max[1];
    sideCoords[1].y_uv = texture_start_y + corner_max[0];
    sideCoords[0].y_uv = texture_start_y + corner_min[0];
    if (neighbors[FACE_NX] && !is_same_fluid(block_id, neighbor_ids[FACE_NX]) && !is_solid(neighbor_ids[FACE_NX]))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_NX]->light);

    sideCoords[3].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[3], -.5f};
    sideCoords[2].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[3], -.5f};
    sideCoords[1].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[2], +.5f};
    sideCoords[0].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[2], +.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[3];
    sideCoords[2].y_uv = texture_start_y + corner_max[3];
    sideCoords[1].y_uv = texture_start_y + corner_max[2];
    sideCoords[0].y_uv = texture_start_y + corner_min[2];
    if (neighbors[FACE_PX] && !is_same_fluid(block_id, neighbor_ids[FACE_PX]) && !is_solid(neighbor_ids[FACE_PX]))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_PX]->light);
    return faceCount * 3;
}

void chunk_t::prepare_render()
{
    this->recalculate();
    this->build_all_vbos();
}

uint32_t chunk_t::size()
{
    uint32_t base_size = sizeof(chunk_t);
    for (int i = 0; i < VERTICAL_SECTION_COUNT; i++)
        base_size += this->vbos[i].cached_solid_buffer_length + this->vbos[i].cached_transparent_buffer_length + sizeof(chunkvbo_t);
    return base_size;
}

// destroy chunk
void destroy_chunk(chunk_t *chunk)
{
    for (int i = 0; i < VERTICAL_SECTION_COUNT; i++)
    {
        chunk->destroy_vbo(i, VBO_ALL);
    }
}