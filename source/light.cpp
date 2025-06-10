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
std::deque<coord> light_engine::pending_updates;

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
        coord &current = pending_updates.front();
        pending_updates.pop_front();
        chunk_t *chunk = current.chunk;
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

void light_engine::post(const coord &location)
{
    ++location.chunk->light_update_count;
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

void light_engine::update(const coord &update)
{
    std::deque<coord> light_updates;
    std::deque<chunk_t *> &chunks = get_chunks();
    coord origin = update;
    light_updates.push_back(origin);
    while (light_updates.size() > 0)
    {
        if (!thread_active)
            return;
        coord current = light_updates.back();
        chunk_t *chunk = current.chunk;
        if (std::find(chunks.begin(), chunks.end(), chunk) == chunks.end()) // FIXME: This is a hacky way
            return;                                                         // to check if the chunk is valid
        light_updates.pop_back();

        if (!chunk || current.coords.y < 0)
            continue;

        block_t *block = &chunk->blockstates[uint16_t(current)];

        uint8_t new_skylight = 0;
        uint8_t new_blocklight = properties(block->id).m_luminance;

        if (use_skylight && current.coords.y >= chunk->height_map[current.coords.h_index])
            new_skylight = 0xF;

        block_t *neighbors[6];
        get_neighbors(vec3i(current), neighbors, chunk);
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

        if (block->light != new_light || origin.index == current.index)
        {
            block->light = new_light;
            for (int i = 0; i < 6; i++)
            {
                if (neighbors[i])
                {
                    vec3i new_pos = vec3i(current) + face_offsets[i];
                    chunk_t *coord_chunk = chunk;

                    // If the new position is outside the chunk, get the correct chunk from the position
                    if (chunk->x != (new_pos.x >> 4) || chunk->z != (new_pos.z >> 4))
                    {
                        coord_chunk = get_chunk_from_pos(new_pos);
                    }
                    light_updates.push_back(coord(new_pos, coord_chunk));
                }
            }
        }
        chunk->vbos[(current.coords.y >> 4) & 15].dirty = true;
    }
}