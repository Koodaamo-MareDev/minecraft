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
#include <stack>
#include <unordered_set>
#include <algorithm>
#include <list>
#include <cstdio>
#include <set>
#include <ported/SystemTime.hpp>

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
        update_optimized(current);
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
    //++chunk->light_update_count;
    pending_updates.push_back(location);
}

/*
 *  Copied from CavEX:
 *  https://github.com/xtreme8000/CavEX/blob/master/source/lighting.c
 */
static inline int8_t MAX_I8(int8_t a, int8_t b)
{
    return a > b ? a : b;
}

/*
 *  This code is HEAVILY inspired by the CavEX source code by xtreme8000
 *  The code can be obtained from the following GitHub repository:
 *  https://github.com/xtreme8000/CavEX/
 *  I prefer it because not only does it work well, it's significantly
 *  faster than any other implementation that I could find online.
 */
void LightEngine::update(const Vec3i &update)
{
    std::set<Vec3i> affected_sections;
    std::deque<Vec3i> light_updates;
    std::deque<Chunk *> &chunks = current_world->chunks;
    Chunk *origin_chunk = current_world->get_chunk_from_pos(update);
    if (!origin_chunk)
        return;
    //--origin.chunk->light_update_count;

    affected_sections.insert(Vec3i(update.x & ~15, update.y & ~15, update.z & ~15));
    light_updates.push_back(update);
    while (light_updates.size() > 0)
    {
        if (!thread_active)
            return;
        Vec3i current = light_updates.front();
        light_updates.pop_front();
        if (current.y < 0 || current.y > MAX_WORLD_Y)
            continue;

        Chunk *chunk = current_world->get_chunk_from_pos(current);
        if (std::find(chunks.begin(), chunks.end(), chunk) == chunks.end()) // FIXME: This is a hacky way
            return;                                                         // to check if the chunk is valid

        if (!chunk)
            continue;

        Block *block = chunk->get_block(current);

        uint8_t new_skylight = 0;
        uint8_t new_blocklight = properties(block->id).m_luminance;

        if (use_skylight && current.y >= chunk->height_map[(current.x & 15) | ((current.z & 15) << 4)])
            new_skylight = 0xF;
        Block *neighbors[6];
        current_world->get_neighbors(current, neighbors);

        int8_t opacity = get_block_opacity(block->get_blockid());
        if (block->id == BlockID::air || can_see_through(properties(block->id)))
        {
            opacity = MAX_I8(opacity, 1);
            for (int i = 0; i < 6; i++)
            {
                if (!neighbors[i])
                    continue;
                new_skylight = MAX_I8(MAX_I8(neighbors[i]->sky_light - opacity, 0), new_skylight);
                new_blocklight = MAX_I8(MAX_I8(neighbors[i]->block_light - opacity, 0), new_blocklight);
            }
        }

        uint8_t new_light = (new_skylight << 4) | new_blocklight;

        if (block->light != new_light || update == current)
        {
            block->light = new_light;
            affected_sections.insert(Vec3i(current.x & ~15, current.y & ~15, current.z & ~15));
            for (int i = 0; i < 6; i++)
            {
                if (neighbors[i])
                {
                    light_updates.push_back(current + face_offsets[i]);
                }
            }
        }
    }
    for (auto sect : affected_sections)
    {
        for (int x = sect.x - 1; x <= sect.x + 1; x++)
        {
            for (int z = sect.z - 1; z <= sect.z + 1; z++)
            {
                Chunk *chunk = current_world->get_chunk_from_pos(Vec3i(x, 0, z));
                if (!chunk)
                    continue;
                for (int y = update.y - 1; y <= update.y + 1; y++)
                {
                    // Skip non-adjacent sections if smooth lighting is disabled
                    if (!current_world->smooth_lighting && std::abs(x - update.x) + std::abs(z - update.z) + std::abs(y - update.y) > 1)
                        continue;
                    if (y < 0 || y > MAX_WORLD_Y)
                        continue;
                    chunk->sections[y >> 4].dirty = true;
                }
            }
        }
    }
}
static ChunkCache build_chunk_cache(World *world, int cx, int cz)
{
    ChunkCache cache{};
    cache.base_cx = cx;
    cache.base_cz = cz;

    for (int dz = -1; dz <= 1; dz++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            cache.chunks[dx + 1][dz + 1] =
                world->get_chunk(cx + dx, cz + dz);
        }
    }
    return cache;
}

