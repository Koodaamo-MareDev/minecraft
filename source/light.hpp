#ifndef _LIGHT_HPP_
#define _LIGHT_HPP_

#include "vec3i.hpp"
#include "chunk_new.hpp"
#include <cstdint>
#include <ogc/gu.h>

extern guVector player_pos;
bool light_engine_busy();
void light_engine_init();
void light_engine_deinit();
void light_engine_loop();
void light_engine_update();
void light_engine_reset();
void update_light(vec3i pos, chunk_t* chunk);
void set_skylight_enabled(bool enabled);

#endif