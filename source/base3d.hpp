#ifndef _BASE3D_HPP_
#define _BASE3D_HPP_
#include <cstdint>
#include <cmath>
#include <ogc/gx.h>
#include <algorithm>
#include <math/vec3f.hpp>
#include "block.hpp"
#include "gertex/gertex.hpp"

constexpr uint8_t BASE3D_POS_FRAC_BITS = 5;
constexpr uint8_t BASE3D_NRM_FRAC_BITS = 4;
constexpr uint8_t BASE3D_UV_FRAC_BITS = 8;

constexpr uint32_t BASE3D_POS_FRAC = (1 << BASE3D_POS_FRAC_BITS);
constexpr uint32_t BASE3D_UV_FRAC = (1 << BASE3D_UV_FRAC_BITS);
constexpr vfloat_t BASE3D_PIXEL_UV_SCALE = (1. / BASE3D_UV_FRAC);
constexpr vfloat_t BASE3D_BLOCK_UV_SCALE = (BASE3D_PIXEL_UV_SCALE * 16.);
constexpr uint16_t PARTICLE_X(uint16_t x) { return (x & 15) << 4; }
constexpr uint16_t PARTICLE_Y(uint16_t y) { return ((y >> 4) & 15) << 4; }

constexpr vfloat_t TEXTURE_X(uint32_t x) { return BASE3D_BLOCK_UV_SCALE * (x & 15); }
constexpr vfloat_t TEXTURE_Y(uint32_t y) { return BASE3D_BLOCK_UV_SCALE * ((y >> 4) & 15); }
constexpr vfloat_t TEXTURE_NX(uint32_t x) { return TEXTURE_X(x); }
constexpr vfloat_t TEXTURE_NY(uint32_t y) { return TEXTURE_Y(y); }
constexpr vfloat_t TEXTURE_PX(uint32_t x) { return TEXTURE_X(x) + BASE3D_BLOCK_UV_SCALE; }
constexpr vfloat_t TEXTURE_PY(uint32_t y) { return TEXTURE_Y(y) + BASE3D_BLOCK_UV_SCALE; }

constexpr uint8_t BASE3D_COLOR_BYTES = 1;

constexpr uint32_t VERTEX_ATTR_LENGTH = (3 * sizeof(int16_t) + 1 * sizeof(uint8_t) + 1 * sizeof(uint8_t) + 2 * sizeof(float));
constexpr uint32_t VERTEX_ATTR_LENGTH_FLOATPOS = (3 * sizeof(float) + 1 * sizeof(uint8_t) + 1 * sizeof(uint8_t) + 2 * sizeof(float));
constexpr uint32_t VERTEX_ATTR_LENGTH_DIRECTCOLOR = (3 * sizeof(int16_t) + 1 * sizeof(uint8_t) + 4 * sizeof(uint8_t) + 2 * sizeof(float));

class Vertex
{
public:
    Vec3f pos = Vec3f(0, 0, 0);
    vfloat_t x_uv = 0;
    vfloat_t y_uv = 0;
    uint8_t color_r = 255;
    uint8_t color_g = 255;
    uint8_t color_b = 255;
    uint8_t color_a = 255;
    uint8_t index = 0;
    Vertex(int x) : pos(x), x_uv(x), y_uv(x) {}
    Vertex(Vec3f pos = Vec3f(0, 0, 0), vfloat_t x_uv = 0, vfloat_t y_uv = 0, uint8_t color_r = 255, uint8_t color_g = 255, uint8_t color_b = 255, uint8_t color_a = 255, uint8_t index = 0) : pos(pos), x_uv(x_uv), y_uv(y_uv), color_r(color_r), color_g(color_g), color_b(color_b), color_a(color_a), index(index) {}
};
class Vertex16
{
public:
    Vec3i pos = Vec3i(0, 0, 0);
    vfloat_t x_uv = 0;
    vfloat_t y_uv = 0;
    uint8_t color_r = 255;
    uint8_t color_g = 255;
    uint8_t color_b = 255;
    uint8_t color_a = 255;
    uint8_t index = 0;
    Vertex16(int x) : pos(x), x_uv(x), y_uv(x) {}
    Vertex16(Vec3i pos = Vec3i(0, 0, 0), vfloat_t x_uv = 0, vfloat_t y_uv = 0, uint8_t color_r = 255, uint8_t color_g = 255, uint8_t color_b = 255, uint8_t color_a = 255, uint8_t index = 0) : pos(pos), x_uv(x_uv), y_uv(y_uv), color_r(color_r), color_g(color_g), color_b(color_b), color_a(color_a), index(index) {}
};

