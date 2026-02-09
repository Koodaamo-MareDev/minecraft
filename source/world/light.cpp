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
    pending_updates.push_back(location);
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

void LightEngine::update(const Vec3i &start)
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

        if (!current_world->hell && n.y >= chunk->height_map[(n.z & 15) << 4 | (n.x & 15)])
            sky = 0xF;

        if (!block->id || can_see_through(properties(block->id)))
        {
            int8_t opacity = std::max<int8_t>(get_block_opacity(block->get_blockid()), 1);

            for (int i = 0; i < 6; i++)
            {
                const Vec3i &o = face_offsets[i];
                Chunk *n_chunk;
                Block *nb = get_block_cached(cache, n.x + o.x, n.y + o.y, n.z + o.z, n_chunk);
                if (!nb)
                    continue;

                sky = std::max<int8_t>(sky, std::max<int8_t>(nb->sky_light - opacity, 0));
                torch = std::max<int8_t>(torch, std::max<int8_t>(nb->block_light - opacity, 0));
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
