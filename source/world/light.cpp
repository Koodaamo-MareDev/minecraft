#include "light.hpp"
#include <world/chunk.hpp>
#include <math/vec3i.hpp>
#include <block/blocks.hpp>
#include <block/block_properties.hpp>
#include <world/util/raycast.hpp>
#include <world/world.hpp>
#include <util/lock.hpp>
#include <util/timers.hpp>
#include <cmath>
#include <sys/unistd.h>
#include <gccore.h>
#include <deque>
#include <algorithm>
#include <list>
#include <cstdio>
#include <set>

lwp_t LightEngine::thread_handle = 0;
bool LightEngine::thread_active = false;
bool LightEngine::use_skylight = true;
World *LightEngine::current_world = nullptr;
std::deque<Vec3i> LightEngine::pending_updates;

void LightEngine::init(World *world)
{
    if (thread_active)
        return;
    thread_active = true;
    current_world = world;
    auto light_engine_thread = [](void *) -> void *
    {
        while (thread_active)
        {
            loop();
        }
        return NULL;
    };

    LWP_CreateThread(&thread_handle,
                     light_engine_thread,
                     NULL,
                     NULL,
                     64 * 1024,
                     50);
}

void LightEngine::enable_skylight(bool enabled)
{
    use_skylight = enabled;
}

void LightEngine::reset()
{
    pending_updates.clear();
}

bool LightEngine::busy()
{
    return pending_updates.size() > 0;
}

void LightEngine::loop()
{
    int updates = 0;
    uint64_t start = time_get();
    while (pending_updates.size() > 0 && thread_active)
    {
        Vec3i current = pending_updates.front();
        pending_updates.pop_front();
        update(current);
        if ((++updates & 1023) == 0)
        {
            if (time_diff_us(start, time_get()) > 100)
            {
                usleep(10);
                start = time_get();
            }
        }
    }
    usleep(1000);
}
void LightEngine::deinit()
{
    if (thread_active)
    {
        thread_active = false;
        pending_updates.clear();
        LWP_JoinThread(thread_handle, NULL);
    }
}

void LightEngine::post(const Vec3i &location)
{
    Chunk *chunk = current_world->get_chunk_from_pos(location);
    if (!chunk)
        return;
    ++chunk->light_update_count;
    pending_updates.push_back(location);
}

/*
 *  This was a pain to implement - I will not be maintaining this myself.
 *  Although it's mostly rewritten by AI, it works and seems to be faster
 *  than any other implementation that I could find online in 2025.
 */
