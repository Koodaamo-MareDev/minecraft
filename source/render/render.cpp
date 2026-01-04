#include "render.hpp"
#include <render/render_blocks.hpp>

#include <ported/Random.hpp>

#include <block/blocks.hpp>
#include <world/world.hpp>

#include <new>

const GXColor sky_color = {0x88, 0xBB, 0xFF, 0xFF};

World *render_world = nullptr;

void set_render_world(World *world)
{
    render_world = world;
}

Camera &get_camera()
{
    static Camera camera = {
        {0.0, 0.0, 0.0},     // Camera rotation
        {0.0, 0.0, 0.0},     // Camera position
        90.0,                // Field of view
        1.0,                 // Aspect ratio
        gertex::CAMERA_NEAR, // Near clipping plane
        gertex::CAMERA_FAR   // Far clipping plane
    };
    return camera;
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

void smooth_light(const Vec3i &pos, uint8_t face_index, const Vec3i &vertex_off, Block *block, uint8_t &lighting, uint8_t &amb_occ)
{
    if (pos.y < 0 || pos.y > 255)
        return;
    Vec3i face = pos + face_offsets[face_index];
    uint8_t total = 1;
    Block *face_block = render_world ? render_world->get_block_at(face) : nullptr;
    if (!face_block)
        face_block = block;
    uint8_t block_light = face_block->block_light;
    uint8_t sky_light = face_block->sky_light;

    Vec3i vertex_offA(0, 0, 0);
    Vec3i vertex_offB(0, 0, 0);

    switch (face_index)
    {
    case FACE_NX:
    case FACE_PX:
    {
        vertex_offA = Vec3i(0, vertex_off.y, 0);
        vertex_offB = Vec3i(0, 0, vertex_off.z);
        break;
    }
    case FACE_NY:
    case FACE_PY:
    {
        vertex_offA = Vec3i(vertex_off.x, 0, 0);
        vertex_offB = Vec3i(0, 0, vertex_off.z);
        break;
    }
    case FACE_NZ:
    case FACE_PZ:
    {
        vertex_offA = Vec3i(vertex_off.x, 0, 0);
        vertex_offB = Vec3i(0, vertex_off.y, 0);
        break;
    }
    break;
    default:
        break;
    }
    Block *blockA = render_world ? render_world->get_block_at(face + vertex_offA) : nullptr;
    Block *blockB = render_world ? render_world->get_block_at(face + vertex_offB) : nullptr;
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
    Block *blockC = render_world ? render_world->get_block_at(face + vertex_off) : nullptr;
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
const Vec3i cube_vertex_offsets[6][4] = {
    {
        // FACE_NX
        Vec3i{0, -1, -1},
        Vec3i{0, 1, -1},
        Vec3i{0, 1, 1},
        Vec3i{0, -1, 1},
    },
    {
        // FACE_PX
        Vec3i{0, -1, 1},
        Vec3i{0, 1, 1},
        Vec3i{0, 1, -1},
        Vec3i{0, -1, -1},
    },
    {
        // FACE_NY
        Vec3i{-1, 0, -1},
        Vec3i{-1, 0, 1},
        Vec3i{1, 0, 1},
        Vec3i{1, 0, -1},
    },
    {
        // FACE_PY
        Vec3i{1, 0, -1},
        Vec3i{1, 0, 1},
        Vec3i{-1, 0, 1},
        Vec3i{-1, 0, -1},
    },
    {
        // FACE_NZ
        Vec3i{1, -1, 0},
        Vec3i{1, 1, 0},
        Vec3i{-1, 1, 0},
        Vec3i{-1, -1, 0},
    },
    {
        // FACE_PZ
        Vec3i{-1, -1, 0},
        Vec3i{-1, 1, 0},
        Vec3i{1, 1, 0},
        Vec3i{1, -1, 0},
    },
};

const Vec3i cube_vertices16[6][4] = {
    {
        // FACE_NX
        Vec3i{-16, -16, -16},
        Vec3i{-16, 16, -16},
        Vec3i{-16, 16, 16},
        Vec3i{-16, -16, 16},
    },
    {
        // FACE_PX
        Vec3i{16, -16, 16},
        Vec3i{16, 16, 16},
        Vec3i{16, 16, -16},
        Vec3i{16, -16, -16},
    },
    {
        // FACE_NY
        Vec3i{-16, -16, -16},
        Vec3i{-16, -16, 16},
        Vec3i{16, -16, 16},
        Vec3i{16, -16, -16},
    },
    {
        // FACE_PY
        Vec3i{16, 16, -16},
        Vec3i{16, 16, 16},
        Vec3i{-16, 16, 16},
        Vec3i{-16, 16, -16},
    },
    {
        // FACE_NZ
        Vec3i{16, -16, -16},
        Vec3i{16, 16, -16},
        Vec3i{-16, 16, -16},
        Vec3i{-16, -16, -16},
    },
    {
        // FACE_PZ
        Vec3i{-16, -16, 16},
        Vec3i{-16, 16, 16},
        Vec3i{16, 16, 16},
        Vec3i{16, -16, 16},
    },
};

const Vec3i cube_vertices[6][4] = {
    {
        // FACE_NX
        Vec3i{-1, -1, -1},
        Vec3i{-1, 1, -1},
        Vec3i{-1, 1, 1},
        Vec3i{-1, -1, 1},
    },
    {
        // FACE_PX
        Vec3i{1, -1, 1},
        Vec3i{1, 1, 1},
        Vec3i{1, 1, -1},
        Vec3i{1, -1, -1},
    },
    {
        // FACE_NY
        Vec3i{-1, -1, -1},
        Vec3i{-1, -1, 1},
        Vec3i{1, -1, 1},
        Vec3i{1, -1, -1},
    },
    {
        // FACE_PY
        Vec3i{1, 1, -1},
        Vec3i{1, 1, 1},
        Vec3i{-1, 1, 1},
        Vec3i{-1, 1, -1},
    },
    {
        // FACE_NZ
        Vec3i{1, -1, -1},
        Vec3i{1, 1, -1},
        Vec3i{-1, 1, -1},
        Vec3i{-1, -1, -1},
    },
    {
        // FACE_PZ
        Vec3i{-1, -1, 1},
        Vec3i{-1, 1, 1},
        Vec3i{1, 1, 1},
        Vec3i{1, -1, 1},
    },
};

Vec3i vertical_add(const Vec3i &a, uint8_t b)
{
    // Multiply a vector by a vertical factor
    // This is used to scale the vertices for rendering side faces
    return Vec3i(a.x * 16, a.y - 17 + b * 2, a.z * 16);
}

inline uint8_t get_face_light_index(Vec3i pos, uint8_t face, Block *default_block = nullptr)
{
    Vec3i other = pos + face_offsets[face];
    Block *other_block = render_world ? render_world->get_block_at(other) : nullptr;
    if (!other_block)
    {
        if (default_block)
            return default_block->light;
        return 255;
    }
    return other_block->light;
}

void get_face(Vec3i pos, uint8_t face, uint32_t texture_index, Block *block, uint8_t min_y, uint8_t max_y, Vertex16 *out_vertices, uint8_t *out_lighting, uint8_t *out_ao)
{
    Vec3i vertex_pos((pos.x & 0xF) << BASE3D_POS_FRAC_BITS, (pos.y & 0xF) << BASE3D_POS_FRAC_BITS, (pos.z & 0xF) << BASE3D_POS_FRAC_BITS);
    uint8_t light_val = get_face_light_index(pos, face, block);
    uint8_t ao[4] = {0, 0, 0, 0};
    uint8_t ao_no_op[4] = {0, 0, 0, 0};
    uint8_t lighting[4] = {light_val, light_val, light_val, light_val};
    Vertex16 vertices[4];
    uint8_t index = 0;
#define SMOOTH(ao_tgt) smooth_light(pos, face, cube_vertex_offsets[face][index], block, lighting[index], ao_tgt[index])
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
    if (out_vertices)
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            out_vertices[i] = vertices[i];
        }
    }
    if (out_lighting)
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            out_lighting[i] = lighting[i];
        }
    }
    if (out_ao)
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            out_ao[i] = ao[i];
        }
    }
}

