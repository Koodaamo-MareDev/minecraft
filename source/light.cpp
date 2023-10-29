#include "light.hpp"
#include "chunk_new.hpp"
#include "vec3i.hpp"
#include "blocks.hpp"
#include "block.hpp"
#include <sys/unistd.h>
#include <gccore.h>
#include <vector>
#include <queue>
#include <cstdio>
static lwp_t __light_engine_thread_handle = (lwp_t)NULL;
static bool __light_engine_init_done = false;
static bool __light_engine_busy = false;
static bool __light_engine_requested = false;

std::queue<lightupdate_t> pending_light_updates;

void __update_light(vec3i coords, uint8_t light);
void __propagate_light(vec3i coords);
void *__light_engine_init_internal(void *);

void light_engine_init()
{
    if (__light_engine_init_done)
        return;
    __light_engine_init_done = true;
    LWP_CreateThread(&__light_engine_thread_handle, /* thread handle */
                     __light_engine_init_internal,  /* code */
                     NULL,                          /* arg pointer for thread */
                     NULL,                          /* stack base */
                     128 * 1024,                    /* stack size */
                     50 /* thread priority */);
}

void *__light_engine_init_internal(void *)
{
    while (__light_engine_init_done)
    {
        if (__light_engine_requested)
            light_engine_loop();
        LWP_YieldThread();
    }
    return NULL;
}

bool light_engine_busy()
{
    return __light_engine_busy;
}

void light_engine_update()
{
    if (__light_engine_init_done && pending_light_updates.size() > 0)
        __light_engine_requested = true;
}

void light_engine_loop()
{
    while (pending_light_updates.size() > 0 && __light_engine_init_done && __light_engine_requested)
    {
        __light_engine_busy = true;
        lightupdate_t update = pending_light_updates.front();
        __update_light(update.pos, update.block);
        cast_skylight(update.pos.x, update.pos.z);
        pending_light_updates.pop();
    }
    __light_engine_busy = false;
    __light_engine_requested = false;
}
void light_engine_deinit()
{
    if (__light_engine_init_done)
    {
        __light_engine_init_done = false;
        __light_engine_requested = false;

        LWP_JoinThread(__light_engine_thread_handle, NULL);
    }
}

void cast_skylight(int x, int z)
{
    chunk_t *chunk = get_chunk_from_pos(x, z, false);
    if (!chunk)
        return;
    for (int y = 255; y >= 0; y--)
    {
        block_t *block = chunk->get_block(x, y, z);
        block->set_blocklight(15);
        if (get_block_opacity(block->get_blockid()) > 0)
        {
            __update_light(vec3i(x, y, z), 0);
            break;
        }
    }
}

void update_light(lightupdate_t lu)
{
    if (lu.pos.y > 255 || lu.pos.y < -1)
        return;
    chunk_t *chunk = get_chunk_from_pos(lu.pos.x, lu.pos.z, false);
    if (!chunk)
        return;
    pending_light_updates.push(lu);
}

void __update_light(vec3i coords, uint8_t light)
{
    if (coords.y > 255 || coords.y < 0)
        return;
    chunk_t *chunk = get_chunk_from_pos(coords.x, coords.z, false);
    if (!chunk)
        return;
    if ((light & 0xF) != light)
        printf("Invalid light value %d\n", light);
    block_t *block = chunk->get_block(coords.x, coords.y, coords.z);
    if (!block)
        return;

    int old_light = block->get_blocklight();
    int old_cast_light = block->get_castlight();
    chunk->vbos[coords.y / 16].dirty = true;

    if (old_light != light)
    {
        block->set_blocklight(light);
        if (light < old_light)
        {
            std::vector<vec3i> neighbors_to_darken;
            for (int p = 0; p < 6; p++)
            {
                vec3i neighbor = coords + face_offsets[p];
                block_t *neighbor_block = get_block_at(neighbor);
                if (!neighbor_block)
                    continue;
                int neighbor_light = neighbor_block->get_blocklight();
                if (neighbor_light <= old_cast_light)
                    neighbors_to_darken.push_back(neighbor);
                else /*if(!get_block_opacity(neighbor_block->get_blockid()))*/
                {
                    __propagate_light(neighbor);
                }
            }
            for (size_t i = 0; i < neighbors_to_darken.size(); i++)
            {
                vec3i neighbor = neighbors_to_darken[i];
                // block_t* neighbor_block = get_block_at(neighbor.x, neighbor.y, neighbor.z);
                // int neighbor_light = neighbor_block->get_blocklight();
                __update_light(neighbor, 0);
            }
        }
        else if (light > 1)
        {
            __propagate_light(coords);
        }
    }
}

void __propagate_light(vec3i coords)
{
    block_t *block = get_block_at(coords);
    if (!block)
        return;
    int light = block->get_blocklight();
    if (light < 2)
        return;

    for (int p = 0; p < 6; p++)
    {
        vec3i neighbor = coords + face_offsets[p];
        block_t *neighbor_block = get_block_at(neighbor);
        if (!neighbor_block)
            continue;
        int neighbor_light = neighbor_block->get_blocklight();
        if (neighbor_light < block->get_castlight())
            __update_light(neighbor, block->get_castlight());
    }
}
