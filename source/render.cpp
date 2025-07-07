#include "render.hpp"
#include "render_blocks.hpp"

#include <ported/Random.hpp>

#include "pnguin/png_loader.hpp"

#include "blocks.hpp"
#include "world.hpp"

const GXColor sky_color = {0x88, 0xBB, 0xFF, 0xFF};

GXTexObj white_texture;
GXTexObj clouds_texture;
GXTexObj sun_texture;
GXTexObj moon_texture;
GXTexObj terrain_texture;
GXTexObj particles_texture;
GXTexObj icons_texture;
GXTexObj container_texture;
GXTexObj underwater_texture;
GXTexObj vignette_texture;
GXTexObj creeper_texture;
GXTexObj player_texture;
GXTexObj font_texture;
GXTexObj items_texture;
GXTexObj inventory_texture;

// Animated textures
water_texanim_t water_still_anim;
lava_texanim_t lava_still_anim;

void init_missing_texture(GXTexObj &texture)
{
    void *texture_buf = memalign(32, 64); // 4x4 RGBA texture
    memset(texture_buf, 0xFF, 64);
    DCFlushRange(texture_buf, 64);

    GX_InitTexObj(&texture, texture_buf, 4, 4, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjFilterMode(&texture, GX_NEAR, GX_NEAR);
}

void init_png_texture(GXTexObj &texture, const std::string &filename)
{
    try
    {
        pnguin::PNGFile png_file("/apps/minecraft/resources/textures/" + filename);
        png_file.to_tpl(texture);
    }
    catch (std::exception &e)
    {
        printf("Failed to load %s: %s\n", filename.c_str(), e.what());
        init_missing_texture(texture);
    }
}

void init_textures()
{

    // Basic textures
    init_missing_texture(white_texture);
    init_png_texture(clouds_texture, "clouds.png");
    init_png_texture(sun_texture, "sun.png");
    init_png_texture(moon_texture, "moon.png");
    init_png_texture(particles_texture, "particles.png");
    init_png_texture(terrain_texture, "terrain.png");
    init_png_texture(icons_texture, "icons.png");
    init_png_texture(container_texture, "container.png");
    init_png_texture(underwater_texture, "underwater.png");
    init_png_texture(vignette_texture, "vignette.png");
    init_png_texture(creeper_texture, "creeper.png");
    init_png_texture(player_texture, "char.png");
    init_png_texture(font_texture, "font.png");
    init_png_texture(items_texture, "items.png");
    init_png_texture(inventory_texture, "inventory.png");

    GX_InitTexObjWrapMode(&clouds_texture, GX_REPEAT, GX_REPEAT);
    GX_InitTexObjWrapMode(&underwater_texture, GX_REPEAT, GX_REPEAT);
    GX_InitTexObjFilterMode(&vignette_texture, GX_LINEAR, GX_LINEAR);

    // Animated textures

    // Lava
    lava_still_anim.target = MEM_PHYSICAL_TO_K1(GX_GetTexObjData(&terrain_texture));
    lava_still_anim.dst_width = 256;
    lava_still_anim.tile_width = 16;
    lava_still_anim.tile_height = 16;
    lava_still_anim.dst_x = 208;
    lava_still_anim.dst_y = 224;
    lava_still_anim.flow_dst_x = 224;
    lava_still_anim.flow_dst_y = 224;

    // Water
    water_still_anim.target = MEM_PHYSICAL_TO_K1(GX_GetTexObjData(&terrain_texture));
    water_still_anim.dst_width = 256;
    water_still_anim.tile_width = 16;
    water_still_anim.tile_height = 16;
    water_still_anim.dst_x = 208;
    water_still_anim.dst_y = 192;
    water_still_anim.flow_dst_x = 224;
    water_still_anim.flow_dst_y = 192;
}

void update_textures()
{
    water_still_anim.update();
    lava_still_anim.update();

    static void *texture_buf = MEM_PHYSICAL_TO_K1(GX_GetTexObjData(&terrain_texture));
    static uint32_t texture_buflen = GX_GetTexBufferSize(GX_GetTexObjWidth(&terrain_texture), GX_GetTexObjHeight(&terrain_texture), GX_GetTexObjFmt(&terrain_texture), GX_FALSE, GX_FALSE);
    DCFlushRange(texture_buf, texture_buflen);
    GX_InvalidateTexAll();
}

void use_texture(GXTexObj &texture)
{
    GX_LoadTexObj(&texture, GX_TEXMAP0);
}

void smooth_light(const vec3i &pos, uint8_t face_index, const vec3i &vertex_off, chunk_t *near, block_t *block, uint8_t &lighting, uint8_t &amb_occ)
{
    if (pos.y < 0 || pos.y > 255)
        return;
    vec3i face = pos + face_offsets[face_index];
    uint8_t total = 1;
    block_t *face_block = get_block_at(face, near);
    if (!face_block)
        face_block = block;
    uint8_t block_light = face_block->block_light;
    uint8_t sky_light = face_block->sky_light;

    vec3i vertex_offA(0, 0, 0);
    vec3i vertex_offB(0, 0, 0);

    switch (face_index)
    {
    case FACE_NX:
    case FACE_PX:
    {
        vertex_offA = vec3i(0, vertex_off.y, 0);
        vertex_offB = vec3i(0, 0, vertex_off.z);
        break;
    }
    case FACE_NY:
    case FACE_PY:
    {
        vertex_offA = vec3i(vertex_off.x, 0, 0);
        vertex_offB = vec3i(0, 0, vertex_off.z);
        break;
    }
    case FACE_NZ:
    case FACE_PZ:
    {
        vertex_offA = vec3i(vertex_off.x, 0, 0);
        vertex_offB = vec3i(0, vertex_off.y, 0);
        break;
    }
    break;
    default:
        break;
    }
    block_t *blockA = get_block_at(face + vertex_offA, near);
    block_t *blockB = get_block_at(face + vertex_offB, near);
    if (blockA)
    {
        if (properties(blockA->id).m_opacity == 15)
        {
            amb_occ += 6;
        }
        else
        {
            total++;
            block_light += blockA->block_light;
            sky_light += blockA->sky_light;
        }
    }
    if (blockB)
    {
        if (properties(blockB->id).m_opacity == 15)
        {
            amb_occ += 6;
        }
        else
        {
            total++;
            block_light += blockB->block_light;
            sky_light += blockB->sky_light;
        }
    }
    block_t *blockC = get_block_at(face + vertex_off, near);
    if (blockC)
    {
        if (properties(blockC->id).m_opacity == 15)
        {
            if (amb_occ < 12)
                amb_occ += 6;
            else
                amb_occ = 18;
        }
        else
        {
            total++;
            block_light += blockC->block_light;
            sky_light += blockC->sky_light;
        }
    }
    if (total > 1)
        lighting = (block_light / total) | ((sky_light / total) << 4);
}
const vec3i cube_vertex_offsets[6][4] = {
    {
        // FACE_NX
        vec3i{0, -1, -1},
        vec3i{0, 1, -1},
        vec3i{0, 1, 1},
        vec3i{0, -1, 1},
    },
    {
        // FACE_PX
        vec3i{0, -1, 1},
        vec3i{0, 1, 1},
        vec3i{0, 1, -1},
        vec3i{0, -1, -1},
    },
    {
        // FACE_NY
        vec3i{-1, 0, -1},
        vec3i{-1, 0, 1},
        vec3i{1, 0, 1},
        vec3i{1, 0, -1},
    },
    {
        // FACE_PY
        vec3i{1, 0, -1},
        vec3i{1, 0, 1},
        vec3i{-1, 0, 1},
        vec3i{-1, 0, -1},
    },
    {
        // FACE_NZ
        vec3i{1, -1, 0},
        vec3i{1, 1, 0},
        vec3i{-1, 1, 0},
        vec3i{-1, -1, 0},
    },
    {
        // FACE_PZ
        vec3i{-1, -1, 0},
        vec3i{-1, 1, 0},
        vec3i{1, 1, 0},
        vec3i{1, -1, 0},
    },
};

const vec3i cube_vertices16[6][4] = {
    {
        // FACE_NX
        vec3i{-16, -16, -16},
        vec3i{-16, 16, -16},
        vec3i{-16, 16, 16},
        vec3i{-16, -16, 16},
    },
    {
        // FACE_PX
        vec3i{16, -16, 16},
        vec3i{16, 16, 16},
        vec3i{16, 16, -16},
        vec3i{16, -16, -16},
    },
    {
        // FACE_NY
        vec3i{-16, -16, -16},
        vec3i{-16, -16, 16},
        vec3i{16, -16, 16},
        vec3i{16, -16, -16},
    },
    {
        // FACE_PY
        vec3i{16, 16, -16},
        vec3i{16, 16, 16},
        vec3i{-16, 16, 16},
        vec3i{-16, 16, -16},
    },
    {
        // FACE_NZ
        vec3i{16, -16, -16},
        vec3i{16, 16, -16},
        vec3i{-16, 16, -16},
        vec3i{-16, -16, -16},
    },
    {
        // FACE_PZ
        vec3i{-16, -16, 16},
        vec3i{-16, 16, 16},
        vec3i{16, 16, 16},
        vec3i{16, -16, 16},
    },
};

const vec3i cube_vertices[6][4] = {
    {
        // FACE_NX
        vec3i{-1, -1, -1},
        vec3i{-1, 1, -1},
        vec3i{-1, 1, 1},
        vec3i{-1, -1, 1},
    },
    {
        // FACE_PX
        vec3i{1, -1, 1},
        vec3i{1, 1, 1},
        vec3i{1, 1, -1},
        vec3i{1, -1, -1},
    },
    {
        // FACE_NY
        vec3i{-1, -1, -1},
        vec3i{-1, -1, 1},
        vec3i{1, -1, 1},
        vec3i{1, -1, -1},
    },
    {
        // FACE_PY
        vec3i{1, 1, -1},
        vec3i{1, 1, 1},
        vec3i{-1, 1, 1},
        vec3i{-1, 1, -1},
    },
    {
        // FACE_NZ
        vec3i{1, -1, -1},
        vec3i{1, 1, -1},
        vec3i{-1, 1, -1},
        vec3i{-1, -1, -1},
    },
    {
        // FACE_PZ
        vec3i{-1, -1, 1},
        vec3i{-1, 1, 1},
        vec3i{1, 1, 1},
        vec3i{1, -1, 1},
    },
};

vec3i vertical_add(const vec3i &a, uint8_t b)
{
    // Multiply a vector by a vertical factor
    // This is used to scale the vertices for rendering side faces
    return vec3i(a.x * 16, a.y - 17 + b * 2, a.z * 16);
}

int render_face(vec3i pos, uint8_t face, uint32_t texture_index, chunk_t *near, block_t *block, uint8_t min_y, uint8_t max_y)
{
    if (!base3d_is_drawing || face >= 6)
    {
        static vertex_property_t tmp;
        // This is just a dummy call to GX_Vertex to keep buffer size up to date.
        for (int i = 0; i < 4; i++)
            GX_VertexLit(tmp, 0, 0);
        return 4;
    }
    vec3i vertex_pos((pos.x & 0xF) << BASE3D_POS_FRAC_BITS, (pos.y & 0xF) << BASE3D_POS_FRAC_BITS, (pos.z & 0xF) << BASE3D_POS_FRAC_BITS);
    if (!block)
        block = get_block_at(pos, near);
    uint8_t light_val = get_face_light_index(pos, face, near, block);
    uint8_t ao[4] = {0, 0, 0, 0};
    uint8_t ao_no_op[4] = {0, 0, 0, 0};
    uint8_t lighting[4] = {light_val, light_val, light_val, light_val};
    vertex_property16_t vertices[4];
    uint8_t index = 0;
#define SMOOTH(ao_tgt) smooth_light(pos, face, cube_vertex_offsets[face][index], near, block, lighting[index], ao_tgt[index])
    if ((face & ~1) != FACE_NY)
    {
        // Side faces
        uint8_t max_y_orig = max_y;
        min_y = (min_y & 0xF) ? min_y : 0;
        max_y = (max_y & 0xF) ? max_y : 0;
        uint8_t *ao_min_target = min_y ? ao_no_op : ao;
        uint8_t *ao_max_target = max_y ? ao_no_op : ao;
        SMOOTH(ao_min_target);
        vertices[index] = {vertex_pos + (min_y ? vertical_add(cube_vertices[face][index], min_y) : cube_vertices16[face][index]), TEXTURE_NX(texture_index), TEXTURE_PY(texture_index) - min_y * BASE3D_PIXEL_UV_SCALE};
        index++;
        SMOOTH(ao_max_target);
        vertices[index] = {vertex_pos + (max_y ? vertical_add(cube_vertices[face][index], max_y) : cube_vertices16[face][index]), TEXTURE_NX(texture_index), TEXTURE_PY(texture_index) - max_y_orig * BASE3D_PIXEL_UV_SCALE};
        index++;
        SMOOTH(ao_max_target);
        vertices[index] = {vertex_pos + (max_y ? vertical_add(cube_vertices[face][index], max_y) : cube_vertices16[face][index]), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index) - max_y_orig * BASE3D_PIXEL_UV_SCALE};
        index++;
        SMOOTH(ao_min_target);
        vertices[index] = {vertex_pos + (min_y ? vertical_add(cube_vertices[face][index], min_y) : cube_vertices16[face][index]), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index) - min_y * BASE3D_PIXEL_UV_SCALE};
    }
    else
    {
        // Top and bottom faces
        min_y = (min_y & 0xF) ? min_y : 0;
        max_y = (max_y & 0xF) ? max_y : 0;
        uint8_t *ao_target = ao;
        if ((min_y && face == FACE_NY) || (max_y && face == FACE_PY))
            ao_target = ao_no_op;
        if (face == FACE_NY)
        {
            SMOOTH(ao_target);
            vertices[index] = {vertex_pos + (min_y ? vertical_add(cube_vertices[face][index], min_y) : cube_vertices16[face][index]), TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)};
            index++;
            SMOOTH(ao_target);
            vertices[index] = {vertex_pos + (min_y ? vertical_add(cube_vertices[face][index], min_y) : cube_vertices16[face][index]), TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)};
            index++;
            SMOOTH(ao_target);
            vertices[index] = {vertex_pos + (min_y ? vertical_add(cube_vertices[face][index], min_y) : cube_vertices16[face][index]), TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)};
            index++;
            SMOOTH(ao_target);
            vertices[index] = {vertex_pos + (min_y ? vertical_add(cube_vertices[face][index], min_y) : cube_vertices16[face][index]), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)};
        }
        else
        {
            SMOOTH(ao_target);
            vertices[index] = {vertex_pos + (max_y ? vertical_add(cube_vertices[face][index], max_y) : cube_vertices16[face][index]), TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)};
            index++;
            SMOOTH(ao_target);
            vertices[index] = {vertex_pos + (max_y ? vertical_add(cube_vertices[face][index], max_y) : cube_vertices16[face][index]), TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)};
            index++;
            SMOOTH(ao_target);
            vertices[index] = {vertex_pos + (max_y ? vertical_add(cube_vertices[face][index], max_y) : cube_vertices16[face][index]), TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)};
            index++;
            SMOOTH(ao_target);
            vertices[index] = {vertex_pos + (max_y ? vertical_add(cube_vertices[face][index], max_y) : cube_vertices16[face][index]), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)};
        }
    }
