#ifndef _LIGHT_HPP_
#define _LIGHT_HPP_

#include <math/vec3i.hpp>
#include <world/chunk.hpp>
#include <cstdint>
#include <ogc/gu.h>
class World;
class LightEngine
{
private:
    static void update(const Vec3i &update);

    static lwp_t thread_handle;
    static bool thread_active;
    static bool use_skylight;
    static std::deque<Vec3i> pending_updates;
    static World *current_world;

public:
    static void init(World *world);
    static void deinit();
    static void loop();
    static void reset();
    static void post(const Vec3i &location);
    static void enable_skylight(bool enabled);
    static bool busy();
    static World *world() { return current_world; }
};

class LightUpdateNode
{
public:
    Vec3i location;
    int8_t direction;

    LightUpdateNode(Vec3i location, int8_t direction = -1)
    {
        this->location = location;
        this->direction = direction;
    }
    Chunk *chunk();
    uint8_t lightmap_index();

private:
    Chunk *_chunk = nullptr;
};

class LightRemoveNode
{
public:
    Vec3i location;
    uint8_t level;
    bool is_sky;

    LightRemoveNode(Vec3i loc, uint8_t lvl, bool sky)
        : location(loc), level(lvl), is_sky(sky) {}
};

// namespace light_engine
#endif