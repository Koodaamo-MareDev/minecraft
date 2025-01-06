#include "light.hpp"
#include "chunk_new.hpp"
#include "vec3i.hpp"
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
lwp_t __light_engine_thread_handle = (lwp_t)NULL;
mutex_t light_mutex = (mutex_t)NULL;
bool __light_engine_init_done = false;
bool __light_engine_busy = false;
bool __light_engine_use_skylight = true;
std::deque<std::pair<vec3i, chunk_t *>> pending_light_updates;
void set_skylight_enabled(bool enabled)
{
    __light_engine_use_skylight = enabled;
}
void light_engine_reset()
{
    pending_light_updates.clear();
}
void __update_light(std::pair<vec3i, chunk_t *>);
void *__light_engine_init_internal(void *);

void light_engine_init()
{
    if (__light_engine_init_done)
        return;
    __light_engine_init_done = true;
    LWP_MutexInit(&light_mutex, false);
    LWP_CreateThread(&__light_engine_thread_handle, /* thread handle */
                     __light_engine_init_internal,  /* code */
                     NULL,                          /* arg pointer for thread */
                     NULL,                          /* stack base */
                     64 * 1024,                     /* stack size */
                     50 /* thread priority */);
}

void *__light_engine_init_internal(void *)
{
    while (__light_engine_init_done)
    {
        light_engine_loop();
    }
    return NULL;
}

bool light_engine_busy()
{
    return pending_light_updates.size() > 0;
}

void light_engine_update()
{
}

void light_engine_loop()
{
    int updates = 0;
    uint64_t start = time_get();
    while (pending_light_updates.size() > 0 && __light_engine_init_done)
    {
        __light_engine_busy = true;
        std::pair<vec3i, chunk_t *> update = pending_light_updates.front();
        pending_light_updates.pop_front();
        chunk_t *chunk = update.second;
        if (chunk)
        {
            __update_light(update);
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
    __light_engine_busy = false;
    usleep(1000);
}
void light_engine_deinit()
{
    if (__light_engine_init_done)
    {
        __light_engine_init_done = false;
        pending_light_updates.clear();
        LWP_JoinThread(__light_engine_thread_handle, NULL);
        LWP_MutexDestroy(light_mutex);
    }
}

void update_light(vec3i pos, chunk_t *chunk)
{
    if (!chunk || pos.y > MAX_WORLD_Y || pos.y < 0)
        return;
    ++chunk->light_update_count;
    pending_light_updates.push_back(std::make_pair(pos, chunk));
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

void __update_light(std::pair<vec3i, chunk_t *> update)
{
    std::deque<std::pair<vec3i, chunk_t *>> light_updates;
    std::deque<chunk_t *> chunks = get_chunks();
    vec3i coords = update.first;
    light_updates.push_back(update);
    while (light_updates.size() > 0)
    {
        if (!__light_engine_init_done)
        {
            light_updates.clear();
            return;
        }
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

        if (__light_engine_use_skylight && pos.y >= chunk->height_map[map_index])
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