#undef SMOOTH
    if (texture_index >= 240 && texture_index < 250)
    {
        // Force full brightness for breaking block override
        lighting[0] = lighting[1] = lighting[2] = lighting[3] = 255;
        // Reset ambient occlusion
        ao[0] = ao[1] = ao[2] = ao[3] = 0;
        // Set the face to the top face
        face = FACE_PY;
    }
    index = (ao[0] + ao[3] > ao[1] + ao[2]);
    GX_VertexLit16(vertices[index], lighting[index], face + ao[index]);
    index = (index + 1) & 3;
    GX_VertexLit16(vertices[index], lighting[index], face + ao[index]);
    index = (index + 1) & 3;
    GX_VertexLit16(vertices[index], lighting[index], face + ao[index]);
    index = (index + 1) & 3;
    GX_VertexLit16(vertices[index], lighting[index], face + ao[index]);
    return 4;
}

void render_single_block(block_t &selected_block, bool transparency)
{
    std::deque<chunk_t *> &chunks = get_chunks();
    if (chunks.size() == 0)
        return;

    // Precalculate the vertex count. Set position to Y = -16 to render "outside the world"
    int vertexCount = render_block(*chunks[0], &selected_block, vec3i(0, -16, 0), transparency);

    // Start drawing the block
    GX_BeginGroup(GX_QUADS, vertexCount);

    // Render the block. Set position to Y = -16 to render "outside the world"
    render_block(*chunks[0], &selected_block, vec3i(0, -16, 0), transparency);

    // End the group
    GX_EndGroup();
}

