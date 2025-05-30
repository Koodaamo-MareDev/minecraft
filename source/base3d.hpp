#ifndef _BASE3D_HPP_
#define _BASE3D_HPP_
#include <cstdint>
#include <cmath>
#include <ogc/gx.h>
#include <math/vec3f.hpp>
#include "block.hpp"
#include "gertex/gertex.hpp"

#define BASE3D_POS_FRAC_BITS 5
#define BASE3D_NRM_FRAC_BITS 4
#define BASE3D_UV_FRAC_BITS 8
#define BASE3D_UV_FRAC_BITS_HI (BASE3D_UV_FRAC_BITS - 8)
#define BASE3D_COLOR_BYTES 1

#define BASE3D_POS_FRAC (1 << BASE3D_POS_FRAC_BITS)
#define BASE3D_UV_FRAC (1 << BASE3D_UV_FRAC_BITS)
#define BASE3D_UV_FRAC_LO (1 << BASE3D_UV_FRAC_BITS_HI)

#define UV_SCALE ((16) << BASE3D_UV_FRAC_BITS_HI)
#define TEXTURE_X(x) (UV_SCALE * (x & 15))
#define TEXTURE_Y(y) (UV_SCALE * ((y >> 4) & 15))

#define TEXTURE_NX(x) (TEXTURE_X(x))
#define TEXTURE_NY(y) (TEXTURE_Y(y))
#define TEXTURE_PX(x) (TEXTURE_X(x) + UV_SCALE)
#define TEXTURE_PY(y) (TEXTURE_Y(y) + UV_SCALE)

#define VERTEX_ATTR_LENGTH (3 * sizeof(int16_t) + 1 * sizeof(uint8_t) + 1 * sizeof(uint8_t) + 2 * sizeof(float_t))
#define VERTEX_ATTR_LENGTH_FLOATPOS (3 * sizeof(float_t) + 1 * sizeof(uint8_t) + 1 * sizeof(uint8_t) + 2 * sizeof(float_t))
#define VERTEX_ATTR_LENGTH_DIRECTCOLOR (3 * sizeof(int16_t) + 1 * sizeof(uint8_t) + 4 * sizeof(uint8_t) + 2 * sizeof(float_t))

class vertex_property_t
{
public:
    vec3f pos = vec3f(0, 0, 0);
    vfloat_t x_uv = 0;
    vfloat_t y_uv = 0;
    uint8_t color_r = 255;
    uint8_t color_g = 255;
    uint8_t color_b = 255;
    uint8_t color_a = 255;
    uint8_t index = 0;
    vertex_property_t(int x) : pos(x), x_uv(x), y_uv(x) {}
    vertex_property_t(vec3f pos = vec3f(0, 0, 0), uint32_t x_uv = 0, uint32_t y_uv = 0, uint8_t color_r = 255, uint8_t color_g = 255, uint8_t color_b = 255, uint8_t color_a = 255, uint8_t index = 0) : pos(pos), x_uv(x_uv), y_uv(y_uv), color_r(color_r), color_g(color_g), color_b(color_b), color_a(color_a), index(index) {}
};
class vertex_property16_t
{
public:
    vec3i pos = vec3i(0, 0, 0);
    uint32_t x_uv = 0;
    uint32_t y_uv = 0;
    uint8_t color_r = 255;
    uint8_t color_g = 255;
    uint8_t color_b = 255;
    uint8_t color_a = 255;
    uint8_t index = 0;
    vertex_property16_t(int x) : pos(x), x_uv(x), y_uv(x) {}
    vertex_property16_t(vec3i pos = vec3i(0, 0, 0), uint32_t x_uv = 0, uint32_t y_uv = 0, uint8_t color_r = 255, uint8_t color_g = 255, uint8_t color_b = 255, uint8_t color_a = 255, uint8_t index = 0) : pos(pos), x_uv(x_uv), y_uv(y_uv), color_r(color_r), color_g(color_g), color_b(color_b), color_a(color_a), index(index) {}
};

#define MAKEVEC(V) (guVector{std::sqrt(1 - ((V) * (V))), -(V), 0})

extern float face_normals[];
extern bool base3d_is_drawing;
extern uint16_t __group_vtxcount;
inline void init_face_normals()
{
    constexpr float normal_scale = 1.0f;
    float multipliers[] = {0.8f, 0.8f, 0.5f, 1.0f, 0.6f, 0.6f};
    float *elem = face_normals;
    for (int i = 0; i < 24; i++)
    {
        float multiplier = (multipliers[i % 6] * (1.f - (0.21f * int(i / 6)))) * normal_scale;
        guVector vec = MAKEVEC(multiplier);
        *(elem++) = vec.x;
        *(elem++) = vec.y;
        *(elem++) = vec.z;
    }
    GX_SetArray(GX_VA_NRM, face_normals, 3 * sizeof(float));
}

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

