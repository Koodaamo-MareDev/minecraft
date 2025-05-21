#include "chunk.hpp"
#include "blocks.hpp"
#include "texturedefs.h"
#include <math/vec2i.hpp>
#include <math/vec3i.hpp>
#include "blocks.hpp"
#include "timers.hpp"
#include "light.hpp"
#include "base3d.hpp"
#include "render.hpp"
#include "raycast.hpp"
#include "lock.hpp"
#include "chunkprovider.hpp"
#include "model.hpp"
#include "light_nether_rgba.h"
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
#include <fstream>
#include "nbt/nbt.hpp"
#include "mcregion.hpp"
#include "world.hpp"
const vec3i face_offsets[] = {
    vec3i{-1, +0, +0},  // Negative X
    vec3i{+1, +0, +0},  // Positive X
    vec3i{+0, -1, +0},  // Negative Y
    vec3i{+0, +1, +0},  // Positive Y
    vec3i{+0, +0, -1},  // Negative Z
    vec3i{+0, +0, +1}}; // Positive Z

mutex_t chunk_mutex = LWP_MUTEX_NULL;
static lwp_t chunk_manager_thread_handle = LWP_THREAD_NULL;
static bool run_chunk_manager = false;

std::deque<chunk_t *> chunks;
std::deque<chunk_t *> pending_chunks;

// Used to set the world to hell
void set_world_hell(bool hell)
{
    if (current_world->hell == hell)
        return;

    // Stop processing any light updates
    light_engine::reset();
    if (hell)
    {
        // Set the light map to the nether light map
        memcpy(light_map, light_nether_rgba, 1024); // 256 * 4
        GX_SetArray(GX_VA_CLR0, light_map, 4 * sizeof(u8));
        GX_InvVtxCache();
    }

    // Disable sky light in hell
    light_engine::enable_skylight(!hell);

    current_world->hell = hell;
}

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

std::map<int32_t, entity_physical *> &get_entities()
{
    static std::map<int32_t, entity_physical *> world_entities;
    return world_entities;
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
    if (!run_chunk_manager || pending_chunks.size() + chunks.size() >= CHUNK_COUNT)
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

    try
    {
        chunk_t *chunk = new chunk_t;
        chunk->x = x;
        chunk->z = z;
        mcr::region *region = mcr::get_region(x >> 5, z >> 5);
        if (region->locations[(x & 31) | ((z & 31) << 5)] == 0)
        {
            chunk->generation_stage = ChunkGenStage::empty;
        }
        else
        {
            chunk->generation_stage = ChunkGenStage::loading;
        }

        pending_chunks.push_back(chunk);
        std::sort(pending_chunks.begin(), pending_chunks.end(), chunk_sorter);
        return true;
    }
    catch (std::bad_alloc &e)
    {
        printf("Failed to allocate memory for chunk\n");
    }
    return false;
}

void deinit_chunk_manager()
{
    if (chunk_manager_thread_handle == LWP_THREAD_NULL)
        return;

    // Tell the chunk manager thread to stop
    run_chunk_manager = false;

    LWP_JoinThread(chunk_manager_thread_handle, NULL);
    chunk_manager_thread_handle = LWP_THREAD_NULL;

    // Cleanup the chunk mutex
    if (chunk_mutex != LWP_MUTEX_NULL)
        LWP_MutexDestroy(chunk_mutex);
    chunk_mutex = LWP_MUTEX_NULL;
}