inline Block *get_block_cached(
    ChunkCache &cache,
    int x, int y, int z,
    Chunk *&out_chunk)
{
    if (y < 0 || y > MAX_WORLD_Y)
        return nullptr;

    int cx = x >> 4;
    int cz = z >> 4;

    int dx = cx - cache.base_cx;
    int dz = cz - cache.base_cz;

    if ((unsigned)(dx + 1) > 2 || (unsigned)(dz + 1) > 2)
        return nullptr;

    Chunk *chunk = cache.chunks[dx + 1][dz + 1];
    if (!chunk)
        return nullptr;

    out_chunk = chunk;
    return &chunk->blockstates[(y << 8) | ((z & 15) << 4) | (x & 15)];
}
void LightEngine::update_optimized(const Vec3i &start)
{
    int start_cx = start.x >> 4;
    int start_cz = start.z >> 4;

    ChunkCache cache = build_chunk_cache(current_world, start_cx, start_cz);
    Chunk *start_chunk = cache.chunks[1][1];
    if (!start_chunk)
        return;
    std::deque<LightNode> stack;
    stack.push_back({start.x, start.y, start.z});

    while (!stack.empty())
    {
        LightNode n = stack.front();
        stack.pop_front();

        Chunk *chunk = nullptr;
        Block *block = get_block_cached(cache, n.x, n.y, n.z, chunk);
        if (!block)
            continue;

        uint8_t old_light = block->light;

        uint8_t sky = 0;
        uint8_t torch = properties(block->id).m_luminance;

        if (use_skylight && n.y >= chunk->height_map[(n.z & 15) << 4 | (n.x & 15)])
            sky = 0xF;

        if (!block->id || can_see_through(properties(block->id)))
        {
            int8_t opacity = MAX_I8(get_block_opacity(block->get_blockid()), 1);

            for (int i = 0; i < 6; i++)
            {
                const Vec3i &o = face_offsets[i];
                Chunk *n_chunk;
                Block *nb = get_block_cached(cache, n.x + o.x, n.y + o.y, n.z + o.z, n_chunk);
                if (!nb)
                    continue;

                sky = MAX_I8(sky, MAX_I8(nb->sky_light - opacity, 0));
                torch = MAX_I8(torch, MAX_I8(nb->block_light - opacity, 0));
            }
        }

        uint8_t new_light = (sky << 4) | torch;
        if (new_light == old_light && !(n.x == start.x && n.y == start.y && n.z == start.z))
            continue;

        block->light = new_light;

        for (int i = 0; i < 6; i++)
        {
            const Vec3i &o = face_offsets[i];
            Chunk *nchunk = nullptr;
            Block *nblock = nullptr;
            if ((nblock = get_block_cached(cache, n.x + o.x, n.y + o.y, n.z + o.z, nchunk)))
            {
                stack.push_back({n.x + o.x, n.y + o.y, n.z + o.z});

                nchunk->sections[std::clamp((n.y + o.y) >> 4, 0, VERTICAL_SECTION_COUNT - 1)].dirty = true;
                // Apply to neighbors if at chunk border
                if (current_world->smooth_lighting)
                    for (int j = 0; j < 6; j++)
                    {
                        const Vec3i &o2 = face_offsets[j];
                        Chunk *nchunk2 = nullptr;
                        if (get_block_cached(cache, n.x + o.x + o2.x, n.y + o.y + o2.y, n.z + o.z + o2.z, nchunk2) && nchunk2 != nchunk)
                        {
                            if ((n.y + o.y + o2.y) >> 4 != (n.y + o2.y) >> 4)
                                nchunk2->sections[std::clamp((n.y + o.y + o2.y) >> 4, 0, VERTICAL_SECTION_COUNT - 1)].dirty = true;
                        }
                    }
            }
        }
    }
}