void render_single_block_at(block_t &selected_block, vec3i pos, bool transparency)
{
    chunk_t *chunk = get_chunk_from_pos(pos);
    if (!chunk)
        return;
    // Precalculate the vertex count. Set position to Y = -16 to render "outside the world"
    int vertexCount = render_block(*chunk, &selected_block, pos, transparency);

    // Start drawing the block
    GX_BeginGroup(GX_QUADS, vertexCount);

    // Render the block. Set position to Y = -16 to render "outside the world"
    render_block(*chunk, &selected_block, pos, transparency);

    // End the group
    GX_EndGroup();
}

void render_single_item(uint32_t texture_index, bool transparency, uint8_t light)
{
    vec3f vertex_pos = vec3f(0, 0, 0);
    GX_BeginGroup(GX_QUADS, 4);
    // Render the selected block.
    GX_VertexLit({vertex_pos + vec3f(0.5, -.5, 0), TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, light, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f(0.5, 0.5, 0), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, light, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f(-.5, 0.5, 0), TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, light, FACE_PY);
    GX_VertexLit({vertex_pos + vec3f(-.5, -.5, 0), TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, light, FACE_PY);
    // End the group
    GX_EndGroup();
}

void render_item_pixel(uint32_t texture_index, uint8_t x, uint8_t y, bool x_next, bool y_next, uint8_t light)
{
    GX_BeginGroup(GX_QUADS, (1 + x_next + y_next) << 2);
    // Render the selected pixel.
    GX_VertexLit({vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625, -.5), TEXTURE_X(texture_index) + (x + 1) * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + y * BASE3D_PIXEL_UV_SCALE}, light, FACE_PX);
    GX_VertexLit({vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625 + 0.0625, -.5), TEXTURE_X(texture_index) + (x + 1) * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + (y + 1) * BASE3D_PIXEL_UV_SCALE}, light, FACE_PX);
    GX_VertexLit({vec3f(+.5 - x * 0.0625, -.5 + y * 0.0625 + 0.0625, -.5), TEXTURE_X(texture_index) + x * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + (y + 1) * BASE3D_PIXEL_UV_SCALE}, light, FACE_PX);
    GX_VertexLit({vec3f(+.5 - x * 0.0625, -.5 + y * 0.0625, -.5), TEXTURE_X(texture_index) + x * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + y * BASE3D_PIXEL_UV_SCALE}, light, FACE_PX);

    if (x_next)
    {
        // Render the selected pixel.
        GX_VertexLit({vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625, -.625), TEXTURE_X(texture_index) + (x + 1) * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + y * BASE3D_PIXEL_UV_SCALE}, light, FACE_PZ);
        GX_VertexLit({vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625 + 0.0625, -.625), TEXTURE_X(texture_index) + (x + 1) * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + (y + 1) * BASE3D_PIXEL_UV_SCALE}, light, FACE_PZ);
        GX_VertexLit({vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625 + 0.0625, -.5), TEXTURE_X(texture_index) + x * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + (y + 1) * BASE3D_PIXEL_UV_SCALE}, light, FACE_PZ);
        GX_VertexLit({vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625, -.5), TEXTURE_X(texture_index) + x * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + y * BASE3D_PIXEL_UV_SCALE}, light, FACE_PZ);
    }
    if (y_next)
    {
        // Render the selected pixel.
        GX_VertexLit({vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625, -.625), TEXTURE_X(texture_index) + (x + 1) * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + y * BASE3D_PIXEL_UV_SCALE}, light, FACE_PY);
        GX_VertexLit({vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625, -.5), TEXTURE_X(texture_index) + (x + 1) * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + (y + 1) * BASE3D_PIXEL_UV_SCALE}, light, FACE_PY);
        GX_VertexLit({vec3f(+.5 - x * 0.0625, -.5 + y * 0.0625, -.5), TEXTURE_X(texture_index) + x * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + (y + 1) * BASE3D_PIXEL_UV_SCALE}, light, FACE_PY);
        GX_VertexLit({vec3f(+.5 - x * 0.0625, -.5 + y * 0.0625, -.625), TEXTURE_X(texture_index) + x * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + y * BASE3D_PIXEL_UV_SCALE}, light, FACE_PY);
    }
    // End the group
    GX_EndGroup();
}