void init_chunk_manager(chunkprovider *chunk_provider)
{
    if (chunk_manager_thread_handle != LWP_THREAD_NULL)
        return;

    // Initialize the chunk mutex if it hasn't been initialized yet
    if (chunk_mutex == LWP_MUTEX_NULL)
        LWP_MutexInit(&chunk_mutex, false);

    // Chunk provider can be null if something else will be providing the chunks.
    // This should only happen when in a remote world which means that chunks are
    // provided via the network code. In such a case, don't start the thread.
    if (!chunk_provider)
    {
        return;
    }
    auto chunk_manager_thread = [](void *arg) -> void *
    {
        chunkprovider *provider = (chunkprovider *)arg;
        run_chunk_manager = true;
        // Chunk provider can be null if we are in a remote world
        while (run_chunk_manager)
        {
            while (pending_chunks.empty())
            {
                LWP_YieldThread();
                if (!run_chunk_manager)
                {
                    return NULL;
                }
            }
            chunk_t *chunk = pending_chunks.back();

            // Generate the base terrain for the chunk
            provider->provide_chunk(chunk);

            // Move the chunk to the active list
            lock_t chunk_lock(chunk_mutex);
            chunks.push_back(chunk);
            pending_chunks.erase(std::find(pending_chunks.begin(), pending_chunks.end(), chunk));
            chunk_lock.unlock();

            // Finish the chunk with features
            provider->populate_chunk(chunk);

            usleep(100);
        }
        return NULL;
    };

    LWP_CreateThread(&chunk_manager_thread_handle,
                     chunk_manager_thread,
                     chunk_provider,
                     NULL,
                     0,
                     50);
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
    if (position.y < 0 || position.y > MAX_WORLD_Y)
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
    if (!current_world->is_remote())
    {
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
            if (block_below == BlockID::air || properties(block_below).m_fluid)
            {
                add_entity(new entity_falling_block(*block, pos));
                block->set_blockid(BlockID::air);
                block->meta = 0;

                update_neighbors(pos);
            }
        }
    }
    chunk->update_height_map(pos);
    light_engine::post(pos, chunk);
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
                if (end_y >= MAX_WORLD_Y || end_y <= 0)
                    return;
                this->height_map[(x << 4) | z] = end_y + 1;
                for (int y = MAX_WORLD_Y; y > end_y; y--)
                    this->get_block(vec3i(x, y, z))->sky_light = 15;
                light_engine::post(vec3i(chunkX + x, end_y + 1, chunkZ + z), this);

                // Update block lights
                for (int y = 0; y < end_y; y++)
                {
                    if (get_block_luminance(this->get_block(vec3i(x, y, z))->get_blockid()))
                    {
                        light_engine::post(vec3i(chunkX + x, y, chunkZ + z), this);
                    }
                }
            }
        }
        lit_state = 1;
    }
}

