#include "light.hpp"
#include "chunk_new.hpp"
#include "vec3i.hpp"
#include "blocks.hpp"
#include "block.hpp"
#include <gccore.h>
#include <vector>
#include <queue>
#include <cstdio>
static lwp_t __light_engine_thread_handle = (lwp_t)NULL;
static bool __light_engine_init_done = false;

std::queue<lightupdate_t> pending_light_updates;

void __update_light(vec3i coords, uint8_t light, chunk_t *chunk = nullptr);
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
        light_engine_loop();
        LWP_SuspendThread(__light_engine_thread_handle);
    }
    return NULL;
}

bool light_engine_busy()
{
    return !LWP_ThreadIsSuspended(__light_engine_thread_handle);
}

void light_engine_update()
{
    if (__light_engine_init_done && pending_light_updates.size() > 0)
        LWP_ResumeThread(__light_engine_thread_handle);
}

void light_engine_loop()
{
    while (pending_light_updates.size() > 0)
    {
        lightupdate_t update = pending_light_updates.front();
        __update_light(update.pos, update.block);
        pending_light_updates.pop();
    }
}
void light_engine_deinit()
{
    if (__light_engine_init_done)
    {
        __light_engine_init_done = false;
        LWP_ResumeThread(__light_engine_thread_handle);
        LWP_JoinThread(__light_engine_thread_handle, NULL);
    }
}

void cast_skylight(int x, int z)
{
    chunk_t *chunk = get_chunk(x >> 4, z >> 4, false);
    if (!chunk)
        return;
    lock_chunk_cache(chunk);
    // bool obstructed = false;
    for (int y = 255; y >= 0; y--)
    {
        block_t *block = chunk->get_block(x, y, z);
        block->set_blocklight(15);
        if (get_block_opacity(block->get_blockid()) > 0)
        {
            update_light({vec3i{x, y, z}, 0});
            break;
        }
    }
    unlock_chunk_cache();
}

void update_light(lightupdate_t lu)
{
    if(lu.pos.y > 255 || lu.pos.y < 0)
        return;
    chunk_t *chunk = lu.chunk ? lu.chunk : get_chunk(lu.pos.x >> 4, lu.pos.z >> 4, false);
    if (!chunk)
        return;
    pending_light_updates.push(lu);
}

void __update_light(vec3i coords, uint8_t light, chunk_t *chunk)
{
    LWP_YieldThread();
    chunk = chunk ? chunk : get_chunk(coords.x >> 4, coords.z >> 4, false);
    if (!chunk)
        return;
    if ((light & 0xF) != light)
        printf("Invalid light value %d\n", light);
    block_t *block = chunk->get_block(coords.x, coords.y, coords.z);
    if (!block)
        return;

    int old_light = block->get_blocklight();
    int old_cast_light = block->get_castlight();
    chunk->vbos[coords.y >> 4].dirty = true;

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
