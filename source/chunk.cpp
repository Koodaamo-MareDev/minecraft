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
#include "render_blocks.hpp"
#include "render_fluids.hpp"
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

    // Function to find a chunk with the given coordinates
    auto find_chunk = [x, z](chunk_t *chunk)
    {
        return chunk && chunk->x == x && chunk->z == z;
    };

    // Check if the chunk already exists
    if (std::find_if(pending_chunks.begin(), pending_chunks.end(), find_chunk) != pending_chunks.end())
        return false;
    if (std::find_if(chunks.begin(), chunks.end(), find_chunk) != chunks.end())
        return false;

    chunk_t *chunk = new chunk_t(x, z);

    // Check if the chunk exists in the region file
    mcr::region *region = mcr::get_region(x >> 5, z >> 5);
    if (region->locations[(x & 31) | ((z & 31) << 5)] != 0)
        chunk->generation_stage = ChunkGenStage::loading;

    // Add the chunk to the pending chunks
    pending_chunks.push_back(chunk);
    std::sort(pending_chunks.begin(), pending_chunks.end());

    return true;
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
    uint8_t *height = &height_map[pos.x | (pos.z << 4)];
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
        if (block->get_blockid() == BlockID::snow_layer)
        {
            block_t *block_below = chunk->get_block(pos + vec3i(0, -1, 0));
            if (block_below && block_below->get_blockid() == BlockID::grass)
            {
                block_below->meta = 1; // Set the snowy flag
                update_block_at(pos + vec3i(0, -1, 0));
            }
        }
    }
    chunk->update_height_map(pos);
    light_engine::post(coord(pos, chunk));
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
        coord pos = coord(vec3i(), this);
        for (int i = 0; i < 256; i++)
        {
            pos.coords.h_index = i;
            int end_y = skycast(vec3i(pos), this);
            if (end_y >= MAX_WORLD_Y || end_y <= 0)
                return;
            this->height_map[i] = end_y + 1;
            for (pos.coords.y = MAX_WORLD_Y; pos.coords.y > end_y; pos.coords.y--)
            {
                this->blockstates[uint16_t(pos)].sky_light = 15;
            }

            // Update top-most block under daylight
            light_engine::post(pos);

            // Update block lights
            for (pos.coords.y = 0; pos.coords.y < end_y; pos.coords.y++)
            {
                block_t *block = &this->blockstates[uint16_t(pos)];
                if (get_block_luminance(block->get_blockid()))
                {
                    light_engine::post(pos);
                }
            }
        }
        lit_state = 1;
    }
}