void chunk_t::recalculate_height_map()
{
    for (int x = 0; x < 16; x++)
    {
        for (int z = 0; z < 16; z++)
        {
            int end_y = skycast(vec3i(x, 0, z), this);
            if (end_y >= MAX_WORLD_Y || end_y <= 0)
                return;
            this->height_map[(x << 4) | z] = end_y + 1;
        }
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

        if (other_block->id != block->id || !properties(block->id).m_transparent)
        {
            RenderType other_rt = properties(other_block->id).m_render_type;
            visibility |= ((other_rt != RenderType::full && other_rt != RenderType::full_special) || properties(other_block->id).m_transparent) << i;
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
        printf("Chunk count: %d\n", chunks.size());
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
    else
    {
        if (block->get_blockid() == BlockID::chest)
        {
            return render_chest(block, pos);
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
    else
    {
        if (block->get_blockid() == BlockID::chest)
        {
            return render_chest(block, pos);
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

int chunk_t::get_chest_texture_index(block_t *block, const vec3i &pos, uint8_t face)
{
    // Bottom and top faces are always the same
    if (face == FACE_NY || face == FACE_PY)
        return 25;

    block_t *neighbors[4];
    {
        block_t *tmp_neighbors[6];
        get_neighbors(pos, tmp_neighbors, this);
        neighbors[0] = tmp_neighbors[0];
        neighbors[1] = tmp_neighbors[1];
        neighbors[2] = tmp_neighbors[4];
        neighbors[3] = tmp_neighbors[5];
    }

    if (std::none_of(neighbors, neighbors + 4, [](block_t *block)
                     { return block->get_blockid() == BlockID::chest; }))
    {
        // Single chest
        uint8_t direction = FACE_PZ;
        if (!(block->visibility_flags & (1 << FACE_PZ)) && (block->visibility_flags & (1 << FACE_NZ)))
            direction = FACE_NZ;
        if (!(block->visibility_flags & (1 << FACE_NX)) && (block->visibility_flags & (1 << FACE_PX)))
            direction = FACE_PX;
        if (!(block->visibility_flags & (1 << FACE_PX)) && (block->visibility_flags & (1 << FACE_NX)))
            direction = FACE_NX;

        return 26 + (face == direction);
    }

    // Double chest

    if ((face != FACE_NZ && face != FACE_PZ) && neighbors[0]->get_blockid() != BlockID::chest && neighbors[1]->get_blockid() != BlockID::chest)
    {
        // X axis
        bool half = neighbors[2]->get_blockid() == BlockID::chest;
        uint8_t other_flags = neighbors[half ? 2 : 3]->visibility_flags;
        uint8_t direction = FACE_PX;
        if ((!(block->visibility_flags & (1 << FACE_PX)) || !(other_flags & (1 << FACE_PX))) && (block->visibility_flags & (1 << FACE_NX)) && (other_flags & (1 << FACE_NX)))
            direction = FACE_NX;

        if (face == FACE_NX)
            half = !half;

        return 26 + (face == direction ? 16 : 32) - half;
    }
    else if ((face != FACE_NX && face != FACE_PX) && neighbors[2]->get_blockid() != BlockID::chest && neighbors[3]->get_blockid() != BlockID::chest)
    {
        // Z axis
        bool half = neighbors[0]->get_blockid() == BlockID::chest;

        uint8_t other_flags = neighbors[half ? 0 : 1]->visibility_flags;
        uint8_t direction = FACE_PZ;
        if ((!(block->visibility_flags & (1 << FACE_PZ)) || !(other_flags & (1 << FACE_PZ))) && (block->visibility_flags & (1 << FACE_NZ)) && (other_flags & (1 << FACE_NZ)))
            direction = FACE_NZ;

        if (face == FACE_PZ)
            half = !half;

        return 26 + (face == direction ? 16 : 32) - half;
    }
    return 26;
}

int chunk_t::render_special(block_t *block, const vec3i &pos)
{
    switch (block->get_blockid())
    {
    case BlockID::torch:
    case BlockID::unlit_redstone_torch:
    case BlockID::redstone_torch:
    case BlockID::lever:
        return render_torch(block, pos);
    case BlockID::wooden_door:
    case BlockID::iron_door:
        return render_door(block, pos);
    default:
        break;
    }
    return 0;
}

int chunk_t::render_chest(block_t *block, const vec3i &pos)
{
    int vertexCount = 0;
    for (uint8_t face = 0; face < 6; face++)
    {
        if (block->get_opacity(face))
            vertexCount += render_face(pos, face, get_chest_texture_index(block, pos, face), this, block);
    }
    return vertexCount;
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
    uint32_t bottom_index = top_index;
    bool render_top = true;
    bool render_bottom = true;
    bool top_half = block->meta & 8;
    int slab_type = block->meta & 7;
    switch (slab_type)
    {
    case 1: // Sandstone slab
        top_index = 176;
        side_index = 192;
        bottom_index = 208;
        break;
    case 2: // Wooden slab
        top_index = side_index = bottom_index = 4;
        break;
    case 3: // Cobblestone slab
        top_index = side_index = bottom_index = 16;
        break;
    case 4: // Brick slab
        top_index = side_index = bottom_index = 7;
        break;
    case 5: // Stone brick slab
        top_index = side_index = bottom_index = 54;
        break;
    default:
        break;
    }
    int vertexCount = 0;
    if (top_half)
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
        lighting = get_face_light_index(pos, FACE_NY, this, block);
        GX_VertexLit({vertex_pos + vec3f(-.5, -.5, -.5), TEXTURE_NX(bottom_index), TEXTURE_PY(bottom_index)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + vec3f(-.5, -.5, 0.5), TEXTURE_NX(bottom_index), TEXTURE_NY(bottom_index)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + vec3f(0.5, -.5, 0.5), TEXTURE_PX(bottom_index), TEXTURE_NY(bottom_index)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + vec3f(0.5, -.5, -.5), TEXTURE_PX(bottom_index), TEXTURE_PY(bottom_index)}, lighting, FACE_NY);
        vertexCount += 4;
    }

    if (render_top)
    {
        lighting = get_face_light_index(pos, FACE_PY, this, block);
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
    vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    vec3f vertex_pos(local_pos.x, local_pos.y + 0.1875, local_pos.z);
    switch (block->meta & 0x7)
    {
    case 1: // Facing east
        vertex_pos.x -= 0.125;
        return render_torch_with_angle(block, vertex_pos, -0.4, 0);
    case 2: // Facing west
        vertex_pos.x += 0.125;
        return render_torch_with_angle(block, vertex_pos, 0.4, 0);
    case 3: // Facing south
        vertex_pos.z -= 0.125;
        return render_torch_with_angle(block, vertex_pos, 0, -0.4);
    case 4: // Facing north
        vertex_pos.z += 0.125;
        return render_torch_with_angle(block, vertex_pos, 0, 0.4);
    default: // Facing up
        vertex_pos.y -= 0.1875;
        return render_torch_with_angle(block, vertex_pos, 0, 0);
    }
}

int chunk_t::render_torch_with_angle(block_t *block, const vec3f &vertex_pos, vfloat_t ax, vfloat_t az)
{
    uint8_t lighting = block->light;
    uint32_t texture_index = get_default_texture_index(block->get_blockid());

    // Negative X side
    GX_VertexLit({vertex_pos + vec3f{-.0625f + ax, -.5f, -.5f + az}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + vec3f{-.0625f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + vec3f{-.0625f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + vec3f{-.0625f + ax, -.5f, 0.5f + az}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    // Positive X side
    GX_VertexLit({vertex_pos + vec3f{0.0625f + ax, -.5f, 0.5f + az}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PX);
    GX_VertexLit({vertex_pos + vec3f{0.0625f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PX);
    GX_VertexLit({vertex_pos + vec3f{0.0625f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PX);
    GX_VertexLit({vertex_pos + vec3f{0.0625f + ax, -.5f, -.5f + az}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PX);
    // Negative Z side
    GX_VertexLit({vertex_pos + vec3f{0.5f + ax, -.5f, -.0625f + az}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.0625f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.0625f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + vec3f{-.5f + ax, -.5f, -.0625f + az}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NZ);
    // Positive Z side
    GX_VertexLit({vertex_pos + vec3f{-.5f + ax, -.5f, 0.0625f + az}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.0625f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.0625f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + vec3f{0.5f + ax, -.5f, 0.0625f + az}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PZ);
    // Top side
    GX_VertexLit({vertex_pos + vec3f{0.0625f + ax * 0.375f, 0.125f, -.0625f + az * 0.375f}, TEXTURE_X(texture_index) + 9 * BASE3D_UV_FRAC_LO, TEXTURE_Y(texture_index) + 8 * BASE3D_UV_FRAC_LO}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f{0.0625f + ax * 0.375f, 0.125f, 0.0625f + az * 0.375f}, TEXTURE_X(texture_index) + 9 * BASE3D_UV_FRAC_LO, TEXTURE_Y(texture_index) + 6 * BASE3D_UV_FRAC_LO}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f{-.0625f + ax * 0.375f, 0.125f, 0.0625f + az * 0.375f}, TEXTURE_X(texture_index) + 7 * BASE3D_UV_FRAC_LO, TEXTURE_Y(texture_index) + 6 * BASE3D_UV_FRAC_LO}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f{-.0625f + ax * 0.375f, 0.125f, -.0625f + az * 0.375f}, TEXTURE_X(texture_index) + 7 * BASE3D_UV_FRAC_LO, TEXTURE_Y(texture_index) + 8 * BASE3D_UV_FRAC_LO}, lighting, FACE_PY);

    return 20;
}

int chunk_t::render_door(block_t *block, const vec3i pos)
{
    uint8_t lighting = block->light;
    vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    vec3f vertex_pos = vec3f(local_pos.x, local_pos.y, local_pos.z) - vec3f(0.5);

    uint32_t texture_index = get_default_texture_index(block->get_blockid());
    bool top_half = block->meta & 8;
    bool open = block->meta & 4;
    uint8_t direction = block->meta & 3;
    if (!open)
        direction = (direction + 3) & 3;
    if (!top_half)
        texture_index += 16;

    constexpr vfloat_t door_thickness = 0.1875;

    aabb_t door_bounds(vec3f(0, 0, 0), vec3f(1, 1, 1));
    if (direction == 0)
        door_bounds.max.z -= 1 - door_thickness;
    else if (direction == 1)
        door_bounds.min.x += 1 - door_thickness;
    else if (direction == 2)
        door_bounds.min.z += 1 - door_thickness;
    else if (direction == 3)
        door_bounds.max.x -= 1 - door_thickness;

    // We'll calculate the vertices and UVs based on the door's bounds

    // Bottom face (we'll not render the bottom face of the top half)
    if (!top_half)
    {
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.min.x * 16), TEXTURE_Y(texture_index) + uint32_t(door_bounds.min.z * 16)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.min.x * 16), TEXTURE_Y(texture_index) + uint32_t(door_bounds.max.z * 16)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.max.x * 16), TEXTURE_Y(texture_index) + uint32_t(door_bounds.max.z * 16)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.max.x * 16), TEXTURE_Y(texture_index) + uint32_t(door_bounds.min.z * 16)}, lighting, FACE_NY);
    }

    // Top face (we'll not render the top face of the bottom half)
    if (top_half)
    {
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.max.x * 16), TEXTURE_Y(texture_index) + uint32_t(door_bounds.min.z * 16)}, lighting, FACE_PY);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.max.x * 16), TEXTURE_Y(texture_index) + uint32_t(door_bounds.max.z * 16)}, lighting, FACE_PY);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.min.x * 16), TEXTURE_Y(texture_index) + uint32_t(door_bounds.max.z * 16)}, lighting, FACE_PY);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.min.x * 16), TEXTURE_Y(texture_index) + uint32_t(door_bounds.min.z * 16)}, lighting, FACE_PY);
    }

    bool flip_states[4] = {false, false, false, false};

    for (int i = 0; i < 4; i++)
    {
        int mirrorFlag = (direction >> 1) + ((i & 1) ^ direction);
        mirrorFlag += open;
        if ((mirrorFlag & 1) != 0)
            flip_states[i] = true;
    }

    // Negative X side
    if (flip_states[1])
    {
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.min.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.max.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.max.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.min.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_NX);
    }
    else
    {
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.min.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.max.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.max.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.min.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_NX);
    }

    // Positive X side
    if (flip_states[3])
    {
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.min.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.max.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.max.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.min.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_PX);
    }
    else
    {
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.min.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.max.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.max.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.min.z * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_PX);
    }

    // Negative Z side
    if (flip_states[2])
    {
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.min.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.max.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.max.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.min.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_NZ);
    }
    else
    {
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.min.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.max.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.max.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.min.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_NZ);
    }

    // Positive Z side
    if (flip_states[0])
    {
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.min.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.max.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.max.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_X(texture_index) + uint32_t(door_bounds.min.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_PZ);
    }
    else
    {
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.min.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.max.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.max.y * 16)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.max.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_PX(texture_index) - uint32_t(door_bounds.min.x * 16), TEXTURE_PY(texture_index) - uint32_t(door_bounds.min.y * 16)}, lighting, FACE_PZ);
    }

    return 20;
}

