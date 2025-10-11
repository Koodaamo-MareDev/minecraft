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
    static void update(const Coord &update);

    static lwp_t thread_handle;
    static bool thread_active;
    static bool use_skylight;
    static std::deque<Coord> pending_updates;
    static World *current_world;

public:
    static void init(World *world);
    static void deinit();
    static void loop();
    static void reset();
    static void post(const Coord &location);
    static void enable_skylight(bool enabled);
    static bool busy();
};
// namespace light_engine
#endif