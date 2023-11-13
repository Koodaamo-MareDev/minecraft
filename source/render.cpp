#include "render.hpp"

#include "chunk_new.hpp"
#include "texturedefs.h"
#include "base3d.hpp"

float get_face_light(vec3i pos, uint8_t face)
{
    vec3i other = pos + face_offsets[face];
    block_t *other_block = get_block_at(other);
    float facelight = 0.05f;
    if (other_block)
        facelight = std::min(1.0f, other_block->get_skylight() / 15.0f + 0.05f);
    switch (face)
    {
    case 0:
    case 1:
        return facelight * 0.90f;
    case 2:
        return facelight * 0.85f;
    case 3:
        return facelight;
    default:
        return facelight * 0.98f;
    }
}
int render_face(vec3i pos, uint8_t face, uint32_t texture_index)
{
    if (!base3d_is_drawing || face >= 6)
    {
        // This is just a dummy call to GX_Vertex to keep buffer size up to date.
        for (int i = 0; i < 4; i++)
            GX_Vertex(vertex_property_t());
        return 4;
    }
    vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    float lightAmount = get_face_light(pos, face);
    switch (face)
    {
    case 0:
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lightAmount);
        break;
    case 1:
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lightAmount);
        break;
    case 2:
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lightAmount);
        break;
    case 3:
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lightAmount);
        break;
    case 4:
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lightAmount);
        break;
    case 5:
        GX_VertexLit({vertex_pos + vec3f{-.5f, -.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lightAmount);
        GX_VertexLit({vertex_pos + vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lightAmount);
        break;
    default:
        break;
    }
    return 4;
}