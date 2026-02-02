#ifndef _LIGHT_HPP_
#define _LIGHT_HPP_

#include <math/vec3i.hpp>
#include <world/chunk.hpp>
#include <world/util/coord.hpp>
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
    static void update_optimized(const Vec3i &start);
    static void init(World *world);
    static void deinit();
    static void loop();
    static void reset();
    static void post(const Vec3i &location);
    static void enable_skylight(bool enabled);
    static bool busy();
};
enum LightType
{
    BLOCK,
    SKY
};

struct LightNode
{
    int x, y, z;
    uint8_t level;
    LightType type;
};

struct ChunkCache
{
    Chunk *chunks[3][3]; // [dx+1][dz+1]
    int base_cx;
    int base_cz;
};

// namespace light_engine
#endif