#ifndef _RAYCAST_H_
#define _RAYCAST_H_

#include "vec3d.hpp"
#include "vec3i.hpp"
#include "chunk_new.hpp"
int checkabove(vec3i pos, chunk_t* chunk = nullptr);
int skycast(vec3i pos, chunk_t* chunk = nullptr);
bool raycast(
    vec3d origin,
    vec3d direction,
    float dst,
    vec3i* output,
    vec3i* output_face);
#endif