vec3f get_fluid_direction(block_t *block, vec3i pos, chunk_t *chunk)
{

    uint8_t fluid_level = get_fluid_meta_level(block);
    if ((fluid_level & 7) == 0)
        return vec3f(0.0, -1.0, 0.0);

    BlockID block_id = block->get_blockid();

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
            if (get_fluid_meta_level(neighbors[i]) < fluid_level)
                direction = direction - vec3f(face_offsets[i].x, 0, face_offsets[i].z);
            else if (get_fluid_meta_level(neighbors[i]) > fluid_level)
                direction = direction + vec3f(face_offsets[i].x, 0, face_offsets[i].z);
            else
                direction_set = false;
        }
    }
    if (!direction_set)
        direction.y = -1.0;
    return direction.normalize();
}

void add_entity(entity_physical *entity)
{
    std::map<int32_t, entity_physical *> &world_entities = get_entities();

    // If the world is local, assign a unique entity ID to prevent conflicts
    if (!current_world->is_remote())
    {
        while (world_entities.find(entity->entity_id) != world_entities.end())
        {
            entity->entity_id++;
        }
    }
    world_entities[entity->entity_id] = entity;
    vec3i entity_pos = vec3i(int(std::floor(entity->position.x)), int(std::floor(entity->position.y)), int(std::floor(entity->position.z)));
    entity->chunk = get_chunk_from_pos(entity_pos);
    if (entity->chunk)
        entity->chunk->entities.push_back(entity);
}

