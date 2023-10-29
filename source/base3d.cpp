#include "base3d.hpp"

bool base3d_is_drawing = false;

#ifdef __wii__

#include "ogc/gx.h"

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

void GX_Vertex(vertex_property_t vert)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    GX_Position3f32(vert.pos.x, vert.pos.y, vert.pos.z);
    GX_Color3u8(vert.color_r, vert.color_g, vert.color_b);
    GX_TexCoord2u8(vert.x_uv, vert.y_uv);
}

void GX_VertexLit(vertex_property_t vert, float light)
{
    ++__group_vtxcount;
    if (!base3d_is_drawing)
        return;
    GX_Position3f32(vert.pos.x, vert.pos.y, vert.pos.z);
    GX_Color3f32(light, light, light);
    GX_TexCoord2u8(vert.x_uv, vert.y_uv);
}
#else // TODO: Add PC alternatives here.

#endif