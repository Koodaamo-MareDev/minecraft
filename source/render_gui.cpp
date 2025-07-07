#include "render_gui.hpp"
#include <math/vec2i.hpp>
#include "font_tile_widths.hpp"
int draw_textured_quad(GXTexObj &texture, int32_t x, int32_t y, int32_t w, int32_t h, vfloat_t u1, vfloat_t v1, vfloat_t u2, vfloat_t v2, float scale)
{
    use_texture(texture);
    float scale_u = 1. / GX_GetTexObjWidth(&texture);
    float scale_v = 1. / GX_GetTexObjHeight(&texture);
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

// NOTE: This function doesn't load the texture automatically because it's used by font rendering. Make sure to call use_texture before calling this function.
int draw_colored_sprite(GXTexObj &texture, vec2i pos, vec2i size, vfloat_t u1, vfloat_t v1, vfloat_t u2, vfloat_t v2, GXColor color, float scale)
{
    uint8_t r = color.r;
    uint8_t g = color.g;
    uint8_t b = color.b;
    uint8_t a = color.a;
    float scale_u = 1. / GX_GetTexObjWidth(&texture);
    float scale_v = 1. / GX_GetTexObjHeight(&texture);
    u1 *= scale_u;
    u2 *= scale_u;
    v1 *= scale_v;
    v2 *= scale_v;
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(vertex_property_t(scale * vec3f(pos.x, pos.y, 0), u1, v1, r, g, b, a));
    GX_Vertex(vertex_property_t(scale * vec3f(pos.x + size.x, pos.y, 0), u2, v1, r, g, b, a));
    GX_Vertex(vertex_property_t(scale * vec3f(pos.x + size.x, pos.y + size.y, 0), u2, v2, r, g, b, a));
    GX_Vertex(vertex_property_t(scale * vec3f(pos.x, pos.y + size.y, 0), u1, v2, r, g, b, a));
    GX_EndGroup();
    return 4;
}

// NOTE: This function doesn't load the texture automatically because it's used by font rendering. Make sure to call use_texture before calling this function.
int __attribute__((noinline)) draw_colored_sprite_3d(GXTexObj &texture, vec3f center, vec3f size, vec3f offset, vec3f right, vec3f up, vfloat_t u1, vfloat_t v1, vfloat_t u2, vfloat_t v2, GXColor color)
{
    uint8_t r = color.r;
    uint8_t g = color.g;
    uint8_t b = color.b;
    uint8_t a = color.a;
    float scale_u = 1. / GX_GetTexObjWidth(&texture);
    float scale_v = 1. / GX_GetTexObjHeight(&texture);
    u1 *= scale_u;
    u2 *= scale_u;
    v1 *= scale_v;
    v2 *= scale_v;
    size = size * 0.5;
    vec3f right_offset = right * size.x;
    vec3f up_offset = up * size.y;
    center = center + right * offset.x + up * offset.y;
    GX_BeginGroup(GX_QUADS, 4);
    GX_VertexF(vertex_property_t(center - right_offset - up_offset, u1, v1, r, g, b, a));
    GX_VertexF(vertex_property_t(center + right_offset - up_offset, u2, v1, r, g, b, a));
    GX_VertexF(vertex_property_t(center + right_offset + up_offset, u2, v2, r, g, b, a));
    GX_VertexF(vertex_property_t(center - right_offset + up_offset, u1, v2, r, g, b, a));
    GX_EndGroup();
    return 4;
}

vfloat_t __attribute__((noinline)) text_width_3d(std::string str)
{
    vfloat_t width = 0;
    vfloat_t max_width = 0;
    for (size_t i = 0; i < str.size(); i++)
    {
        uint8_t c = str[i] & 0xFF;
        if (c == '\n')
        {
            max_width = std::max(max_width, width);
            width = 0;
            continue;
        }
        if (c == ' ')
        {
            width += 3;
            continue;
        }
        width += font_tile_widths[c];
    }
    return std::max(max_width, width);
}

void __attribute__((noinline)) draw_text_3d(vec3f pos, std::string str, GXColor color)
{
    vec3f char_size = vec3f(0.25);
    vec3f right_vec = -angles_to_vector(0, yrot + 90);
    vec3f up_vec = -angles_to_vector(xrot + 90, yrot);

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Use floats for vertex positions
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);

    use_texture(font_texture);

    vfloat_t half_width = text_width_3d(str) / 2.0;

    int x_offset = 0;
    int y_offset = 0;
    for (size_t i = 0; i < str.size(); i++)
    {
        uint8_t c = str[i] & 0xFF;
        if (c == ' ')
        {
            x_offset += 3;
            continue;
        }
        if (c == '\n')
        {
            x_offset = 0;
            y_offset += 8;
            continue;
        }

        uint16_t cx = uint16_t(c & 15) << 3;
        uint16_t cy = uint16_t(c >> 4) << 3;

        vec3f off = vec3f(x_offset - half_width + 4, y_offset, 0.0) * (char_size.x * 0.125);
        // Draw the character
        draw_colored_sprite_3d(font_texture, pos, char_size, off, right_vec, up_vec, cx, cy, cx + 8, cy + 8, color);

        // Move the cursor to the next column
        x_offset += font_tile_widths[c];
    }

    // Restore fixed point format
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
    
    // Restore indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
}