void remove_entity(int32_t entity_id)
{
    entity_physical *entity = get_entity_by_id(entity_id);
    if (!entity)
    {
        printf("Removing unknown entity %d\n", entity_id);
        return;
    }
    entity->dead = true;
    if (!entity->chunk)
    {
        std::map<int32_t, entity_physical *> &world_entities = get_entities();
        if (world_entities.find(entity_id) != world_entities.end())
            world_entities.erase(entity_id);
        delete entity;
    }
}

entity_physical *get_entity_by_id(int32_t entity_id)
{
    try
    {
        return get_entities().at(entity_id);
    }
    catch (std::out_of_range &)
    {
        return nullptr;
    }
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
    if (!current_world->is_remote())
    {
        // Resolve collisions with current chunk and neighboring chunks' entities
        for (int i = 0; i <= 6; i++)
        {
            if (i == FACE_NY)
                i += 2;
            chunk_t *neighbor = (i == 6 ? this : get_chunk(this->x + face_offsets[i].x, this->z + face_offsets[i].z));
            if (neighbor)
            {
                for (entity_physical *&entity : neighbor->entities)
                {
                    for (entity_physical *&this_entity : this->entities)
                    {
                        // Prevent entities from colliding with themselves
                        if (entity != this_entity && entity->collides(this_entity))
                        {
                            // Resolve the collision - always resolve from the perspective of the non-player entity
                            if (dynamic_cast<entity_player_local *>(this_entity))
                                entity->resolve_collision(this_entity);
                            else
                                this_entity->resolve_collision(entity);
                        }
                    }
                }
            }
        }
    }

    // Move entities to the correct chunk
    auto out_of_bounds_selector = [&](entity_physical *&entity)
    {
        chunk_t *new_chunk = get_chunk_from_pos(entity->get_foot_blockpos());
        if (new_chunk != entity->chunk)
        {
            entity->chunk = new_chunk;
            if (new_chunk)
                new_chunk->entities.push_back(entity);
            return true;
        }
        return false;
    };

    // For any moved entities, remove them from the current chunk
    entities.erase(std::remove_if(entities.begin(), entities.end(), out_of_bounds_selector), entities.end());

    // When in multiplayer, the server will handle entity removal
    if (!current_world->is_remote())
    {
        auto remove_selector = [&](entity_physical *&entity)
        {
            if (entity->can_remove())
            {
                entity->chunk = nullptr;
                remove_entity(entity->entity_id);
                return true;
            }
            return false;
        };
        entities.erase(std::remove_if(entities.begin(), entities.end(), remove_selector), entities.end());
    }
}