// Assuming a right-handed coordinate system and that the angles are in degrees. X = pitch, Y = yaw
vec3f angles_to_vector(float x, float y)
{
    vec3f result;
    float x_rad = M_DTOR * x;
    float y_rad = M_DTOR * y;
    result.x = -std::cos(x_rad) * std::sin(y_rad);
    result.y = std::sin(x_rad);
    result.z = -std::cos(x_rad) * std::cos(y_rad);
    return result;
}

// Assuming a right-handed coordinate system and that the angles are in degrees. X = pitch, Y = yaw
vec3f vector_to_angles(const vec3f &vec)
{
    vec3f result;
    result.x = std::asin(-vec.y);
    result.y = std::atan2(vec.x, vec.z);

    // Convert to degrees
    result.x /= M_DTOR;
    result.y /= M_DTOR;

    return result;
}

// Function to calculate the signed distance from a point to a frustum plane
float distance_to_plane(const vec3f &point, const frustum_t &frustum, int planeIndex)
{
    const plane_t &plane = frustum.planes[planeIndex];
    return (plane.direction.x * point.x + plane.direction.y + plane.direction.z * point.z + plane.distance) / plane.direction.fast_magnitude();
}

// Function to calculate the distance from a point to a frustum
float distance_to_frustum(const vec3f &point, const frustum_t &frustum)
{
    float minDistance = distance_to_plane(point, frustum, 0);

    for (int i = 1; i < 6; ++i)
    {
        float distance = distance_to_plane(point, frustum, i);
        if (distance < minDistance)
        {
            minDistance = distance;
        }
    }

    return minDistance;
}