int draw_colored_quad(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float scale)
{
    use_texture(icons_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(vertex_property_t(scale * vec3f(x, y, 0), 0 * BASE3D_PIXEL_UV_SCALE, 16 * BASE3D_PIXEL_UV_SCALE, r, g, b, a));
    GX_Vertex(vertex_property_t(scale * vec3f(x + w, y, 0), 8 * BASE3D_PIXEL_UV_SCALE, 16 * BASE3D_PIXEL_UV_SCALE, r, g, b, a));
    GX_Vertex(vertex_property_t(scale * vec3f(x + w, y + h, 0), 8 * BASE3D_PIXEL_UV_SCALE, 24 * BASE3D_PIXEL_UV_SCALE, r, g, b, a));
    GX_Vertex(vertex_property_t(scale * vec3f(x, y + h, 0), 0 * BASE3D_PIXEL_UV_SCALE, 24 * BASE3D_PIXEL_UV_SCALE, r, g, b, a));
    GX_EndGroup();
    return 4;
}

int fill_screen_texture(GXTexObj &texture, gertex::GXView &view, vfloat_t u1, vfloat_t v1, vfloat_t u2, vfloat_t v2, float scale)
{
    use_texture(texture);
    float scale_u = 1.;// / GX_GetTexObjWidth(&texture);
    float scale_v = 1.;// / GX_GetTexObjHeight(&texture);
    u1 *= scale_u;
    u2 *= scale_u;
    v1 *= scale_v;
    v2 *= scale_v;
    int w = 48;
    int h = 48;
    int cols = std::ceil(view.width / w);
    int rows = std::ceil(view.height / h);

    constexpr uint8_t intensity = 63;

    GX_BeginGroup(GX_QUADS, 4 * cols * rows);

    for (int i = 0; i < cols; i++)
    {
        for (int j = 0; j < rows; j++)
        {
            int x = i * w;
            int y = j * h;
            GX_Vertex(vertex_property_t(scale * vec3f(x, y, 0), u1, v1, intensity, intensity, intensity));
            GX_Vertex(vertex_property_t(scale * vec3f(x + w, y, 0), u2, v1, intensity, intensity, intensity));
            GX_Vertex(vertex_property_t(scale * vec3f(x + w, y + h, 0), u2, v2, intensity, intensity, intensity));
            GX_Vertex(vertex_property_t(scale * vec3f(x, y + h, 0), u1, v2, intensity, intensity, intensity));
        }
    }
    GX_EndGroup();

    return 4 * cols * rows;
}

int draw_simple_textured_quad(GXTexObj &texture, int32_t x, int32_t y, int32_t w, int32_t h, float scale)
{
    return draw_textured_quad(texture, x, y, w, h, 0, 0, GX_GetTexObjWidth(&texture), GX_GetTexObjHeight(&texture), scale);
}