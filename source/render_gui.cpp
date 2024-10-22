#include "render_gui.hpp"

int draw_textured_quad(GXTexObj &texture, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t u1, uint32_t v1, uint32_t u2, uint32_t v2, float scale)
{
    use_texture(texture);
    float scale_u = float(BASE3D_UV_FRAC) / GX_GetTexObjWidth(&texture);
    float scale_v = float(BASE3D_UV_FRAC) / GX_GetTexObjHeight(&texture);
    u1 *= scale_u;
    u2 *= scale_u;
    v1 *= scale_v;
    v2 *= scale_v;
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(vertex_property_t(scale * vec3f(x, y, 0), u1, v1));
    GX_Vertex(vertex_property_t(scale * vec3f(x + w, y, 0), u2, v1));
    GX_Vertex(vertex_property_t(scale * vec3f(x + w, y + h, 0), u2, v2));
    GX_Vertex(vertex_property_t(scale * vec3f(x, y + h, 0), u1, v2));
    GX_EndGroup();
    return 4;
}

int fill_screen_texture(GXTexObj &texture, view_t &view, int32_t u1, int32_t v1, int32_t u2, int32_t v2, float scale)
{
    use_texture(texture);
    float scale_u = float(BASE3D_UV_FRAC) / GX_GetTexObjWidth(&texture);
    float scale_v = float(BASE3D_UV_FRAC) / GX_GetTexObjHeight(&texture);
    u1 *= scale_u;
    u2 *= scale_u;
    v1 *= scale_v;
    v2 *= scale_v;
    int w = 48;
    int h = 48;
    int cols = std::ceil(view.width / w);
    int rows = std::ceil(view.height / h);

    GX_BeginGroup(GX_QUADS, 4 * cols * rows);

    for (int i = 0; i < cols; i++)
    {
        for (int j = 0; j < rows; j++)
        {
            int x = i * w;
            int y = j * h;
            GX_Vertex(vertex_property_t(scale * vec3f(x, y, 0), u1, v1, 63, 63, 63));
            GX_Vertex(vertex_property_t(scale * vec3f(x + w, y, 0), u2, v1, 63, 63, 63));
            GX_Vertex(vertex_property_t(scale * vec3f(x + w, y + h, 0), u2, v2, 63, 63, 63));
            GX_Vertex(vertex_property_t(scale * vec3f(x, y + h, 0), u1, v2, 63, 63, 63));
        }
    }
    GX_EndGroup();

    return 4 * cols * rows;
}

int draw_simple_textured_quad(GXTexObj &texture, int32_t x, int32_t y, int32_t w, int32_t h, float scale)
{
    return draw_textured_quad(texture, x, y, w, h, 0, 0, GX_GetTexObjWidth(&texture), GX_GetTexObjHeight(&texture), scale);
}