void chunk_t::render_entities(float partial_ticks, bool transparency)
{
    for (entity_physical *&entity : entities)
    {
        // Make sure the entity is valid
        if (!entity)
            continue;
        // Skip rendering dead entities
        if (entity->dead)
            continue;
        entity->render(partial_ticks, transparency);
    }

    // Restore default texture
    use_texture(terrain_texture);

    // Restore default colors
    gertex::set_color_mul(GXColor{255, 255, 255, 255});
    gertex::set_color_add(GXColor{0, 0, 0, 255});
}

uint32_t chunk_t::size()
{
    uint32_t base_size = sizeof(chunk_t);
    for (int i = 0; i < VERTICAL_SECTION_COUNT; i++)
        base_size += this->vbos[i].cached_solid.length + this->vbos[i].cached_transparent.length + sizeof(chunkvbo_t);
    for (entity_physical *&entity : entities)
        base_size += entity->size();
    return base_size;
}

chunk_t::~chunk_t()
{
    for (entity_physical *&entity : entities)
    {
        if (!entity)
            continue;
        if (entity->chunk == this)
        {
            entity->chunk = nullptr;
            if (!current_world->is_remote())
            {
                // Remove the entity from the global entity list
                // NOTE: This will also delete the entity as its chunk is nullptr
                remove_entity(entity->entity_id);
            }
        }
    }
}

void chunk_t::save(NBTTagCompound &compound)
{
    compound.setTag("xPos", new NBTTagInt(x));
    compound.setTag("zPos", new NBTTagInt(z));
    compound.setTag("LastUpdate", new NBTTagLong(current_world->ticks));

    std::vector<uint8_t> blocks = std::vector<uint8_t>(32768);
    std::vector<uint8_t> data = std::vector<uint8_t>(16384);
    std::vector<uint8_t> blocklight = std::vector<uint8_t>(16384);
    std::vector<uint8_t> skylight = std::vector<uint8_t>(16384);
    std::vector<uint8_t> heightmap = std::vector<uint8_t>(256);
    for (int i = 0; i < 16384; i++)
    {
        uint32_t out_index = i << 1;
        uint32_t iy = out_index & 0x7F;
        uint32_t iz = (out_index >> 7) & 0xF;
        uint32_t ix = (out_index >> 11) & 0xF;
        uint32_t index1 = ix | (iz << 4) | (iy << 8);
        uint32_t index2 = index1 | 0x100;

        blocks[out_index] = blockstates[index1].id;
        blocks[out_index | 1] = blockstates[index2].id;

        data[i] = (blockstates[index1].meta & 0xF) | ((blockstates[index2].meta & 0xF) << 4);
        blocklight[i] = (blockstates[index1].block_light) | (blockstates[index2].block_light << 4);
        skylight[i] = (blockstates[index1].sky_light) | (blockstates[index2].sky_light << 4);
    }
    for (int i = 0; i < 256; i++)
    {
        heightmap[i] = lightcast(vec3i(i & 0xF, 0, i >> 4), this);
    }
    compound.setTag("Blocks", new NBTTagByteArray(blocks));
    compound.setTag("Data", new NBTTagByteArray(data));
    compound.setTag("BlockLight", new NBTTagByteArray(blocklight));
    compound.setTag("SkyLight", new NBTTagByteArray(skylight));
    compound.setTag("HeightMap", new NBTTagByteArray(heightmap));
    compound.setTag("Entities", new NBTTagList());
    compound.setTag("TileEntities", new NBTTagList());
}