inline void GX_Vertex(const vertex_property_t &vert, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    constexpr float uv_invscale = 1. / BASE3D_UV_FRAC;
    GX_Position3s16(BASE3D_POS_FRAC * vert.pos.x, BASE3D_POS_FRAC * vert.pos.y, BASE3D_POS_FRAC * vert.pos.z);
    GX_Normal1x8(face);
    GX_Color4u8(vert.color_r, vert.color_g, vert.color_b, vert.color_a);
    GX_TexCoord2f32(uv_invscale * vert.x_uv, uv_invscale * vert.y_uv);
}

inline void GX_VertexLit(const vertex_property_t &vert, uint8_t light, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    constexpr vfloat_t uv_invscale = 1. / BASE3D_UV_FRAC;
    GX_Position3s16(BASE3D_POS_FRAC * vert.pos.x, BASE3D_POS_FRAC * vert.pos.y, BASE3D_POS_FRAC * vert.pos.z);
    GX_Normal1x8(face);
    GX_Color1x8(light);
    GX_TexCoord2f32(uv_invscale * vert.x_uv, uv_invscale * vert.y_uv);
}

inline void GX_Vertex16(const vertex_property16_t &vert, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    constexpr vfloat_t uv_invscale = 1. / BASE3D_UV_FRAC;
    GX_Position3s16(vert.pos.x, vert.pos.y, vert.pos.z);
    GX_Normal1x8(face);
    GX_Color4u8(vert.color_r, vert.color_g, vert.color_b, vert.color_a);
    GX_TexCoord2f32(uv_invscale * vert.x_uv, uv_invscale * vert.y_uv);
}

inline void GX_VertexLit16(const vertex_property16_t &vert, uint8_t light, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    constexpr float uv_invscale = 1. / BASE3D_UV_FRAC;
    GX_Position3s16(vert.pos.x, vert.pos.y, vert.pos.z);
    GX_Normal1x8(face);
    GX_Color1x8(light);
    GX_TexCoord2f32(uv_invscale * vert.x_uv, uv_invscale * vert.y_uv);
}

inline void GX_VertexF(const vertex_property_t &vert, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    constexpr float uv_invscale = 1. / BASE3D_UV_FRAC;
    GX_Position3f32(vert.pos.x, vert.pos.y, vert.pos.z);
    GX_Normal1x8(face);
    GX_Color4u8(vert.color_r, vert.color_g, vert.color_b, vert.color_a);
    GX_TexCoord2f32(uv_invscale * vert.x_uv, uv_invscale * vert.y_uv);
}

inline void GX_VertexLitF(const vertex_property_t &vert, uint8_t light, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    constexpr float uv_invscale = 1. / BASE3D_UV_FRAC;
    GX_Position3f32(vert.pos.x, vert.pos.y, vert.pos.z);
    GX_Normal1x8(face);
    GX_Color1x8(light);
    GX_TexCoord2f32(uv_invscale * vert.x_uv, uv_invscale * vert.y_uv);
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
inline void GX_VertexT(const vertex_property_t &vert, const gertex::GXMatrix &mtx, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    guVector vec = vert.pos;
    guVecMultiply(mtx.mtx, &vec, &vec);

    constexpr float uv_invscale = 1. / BASE3D_UV_FRAC;
    GX_Position3f32(vec.x, vec.y, vec.z);
    GX_Normal1x8(face);
    GX_Color4u8(vert.color_r, vert.color_g, vert.color_b, vert.color_a);
    GX_TexCoord2f32(uv_invscale * vert.x_uv, uv_invscale * vert.y_uv);
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
inline void GX_VertexLitT(const vertex_property_t &vert, const gertex::GXMatrix &mtx, uint8_t light, uint8_t face = 3)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    guVector vec = vert.pos;
    guVecMultiply(mtx.mtx, &vec, &vec);

    constexpr float uv_invscale = 1. / BASE3D_UV_FRAC;
    GX_Position3f32(vec.x, vec.y, vec.z);
    GX_Normal1x8(face);
    GX_Color1x8(light);
    GX_TexCoord2f32(uv_invscale * vert.x_uv, uv_invscale * vert.y_uv);
}
#endif