extern bool base3d_is_drawing;
extern uint16_t __group_vtxcount;
void init_face_normals();

inline void GX_BeginGroup(uint8_t primitive, uint16_t vtxcount = 0)
{
    __group_vtxcount = 0;
    base3d_is_drawing = vtxcount;
    if (base3d_is_drawing)
        GX_Begin(primitive, GX_VTXFMT0, vtxcount);
}

inline uint16_t GX_EndGroup()
{
    GX_End();
    base3d_is_drawing = false;
    return __group_vtxcount;
}

inline void GX_Vertex(const Vertex &vert, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    GX_Position3s16(BASE3D_POS_FRAC * vert.pos.x, BASE3D_POS_FRAC * vert.pos.y, BASE3D_POS_FRAC * vert.pos.z);
    GX_Color4u8(vert.color_r, vert.color_g, vert.color_b, vert.color_a);
    GX_Color1x8(face);
    GX_TexCoord2f32(vert.x_uv, vert.y_uv);
}

inline void GX_VertexLit(const Vertex &vert, uint8_t light, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    GX_Position3s16(BASE3D_POS_FRAC * vert.pos.x, BASE3D_POS_FRAC * vert.pos.y, BASE3D_POS_FRAC * vert.pos.z);
    GX_Color1x8(light);
    GX_Color1x8(face);
    GX_TexCoord2f32(vert.x_uv, vert.y_uv);
}

inline void GX_Vertex16(const Vertex16 &vert, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    GX_Position3s16(vert.pos.x, vert.pos.y, vert.pos.z);
    GX_Color4u8(vert.color_r, vert.color_g, vert.color_b, vert.color_a);
    GX_Color1x8(face);
    GX_TexCoord2f32(vert.x_uv, vert.y_uv);
}

inline void GX_VertexLit16(const Vertex16 &vert, uint8_t light, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    GX_Position3s16(vert.pos.x, vert.pos.y, vert.pos.z);
    GX_Color1x8(light);
    GX_Color1x8(face);
    GX_TexCoord2f32(vert.x_uv, vert.y_uv);
}

inline void GX_VertexF(const Vertex &vert, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    GX_Position3f32(vert.pos.x, vert.pos.y, vert.pos.z);
    GX_Color4u8(vert.color_r, vert.color_g, vert.color_b, vert.color_a);
    GX_Color1x8(face);
    GX_TexCoord2f32(vert.x_uv, vert.y_uv);
}

inline void GX_VertexLitF(const Vertex &vert, uint8_t light, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    GX_Position3f32(vert.pos.x, vert.pos.y, vert.pos.z);
    GX_Color1x8(light);
    GX_Color1x8(face);
    GX_TexCoord2f32(vert.x_uv, vert.y_uv);
}
/**
 * @brief Draw a vertex with a transformation matrix
 *
 * @param vert The vertex to draw
 * @param mtx The transformation matrix to apply
 * @param face The index of the face normal to use
 *
 * @note This function is quite slow and should be used only when necessary
 */
inline void GX_VertexT(const Vertex &vert, const gertex::GXMatrix &mtx, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    guVector vec = vert.pos;
    guVecMultiply(mtx.mtx, &vec, &vec);

    GX_Position3f32(vec.x, vec.y, vec.z);
    GX_Color4u8(vert.color_r, vert.color_g, vert.color_b, vert.color_a);
    GX_Color1x8(face);
    GX_TexCoord2f32(vert.x_uv, vert.y_uv);
}

/**
 * @brief Draw a vertex with a transformation matrix
 *
 * @param vert The vertex to draw
 * @param mtx The transformation matrix
 * @param light The light level to use
 * @param face The index of the face normal to use
 *
 * @note This function is quite slow and should be used only when necessary
 */
inline void GX_VertexLitT(const Vertex &vert, const gertex::GXMatrix &mtx, uint8_t light, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    guVector vec = vert.pos;
    guVecMultiply(mtx.mtx, &vec, &vec);

    GX_Position3f32(vec.x, vec.y, vec.z);
    GX_Color1x8(light);
    GX_Color1x8(face);
    GX_TexCoord2f32(vert.x_uv, vert.y_uv);
}
#endif