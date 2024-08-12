#include "light.hpp"
#include "chunk_new.hpp"
#include "vec3i.hpp"
#include "blocks.hpp"
#include "block.hpp"
#include "raycast.hpp"
#include "lock.hpp"
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
std::deque<vec3i> pending_light_updates;
void __update_light(vec3i coords);
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
    while (pending_light_updates.size() > 0 && __light_engine_init_done)
    {
        __light_engine_busy = true;
        lock_t light_lock(light_mutex);
        vec3i pos = pending_light_updates.front();
        pending_light_updates.pop_front();
        light_lock.unlock();
        chunk_t *chunk = get_chunk_from_pos(pos, false);
        if (chunk)
        {
            __update_light(pos);
            if (++updates % 1000 == 0)
                usleep(1);
        }
    }
    __light_engine_busy = false;
    usleep(100);
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

void update_light(vec3i pos)
{
    if (pos.y > 255 || pos.y < 0)
        return;
    chunk_t *chunk = get_chunk_from_pos(pos, false, false);
    if (!chunk)
        return;
    lock_t light_lock(light_mutex);
    pending_light_updates.push_back(pos);
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

void __update_light(vec3i coords)
{
    static std::deque<vec3i> light_updates;

    light_updates.push_back(coords);

    while (light_updates.size() > 0)
    {
        if (!__light_engine_init_done)
        {
            light_updates.clear();
            return;
        }
        vec3i pos = light_updates.back();
        light_updates.pop_back();

        if (pos.y > 255 || pos.y < 0)
            continue;
        chunk_t *chunk = get_chunk_from_pos(pos, false, false);
        if (!chunk)
            continue;
        int map_index = ((pos.x & 15) << 4) | (pos.z & 15);

        block_t *block = chunk->get_block(pos);

        uint8_t new_skylight = 0;
        uint8_t new_blocklight = get_block_luminance(block->get_blockid());

        if (pos.y >= chunk->height_map[map_index])
            new_skylight = 0xF;

        block_t *neighbors[6];
        get_neighbors(pos, neighbors);
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
                    light_updates.push_back(pos + face_offsets[i]);
            }
        }
        chunk->vbos[(pos.y >> 4) & 15].dirty = true;
    }
}