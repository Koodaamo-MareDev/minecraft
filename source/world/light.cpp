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
#include <deque>
#include <algorithm>
#include <list>
#include <cstdio>
#include <set>

lwp_t LightEngine::thread_handle = 0;
bool LightEngine::thread_active = false;
bool LightEngine::use_skylight = true;
World *LightEngine::current_world = nullptr;
std::deque<Coord> LightEngine::pending_updates;

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
        Coord current = pending_updates.front();
        pending_updates.pop_front();
        Chunk *chunk = current.chunk;
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
void LightEngine::deinit()
{
    if (thread_active)
    {
        thread_active = false;
        pending_updates.clear();
        LWP_JoinThread(thread_handle, NULL);
    }
}

void LightEngine::post(const Coord &location)
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

void LightEngine::update(const Coord &update)
{
    std::set<Section *> affected_sections;
    std::deque<Coord> light_updates;
    std::deque<Chunk *> &chunks = current_world->chunks;
    Coord origin = update;
    affected_sections.insert(&origin.chunk->sections[(origin.coords.y >> 4) & 15]);
    light_updates.push_back(origin);
    while (light_updates.size() > 0)
    {
        if (!thread_active)
            return;
        Coord current = light_updates.back();
        light_updates.pop_back();

        Chunk *chunk = current.chunk;
        if (std::find(chunks.begin(), chunks.end(), chunk) == chunks.end()) // FIXME: This is a hacky way
            return;                                                         // to check if the chunk is valid

        if (!chunk || current.coords.y < 0)
            continue;

        Block *block = &chunk->blockstates[uint16_t(current)];

        uint8_t new_skylight = 0;
        uint8_t new_blocklight = properties(block->id).m_luminance;

        if (use_skylight && current.coords.y >= chunk->height_map[current.coords.x | (current.coords.z << 4)])
            new_skylight = 0xF;

        Block *neighbors[6];
        current_world->get_neighbors(Vec3i(current), neighbors);
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
                    Vec3i new_pos = Vec3i(current) + face_offsets[i];
                    Chunk *coord_chunk = chunk;

                    // If the new position is outside the chunk, get the correct chunk from the position
                    if (chunk->x != (new_pos.x >> 4) || chunk->z != (new_pos.z >> 4))
                    {
                        coord_chunk = current_world->get_chunk_from_pos(new_pos);
                    }
                    if (!coord_chunk)
                        continue;
                    light_updates.push_back(Coord(new_pos, coord_chunk));
                    affected_sections.insert(&coord_chunk->sections[(new_pos.y >> 4) & 15]);
                }
            }
        }
    }
    while (!affected_sections.empty())
    {
        auto first = affected_sections.begin();
        Section *sect = *first;
        if (sect && sect->chunk && sect->chunk->state != ChunkState::invalid)
            (*first)->dirty = true;
        affected_sections.erase(first);
    }
}