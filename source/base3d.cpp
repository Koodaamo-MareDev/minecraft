#include "base3d.hpp"
bool base3d_is_drawing = false;

#ifdef __wii__

#include "ogc/gx.h"

float face_normals[6 * 3];

uint16_t __group_vtxcount;

void GX_BeginGroup(uint8_t primitive, uint16_t vtxcount)
{
    __group_vtxcount = 0;
    base3d_is_drawing = vtxcount;
    if (base3d_is_drawing)
        GX_Begin(primitive, GX_VTXFMT0, vtxcount);
}

uint16_t GX_EndGroup()
{
    GX_End();
    base3d_is_drawing = false;
    return __group_vtxcount;
}

void GX_Vertex(vertex_property_t vert, uint8_t face)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    GX_Position3s16(BASE3D_POS_FRAC * vert.pos.x, BASE3D_POS_FRAC * vert.pos.y, BASE3D_POS_FRAC * vert.pos.z);
    GX_Normal1x8(face);
    GX_Color4u8(vert.color_r, vert.color_g, vert.color_b, vert.color_a);
    GX_TexCoord2u16(vert.x_uv, vert.y_uv);
}

void GX_VertexLit(vertex_property_t vert, uint8_t light, uint8_t face)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    GX_Position3s16(BASE3D_POS_FRAC * vert.pos.x, BASE3D_POS_FRAC * vert.pos.y, BASE3D_POS_FRAC * vert.pos.z);
    GX_Normal1x8(face);
    GX_Color1x8(light);
    GX_TexCoord2u16(vert.x_uv, vert.y_uv);
}
#else // TODO: Add PC alternatives here.

#endif