int render_face(Vec3i pos, uint8_t face, uint32_t texture_index, Block *block, uint8_t min_y, uint8_t max_y)
{
    if (!base3d_is_drawing || face >= 6)
    {
        Vertex16 tmp;
        // This is just a dummy call to GX_Vertex to keep buffer size up to date.
        for (int i = 0; i < 4; i++)
            GX_VertexLit16(tmp, 0, 0);
        return 4;
    }
    uint8_t ao[4] = {0, 0, 0, 0};
    uint8_t lighting[4] = {0, 0, 0, 0};
    Vertex16 vertices[4];
    uint8_t index = 0;
    get_face(pos, face, texture_index, block, min_y, max_y, vertices, lighting, ao);
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

int render_back_face(Vec3i pos, uint8_t face, uint32_t texture_index, Block *block, uint8_t min_y, uint8_t max_y)
{
    if (!base3d_is_drawing || face >= 6)
    {
        static Vertex16 tmp;
        // This is just a dummy call to GX_Vertex to keep buffer size up to date.
        for (int i = 0; i < 4; i++)
            GX_VertexLit16(tmp, 0, 0);
        return 4;
    }
    uint8_t ao[4] = {0, 0, 0, 0};
    uint8_t lighting[4] = {0, 0, 0, 0};
    Vertex16 vertices[4];
    uint8_t index = 0;
    get_face(pos, face, texture_index, block, min_y, max_y, vertices, lighting, ao);
    if (texture_index >= 240 && texture_index < 250)
    {
        // Force full brightness for breaking block override
        lighting[0] = lighting[1] = lighting[2] = lighting[3] = 255;
        // Reset ambient occlusion
        ao[0] = ao[1] = ao[2] = ao[3] = 0;
        // Set the face to the top face
        face = FACE_PY;
    }
    // Reverse the order of vertices for back face rendering (3 - index)
    index = (ao[0] + ao[3] > ao[1] + ao[2]);
    GX_VertexLit16(vertices[3 - index], lighting[3 - index], face + ao[3 - index]);
    index = (index + 1) & 3;
    GX_VertexLit16(vertices[3 - index], lighting[3 - index], face + ao[3 - index]);
    index = (index + 1) & 3;
    GX_VertexLit16(vertices[3 - index], lighting[3 - index], face + ao[3 - index]);
    index = (index + 1) & 3;
    GX_VertexLit16(vertices[3 - index], lighting[3 - index], face + ao[3 - index]);
    return 4;
}

void render_single_block(Block &selected_block, bool transparency)
{
    // Precalculate the vertex count. Set position to Y = -16 to render "outside the world"
    int vertexCount = render_block(&selected_block, Vec3i(0, -16, 0), transparency);

    // Start drawing the block
    GX_BeginGroup(GX_QUADS, vertexCount);

    // Render the block. Set position to Y = -16 to render "outside the world"
    render_block(&selected_block, Vec3i(0, -16, 0), transparency);

    // End the group
    GX_EndGroup();
}

void render_single_block_at(Block &selected_block, Vec3i pos, bool transparency)
{
    // Precalculate the vertex count. Set position to Y = -16 to render "outside the world"
    int vertexCount = render_block(&selected_block, pos, transparency);

    // Start drawing the block
    GX_BeginGroup(GX_QUADS, vertexCount);

    // Render the block. Set position to Y = -16 to render "outside the world"
    render_block(&selected_block, pos, transparency);

    // End the group
    GX_EndGroup();
}

void render_single_item(uint32_t texture_index, bool transparency, uint8_t light)
{
    Vec3f vertex_pos = Vec3f(0, 0, 0);
    GX_BeginGroup(GX_QUADS, 4);
    // Render the selected block.
    GX_VertexLit({vertex_pos + Vec3f(0.5, -.5, 0), TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, light, FACE_PY);
    GX_VertexLit({vertex_pos + Vec3f(0.5, 0.5, 0), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, light, FACE_PY);
    GX_VertexLit({vertex_pos + Vec3f(-.5, 0.5, 0), TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, light, FACE_PY);
    GX_VertexLit({vertex_pos + Vec3f(-.5, -.5, 0), TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, light, FACE_PY);
    // End the group
    GX_EndGroup();
}

void render_item_pixel(uint32_t texture_index, uint8_t x, uint8_t y, bool x_next, bool y_next, uint8_t light)
{
    GX_BeginGroup(GX_QUADS, (1 + x_next + y_next) << 2);
    // Render the selected pixel.
    GX_VertexLit({Vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625, -.5), TEXTURE_X(texture_index) + (x + 1) * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + y * BASE3D_PIXEL_UV_SCALE}, light, FACE_PX);
    GX_VertexLit({Vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625 + 0.0625, -.5), TEXTURE_X(texture_index) + (x + 1) * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + (y + 1) * BASE3D_PIXEL_UV_SCALE}, light, FACE_PX);
    GX_VertexLit({Vec3f(+.5 - x * 0.0625, -.5 + y * 0.0625 + 0.0625, -.5), TEXTURE_X(texture_index) + x * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + (y + 1) * BASE3D_PIXEL_UV_SCALE}, light, FACE_PX);
    GX_VertexLit({Vec3f(+.5 - x * 0.0625, -.5 + y * 0.0625, -.5), TEXTURE_X(texture_index) + x * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + y * BASE3D_PIXEL_UV_SCALE}, light, FACE_PX);

    if (x_next)
    {
        // Render the selected pixel.
        GX_VertexLit({Vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625, -.625), TEXTURE_X(texture_index) + (x + 1) * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + y * BASE3D_PIXEL_UV_SCALE}, light, FACE_PZ);
        GX_VertexLit({Vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625 + 0.0625, -.625), TEXTURE_X(texture_index) + (x + 1) * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + (y + 1) * BASE3D_PIXEL_UV_SCALE}, light, FACE_PZ);
        GX_VertexLit({Vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625 + 0.0625, -.5), TEXTURE_X(texture_index) + x * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + (y + 1) * BASE3D_PIXEL_UV_SCALE}, light, FACE_PZ);
        GX_VertexLit({Vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625, -.5), TEXTURE_X(texture_index) + x * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + y * BASE3D_PIXEL_UV_SCALE}, light, FACE_PZ);
    }
    if (y_next)
    {
        // Render the selected pixel.
        GX_VertexLit({Vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625, -.625), TEXTURE_X(texture_index) + (x + 1) * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + y * BASE3D_PIXEL_UV_SCALE}, light, FACE_PY);
        GX_VertexLit({Vec3f(+.5 - x * 0.0625 - 0.0625, -.5 + y * 0.0625, -.5), TEXTURE_X(texture_index) + (x + 1) * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + (y + 1) * BASE3D_PIXEL_UV_SCALE}, light, FACE_PY);
        GX_VertexLit({Vec3f(+.5 - x * 0.0625, -.5 + y * 0.0625, -.5), TEXTURE_X(texture_index) + x * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + (y + 1) * BASE3D_PIXEL_UV_SCALE}, light, FACE_PY);
        GX_VertexLit({Vec3f(+.5 - x * 0.0625, -.5 + y * 0.0625, -.625), TEXTURE_X(texture_index) + x * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + y * BASE3D_PIXEL_UV_SCALE}, light, FACE_PY);
    }
    // End the group
    GX_EndGroup();
}

