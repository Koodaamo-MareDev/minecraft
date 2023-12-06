#include "render.hpp"

#include "chunk_new.hpp"
#include "texturedefs.h"
#include "base3d.hpp"

uint8_t get_face_brightness(uint8_t face)
{
    switch (face)
    {
    case 0:
    case 1:
        return 153;
    case 2:
        return 127;
    case 4:
    case 5:
        return 204;
    default:
        return 255;
    }
}
extern uint8_t light_map[256];
float get_face_light_index(vec3i pos, uint8_t face)
{
    vec3i other = pos + face_offsets[face];
    block_t *other_block = get_block_at(other);
    if (!other_block)
        return get_block_at(pos)->light;
    return other_block->light;
}

int render_face(vec3i pos, uint8_t face, uint32_t texture_index)
{
    if (!base3d_is_drawing || face >= 6)
    {
        // This is just a dummy call to GX_Vertex to keep buffer size up to date.
        for (int i = 0; i < 4; i++)
            GX_VertexLit(vertex_property_t(), 0, 0);
        return 4;
    }
    vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint8_t lighting = get_face_light_index(pos, face);
    uint8_t brightness = get_face_brightness(face);
    switch (face)
    {
    case 0:
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, brightness);
        break;
    case 1:
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, brightness);
        break;
    case 2:
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, brightness);
        break;
    case 3:
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, brightness);
        break;
    case 4:
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, brightness);
        break;
    case 5:
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, brightness);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, brightness);
        break;
    default:
        break;
    }
    return 4;
}