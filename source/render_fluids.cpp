#include "render_fluids.hpp"
#include "chunk.hpp"
#include "blocks.hpp"
#include "render.hpp"

inline static int DrawHorizontalQuad(vertex_property_t p0, vertex_property_t p1, vertex_property_t p2, vertex_property_t p3, uint8_t light)
{
    vertex_property_t vertices[4] = {p0, p1, p2, p3};
    uint8_t curr = 0;
    // Find the vertex with smallest y position and start there.
    vertex_property_t min_vertex = vertices[0];
    for (int i = 1; i < 4; i++)
    {
        if (vertices[i].pos.y < min_vertex.pos.y)
            min_vertex = vertices[i];
    }
    curr = min_vertex.index;
    for (int i = 0; i < 3; i++)
    {
        GX_VertexLit(vertices[curr], light, 3);
        curr = (curr + 1) & 3;
    }
    curr = (min_vertex.index + 2) & 3;
    for (int i = 0; i < 3; i++)
    {
        GX_VertexLit(vertices[curr], light, 3);
        curr = (curr + 1) & 3;
    }
    return 2;
}

inline static int DrawVerticalQuad(vertex_property_t p0, vertex_property_t p1, vertex_property_t p2, vertex_property_t p3, uint8_t light)
{
    vertex_property_t vertices[4] = {p0, p1, p2, p3};
    int faceCount = 0;
    uint8_t curr = 0;
    if (p0.y_uv != p1.y_uv)
    {
        for (int i = 0; i < 3; i++)
        {
            GX_VertexLit(vertices[curr], light, 3);
            curr = (curr + 1) & 3;
        }
        faceCount++;
    }
    curr = 2;
    if (p2.y_uv != p3.y_uv)
    {
        for (int i = 0; i < 3; i++)
        {
            GX_VertexLit(vertices[curr], light, 3);
            curr = (curr + 1) & 3;
        }
        faceCount++;
    }
    return faceCount;
}