// Assuming a right-handed coordinate system and that the angles are in degrees. X = pitch, Y = yaw
Vec3f angles_to_vector(float x, float y)
{
    Vec3f result;
    float x_rad = M_DTOR * x;
    float y_rad = M_DTOR * y;
    result.x = -std::cos(x_rad) * std::sin(y_rad);
    result.y = std::sin(x_rad);
    result.z = -std::cos(x_rad) * std::cos(y_rad);
    return result;
}

// Assuming a right-handed coordinate system and that the angles are in degrees. X = pitch, Y = yaw
Vec3f vector_to_angles(const Vec3f &vec)
{
    Vec3f result;
    result.x = std::asin(-vec.y);
    result.y = std::atan2(vec.x, vec.z);

    // Convert to degrees
    result.x /= M_DTOR;
    result.y /= M_DTOR;

    return result;
}

bool is_cube_visible(const Frustum &frustum, const Vec3f &center, float size)
{
    vfloat_t r = (size * 0.5f) * M_SQRT3; // Radius of the cube's bounding sphere

    for (int i = 0; i < 6; ++i)
    {
        const Plane &plane = frustum.planes[i];
        vfloat_t dist = plane.direction.x * center.x +
                        plane.direction.y * center.y +
                        plane.direction.z * center.z +
                        plane.distance;

        if (dist > r)
            return false; // Completely outside
    }

    return true; // At least partially visible
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

    Camera &camera = get_camera();

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
    guMtxTrans(posmtx, camera.position.x, camera.position.y, camera.position.z);

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
    guMtxRotAxisDeg(rotx, &axis, camera.rot.x);
    axis.x = 0;
    axis.y = 1;
    axis.z = 0;
    guMtxRotAxisDeg(roty, &axis, camera.rot.y);
    axis.x = 0;
    axis.y = 0;
    axis.z = 1;
    guMtxRotAxisDeg(rotz, &axis, camera.rot.z);

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

void draw_particle(Camera &camera, Vec3f pos, uint32_t texture_index, float size, uint8_t brightness)
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

        GX_VertexLit(Vertex(vertex, TEXTURE_X(texture_index) + x * 4, TEXTURE_Y(texture_index) + y * 4), brightness);
    }
    GX_EndGroup();
}