void chunk_t::load(NBTTagCompound &stream)
{
    std::vector<uint8_t> blocks = stream.getUByteArray("Blocks");
    std::vector<uint8_t> data = stream.getUByteArray("Data");
    std::vector<uint8_t> blocklight = stream.getUByteArray("BlockLight");
    std::vector<uint8_t> skylight = stream.getUByteArray("SkyLight");
    std::vector<uint8_t> heightmap = stream.getUByteArray("HeightMap");

    if (blocks.size() != 32768 || data.size() != 16384 || blocklight.size() != 16384 || skylight.size() != 16384 || heightmap.size() != 256)
    {
        throw std::runtime_error("Invalid chunk data");
    }

    for (int i = 0; i < 16384; i++)
    {
        uint32_t in_index = i << 1;
        uint32_t iy = in_index & 0x7F;
        uint32_t iz = (in_index >> 7) & 0xF;
        uint32_t ix = (in_index >> 11) & 0xF;
        uint32_t index1 = ix | (iz << 4) | (iy << 8);
        uint32_t index2 = index1 | 0x100;

        blockstates[index1].set_blockid((BlockID)blocks[in_index]);
        blockstates[index2].set_blockid((BlockID)blocks[in_index | 1]);

        blockstates[index1].meta = data[i] & 0xF;
        blockstates[index2].meta = (data[i] >> 4) & 0xF;

        blockstates[index1].block_light = blocklight[i] & 0xF;
        blockstates[index2].block_light = (blocklight[i] >> 4) & 0xF;

        blockstates[index1].sky_light = skylight[i] & 0xF;
        blockstates[index2].sky_light = (skylight[i] >> 4) & 0xF;
    }

    for (int i = 0; i < 256; i++)
    {
        height_map[i] = heightmap[(i >> 4) | (i & 0xF) << 4];
    }
}

void chunk_t::serialize()
{
    // Write the chunk using mcregion format

    mcr::region *region = mcr::get_region(x >> 5, z >> 5);
    if (!region)
    {
        throw std::runtime_error("Failed to get region for chunk " + std::to_string(x) + ", " + std::to_string(z));
    }

    std::fstream &chunk_file = region->open();

    if (!chunk_file.is_open())
    {
        throw std::runtime_error("Failed to open region file for writing");
    }

    // Save the chunk data to an NBT compound
    // Due to the way C++ manages memory, it's wise to fill in the compounds AFTER adding them to their parent
    NBTTagCompound root_compound;
    save(*(NBTTagCompound *)root_compound.setTag("Level", new NBTTagCompound));

    // Write the uncompressed compound to a buffer
    ByteBuffer uncompressed_buffer;
    root_compound.writeTag(uncompressed_buffer);

    // Compress the data
    uint32_t uncompressed_size = uncompressed_buffer.size();
    mz_ulong compressed_size = uncompressed_size;

    ByteBuffer buffer;
    buffer.resize(uncompressed_size);

    int result = mz_compress2(buffer.ptr(), &compressed_size, uncompressed_buffer.ptr(), uncompressed_size, MZ_BEST_SPEED);

    uncompressed_buffer.clear();

    buffer.resize(compressed_size);
    if (result != MZ_OK)
    {
        throw std::runtime_error("Failed to compress chunk data");
    }

    // Calculate the offset of the chunk in the file
    uint16_t offset = (x & 0x1F) | ((z & 0x1F) << 5);

    // Allocate the buffer in file
    uint32_t chunk_offset = region->allocate(buffer.size() + 5, offset);
    if (chunk_offset == 0)
    {
        throw std::runtime_error("Failed to allocate space for chunk");
    }

    // Write the header

    // Calculate the location of the chunk in the file
    uint32_t loc = (((buffer.size() + 5 + 4095) >> 12) & 0xFF) | (chunk_offset << 8);

    // Write the chunk location
    chunk_file.seekp(offset << 2);
    chunk_file.write(reinterpret_cast<char *>(&loc), sizeof(uint32_t));

    // Write the timestamp
    uint32_t timestamp = 1000; // TODO: Get the current time
    chunk_file.seekp((offset << 2) | 4096);
    chunk_file.write(reinterpret_cast<char *>(&timestamp), sizeof(uint32_t));

    // Get the length of the compressed data
    uint32_t length = compressed_size + 1;

    // Using zlib compression
    uint8_t compression = 2;

    // Write the chunk header
    chunk_file.seekp(chunk_offset << 12);
    chunk_file.write(reinterpret_cast<char *>(&length), sizeof(uint32_t));
    chunk_file.write(reinterpret_cast<char *>(&compression), sizeof(uint8_t));

    // Write the compressed data
    chunk_file.write(reinterpret_cast<char *>(buffer.ptr()), buffer.size());
    chunk_file.flush();

    uint32_t pos = chunk_file.seekg(0, std::ios::end).tellg();
    if ((pos & 0xFFF) != 0)
    {
        chunk_file.seekp(pos | 0xFFF);
        uint8_t padding = 0;
        chunk_file.write(reinterpret_cast<char *>(&padding), 1);
    }

    chunk_file.flush();
}