// Function to calculate the frustum planes from camera parameters
frustum_t calculate_frustum(camera_t &camera)
{
    frustum_t frustum;

    constexpr float fov_margin = 10.0f; // Used to prevent overculling of blocks at the edges of the screen

    // Calculate half-width and half-height at near plane
    float half_fov = camera.fov * 0.5f;

    // Calculate forward vector
    guVector forward = angles_to_vector(camera.rot.x, camera.rot.y);
    guVector backward = vec3f() - forward;

    // Calculate the 4 perspective frustum planes (not near and far)
    guVector right_vec = -angles_to_vector(camera.rot.x, camera.rot.y + 90 + half_fov + fov_margin);
    guVector left_vec = -angles_to_vector(camera.rot.x, camera.rot.y - 90 - half_fov + fov_margin);
    guVector up_vec = -angles_to_vector(camera.rot.x + 90 + half_fov + fov_margin, camera.rot.y);
    guVector down_vec = -angles_to_vector(camera.rot.x - 90 - half_fov + fov_margin, camera.rot.y);

    // Calculate points on the near and far planes
    guVector near_center = (vec3f(forward) * (camera.near));

    guVecScale(&left_vec, &left_vec, camera.aspect);
    guVecScale(&right_vec, &right_vec, camera.aspect);

    // Calculate the frustum plane equations in the form Ax + By + Cz + D = 0
    frustum.planes[0].direction = left_vec;
    frustum.planes[0].distance = guVecDotProduct(&left_vec, &near_center);

    frustum.planes[1].direction = right_vec;
    frustum.planes[1].distance = guVecDotProduct(&right_vec, &near_center);

    frustum.planes[2].direction = down_vec;
    frustum.planes[2].distance = guVecDotProduct(&down_vec, &near_center);

    frustum.planes[3].direction = up_vec;
    frustum.planes[3].distance = guVecDotProduct(&up_vec, &near_center);

    frustum.planes[4].direction = forward;
    frustum.planes[4].distance = camera.near;

    frustum.planes[5].direction = backward;
    frustum.planes[5].distance = camera.far;

    // Normalize the plane equations
    for (int i = 0; i < 6; ++i)
    {
        plane_t &plane = frustum.planes[i];
        vfloat_t length = Q_rsqrt(plane.direction.x * plane.direction.x +
                                  plane.direction.y * plane.direction.y +
                                  plane.direction.z * plane.direction.z);
        plane.direction.x *= length;
        plane.direction.y *= length;
        plane.direction.z *= length;
        plane.distance *= length;
    }

    return frustum;
}

void transform_view(gertex::GXMatrix view, guVector world_pos, guVector object_scale, guVector object_rot, bool load)
{
    Mtx model, modelview;
    Mtx offset;
    Mtx posmtx;
    Mtx scalemtx;
    Mtx rotx;
    Mtx roty;
    Mtx rotz;
    Mtx objrotx;
    Mtx objroty;
    Mtx objrotz;
    guVector axis; // Axis to rotate on

    // Reset matrices
    guMtxIdentity(model);
    guMtxIdentity(offset);
    guMtxIdentity(posmtx);
    guMtxIdentity(scalemtx);
    guMtxIdentity(rotz);
    guMtxIdentity(roty);
    guMtxIdentity(rotx);

    // Position the chunks on the screen
    guMtxTrans(offset, -world_pos.x, -world_pos.y, -world_pos.z);
    guMtxTrans(posmtx, player_pos.x, player_pos.y, player_pos.z);

    // Scale the object
    guMtxScale(scalemtx, object_scale.x, object_scale.y, object_scale.z);
    guMtxInverse(scalemtx, scalemtx);

    // Rotate object
    axis.x = 0;
    axis.y = 0;
    axis.z = 1;
    guMtxRotAxisDeg(objrotz, &axis, -object_rot.z);
    axis.x = 0;
    axis.y = 1;
    axis.z = 0;
    guMtxRotAxisDeg(objroty, &axis, -object_rot.y);
    axis.x = 1;
    axis.y = 0;
    axis.z = 0;
    guMtxRotAxisDeg(objrotx, &axis, -object_rot.x);

    // Rotate view
    axis.x = 1;
    axis.y = 0;
    axis.z = 0;
    guMtxRotAxisDeg(rotx, &axis, xrot);
    axis.x = 0;
    axis.y = 1;
    axis.z = 0;
    guMtxRotAxisDeg(roty, &axis, yrot);

    // Apply matrices
    guMtxConcat(objrotz, objroty, objroty);
    guMtxConcat(objroty, objrotx, objrotx);
    guMtxConcat(objrotx, scalemtx, scalemtx);
    guMtxConcat(scalemtx, posmtx, posmtx);
    guMtxConcat(posmtx, offset, offset);
    guMtxConcat(offset, rotz, rotz);
    guMtxConcat(rotz, roty, roty);
    guMtxConcat(roty, rotx, model);
    guMtxInverse(model, model);
    guMtxConcat(model, view.mtx, modelview);
    gertex::use_matrix(modelview, load);
}

void transform_view_screen(gertex::GXMatrix view, guVector screen_pos, guVector object_scale, guVector object_rot, bool load)
{
    Mtx model, modelview;
    Mtx posmtx;
    Mtx scalemtx;
    Mtx objrotx;
    Mtx objroty;
    Mtx objrotz;

    // Reset matrices
    guMtxIdentity(model);
    guMtxIdentity(posmtx);

    // Position the object on the screen
    guMtxTrans(posmtx, -screen_pos.x, -screen_pos.y, -screen_pos.z);

    // Scale the object
    guMtxScale(scalemtx, object_scale.x, object_scale.y, object_scale.z);
    guMtxInverse(scalemtx, scalemtx);

    guVector axis; // Axis to rotate on

    // Rotate object
    axis.x = 0;
    axis.y = 0;
    axis.z = 1;
    guMtxRotAxisDeg(objrotz, &axis, object_rot.z);
    axis.x = 0;
    axis.y = 1;
    axis.z = 0;
    guMtxRotAxisDeg(objroty, &axis, object_rot.y);
    axis.x = 1;
    axis.y = 0;
    axis.z = 0;
    guMtxRotAxisDeg(objrotx, &axis, object_rot.x);

    // Apply matrices
    guMtxConcat(objrotz, objroty, objroty);
    guMtxConcat(objroty, objrotx, objrotx);
    guMtxConcat(objrotx, scalemtx, scalemtx);
    guMtxConcat(scalemtx, posmtx, posmtx);
    guMtxConcat(posmtx, model, model);
    guMtxInverse(model, model);
    guMtxConcat(model, view.mtx, modelview);
    gertex::use_matrix(modelview, load);
}