void draw_particles(Camera &camera, Particle *particles, int count)
{

    // Bake vertex properties
    Mtx44 rot_mtx;
    Vertex vertices[4];
    for (int i = 0; i < 4; i++)
    {
        int x = (i == 0 || i == 3);
        int y = (i > 1);
        guVector vertex{(x - 0.5f), (y - 0.5f), 0};

        guMtxRotDeg(rot_mtx, 'x', camera.rot.x);
        guVecMultiply(rot_mtx, &vertex, &vertex);
        guMtxRotDeg(rot_mtx, 'y', camera.rot.y);
        guVecMultiply(rot_mtx, &vertex, &vertex);

        vertices[i] = Vertex(vertex, (x << 2) * BASE3D_PIXEL_UV_SCALE, (y << 2) * BASE3D_PIXEL_UV_SCALE);
    }

    // Draw particles on a per-type basis
    for (int t = 0; t < PTYPE_MAX; t++)
    {
        // Enumerate visible particles
        int visible_count = 0;
        for (int i = 0; i < count; i++)
        {
            Particle &particle = particles[i];
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
            Particle &particle = particles[i];
            if (particle.life_time && particle.type == t)
            {
                for (int j = 0; j < 4; j++)
                {
                    Vertex vertex = vertices[j];
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

Vec3f cross(const Vec3f &a, const Vec3f &b)
{
    return Vec3f(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

Vec3f rotate_vector(const Vec3f &v, const Vec3f &rot_deg)
{
    float rx = rot_deg.x * (M_DTOR);
    float ry = rot_deg.y * (M_DTOR);
    float rz = rot_deg.z * (M_DTOR);

    Vec3f res = v;

    // Rotate around Z
    res = Vec3f(
        res.x * cos(rz) - res.y * sin(rz),
        res.x * sin(rz) + res.y * cos(rz),
        res.z);

    // Rotate around X
    res = Vec3f(
        res.x,
        res.y * cos(rx) - res.z * sin(rx),
        res.y * sin(rx) + res.z * cos(rx));

    // Rotate around Y
    res = Vec3f(
        res.x * cos(ry) + res.z * sin(ry),
        res.y,
        -res.x * sin(ry) + res.z * cos(ry));

    return res;
}

void draw_frustum(const Camera &cam)
{
    use_texture(white_texture);
    float nearH = tan(cam.fov * 0.5f * M_DTOR) * cam.near;
    float nearW = nearH * cam.aspect;
    float farH = tan(cam.fov * 0.5f * M_DTOR) * cam.far;
    float farW = farH * cam.aspect;

    // Camera orientation vectors
    Vec3f forward = rotate_vector(Vec3f(0, 0, -1), cam.rot);
    Vec3f up = rotate_vector(Vec3f(0, 1, 0), cam.rot);
    Vec3f right = cross(forward, up).normalize();

    Vec3f camPos = cam.position;

    Vec3f nearCenter = camPos + forward * cam.near;
    Vec3f farCenter = camPos + forward * cam.far;

    // 8 corners of the frustum
    Vec3f ntl = nearCenter + up * nearH - right * nearW;
    Vec3f ntr = nearCenter + up * nearH + right * nearW;
    Vec3f nbl = nearCenter - up * nearH - right * nearW;
    Vec3f nbr = nearCenter - up * nearH + right * nearW;

    Vec3f ftl = farCenter + up * farH - right * farW;
    Vec3f ftr = farCenter + up * farH + right * farW;
    Vec3f fbl = farCenter - up * farH - right * farW;
    Vec3f fbr = farCenter - up * farH + right * farW;

    // Draw each frustum face using GX

    // Use floats for vertex positions
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);

    auto draw_quad = [](Vec3f a, Vec3f b, Vec3f c, Vec3f d, uint8_t r, uint8_t g, uint8_t bcol)
    {
        GX_BeginGroup(GX_QUADS, 4);
        GX_VertexF({d, 0, 1, r, g, bcol, 127});
        GX_VertexF({c, 1, 1, r, g, bcol, 127});
        GX_VertexF({b, 1, 0, r, g, bcol, 127});
        GX_VertexF({a, 0, 0, r, g, bcol, 127});
        GX_EndGroup();
    };

    // Near plane - RED
    draw_quad(nbl, nbr, ntr, ntl, 255, 0, 0);
    // Far plane - GREEN
    draw_quad(fbr, fbl, ftl, ftr, 0, 255, 0);
    // Left plane - BLUE
    draw_quad(fbl, nbl, ntl, ftl, 0, 0, 255);
    // Right plane - YELLOW
    draw_quad(nbr, fbr, ftr, ntr, 255, 255, 0);
    // Top plane - CYAN
    draw_quad(ntl, ntr, ftr, ftl, 0, 255, 255);
    // Bottom plane - MAGENTA
    draw_quad(fbl, fbr, nbr, nbl, 255, 0, 255);
}

// Returns normalized normal and signed plane distance
Plane make_plane(const Vec3f &a, const Vec3f &b, const Vec3f &c)
{
    Vec3f ab = b - a;
    Vec3f ac = c - a;
    Vec3f normal = cross(ab, ac).normalize();
    float distance = -(normal.x * a.x + normal.y * a.y + normal.z * a.z);
    return {normal, distance};
}

void build_frustum(const Camera &cam, Frustum &frustum)
{
    float nearH = tan(cam.fov * 0.5f * M_DTOR) * cam.near;
    float nearW = nearH * cam.aspect;
    float farH = tan(cam.fov * 0.5f * M_DTOR) * cam.far;
    float farW = farH * cam.aspect;

    Vec3f forward = rotate_vector(Vec3f(0, 0, -1), cam.rot);
    Vec3f up = rotate_vector(Vec3f(0, 1, 0), cam.rot);
    Vec3f right = cross(forward, up).normalize();

    Vec3f camPos = cam.position;
    Vec3f nearCenter = camPos + forward * cam.near;
    Vec3f farCenter = camPos + forward * cam.far;

    Vec3f ntl = nearCenter + up * nearH - right * nearW;
    Vec3f ntr = nearCenter + up * nearH + right * nearW;
    Vec3f nbl = nearCenter - up * nearH - right * nearW;
    Vec3f nbr = nearCenter - up * nearH + right * nearW;

    Vec3f ftl = farCenter + up * farH - right * farW;
    Vec3f ftr = farCenter + up * farH + right * farW;
    Vec3f fbl = farCenter - up * farH - right * farW;
    Vec3f fbr = farCenter - up * farH + right * farW;

    // Define planes using counter-clockwise winding to keep normals facing *inward*
    frustum.planes[0] = make_plane(nbl, nbr, ntr); // Near
    frustum.planes[1] = make_plane(fbr, fbl, ftl); // Far
    frustum.planes[2] = make_plane(fbl, nbl, ntl); // Left
    frustum.planes[3] = make_plane(nbr, fbr, ftr); // Right
    frustum.planes[4] = make_plane(ntl, ntr, ftr); // Top
    frustum.planes[5] = make_plane(fbl, fbr, nbr); // Bottom
}

void draw_stars()
{
    static bool generated = false;
    static std::vector<Vec3f> vertices;
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
                    vertices[index++] = Vec3f(xDist + vertX, yDist + vertY, zDist + vertZ);
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
            GX_Vertex(Vertex(vertices[i ^ 3], 0, 0, brightness_level, brightness_level, brightness_level, brightness_level));
        GX_EndGroup();
    }
}

float get_celestial_angle()
{
    int daytime_ticks = (render_world->time_of_day % 24000);
    float normalized_daytime = ((float)daytime_ticks + render_world->partial_ticks) / 24000.0F - 0.25F;
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
    if (render_world && render_world->hell)
        return GXColor{0x20, 0, 0, 0xFF};

    Camera &camera = get_camera();

    float elevation_brightness = std::pow(std::clamp((camera.position.y) * 0.03125, 0.0, 1.0), 2.0);
    if (camera.position.y < -500.0)
    {
        elevation_brightness = std::clamp((camera.position.y + 500.0) / -250.0, 0.0, 1.0);
    }
    float sky_multiplier = get_sky_multiplier();
    float brightness = elevation_brightness * sky_multiplier;
    return GXColor{uint8_t(sky_color.r * brightness), uint8_t(sky_color.g * brightness), uint8_t(sky_color.b * brightness), 0xFF};
}

GXColor get_fog_color()
{
    if (render_world && render_world->hell)
        return GXColor{0x20, 0, 0, 0xFF};
    float sky_multiplier = get_sky_multiplier();
    return GXColor{uint8_t(0xC0 * sky_multiplier), uint8_t(0xD8 * sky_multiplier), uint8_t(0xFF * sky_multiplier), 0xFF};
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
    transform_view(mtx, get_camera().position);

    uint8_t quality = 16;
    float centerX = 0.0f;
    float centerY = 100.0f;
    float centerZ = 0.0f;

    GX_BeginGroup(GX_TRIANGLEFAN, quality + 2);

    // Draw the center vertex
    GX_VertexF(Vertex(Vec3f(centerX, centerY, centerZ), 0, 0, r, g, b, a));

    // Draw the outer vertices
    for (int i = 0; i <= quality; ++i)
    {
        float v = (float)i * (float)M_PI * -2.0f / (float)quality;
        float x = std::sin(v);
        float z = std::cos(v);
        GX_VertexF(Vertex(Vec3f(x * 120.0f, z * 120.0f, -z * 40.0f * sunrise_color[3]), 0, 0, r, g, b, 0));
    }
    GX_EndGroup();

    gertex::set_state(state);
}

void draw_sky()
{
    if (!render_world)
        return;
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

    float sky_alpha = get_sky_multiplier();
    if (sky_alpha > 0.0f)
    {
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

        // Use short vertex positions with no fractional bits
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);

        // Use default blend mode
        gertex::set_blending(gertex::GXBlendMode::normal);
        gertex::set_alpha_cutoff(0);

        transform_view(gertex::get_view_matrix(), get_camera().position);

        float sky_dist = FOG_DISTANCE * 2 / BASE3D_POS_FRAC;
        GXColor sky_color = get_sky_color();

        // Approach 2: Draw XZ cylinder (without caps) with radius = sky_dist, with 16 faces
        GX_BeginGroup(GX_TRIANGLES, 48);
        for (int i = 0; i < 16;)
        {
            float v1 = (float)i++ * (float)M_PI * 2.0f / 16.0f;
            float v2 = (float)i * (float)M_PI * 2.0f / 16.0f;
            float x1 = std::sin(v1);
            float z1 = std::cos(v1);
            float x2 = std::sin(v2);
            float z2 = std::cos(v2);
            GX_Vertex(Vertex(Vec3f(x1 * sky_dist, 0, z1 * sky_dist), 0, 1, sky_color.r, sky_color.g, sky_color.b, 0));
            GX_Vertex(Vertex(Vec3f(x2 * sky_dist, 0, z2 * sky_dist), 1, 1, sky_color.r, sky_color.g, sky_color.b, 0));
            GX_Vertex(Vertex(Vec3f(0, +1, 0), 1, 0, sky_color.r, sky_color.g, sky_color.b, uint8_t(sky_alpha * 255)));
        }
        GX_EndGroup();

        // Restore previous blend params
        gertex::set_blending(gertex::GXBlendMode::additive);
        GX_SetAlphaUpdate(GX_FALSE);
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
        gertex::use_fog(false);
    }

    // Use short vertex positions with fractional bits
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);

    // Prepare rendering the celestial bodies
    gertex::GXMatrix celestial_rotated_view;
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    guVector axis{1, 0, 0};
    guMtxRotAxisDeg(celestial_rotated_view.mtx, &axis, get_celestial_angle() * 360.0f);
    guMtxConcat(gertex::get_view_matrix().mtx, celestial_rotated_view.mtx, celestial_rotated_view.mtx);

    transform_view(celestial_rotated_view, get_camera().position);
    draw_stars();
    float size = 30.0f;
    float dist = 98.0f;

    // Draw sun
    use_texture(sun_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(Vertex(Vec3f(-size, +dist, +size), 0, 1));
    GX_Vertex(Vertex(Vec3f(+size, +dist, +size), 1, 1));
    GX_Vertex(Vertex(Vec3f(+size, +dist, -size), 1, 0));
    GX_Vertex(Vertex(Vec3f(-size, +dist, -size), 0, 0));
    GX_EndGroup();

    // Draw moon
    use_texture(moon_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(Vertex(Vec3f(-size, -dist, -size), 0, 0));
    GX_Vertex(Vertex(Vec3f(+size, -dist, -size), 1, 0));
    GX_Vertex(Vertex(Vec3f(+size, -dist, +size), 1, 1));
    GX_Vertex(Vertex(Vec3f(-size, -dist, +size), 0, 1));
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
    float uv_offset = (render_world->ticks % 40960 + render_world->partial_ticks) * 0.05f / 2048.0f;

    // Clouds are a bit yellowish white. They are also affected by the time
    float sky_multiplier = get_sky_multiplier();
    uint8_t sky_r = (sky_multiplier * 229.5f + 25.5f);
    uint8_t sky_g = (sky_multiplier * 229.5f + 25.5f);
    uint8_t sky_b = (sky_multiplier * 216.75f + 38.25f);

    // Draw clouds
    use_texture(clouds_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(Vertex(Vec3f(-size, dist, +size), 0, 2 + uv_offset, sky_r, sky_g, sky_b));
    GX_Vertex(Vertex(Vec3f(+size, dist, +size), 2, 2 + uv_offset, sky_r, sky_g, sky_b));
    GX_Vertex(Vertex(Vec3f(+size, dist, -size), 2, 0 + uv_offset, sky_r, sky_g, sky_b));
    GX_Vertex(Vertex(Vec3f(-size, dist, -size), 0, 0 + uv_offset, sky_r, sky_g, sky_b));
    GX_EndGroup();
}
