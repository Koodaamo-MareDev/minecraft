#ifndef _LIGHT_HPP_
#define _LIGHT_HPP_

#include <math/vec3i.hpp>
#include "chunk.hpp"
#include <cstdint>
#include <ogc/gu.h>
class light_engine
{
private:
    static void update(const std::pair<vec3i, chunk_t *> &update);

    static lwp_t thread_handle;
    static bool thread_active;
    static bool use_skylight;
    static std::deque<std::pair<vec3i, chunk_t *>> pending_updates;

public:
    static void init();
    static void deinit();
    static void loop();
    static void reset();
    static void post(vec3i pos, chunk_t *chunk);
    static void enable_skylight(bool enabled);
    static bool busy();
};
// namespace light_engine
#endif