void draw_particle(camera_t &camera, vec3f pos, uint32_t texture_index, float size, uint8_t brightness)
{
    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    GX_BeginGroup(GX_QUADS, 4);
    for (int i = 0; i < 4; i++)
    {
        int x = (i == 0 || i == 3);
        int y = (i > 1);
        guVector vertex{(x - 0.5f) * size, (y - 0.5f) * size, 0};

        Mtx44 rot_mtx;

        // Rotate the vertex
        guMtxRotDeg(rot_mtx, 'x', camera.rot.x);
        guVecMultiply(rot_mtx, &vertex, &vertex);
        guMtxRotDeg(rot_mtx, 'y', camera.rot.y);
        guVecMultiply(rot_mtx, &vertex, &vertex);

        // Translate the vertex
        vertex.x += pos.x;
        vertex.y += pos.y;
        vertex.z += pos.z;

        GX_VertexLit(vertex_property_t(vertex, TEXTURE_X(texture_index) + x * 4, TEXTURE_Y(texture_index) + y * 4), brightness);
    }
    GX_EndGroup();
}

void draw_particles(camera_t &camera, particle *particles, int count)
{

    // Bake vertex properties
    Mtx44 rot_mtx;
    vertex_property_t vertices[4];
    for (int i = 0; i < 4; i++)
    {
        int x = (i == 0 || i == 3);
        int y = (i > 1);
        guVector vertex{(x - 0.5f), (y - 0.5f), 0};

        guMtxRotDeg(rot_mtx, 'x', camera.rot.x);
        guVecMultiply(rot_mtx, &vertex, &vertex);
        guMtxRotDeg(rot_mtx, 'y', camera.rot.y);
        guVecMultiply(rot_mtx, &vertex, &vertex);

        vertices[i] = vertex_property_t(vertex, (x << 2) * BASE3D_PIXEL_UV_SCALE, (y << 2) * BASE3D_PIXEL_UV_SCALE);
    }

    // Draw particles on a per-type basis
    for (int t = 0; t < PTYPE_MAX; t++)
    {
        // Enumerate visible particles
        int visible_count = 0;
        for (int i = 0; i < count; i++)
        {
            particle &particle = particles[i];
            if (particle.life_time && particle.type == t)
                visible_count++;
        }

        if (visible_count == 0)
            continue;

        if (t == PTYPE_BLOCK_BREAK || t == PTYPE_GENERIC)
        {
            // Enable indexed colors
            GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
        }
        else if (t == PTYPE_TINY_SMOKE)
        {
            // Enable direct colors
            GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        }

        use_texture(t == 0 ? terrain_texture : particles_texture);

        // Use floats for vertex positions
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);

        GX_BeginGroup(GX_QUADS, visible_count << 2);

        // Draw particles
        for (int i = 0; i < count; i++)
        {
            particle &particle = particles[i];
            if (particle.life_time && particle.type == t)
            {
                for (int j = 0; j < 4; j++)
                {
                    vertex_property_t vertex = vertices[j];
                    vertex.pos = vertex.pos * (particle.size / 64.f);
                    vertex.pos = vertex.pos + particle.position;

                    if (t == PTYPE_BLOCK_BREAK)
                    {
                        vertex.x_uv += particle.u * BASE3D_PIXEL_UV_SCALE;
                        vertex.y_uv += particle.v * BASE3D_PIXEL_UV_SCALE;
                        GX_VertexLitF(vertex, particle.brightness);
                    }
                    else if (t == PTYPE_TINY_SMOKE)
                    {
                        int x = (j == 0 || j == 3);
                        int y = (j > 1);
                        float brightness_value = ((particle.brightness & 0xF) | (particle.brightness >> 4)) * 0.0625f + 0.0625f;
                        vertex.x_uv = ((x << 4) + (int(particle.life_time * 16.0f / float(particle.max_life_time)) << 4)) * BASE3D_PIXEL_UV_SCALE;
                        vertex.y_uv = (y << 4) * BASE3D_PIXEL_UV_SCALE;
                        vertex.color_r = particle.r * brightness_value;
                        vertex.color_g = particle.g * brightness_value;
                        vertex.color_b = particle.b * brightness_value;
                        vertex.color_a = particle.a;
                        GX_VertexF(vertex);
                    }
                    else if (t == PTYPE_GENERIC)
                    {
                        int x = (j == 0 || j == 3);
                        int y = (j > 1);
                        vertex.x_uv = ((x << 4) + particle.u) * BASE3D_PIXEL_UV_SCALE;
                        vertex.y_uv = ((y << 4) + particle.v) * BASE3D_PIXEL_UV_SCALE;
                        GX_VertexLitF(vertex, particle.brightness);
                    }
                }
            }
        }

        GX_EndGroup();

        // Restore default vertex format
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
    }
}

