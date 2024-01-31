#include "render_gui.hpp"

int draw_textured_quad(GXTexObj &texture, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t u1, uint32_t v1, uint32_t u2, uint32_t v2)
{
    static float global_scale = 1.0f / BASE3D_POS_FRAC;
    use_texture(texture);
    float scale_x = float(BASE3D_UV_FRAC) / GX_GetTexObjWidth(&texture);
    float scale_y = float(BASE3D_UV_FRAC) / GX_GetTexObjHeight(&texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(vertex_property_t(global_scale * vec3f(x, y, 0), (scale_x * u1), (scale_y * v1)));
    GX_Vertex(vertex_property_t(global_scale * vec3f(x + w - 1, y, 0), (scale_x * u2), (scale_y * v1)));
    GX_Vertex(vertex_property_t(global_scale * vec3f(x + w - 1, y + h - 1, 0), (scale_x * u2), (scale_y * v2)));
    GX_Vertex(vertex_property_t(global_scale * vec3f(x, y + h - 1, 0), (scale_x * u1), (scale_y * v2)));
    GX_EndGroup();
    return 4;
}

int draw_simple_textured_quad(GXTexObj &texture, int32_t x, int32_t y, int32_t w, int32_t h)
{
    return draw_textured_quad(texture, x, y, w, h, 0, 0, GX_GetTexObjWidth(&texture), GX_GetTexObjHeight(&texture));
}