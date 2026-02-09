#include "render_fluids.hpp"

#include <world/world.hpp>
#include <world/chunk.hpp>
#include <block/blocks.hpp>
#include <render/render.hpp>

inline static int DrawHorizontalQuad(Vertex p0, Vertex p1, Vertex p2, Vertex p3, uint8_t light)
{
    Vertex vertices[4] = {p0, p1, p2, p3};
    uint8_t curr = 0;
    // Find the vertex with smallest y position and start there.
    Vertex min_vertex = vertices[0];
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

inline static int DrawVerticalQuad(Vertex p0, Vertex p1, Vertex p2, Vertex p3, uint8_t light)
{
    Vertex vertices[4] = {p0, p1, p2, p3};
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

Vec3f get_fluid_direction(World *world, Block *block, Vec3i pos)
{

    uint8_t fluid_level = get_fluid_meta_level(block);
    if ((fluid_level & 7) == 0)
        return Vec3f(0.0, -1.0, 0.0);

    BlockID block_id = block->blockid;

    // Used to check block types around the fluid
    Block *neighbors[6];
    world->get_neighbors(pos, neighbors);

    Vec3f direction = Vec3f(0.0, 0.0, 0.0);

    bool direction_set = false;
    for (int i = 0; i < 6; i++)
    {
        if (i == FACE_NY || i == FACE_PY)
            continue;
        if (neighbors[i] && is_same_fluid(neighbors[i]->blockid, block_id))
        {
            direction_set = true;
            if (get_fluid_meta_level(neighbors[i]) < fluid_level)
                direction = direction - Vec3f(face_offsets[i].x, 0, face_offsets[i].z);
            else if (get_fluid_meta_level(neighbors[i]) > fluid_level)
                direction = direction + Vec3f(face_offsets[i].x, 0, face_offsets[i].z);
            else
                direction_set = false;
        }
    }
    if (!direction_set)
        direction.y = -1.0;
    return direction.fast_normalize();
}

int render_fluid(Block *block, const Vec3i &pos, World *world)
{
    BlockID block_id = block->blockid;

    int faceCount = 0;
    Vec3i local_pos = {pos.x & 0xF, pos.y & 0xF, pos.z & 0xF};

    // Used to check block types around the fluid
    Block *neighbors[6];
    BlockID neighbor_ids[6];

    // Sorted maximum and minimum values of the corners above
    int corner_min[4];
    int corner_max[4];

    // These determine the placement of the quad
    float corner_bottoms[4];
    float corner_tops[4];

    if (world)
        world->get_neighbors(pos, neighbors);
    for (int x = 0; x < 6; x++)
    {
        neighbor_ids[x] = neighbors[x] ? neighbors[x]->blockid : BlockID::air;
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

    Vec3i corner_offsets[4] = {
        {0, 0, 0},
        {0, 0, 1},
        {1, 0, 1},
        {1, 0, 0},
    };
    for (int i = 0; i < 4; i++)
    {
        corner_max[i] = (get_fluid_visual_level(world, pos + corner_offsets[i], block_id) << 1);
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

    vfloat_t texture_start_x = TEXTURE_NX(texture_offset);
    vfloat_t texture_start_y = TEXTURE_NY(texture_offset);
    vfloat_t texture_end_x = TEXTURE_PX(texture_offset);
    vfloat_t texture_end_y = TEXTURE_PY(texture_offset);

    Vertex bottomPlaneCoords[4] = {
        {(local_pos + Vec3f{-.5f, -.5f, -.5f}), texture_start_x, texture_end_y},
        {(local_pos + Vec3f{-.5f, -.5f, +.5f}), texture_start_x, texture_start_y},
        {(local_pos + Vec3f{+.5f, -.5f, +.5f}), texture_end_x, texture_start_y},
        {(local_pos + Vec3f{+.5f, -.5f, -.5f}), texture_end_x, texture_end_y},
    };
    if (!is_same_fluid(block_id, neighbor_ids[FACE_NY]) && (!is_solid(neighbor_ids[FACE_NY]) || properties(neighbor_ids[FACE_NY]).m_transparent))
        faceCount += DrawHorizontalQuad(bottomPlaneCoords[0], bottomPlaneCoords[1], bottomPlaneCoords[2], bottomPlaneCoords[3], neighbors[FACE_NY] ? neighbors[FACE_NY]->light : light);

    Vec3f direction = get_fluid_direction(world, block, pos);
    float angle = -1000;
    float cos_angle = 8 * BASE3D_PIXEL_UV_SCALE;
    float sin_angle = 0;

    if (direction.x != 0 || direction.z != 0)
    {
        angle = std::atan2(direction.x, direction.z) + M_PI;
        cos_angle = std::cos(angle) * (8 * BASE3D_PIXEL_UV_SCALE);
        sin_angle = std::sin(angle) * (8 * BASE3D_PIXEL_UV_SCALE);
    }

    vfloat_t tex_off_x = TEXTURE_PX(texture_offset);
    vfloat_t tex_off_y = TEXTURE_PY(texture_offset);
    if (angle < -999)
    {
        int basefluid_offset = texture_offset = block_properties[uint8_t(basefluid(block_id))].m_texture_index;
        tex_off_x = TEXTURE_X(basefluid_offset) + (8 * BASE3D_PIXEL_UV_SCALE);
        tex_off_y = TEXTURE_Y(basefluid_offset) + (8 * BASE3D_PIXEL_UV_SCALE);
    }

    Vertex topPlaneCoords[4] = {
        {(local_pos + Vec3f{+.5f, -.5f + corner_tops[3], -.5f}), (tex_off_x - cos_angle - sin_angle), (tex_off_y - cos_angle + sin_angle)},
        {(local_pos + Vec3f{+.5f, -.5f + corner_tops[2], +.5f}), (tex_off_x - cos_angle + sin_angle), (tex_off_y + cos_angle + sin_angle)},
        {(local_pos + Vec3f{-.5f, -.5f + corner_tops[1], +.5f}), (tex_off_x + cos_angle + sin_angle), (tex_off_y + cos_angle - sin_angle)},
        {(local_pos + Vec3f{-.5f, -.5f + corner_tops[0], -.5f}), (tex_off_x + cos_angle - sin_angle), (tex_off_y - cos_angle - sin_angle)},
    };
    if (!is_same_fluid(block_id, neighbor_ids[FACE_PY]))
        faceCount += DrawHorizontalQuad(topPlaneCoords[0], topPlaneCoords[1], topPlaneCoords[2], topPlaneCoords[3], !neighbors[FACE_PY] || is_solid(neighbor_ids[FACE_PY]) ? light : neighbors[FACE_PY]->light);

    texture_offset = get_default_texture_index(flowfluid(block_id));

    Vertex sideCoords[4] = {0};

    texture_start_x = TEXTURE_NX(texture_offset);
    texture_end_x = TEXTURE_PX(texture_offset);
    texture_start_y = TEXTURE_NY(texture_offset);

    sideCoords[0].x_uv = sideCoords[1].x_uv = texture_start_x;
    sideCoords[2].x_uv = sideCoords[3].x_uv = texture_end_x;
    sideCoords[3].pos = local_pos + Vec3f{-.5f, -.5f + corner_bottoms[0], -.5f};
    sideCoords[2].pos = local_pos + Vec3f{-.5f, -.5f + corner_tops[0], -.5f};
    sideCoords[1].pos = local_pos + Vec3f{+.5f, -.5f + corner_tops[3], -.5f};
    sideCoords[0].pos = local_pos + Vec3f{+.5f, -.5f + corner_bottoms[3], -.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[0] * BASE3D_PIXEL_UV_SCALE;
    sideCoords[2].y_uv = texture_start_y + corner_max[0] * BASE3D_PIXEL_UV_SCALE;
    sideCoords[1].y_uv = texture_start_y + corner_max[3] * BASE3D_PIXEL_UV_SCALE;
    sideCoords[0].y_uv = texture_start_y + corner_min[3] * BASE3D_PIXEL_UV_SCALE;
    if (neighbors[FACE_NZ] && !is_same_fluid(block_id, neighbor_ids[FACE_NZ]) && (!is_solid(neighbor_ids[FACE_NZ]) || properties(neighbor_ids[FACE_NZ]).m_transparent))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_NZ]->light);

    sideCoords[3].pos = local_pos + Vec3f{+.5f, -.5f + corner_bottoms[2], +.5f};
    sideCoords[2].pos = local_pos + Vec3f{+.5f, -.5f + corner_tops[2], +.5f};
    sideCoords[1].pos = local_pos + Vec3f{-.5f, -.5f + corner_tops[1], +.5f};
    sideCoords[0].pos = local_pos + Vec3f{-.5f, -.5f + corner_bottoms[1], +.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[2] * BASE3D_PIXEL_UV_SCALE;
    sideCoords[2].y_uv = texture_start_y + corner_max[2] * BASE3D_PIXEL_UV_SCALE;
    sideCoords[1].y_uv = texture_start_y + corner_max[1] * BASE3D_PIXEL_UV_SCALE;
    sideCoords[0].y_uv = texture_start_y + corner_min[1] * BASE3D_PIXEL_UV_SCALE;
    if (neighbors[FACE_PZ] && !is_same_fluid(block_id, neighbor_ids[FACE_PZ]) && (!is_solid(neighbor_ids[FACE_PZ]) || properties(neighbor_ids[FACE_PZ]).m_transparent))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_PZ]->light);

    sideCoords[3].pos = local_pos + Vec3f{-.5f, -.5f + corner_bottoms[1], +.5f};
    sideCoords[2].pos = local_pos + Vec3f{-.5f, -.5f + corner_tops[1], +.5f};
    sideCoords[1].pos = local_pos + Vec3f{-.5f, -.5f + corner_tops[0], -.5f};
    sideCoords[0].pos = local_pos + Vec3f{-.5f, -.5f + corner_bottoms[0], -.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[1] * BASE3D_PIXEL_UV_SCALE;
    sideCoords[2].y_uv = texture_start_y + corner_max[1] * BASE3D_PIXEL_UV_SCALE;
    sideCoords[1].y_uv = texture_start_y + corner_max[0] * BASE3D_PIXEL_UV_SCALE;
    sideCoords[0].y_uv = texture_start_y + corner_min[0] * BASE3D_PIXEL_UV_SCALE;
    if (neighbors[FACE_NX] && !is_same_fluid(block_id, neighbor_ids[FACE_NX]) && (!is_solid(neighbor_ids[FACE_NX]) || properties(neighbor_ids[FACE_NX]).m_transparent))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_NX]->light);

    sideCoords[3].pos = local_pos + Vec3f{+.5f, -.5f + corner_bottoms[3], -.5f};
    sideCoords[2].pos = local_pos + Vec3f{+.5f, -.5f + corner_tops[3], -.5f};
    sideCoords[1].pos = local_pos + Vec3f{+.5f, -.5f + corner_tops[2], +.5f};
    sideCoords[0].pos = local_pos + Vec3f{+.5f, -.5f + corner_bottoms[2], +.5f};
    sideCoords[3].y_uv = texture_start_y + corner_min[3] * BASE3D_PIXEL_UV_SCALE;
    sideCoords[2].y_uv = texture_start_y + corner_max[3] * BASE3D_PIXEL_UV_SCALE;
    sideCoords[1].y_uv = texture_start_y + corner_max[2] * BASE3D_PIXEL_UV_SCALE;
    sideCoords[0].y_uv = texture_start_y + corner_min[2] * BASE3D_PIXEL_UV_SCALE;
    if (neighbors[FACE_PX] && !is_same_fluid(block_id, neighbor_ids[FACE_PX]) && (!is_solid(neighbor_ids[FACE_PX]) || properties(neighbor_ids[FACE_PX]).m_transparent))
        faceCount += DrawVerticalQuad(sideCoords[0], sideCoords[1], sideCoords[2], sideCoords[3], neighbors[FACE_PX]->light);
    return faceCount * 3;
}