void draw_stars()
{
    static bool generated = false;
    static std::vector<vec3f> vertices;
    if (!generated)
    {
        vertices.resize(6000); // 1500 quads
        javaport::Random rng(10842);
        size_t index = 0;
        for (size_t i = 0; i < 1500; i++)
        {
            double dirX = (double)(rng.nextFloat() * 2.0F - 1.0F);
            double dirY = (double)(rng.nextFloat() * 2.0F - 1.0F);
            double dirZ = (double)(rng.nextFloat() * 2.0F - 1.0F);
            double size = (double)(0.25F + rng.nextFloat() * 0.25F);
            double sqrMagnitude = dirX * dirX + dirY * dirY + dirZ * dirZ;
            if (sqrMagnitude < 1.0 && sqrMagnitude > 0.01)
            {
                sqrMagnitude = Q_rsqrt_d(sqrMagnitude);
                dirX *= sqrMagnitude;
                dirY *= sqrMagnitude;
                dirZ *= sqrMagnitude;
                double xDist = dirX * 100.0;
                double yDist = dirY * 100.0;
                double zDist = dirZ * 100.0;
                double pitch = std::atan2(dirX, dirZ);
                double sinPitch = std::sin(pitch);
                double cosPitch = std::cos(pitch);
                double yaw = std::atan2(Q_sqrt_d(dirX * dirX + dirZ * dirZ), dirY);
                double sinYaw = std::sin(yaw);
                double cosYaw = std::cos(yaw);
                double roll = rng.nextDouble() * M_TWOPI;
                double sinRoll = std::sin(roll);
                double cosRoll = std::cos(roll);

                for (int vertIndex = 0; vertIndex < 4; ++vertIndex)
                {
                    double x = (double)((vertIndex & 2) - 1) * size;
                    double z = (double)(((vertIndex + 1) & 2) - 1) * size;
                    double rotatedX = x * cosRoll - z * sinRoll;
                    double rotatedZ = z * cosRoll + x * sinRoll;
                    double vertY = rotatedX * sinYaw;
                    double rotatedY = -rotatedX * cosYaw;
                    double vertX = rotatedY * sinPitch - rotatedZ * cosPitch;
                    double vertZ = rotatedZ * sinPitch + rotatedY * cosPitch;
                    vertices[index++] = vec3f(xDist + vertX, yDist + vertY, zDist + vertZ);
                }
            }
        }
        // Resize the vector to the actual number of vertices generated
        vertices.resize(index);
        generated = true;
    }
    int brightness_level = (get_star_brightness() * 255);
    if (brightness_level > 0)
    {
        GX_BeginGroup(GX_QUADS, vertices.size());
        for (size_t i = 0; i < vertices.size(); i++)
            GX_Vertex(vertex_property_t(vertices[i ^ 3], 0, 0, brightness_level, brightness_level, brightness_level, brightness_level));
        GX_EndGroup();
    }
}

float get_celestial_angle()
{
    int daytime_ticks = (current_world->time_of_day % 24000);
    float normalized_daytime = ((float)daytime_ticks + current_world->partial_ticks) / 24000.0F - 0.25F;
    if (normalized_daytime < 0.0F)
    {
        ++normalized_daytime;
    }

    if (normalized_daytime > 1.0F)
    {
        --normalized_daytime;
    }

    float tmp_daytime = normalized_daytime;
    normalized_daytime = 1.0F - (float)((std::cos((double)normalized_daytime * M_PI) + 1.0) / 2.0);
    normalized_daytime = tmp_daytime + (normalized_daytime - tmp_daytime) / 3.0F;
    return normalized_daytime;
}
float get_star_brightness()
{
    float brightness = 1.0F - (std::cos(get_celestial_angle() * M_TWOPI) * 2.0F + 0.75F);
    if (brightness < 0.0F)
    {
        brightness = 0.0F;
    }

    if (brightness > 1.0F)
    {
        brightness = 1.0F;
    }

    return brightness * brightness * 0.5F;
}
float get_sky_multiplier()
{
    float multiplier = std::cos(get_celestial_angle() * M_TWOPI) * 2.0F + 0.5F;
    if (multiplier < 0.0F)
    {
        multiplier = 0.0F;
    }

    if (multiplier > 1.0F)
    {
        multiplier = 1.0F;
    }
    return multiplier;
}
GXColor get_sky_color(bool cave_darkness)
{
    if (current_world && current_world->hell)
        return GXColor{0x20, 0, 0, 0xFF};

    float elevation_brightness = std::pow(std::clamp((player_pos.y) * 0.03125f, 0.0f, 1.0f), 2.0f);
    if (player_pos.y < -500.0f)
    {
        elevation_brightness = std::clamp((player_pos.y + 500.0f) / -250.0f, 0.0f, 1.0f);
    }
    float sky_multiplier = get_sky_multiplier();
    float brightness = elevation_brightness * sky_multiplier;
    return GXColor{uint8_t(sky_color.r * brightness), uint8_t(sky_color.g * brightness), uint8_t(sky_color.b * brightness), 0xFF};
}

float *get_sunrise_color()
{
    static float out_color[4] = {0.0F, 0.0F, 0.0F, 0.0F};

    float blendRange = 0.4F;

    // Map time of day to cosine curve over full day (0 to 1 -> 0 to 2π)
    float skyAngle = std::cos(get_celestial_angle() * M_PI * 2.0F);

    // Center the glow range at 0.0 (sunrise/sunset)
    float glowCenter = 0.0F;

    // Only compute glow when within blend range of the center
    if (skyAngle >= glowCenter - blendRange && skyAngle <= glowCenter + blendRange)
    {
        // Normalize [-blendRange, blendRange] to [0, 1]
        float fade = (skyAngle - glowCenter) / blendRange * 0.5F + 0.5F;

        // Alpha is a sin curve for smooth appearance, squared for extra fade
        float alpha = 1.0F - (1.0F - std::sin(fade * M_PI)) * 0.99F;
        alpha *= alpha;

        // RGB values are faded based on 'fade'
        float red = fade * 0.3F + 0.7F;          // Starts at 0.7, becomes more red as fade → 1
        float green = fade * fade * 0.7F + 0.2F; // Strong green component near mid-fade
        float blue = 0.2F;                       // Blue stays constant

        // Store and return as RGBA
        out_color[0] = red;
        out_color[1] = green;
        out_color[2] = blue;
        out_color[3] = alpha;
        return out_color;
    }
    else
    {
        // Not within sunrise/sunset range — no color
        return nullptr;
    }
}

