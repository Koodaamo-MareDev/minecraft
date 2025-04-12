#include "light.hpp"
#include "chunk.hpp"
#include <math/vec3i.hpp>
#include "blocks.hpp"
#include "block.hpp"
#include "raycast.hpp"
#include "lock.hpp"
#include "timers.hpp"
#include <cmath>
#include <sys/unistd.h>
#include <gccore.h>
#include <deque>
#include <algorithm>
#include <list>
#include <cstdio>

lwp_t light_engine::thread_handle = 0;
bool light_engine::thread_active = false;
bool light_engine::use_skylight = true;
std::deque<std::pair<vec3i, chunk_t *>> light_engine::pending_updates;

void light_engine::init()
{
    if (thread_active)
        return;
    thread_active = true;
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

void light_engine::enable_skylight(bool enabled)
{
    use_skylight = enabled;
}

void light_engine::reset()
{
    pending_updates.clear();
}

bool light_engine::busy()
{
    return pending_updates.size() > 0;
}

void light_engine::loop()
{
    int updates = 0;
    uint64_t start = time_get();
    while (pending_updates.size() > 0 && thread_active)
    {
        std::pair<vec3i, chunk_t *> current = pending_updates.front();
        pending_updates.pop_front();
        chunk_t *chunk = current.second;
        if (chunk)
        {
            update(current);
            --chunk->light_update_count;
            if ((++updates & 1023) == 0)
            {
                if (time_diff_us(start, time_get()) > 100)
                {
                    usleep(10);
                    start = time_get();
                }
            }
        }
    }
    usleep(1000);
}
void light_engine::deinit()
{
    if (thread_active)
    {
        thread_active = false;
        pending_updates.clear();
        LWP_JoinThread(thread_handle, NULL);
    }
}

void light_engine::post(vec3i pos, chunk_t *chunk)
{
    if (!chunk || pos.y > MAX_WORLD_Y || pos.y < 0)
        return;
    ++chunk->light_update_count;
    pending_updates.push_back(std::make_pair(pos, chunk));
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

void light_engine::update(const std::pair<vec3i, chunk_t *> &update)
{
    std::deque<std::pair<vec3i, chunk_t *>> light_updates;
    std::deque<chunk_t *> &chunks = get_chunks();
    vec3i coords = update.first;
    light_updates.push_back(update);
    while (light_updates.size() > 0)
    {
        if (!thread_active)
            return;
        std::pair item = light_updates.back();
        vec3i pos = item.first;
        chunk_t *chunk = item.second;
        if (std::find(chunks.begin(), chunks.end(), chunk) == chunks.end()) // FIXME: This is a hacky way
            return;                                                         // to check if the chunk is valid
        light_updates.pop_back();

        if (pos.y > MAX_WORLD_Y || pos.y < 0)
            continue;
        if (!chunk)
        {
            continue;
        }
        vec2i chunk_pos = block_to_chunk_pos(pos);
        int map_index = ((pos.x & 15) << 4) | (pos.z & 15);

        block_t *block = chunk->get_block(pos);

        uint8_t new_skylight = 0;
        uint8_t new_blocklight = properties(block->id).m_luminance;

        if (use_skylight && pos.y >= chunk->height_map[map_index])
            new_skylight = 0xF;

        block_t *neighbors[6];
        get_neighbors(pos, neighbors, chunk);
        int8_t opacity = get_block_opacity(block->get_blockid());
        if (opacity != 15)
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

        if (block->light != new_light || coords == pos)
        {
            block->light = new_light;
            for (int i = 0; i < 6; i++)
            {
                if (neighbors[i])
                {
                    std::pair<vec3i, chunk_t *> update = std::make_pair(pos + face_offsets[i], chunk);
                    if (block_to_chunk_pos(update.first) != chunk_pos)
                    {
                        update.second = get_chunk_from_pos(update.first);
                    }
                    light_updates.push_back(update);
                }
            }
        }
        chunk->vbos[(pos.y >> 4) & 15].dirty = true;
    }
}