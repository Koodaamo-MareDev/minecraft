#include "light.hpp"
#include "chunk_new.hpp"
#include "vec3i.hpp"
#include "blocks.hpp"
#include "block.hpp"
#include "raycast.hpp"
#include <cmath>
#include <sys/unistd.h>
#include <gccore.h>
#include <vector>
#include <list>
#include <cstdio>
static lwp_t __light_engine_thread_handle = (lwp_t)NULL;
static mutex_t __light_update_mutex = (mutex_t)NULL;
static bool __light_engine_init_done = false;
static bool __light_engine_busy = false;
std::list<lightupdate_t> pending_light_updates;
void __update_light(vec3i coords);
void *__light_engine_init_internal(void *);

bool operator<(lightupdate_t const &posA, lightupdate_t const &posB)
{
    vec3i target(player_pos.x, player_pos.y, player_pos.z);
    vec3i diffA = posA.pos - target;
    vec3i diffB = posB.pos - target;
    return (diffA.x * diffA.x + diffA.y * diffA.y + diffA.z * diffA.z) < (diffB.x * diffB.x + diffB.y * diffB.y + diffB.z * diffB.z);
}

void light_engine_init()
{
    if (__light_engine_init_done)
        return;
    __light_engine_init_done = true;
    LWP_MutexInit(&__light_update_mutex, false);
    LWP_CreateThread(&__light_engine_thread_handle, /* thread handle */
                     __light_engine_init_internal,  /* code */
                     NULL,                          /* arg pointer for thread */
                     NULL,                          /* stack base */
                     512 * 1024,                    /* stack size */
                     50 /* thread priority */);
}

void *__light_engine_init_internal(void *)
{
    while (__light_engine_init_done)
    {
        light_engine_loop();
        LWP_YieldThread();
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
        LWP_MutexLock(__light_update_mutex);
        lightupdate_t update = pending_light_updates.front();
        pending_light_updates.pop_front();
        LWP_MutexUnlock(__light_update_mutex);
        chunk_t *chunk = get_chunk_from_pos(update.pos.x, update.pos.z, false);
        if (chunk)
        {
            block_t *block = chunk->get_block(update.pos.x, update.pos.y, update.pos.z);
            block->set_blocklight(update.block);
            block->set_skylight(update.sky);
            __update_light(update.pos);
            --chunk->light_updates;
        }
        if (++updates % 1000 == 0)
            LWP_YieldThread();
    }
    __light_engine_busy = false;
}
void light_engine_deinit()
{
    if (__light_engine_init_done)
    {
        __light_engine_init_done = false;

        LWP_MutexDestroy(__light_update_mutex);
        LWP_JoinThread(__light_engine_thread_handle, NULL);
    }
}

void cast_skylight(int x, int z)
{
    chunk_t *chunk = get_chunk_from_pos(x, z, false);
    if (!chunk)
        return;
    int end_y = skycast(x, z);
    if (end_y >= 255 || end_y <= 0)
        return;
    for (int y = 255; y > end_y; y--)
        chunk->get_block(x, y, z)->set_skylight(15);
    update_light(lightupdate_t(vec3i(x, end_y, z), chunk->get_block(x, end_y, z)->get_blocklight(), 14));
}

void update_light(lightupdate_t lu)
{
    if (lu.pos.y > 255 || lu.pos.y < 0)
        return;
    chunk_t *chunk = get_chunk_from_pos(lu.pos.x, lu.pos.z, false);
    if (!chunk)
        return;
    LWP_MutexLock(__light_update_mutex);
    for (lightupdate_t &update : pending_light_updates)
    {
        if (update.pos == lu.pos)
        {
            update.block = lu.block;
            update.sky = lu.sky;
            LWP_MutexUnlock(__light_update_mutex);
            return;
        }
    }
    ++chunk->light_updates;
    pending_light_updates.push_back(lu);
    LWP_MutexUnlock(__light_update_mutex);
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
    static std::list<vec3i> light_updates;

    light_updates.push_back(coords);

    while (light_updates.size() > 0)
    {
        vec3i pos = light_updates.back();
        light_updates.pop_back();

        if (pos.y > 255 || pos.y < 0)
            continue;
        chunk_t *chunk = get_chunk_from_pos(pos.x, pos.z, false);
        if (!chunk)
            continue;
        int map_index = (pos.x & 15) * 16 + (pos.z & 15);

        block_t *block = chunk->get_block(pos.x, pos.y, pos.z);

        int new_skylight = 0;
        int new_blocklight = 0;
        int old_light = block->light;

        if (pos.y > chunk->height_map[map_index])
            chunk->height_map[map_index] = skycast(pos.x, pos.z) + 1;
        if (pos.y >= chunk->height_map[map_index])
            new_skylight = 0xF;

        block_t *neighbors[6];
        get_neighbors(pos, neighbors);

        for (int i = 0; i < 6; i++)
        {
            if (!neighbors[i])
                continue;
            new_skylight = std::max(int(neighbors[i]->get_cast_skylight()), new_skylight);
            new_blocklight = std::max(int(neighbors[i]->get_cast_blocklight()), new_blocklight);
        }

        int new_light = (new_skylight << 4) | new_blocklight;

        if (old_light != new_light || coords == pos)
        {
            block->light = uint8_t(new_light & 0xFF);
            for (int i = 0; i < 6; i++)
            {
                if (neighbors[i])
                    light_updates.push_back(pos + face_offsets[i]);
            }
            chunk->vbos[pos.y / 16].dirty = true;
        }
    }
}