#ifndef _LIGHT_HPP_
#define _LIGHT_HPP_

#include "vec3i.hpp"
#include "chunk_new.hpp"
#include <cstdint>

struct lightupdate_t
{
    vec3i pos{0};
    uint8_t block;
    uint8_t sky;
    chunk_t* chunk = nullptr;
};


bool light_engine_busy();
void light_engine_init();
void light_engine_deinit();
void light_engine_loop();
void light_engine_update();

void cast_skylight(int x, int z);
void update_light(lightupdate_t lu);

#endif