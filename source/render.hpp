#ifndef _RENDER_HPP_
#define _RENDER_HPP_
#include "vec3i.hpp"
#include "vec3f.hpp"
#include <cstdint>

float get_face_light(vec3i pos, uint8_t face);
int render_face(vec3i pos, uint8_t face, uint32_t texture_index);

#endif