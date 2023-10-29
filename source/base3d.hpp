#ifndef _BASE3D_HPP_
#define _BASE3D_HPP_
#include <cstdint>
#include "vec3f.hpp"

class vertex_property_t
{
public:
    vec3f pos = vec3f(0, 0, 0);
    uint32_t x_uv = 0;
    uint32_t y_uv = 0;
    uint8_t color_r = 255;
    uint8_t color_g = 255;
    uint8_t color_b = 255;
    uint8_t index = 0;
    vertex_property_t(int x) : pos(x), x_uv(x), y_uv(x) {}
    vertex_property_t(vec3f pos = vec3f(0, 0, 0), uint32_t x_uv = 0, uint32_t y_uv = 0, uint8_t color_r = 255, uint8_t color_g = 255, uint8_t color_b = 255, uint8_t index = 0) : pos(pos), x_uv(x_uv), y_uv(y_uv), color_r(color_r), color_g(color_g), color_b(color_b), index(index) {}
};

extern bool base3d_is_drawing;
void GX_BeginGroup(uint8_t primitive, uint16_t vtxcount = 0);
uint16_t GX_EndGroup();

void GX_Vertex(vertex_property_t vert);
void GX_VertexLit(vertex_property_t vert, float light);

#endif