GXColor get_lightmap_color(uint8_t light)
{
    return *(GXColor *)&light_map[uint32_t(light) << 2];
}

void draw_sunrise()
{
    float *sunrise_color = get_sunrise_color();
    if (!sunrise_color)
        return;

    // Convert the color to 8-bit values
    uint8_t r = uint8_t(sunrise_color[0] * 255);
    uint8_t g = uint8_t(sunrise_color[1] * 255);
    uint8_t b = uint8_t(sunrise_color[2] * 255);
    uint8_t a = uint8_t(sunrise_color[3] * 255);

    // Store the current state
    gertex::GXState state = gertex::get_state();

    gertex::set_blending(gertex::GXBlendMode::normal);

    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);

    // Use floats for vertex positions
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);

    use_texture(white_texture);

    gertex::GXMatrix mtx_tmp;
    gertex::GXMatrix mtx;
    guVector axis{1, 0, 0};
    guMtxRotAxisDeg(mtx.mtx, &axis, 90.0f);
    float celestial_angle = get_celestial_angle();
    axis = {0, 0, 1};
    guMtxRotAxisDeg(mtx_tmp.mtx, &axis, celestial_angle > 0.5f ? 180.0f : 0.0f);
    guMtxConcat(mtx.mtx, mtx_tmp.mtx, mtx.mtx);
    transform_view(mtx, player_pos);

    uint8_t quality = 16;
    float centerX = 0.0f;
    float centerY = 100.0f;
    float centerZ = 0.0f;

    GX_BeginGroup(GX_TRIANGLEFAN, quality + 2);

    // Draw the center vertex
    GX_VertexF(vertex_property_t(vec3f(centerX, centerY, centerZ), 0, 0, r, g, b, a));

    // Draw the outer vertices
    for (int i = 0; i <= quality; ++i)
    {
        float v = (float)i * (float)M_PI * -2.0f / (float)quality;
        float x = std::sin(v);
        float z = std::cos(v);
        GX_VertexF(vertex_property_t(vec3f(x * 120.0f, z * 120.0f, -z * 40.0f * sunrise_color[3]), 0, 0, r, g, b, 0));
    }
    GX_EndGroup();

    gertex::set_state(state);
}

void draw_sky(GXColor background)
{
    // Disable fog
    gertex::use_fog(false);

    // Use additive blending
    gertex::set_blending(gertex::GXBlendMode::additive);
    GX_SetAlphaUpdate(GX_FALSE);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);

    // Use solid color texture
    use_texture(white_texture);

    // Draw sunrise
    draw_sunrise();

    // Use short vertex positions with fractional bits
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);

    // Prepare rendering the celestial bodies
    gertex::GXMatrix celestial_rotated_view;
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    guVector axis{1, 0, 0};
    guMtxRotAxisDeg(celestial_rotated_view.mtx, &axis, get_celestial_angle() * 360.0f);
    guMtxConcat(gertex::get_view_matrix().mtx, celestial_rotated_view.mtx, celestial_rotated_view.mtx);

    transform_view(celestial_rotated_view, player_pos);
    draw_stars();
    float size = 30.0f;
    float dist = 98.0f;

    // Draw sun
    use_texture(sun_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(vertex_property_t(vec3f(-size, +dist, +size), 0, 1));
    GX_Vertex(vertex_property_t(vec3f(+size, +dist, +size), 1, 1));
    GX_Vertex(vertex_property_t(vec3f(+size, +dist, -size), 1, 0));
    GX_Vertex(vertex_property_t(vec3f(-size, +dist, -size), 0, 0));
    GX_EndGroup();

    // Draw moon
    use_texture(moon_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(vertex_property_t(vec3f(-size, -dist, -size), 0, 0));
    GX_Vertex(vertex_property_t(vec3f(+size, -dist, -size), 1, 0));
    GX_Vertex(vertex_property_t(vec3f(+size, -dist, +size), 1, 1));
    GX_Vertex(vertex_property_t(vec3f(-size, -dist, +size), 0, 1));
    GX_EndGroup();

    // Enable fog
    gertex::use_fog(true);

    // Use default blend mode
    gertex::set_blending(gertex::GXBlendMode::normal);
    GX_SetAlphaUpdate(GX_TRUE);
    gertex::set_alpha_cutoff(0);
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);

    // Here we use 0 fractional bits for the position data, because we're drawing large objects.
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    constexpr float scale = 1.0f / BASE3D_POS_FRAC;
    // Clouds texture is massive.
    size = 2048.0f * scale;

    // The clouds are placed at y=108
    dist = 108.0f * scale;

    // Reset transform
    transform_view(gertex::get_view_matrix(), guVector{0, 0, 0});

    // Clouds are moving at a speed of 0.05 blocks per tick.
    float uv_offset = (current_world->ticks % 40960 + current_world->partial_ticks) * 0.05f / 2048.0f;

    // Clouds are a bit yellowish white. They are also affected by the time
    float sky_multiplier = get_sky_multiplier();
    uint8_t sky_r = (sky_multiplier * 229.5f + 25.5f);
    uint8_t sky_g = (sky_multiplier * 229.5f + 25.5f);
    uint8_t sky_b = (sky_multiplier * 216.75f + 38.25f);

    // Draw clouds
    use_texture(clouds_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(vertex_property_t(vec3f(-size, dist, +size), 0, 2 + uv_offset, sky_r, sky_g, sky_b));
    GX_Vertex(vertex_property_t(vec3f(+size, dist, +size), 2, 2 + uv_offset, sky_r, sky_g, sky_b));
    GX_Vertex(vertex_property_t(vec3f(+size, dist, -size), 2, 0 + uv_offset, sky_r, sky_g, sky_b));
    GX_Vertex(vertex_property_t(vec3f(-size, dist, -size), 0, 0 + uv_offset, sky_r, sky_g, sky_b));
    GX_EndGroup();
}
