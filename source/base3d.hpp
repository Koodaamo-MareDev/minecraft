#ifndef _BASE3D_HPP_
#define _BASE3D_HPP_
#include <cstdint>
#include <cmath>
#include "vec3f.hpp"
#include "block.hpp"
#include "ogc/gx.h"

#define BASE3D_POS_FRAC_BITS 5
#define BASE3D_NRM_FRAC_BITS 4
#define BASE3D_UV_FRAC_BITS 8
#define BASE3D_COLOR_BYTES 1

#define BASE3D_POS_FRAC (1 << BASE3D_POS_FRAC_BITS)
#define BASE3D_UV_FRAC (1 << BASE3D_UV_FRAC_BITS)

class vertex_property_t
{
public:
    vec3f pos = vec3f(0, 0, 0);
    uint32_t x_uv = 0;
    uint32_t y_uv = 0;
    uint8_t color_r = 255;
    uint8_t color_g = 255;
    uint8_t color_b = 255;
    uint8_t color_a = 255;
    uint8_t index = 0;
    vertex_property_t(int x) : pos(x), x_uv(x), y_uv(x) {}
    vertex_property_t(vec3f pos = vec3f(0, 0, 0), uint32_t x_uv = 0, uint32_t y_uv = 0, uint8_t color_r = 255, uint8_t color_g = 255, uint8_t color_b = 255, uint8_t color_a = 255, uint8_t index = 0) : pos(pos), x_uv(x_uv), y_uv(y_uv), color_r(color_r), color_g(color_g), color_b(color_b), color_a(color_a), index(index) {}
};

#define MAKEVEC(V) (guVector{std::sqrt(1-(V*V)), -V, 0})

extern float face_normals[];
extern bool base3d_is_drawing;
void GX_BeginGroup(uint8_t primitive, uint16_t vtxcount = 0);
uint16_t GX_EndGroup();
inline void init_face_normals()
{
    guVector vecs[] = {
        MAKEVEC(0.8f),
        MAKEVEC(0.8f),
        MAKEVEC(0.5f),
        MAKEVEC(1.0f),
        MAKEVEC(0.6f),
        MAKEVEC(0.6f)};
    guVector *vec = vecs;
    float* elem = face_normals;
    for (int i = 0; i < 6; i++, vec++)
    {
        //guVecNormalize(vec);
        *(elem++) = vec->x;
        *(elem++) = vec->y;
        *(elem++) = vec->z;
    }
    GX_SetArray(GX_VA_NRM, face_normals, 3*sizeof(float));
}
void GX_Vertex(vertex_property_t vert, uint8_t face = FACE_PY);
void GX_VertexLit(vertex_property_t vert, uint8_t light, uint8_t face = FACE_PY);

#endif