void chunk_t::recalculate_height_map()
{
    coord pos = coord(vec3i(), this);
    for (int i = 0; i < 256; i++)
    {
        pos.coords.h_index = i;
        int end_y = skycast(vec3i(pos), this);
        if (end_y >= MAX_WORLD_Y || end_y <= 0)
            return;
        this->height_map[i] = end_y + 1;
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
void chunk_t::refresh_section_block_visibility(int index)
{
    vec3i chunk_pos(this->x * 16, index * 16, this->z * 16);

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

// These variables are used for the flood fill algorithm
static std::vector<vec3i> floodfill_start_points;
static uint32_t floodfill_counter = 0;
static uint32_t floodfill_to_replace = 1;
static uint32_t floodfill_flood_id = 0;
static uint16_t floodfill_grid[4096] = {0}; // 16 * 16 * 16
static uint8_t floodfill_faces_touched[6] = {0};

// Initialize the flood fill start points for visibility calculations
// This function generates the start points for the flood fill algorithm
void chunk_t::init_floodfill_startpoints()
{
    floodfill_start_points.clear();
    constexpr int size = 16;
    constexpr int max = size - 1;
    // Generate faces at x = 0 and x = max
    for (int y = 0; y < size; ++y)
        for (int z = 0; z < size; ++z)
        {
            floodfill_start_points.push_back(vec3i(0, y, z));   // x = 0
            floodfill_start_points.push_back(vec3i(max, y, z)); // x = max
        }

    // Generate faces at y = 0 and y = max, skipping already added edges
    for (int x = 1; x < max; ++x)
        for (int z = 0; z < size; ++z)
        {
            floodfill_start_points.push_back(vec3i(x, 0, z));   // y = 0
            floodfill_start_points.push_back(vec3i(x, max, z)); // y = max
        }

    // Generate faces at z = 0 and z = max, skipping already added edges
    for (int x = 1; x < max; ++x)
        for (int y = 1; y < max; ++y)
        {
            floodfill_start_points.push_back(vec3i(x, y, 0));   // z = 0
            floodfill_start_points.push_back(vec3i(x, y, max)); // z = max
        }
}

void chunk_t::vbo_visibility_flood_fill(vec3i start_pos)
{
    std::deque<vec3i> queue;
    queue.push_back(start_pos);

    while (!queue.empty())
    {
        vec3i pos = queue.front();
        queue.pop_front();

        // Bounds check: If out of bounds, mark touched face and continue
        if (pos.x < 0)
        {
            floodfill_counter++;
            floodfill_faces_touched[0] = 1;
            continue;
        }
        if (pos.x > 15)
        {
            floodfill_counter++;
            floodfill_faces_touched[1] = 1;
            continue;
        }
        if (pos.y < 0)
        {
            floodfill_counter++;
            floodfill_faces_touched[2] = 1;
            continue;
        }
        if (pos.y > 15)
        {
            floodfill_counter++;
            floodfill_faces_touched[3] = 1;
            continue;
        }
        if (pos.z < 0)
        {
            floodfill_counter++;
            floodfill_faces_touched[4] = 1;
            continue;
        }
        if (pos.z > 15)
        {
            floodfill_counter++;
            floodfill_faces_touched[5] = 1;
            continue;
        }

        int index = pos.x | (pos.y << 8) | (pos.z << 4);
        if (floodfill_grid[index] != floodfill_to_replace)
            continue;

        // Mark the block as visited
        floodfill_grid[index] = floodfill_flood_id;

        // Add neighboring positions to the queue
        for (int i = 0; i < 6; i++)
        {
            queue.push_back(pos + face_offsets[i]);
        }
    }
}

void chunk_t::refresh_section_visibility(int index)
{
    // Converts a face pair to a bitmask for visibility flags.
    // Assumes a != b
    auto faces_to_index = [](int a, int b)
    {
        return 1 << (a * 5 + (b < a ? b : b - 1));
    };
    section &vbo = this->sections[index];

    // Initialize the flood fill start points if not already initialized
    if (floodfill_start_points.empty())
        init_floodfill_startpoints();

    // Rebuild the flood fill grid
    block_t *block = this->get_block(vec3i(0, index << 4, 0));
    bool empty = true;
    for (uint32_t i = 0; i < 4096; i++, block++)
    {
        // Default to non-solid block
        floodfill_grid[i] = 1;

        // Skip air blocks
        if (!block->id)
            continue;

        blockproperties_t prop = properties(block->id);

        if (prop.m_fluid || prop.m_transparent)
            continue;

        if (prop.m_render_type != RenderType::full && prop.m_render_type != RenderType::full_special)
            continue;

        // Mark the block as a solid block in the flood fill grid
        floodfill_grid[i] = 0;
        empty = false;
    }
    if (empty)
    {
        // No blocks to process, skip the flood fill
        vbo.visibility_flags = 0x3FFFFFFF;
        return;
    }
    vbo.visibility_flags = 0;
    std::memset(floodfill_faces_touched, 0, sizeof(floodfill_faces_touched));
    floodfill_flood_id = 2;
    for (uint32_t i = 0; i < floodfill_start_points.size(); i++)
    {
        vec3i pos = floodfill_start_points[i];

        // Reset the flood fill state
        floodfill_counter = 0;
        floodfill_to_replace = 1; // Reset the flood fill to replace value
        std::memset(floodfill_faces_touched, 0, sizeof(floodfill_faces_touched));

        // Start the flood fill from the current position
        vbo_visibility_flood_fill(pos);

        if (floodfill_counter > 1)
        {
            floodfill_flood_id++;

            // Connect the faces that were touched during the flood fill
            for (int j = 0; j < 6; j++)
                if (floodfill_faces_touched[j])
                    for (int k = 0; k < 6; k++)
                        if (j != k && floodfill_faces_touched[k])
                            vbo.visibility_flags |= faces_to_index(j, k) | faces_to_index(k, j);
        }
        else
        {
            // Cancel the flood fill as it failed.
            floodfill_to_replace = floodfill_flood_id;
            floodfill_flood_id = 1;
            vbo_visibility_flood_fill(pos);
            floodfill_flood_id = floodfill_to_replace;
        }
    }
}

int chunk_t::build_vbo(int index, bool transparent)
{
    section &vbo = this->sections[index];
    static uint8_t vbo_buffer[64000 * VERTEX_ATTR_LENGTH] __attribute__((aligned(32)));
    DCInvalidateRange(vbo_buffer, sizeof(vbo_buffer));

    GX_BeginDispList(vbo_buffer, sizeof(vbo_buffer));

    // The first byte (GX_Begin command) is skipped, and after that the vertex count is stored as a uint16_t
    uint16_t *quadVtxCountPtr = (uint16_t *)(&vbo_buffer[1]);

    // Render the block mesh
    int quadVtxCount = render_section_blocks(*this, index, transparent, 64000U);

    // 3 bytes for the GX_Begin command, with 1 byte offset to access the vertex count (uint16_t)
    int offset = (quadVtxCount * VERTEX_ATTR_LENGTH) + 1 + 3;

    // The vertex count for the fluid mesh is stored as a uint16_t after the block mesh vertex data
    uint16_t *triVtxCountPtr = (uint16_t *)(&vbo_buffer[offset]);

    int triVtxCount = render_section_fluids(*this, index, transparent, 64000U);

    // End the display list - we don't need to store the size, as we can calculate it from the vertex counts
    uint32_t success = GX_EndDispList();

    if (!success)
    {
        printf("Failed to create display list for section %d at (%d, %d)\n", index, this->x, this->z);
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
        printf("Failed to allocate %d bytes for section %d VBO at (%d, %d)\n", displist_size, index, this->x, this->z);
        printf("Chunk count: %d\n", chunks.size());
        return (1);
    }

    uint32_t pos = 0;
    // Copy the quad data
    if (quadVtxCount)
    {
        uint32_t quadSize = quadVtxCount * VERTEX_ATTR_LENGTH + 3;
        memcpy(displist_vbo, vbo_buffer, quadSize);
        pos += quadSize;
    }

    // Copy the triangle data
    if (triVtxCount)
    {
        uint32_t triSize = triVtxCount * VERTEX_ATTR_LENGTH + 3;
        memcpy((void *)((u32)displist_vbo + pos), (void *)((u32)vbo_buffer + (quadVtxCount * VERTEX_ATTR_LENGTH + 3)), triSize);
        pos += triSize;
    }

    // Set the rest of the buffer to 0
    memset((void *)((u32)displist_vbo + pos), 0, displist_size - pos);

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
    return (0);
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
    return direction.fast_normalize();
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
        base_size += this->sections[i].cached_solid.length + this->sections[i].cached_transparent.length + sizeof(section);
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
        height_map[i] = heightmap[i];
    }
}

void chunk_t::write()
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

void chunk_t::read()
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
        sections[vbo_index].dirty = true;
    }
}