void chunk_t::deserialize()
{
    mcr::region *region = mcr::get_region(x >> 5, z >> 5);

    // Calculate the offset of the chunk in the file
    uint16_t offset = (x & 0x1F) | ((z & 0x1F) << 5);

    // Check if the chunk is stored in the region
    if (region->locations[offset] == 0)
    {
        return;
    }

    // Get the location data
    uint32_t loc = region->locations[offset];

    // Calculate the offset of the chunk in the file
    uint32_t chunk_offset = loc >> 8;

    // Open the region file
    std::fstream &chunk_file = region->open();
    if (!chunk_file.is_open())
    {
        printf("Failed to open region file for reading\n");
        return;
    }

    // Read the chunk header
    chunk_file.seekg(chunk_offset << 12);
    uint32_t length;
    uint8_t compression;
    chunk_file.read(reinterpret_cast<char *>(&length), sizeof(uint32_t));
    chunk_file.read(reinterpret_cast<char *>(&compression), sizeof(uint8_t));

    if (length >= 0x100000) // 1MB
    {
        printf("Resetting chunk because its length is too large: %u\n", length);

        // Erase the chunk from the region
        region->locations[offset] = 0;

        // Regenerate the chunk
        this->generation_stage = ChunkGenStage::empty;
        return;
    }

    // Read the compressed data
    ByteBuffer buffer(length - 1);
    chunk_file.read(reinterpret_cast<char *>(buffer.ptr()), length - 1);

    NBTTagCompound *compound = nullptr;
    try
    {
        if (compression == 1)
        {
            // Read as gzip compressed data
            compound = NBTBase::readGZip(buffer);
        }
        else if (compression == 2)
        {
            // Read as zlib compressed data
            compound = NBTBase::readZlib(buffer);
        }
        else
        {
            throw std::runtime_error("Unsupported compression type");
        }
    }
    catch (std::runtime_error &e)
    {
        printf("Failed to read chunk data: %s\n", e.what());

        // Regenerate the chunk
        this->generation_stage = ChunkGenStage::empty;
        return;
    }

    try
    {
        // Load the chunk data
        NBTTagCompound *level = compound->getCompound("Level");
        if (!level)
        {
            throw std::runtime_error("Missing Level compound");
        }

        load(*level);
    }
    catch (std::runtime_error &e)
    {
        printf("Failed to load chunk data: %s\n", e.what());

        // Regenerate the chunk
        this->generation_stage = ChunkGenStage::empty;

        // Clean up
        delete compound;
        return;
    }
    delete compound;

    lit_state = 1;
    generation_stage = ChunkGenStage::done;

    // Mark chunk as dirty
    for (int vbo_index = 0; vbo_index < VERTICAL_SECTION_COUNT; vbo_index++)
    {
        vbos[vbo_index].dirty = true;
    }
}