void LightEngine::update(const Vec3i &update)
{
    Chunk *origin_chunk = current_world->get_chunk_from_pos(update);
    if (!origin_chunk)
        return;
    --origin_chunk->light_update_count;

    /*
     * Simple cache system for a subset of chunks near
     * the light update. This relies on the fact that
     * light updates can only affect blocks within 15
     * blocks (taxicab).
     */
    std::vector<Chunk *> nearby_chunks;
    for (int x = -16; x <= 16; x += 16)
    {
        for (int z = -16; z <= 16; z += 16)
        {
            Chunk *chunk = current_world->get_chunk_from_pos(update + Vec3i(x, 0, z));
            if (chunk && std::find(nearby_chunks.begin(), nearby_chunks.end(), chunk) == nearby_chunks.end())
                nearby_chunks.push_back(chunk);
        }
    }

    auto get_nearby_chunk = [&](int x, int z) -> Chunk *
    {
        for (Chunk *chunk : nearby_chunks)
        {
            if (chunk->x == x && chunk->z == z)
                return chunk;
        }
        return nullptr;
    };

    auto get_neighbors = [&](const Vec3i &pos, Block *neighbors[6])
    {
        for (int i = 0; i < 6; i++)
        {
            Chunk *chunk = get_nearby_chunk((pos.x + face_offsets[i].x) >> 4, (pos.z + face_offsets[i].z) >> 4);
            if (!chunk)
                neighbors[i] = nullptr;
            else
                neighbors[i] = chunk->get_block(pos + face_offsets[i]);
        }
    };

    Block *source_block = origin_chunk->get_block(update);
    uint8_t old_skylight = source_block->sky_light;
    uint8_t old_blocklight = source_block->block_light;

    // Set source block's light to its own emission value.
    // Propagation will handle light from neighbors.
    uint8_t initial_blocklight = properties(source_block->id).m_luminance;
    uint8_t initial_skylight = (use_skylight && update.y >= origin_chunk->height_map[LightUpdateNode(update).lightmap_index()]) ? 0xF : 0;

    source_block->light = (initial_skylight << 4) | initial_blocklight;

    std::deque<LightRemoveNode> remove_queue;
    std::deque<LightUpdateNode> prop_queue;
    std::set<Vec3i> prop_set;

    // --- Queue Initialization ---
    if (initial_skylight < old_skylight)
        remove_queue.emplace_back(update, old_skylight, true);

    if (initial_blocklight < old_blocklight)
        remove_queue.emplace_back(update, old_blocklight, false);

    if (source_block->light > 0)
    {
        prop_queue.emplace_back(update, -1);
        prop_set.insert(update);
    }

    std::set<Section *> affected_sections;
    affected_sections.insert(&origin_chunk->sections[(update.y >> 4) & 7]);
    // --- Removal Pass ---
    while (!remove_queue.empty())
    {
        LightRemoveNode current = remove_queue.front();
        remove_queue.pop_front();

        for (int i = 0; i < 6; i++)
        {
            Vec3i neighbor_pos = current.location + face_offsets[i];
            Chunk *neighbor_chunk = get_nearby_chunk(neighbor_pos.x >> 4, neighbor_pos.z >> 4);
            if (!neighbor_chunk)
                continue;
            Block *neighbor_block = neighbor_chunk->get_block(neighbor_pos);
            if (!neighbor_block)
                continue;

            uint8_t neighbor_level = current.is_sky ? neighbor_block->sky_light : neighbor_block->block_light;

            if (neighbor_level > 0)
            {
                // If the neighbor's light level is dimmer than the light being removed,
                // it was likely lit by this path.
                if (neighbor_level < current.level)
                {
                    uint8_t old_neighbor_level = neighbor_level;
                    (current.is_sky ? neighbor_block->sky_light : neighbor_block->block_light) = 0;
                    affected_sections.insert(&neighbor_chunk->sections[(neighbor_pos.y >> 4) & 7]);
                    remove_queue.emplace_back(neighbor_pos, old_neighbor_level, current.is_sky);
                }
                else
                {
                    // This neighbor is a potential light source for the propagation pass.
                    if (prop_set.find(neighbor_pos) == prop_set.end())
                    {
                        prop_queue.emplace_back(neighbor_pos, -1);
                        prop_set.insert(neighbor_pos);
                    }
                }
            }
        }
    }

    // --- Propagation Pass ---
    while (!prop_queue.empty())
    {
        LightUpdateNode current = prop_queue.front();
        prop_queue.pop_front();
        prop_set.erase(current.location);

        if (!thread_active)
            continue;
        Chunk *chunk = get_nearby_chunk(current.location.x >> 4, current.location.z >> 4);

        if (!chunk || current.location.y < 0)
            continue;

        Block *current_block = chunk->get_block(current.location);
        uint8_t old_prop_light = current_block->light;

        // Recalculate light for the current propagation node
        uint8_t prop_skylight = (use_skylight && current.location.y >= chunk->height_map[current.lightmap_index()]) ? 0xF : 0;
        uint8_t prop_blocklight = properties(current_block->id).m_luminance;

        Block *prop_neighbors[6];
        get_neighbors(current.location, prop_neighbors);
        int8_t prop_opacity = get_block_opacity(current_block->get_blockid());

        if (prop_opacity < 15)
        {
            prop_opacity = std::max<int8_t>(prop_opacity, 1);
            for (int i = 0; i < 6; i++)
            {
                if (!prop_neighbors[i])
                    continue;
                prop_skylight = std::max<int8_t>(prop_skylight, std::max<int8_t>(prop_neighbors[i]->sky_light - prop_opacity, 0));
                prop_blocklight = std::max<int8_t>(prop_blocklight, std::max<int8_t>(prop_neighbors[i]->block_light - prop_opacity, 0));
            }
        }
        uint8_t prop_new_light = (prop_skylight << 4) | prop_blocklight;

        // Only propagate if the new light is stronger
        if (prop_new_light > old_prop_light || current.location == update)
        {
            current_block->light = prop_new_light;
            affected_sections.insert(&chunk->sections[(current.location.y >> 4) & 7]);
            for (int i = 0; i < 6; i++)
            {
                if (prop_neighbors[i])
                {
                    Vec3i new_pos = current.location + face_offsets[i];
                    if (prop_set.find(new_pos) == prop_set.end())
                    {
                        prop_queue.emplace_back(new_pos, i);
                        prop_set.insert(new_pos);
                    }
                }
            }
        }
    }
    while (!affected_sections.empty())
    {
        auto first = affected_sections.begin();
        Section *sect = *first;
        if (sect && sect->chunk && sect->chunk->state != ChunkState::invalid)
            (*first)->dirty = true;
        affected_sections.erase(first);
    }
}

Chunk *LightUpdateNode::chunk()
{
    if (_chunk)
        return _chunk;
    World *world = LightEngine::world();
    if (world)
        _chunk = world->get_chunk_from_pos(location);
    return _chunk;
}

uint8_t LightUpdateNode::lightmap_index()
{
    return (location.x & 0xF) | ((location.z & 0xF) << 4);
}