int render_fluid(chunk_t &chunk, block_t *block, const vec3i &pos)
{
    BlockID block_id = block->get_blockid();

    int faceCount = 0;
    vec3i local_pos = {pos.x & 0xF, pos.y & 0xF, pos.z & 0xF};

    // Used to check block types around the fluid
    block_t *neighbors[6];
    BlockID neighbor_ids[6];

    // Sorted maximum and minimum values of the corners above
    int corner_min[4];
    int corner_max[4];

    // These determine the placement of the quad
    float corner_bottoms[4];
    float corner_tops[4];

    get_neighbors(pos, neighbors, &chunk);
    for (int x = 0; x < 6; x++)
    {
        neighbor_ids[x] = neighbors[x] ? neighbors[x]->get_blockid() : BlockID::air;
    }

    if (!base3d_is_drawing)
    {
        if (!is_same_fluid(block_id, neighbor_ids[FACE_NY]) && (!is_solid(neighbor_ids[FACE_NY]) || properties(neighbor_ids[FACE_NY]).m_transparent))
            faceCount += 2;
        if (!is_same_fluid(block_id, neighbor_ids[FACE_PY]))
            faceCount += 2;
        if (!is_same_fluid(block_id, neighbor_ids[FACE_NZ]) && (!is_solid(neighbor_ids[FACE_NZ]) || properties(neighbor_ids[FACE_NZ]).m_transparent))
            faceCount += 2;
        if (!is_same_fluid(block_id, neighbor_ids[FACE_PZ]) && (!is_solid(neighbor_ids[FACE_PZ]) || properties(neighbor_ids[FACE_PZ]).m_transparent))
            faceCount += 2;
        if (!is_same_fluid(block_id, neighbor_ids[FACE_NX]) && (!is_solid(neighbor_ids[FACE_NX]) || properties(neighbor_ids[FACE_NX]).m_transparent))
            faceCount += 2;
        if (!is_same_fluid(block_id, neighbor_ids[FACE_PX]) && (!is_solid(neighbor_ids[FACE_PX]) || properties(neighbor_ids[FACE_PX]).m_transparent))
            faceCount += 2;
        return faceCount * 3;
    }

    vec3i corner_offsets[4] = {
        {0, 0, 0},
        {0, 0, 1},
        {1, 0, 1},
        {1, 0, 0},
    };
    for (int i = 0; i < 4; i++)
    {
        corner_max[i] = (get_fluid_visual_level(pos + corner_offsets[i], block_id, &chunk) << 1);
        if (!corner_max[i])
            corner_max[i] = 1;
        corner_min[i] = 0;
        corner_tops[i] = float(corner_max[i]) * 0.0625f;
        corner_bottoms[i] = 0.0f;
    }

    uint8_t light = block->light;

    // TEXTURE HANDLING: Check surrounding water levels > &chunk:
    // If surrounded by 1, the water texture is flowing to the opposite direction
    // If surrounded by 2 in I-shaped pattern, the water texture is still
    // If surrounded by 2 diagonally, the water texture is flowing diagonally to the other direction
    // If surrounded by 3, the water texture is flowing to the 1 other direction
    // If surrounded by 4, the water texture is still

    uint8_t texture_offset = block_properties[uint8_t(flowfluid(block_id))].m_texture_index;

    uint32_t texture_start_x = TEXTURE_X(texture_offset);
    uint32_t texture_start_y = TEXTURE_Y(texture_offset);
    uint32_t texture_end_x = texture_start_x + UV_SCALE;
    uint32_t texture_end_y = texture_start_y + UV_SCALE;

    vertex_property_t bottomPlaneCoords[4] = {
        {(local_pos + vec3f{-.5f, -.5f, -.5f}), texture_start_x, texture_end_y},
        {(local_pos + vec3f{-.5f, -.5f, +.5f}), texture_start_x, texture_start_y},
        {(local_pos + vec3f{+.5f, -.5f, +.5f}), texture_end_x, texture_start_y},
        {(local_pos + vec3f{+.5f, -.5f, -.5f}), texture_end_x, texture_end_y},
    };
    if (!is_same_fluid(block_id, neighbor_ids[FACE_NY]) && (!is_solid(neighbor_ids[FACE_NY]) || properties(neighbor_ids[FACE_NY]).m_transparent))
        faceCount += DrawHorizontalQuad(bottomPlaneCoords[0], bottomPlaneCoords[1], bottomPlaneCoords[2], bottomPlaneCoords[3], neighbors[FACE_NY] ? neighbors[FACE_NY]->light : light);

    vec3f direction = get_fluid_direction(block, pos, &chunk);
    float angle = -1000;
    float cos_angle = 8;
    float sin_angle = 0;

    if (direction.x != 0 || direction.z != 0)
    {
        angle = std::atan2(direction.x, direction.z) + M_PI;
        cos_angle = std::cos(angle) * 8;
        sin_angle = std::sin(angle) * 8;
    }

    uint32_t tex_off_x = TEXTURE_X(texture_offset) + 16;
    uint32_t tex_off_y = TEXTURE_Y(texture_offset) + 16;
    if (angle < -999)
    {
        int basefluid_offset = texture_offset = block_properties[uint8_t(basefluid(block_id))].m_texture_index;
        tex_off_x = TEXTURE_X(basefluid_offset) + 8;
        tex_off_y = TEXTURE_Y(basefluid_offset) + 8;
    }

    vertex_property_t topPlaneCoords[4] = {
        {(local_pos + vec3f{+.5f, -.5f + corner_tops[3], -.5f}), uint32_t(tex_off_x - cos_angle - sin_angle), uint32_t(tex_off_y - cos_angle + sin_angle)},
        {(local_pos + vec3f{+.5f, -.5f + corner_tops[2], +.5f}), uint32_t(tex_off_x - cos_angle + sin_angle), uint32_t(tex_off_y + cos_angle + sin_angle)},
        {(local_pos + vec3f{-.5f, -.5f + corner_tops[1], +.5f}), uint32_t(tex_off_x + cos_angle + sin_angle), uint32_t(tex_off_y + cos_angle - sin_angle)},
        {(local_pos + vec3f{-.5f, -.5f + corner_tops[0], -.5f}), uint32_t(tex_off_x + cos_angle - sin_angle), uint32_t(tex_off_y - cos_angle - sin_angle)},
    };
    if (!is_same_fluid(block_id, neighbor_ids[FACE_PY]))
        faceCount += DrawHorizontalQuad(topPlaneCoords[0], topPlaneCoords[1], topPlaneCoords[2], topPlaneCoords[3], is_solid(neighbor_ids[FACE_PY]) ? light : neighbors[FACE_PY]->light);

    texture_offset = get_default_texture_index(flowfluid(block_id));

    vertex_property_t sideCoords[4] = {0};

    texture_start_x = TEXTURE_X(texture_offset);
    texture_end_x = texture_start_x + UV_SCALE;
    texture_start_y = TEXTURE_Y(texture_offset);

    sideCoords[0].x_uv = sideCoords[1].x_uv = texture_start_x;
    sideCoords[2].x_uv = sideCoords[3].x_uv = texture_end_x;
    sideCoords[3].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[0], -.5f};
    sideCoords[2].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[0], -.5f};
    sideCoords[1].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[3], -.5f};
    sideCoords[0].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[3], -.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[0];
    sideCoords[2].y_uv = texture_start_y + corner_max[0];
    sideCoords[1].y_uv = texture_start_y + corner_max[3];
    sideCoords[0].y_uv = texture_start_y + corner_min[3];
    if (!is_same_fluid(block_id, neighbor_ids[FACE_NZ]) && (!is_solid(neighbor_ids[FACE_NZ]) || properties(neighbor_ids[FACE_NZ]).m_transparent))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_NZ]->light);

    sideCoords[3].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[2], +.5f};
    sideCoords[2].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[2], +.5f};
    sideCoords[1].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[1], +.5f};
    sideCoords[0].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[1], +.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[2];
    sideCoords[2].y_uv = texture_start_y + corner_max[2];
    sideCoords[1].y_uv = texture_start_y + corner_max[1];
    sideCoords[0].y_uv = texture_start_y + corner_min[1];
    if (!is_same_fluid(block_id, neighbor_ids[FACE_PZ]) && (!is_solid(neighbor_ids[FACE_PZ]) || properties(neighbor_ids[FACE_PZ]).m_transparent))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_PZ]->light);

    sideCoords[3].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[1], +.5f};
    sideCoords[2].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[1], +.5f};
    sideCoords[1].pos = local_pos + vec3f{-.5f, -.5f + corner_tops[0], -.5f};
    sideCoords[0].pos = local_pos + vec3f{-.5f, -.5f + corner_bottoms[0], -.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[1];
    sideCoords[2].y_uv = texture_start_y + corner_max[1];
    sideCoords[1].y_uv = texture_start_y + corner_max[0];
    sideCoords[0].y_uv = texture_start_y + corner_min[0];
    if (!is_same_fluid(block_id, neighbor_ids[FACE_NX]) && (!is_solid(neighbor_ids[FACE_NX]) || properties(neighbor_ids[FACE_NX]).m_transparent))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_NX]->light);

    sideCoords[3].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[3], -.5f};
    sideCoords[2].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[3], -.5f};
    sideCoords[1].pos = local_pos + vec3f{+.5f, -.5f + corner_tops[2], +.5f};
    sideCoords[0].pos = local_pos + vec3f{+.5f, -.5f + corner_bottoms[2], +.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[3];
    sideCoords[2].y_uv = texture_start_y + corner_max[3];
    sideCoords[1].y_uv = texture_start_y + corner_max[2];
    sideCoords[0].y_uv = texture_start_y + corner_min[2];
    if (!is_same_fluid(block_id, neighbor_ids[FACE_PX]) && (!is_solid(neighbor_ids[FACE_PX]) || properties(neighbor_ids[FACE_PX]).m_transparent))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_PX]->light);
    return faceCount * 3;
}

int render_section_fluids(chunk_t &chunk, int index, bool transparent, int vertexCount)
{
    vec3i chunk_offset = vec3i(chunk.x * 16, index * 16, chunk.z * 16);

    GX_BeginGroup(GX_TRIANGLES, vertexCount);
    vertexCount = 0;
    block_t *block = chunk.get_block(chunk_offset);
    for (int _y = 0; _y < 16; _y++)
    {
        for (int _z = 0; _z < 16; _z++)
        {
            for (int _x = 0; _x < 16; _x++, block++)
            {
                vec3i blockpos = vec3i(_x, _y, _z) + chunk_offset;
                if (properties(block->id).m_fluid && transparent == properties(block->id).m_transparent)
                    vertexCount += render_fluid(chunk, block, blockpos);
            }
        }
    }
    GX_EndGroup();
    return vertexCount;
}
