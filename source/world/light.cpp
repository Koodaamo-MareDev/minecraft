#include "light.hpp"
#include <world/chunk.hpp>
#include <world/chunk_cache.hpp>
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

static bool busy_flag = false;

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
    return busy_flag;
}

void LightEngine::loop()
{
    int updates = 0;
    busy_flag = true;
    while (pending_updates.size() > 0 && thread_active)
    {
        Vec3i current = pending_updates.front();
        pending_updates.pop_front();
        update(current);
        if ((++updates & 1023) == 0)
            return;
    }
    busy_flag = false;
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

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            Chunk *chunk = cache.chunks[i][j];
            if (!chunk)
                continue;
            chunk->light_pending = true;
        }
    }

    Vec3i update_volume = {0, 0, 0};

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
            int8_t opacity = std::max<int8_t>(get_block_opacity(block->blockid), 1);

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
                update_volume.x = std::max(update_volume.x, std::abs(n.x + o.x - start.x));
                update_volume.y = std::max(update_volume.y, std::abs(n.y + o.y - start.y));
                update_volume.z = std::max(update_volume.z, std::abs(n.z + o.z - start.z));
            }
        }
    }
    start_chunk->sections[std::clamp(start.y >> 4, 0, VERTICAL_SECTION_COUNT - 1)].dirty = true;

    // Apply to neighbors if at chunk border
    if (current_world->smooth_lighting)
    {
        update_volume.x++;
        update_volume.y++;
        update_volume.z++;
    }

    update_volume.x = (update_volume.x + 15) & ~15;
    update_volume.y = (update_volume.y + 15) & ~15;
    update_volume.z = (update_volume.z + 15) & ~15;

    // Update the (potentially) affected sections.
    for (int y = start.y - update_volume.y; y <= start.y + update_volume.y; y += 16)
        for (int z = start.z - update_volume.z; z <= start.z + update_volume.z; z += 16)
            for (int x = start.x - update_volume.x; x <= start.x + update_volume.x; x += 16)
            {
                Chunk *nchunk2 = nullptr;
                if (get_block_cached(cache, x, y, z, nchunk2))
                {
                    nchunk2->sections[std::clamp(y >> 4, 0, VERTICAL_SECTION_COUNT - 1)].dirty = true;
                }
            }

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            Chunk *chunk = cache.chunks[i][j];
            if (!chunk)
                continue;
            chunk->light_pending = false;
        }
    }
}
