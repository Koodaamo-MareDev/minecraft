#ifndef _RAYCAST_H_
#define _RAYCAST_H_

#include "vec3d.hpp"
#include "vec3i.hpp"
int checkabove(int x, int y, int z);
int skycast(int x, int z);
bool raycast(
    vec3d origin,
    vec3d direction,
    float dst,
    vec3i* output,
    vec3i* output_face);
#endif