#include "render_blocks.hpp"

#include <world/world.hpp>
#include <world/chunk.hpp>
#include <block/blocks.hpp>
#include <render/render.hpp>

int render_block(Block *block, const Vec3i &pos, bool transparent)
{
    if (!block->get_visibility() || properties(block->id).m_fluid || transparent != properties(block->id).m_transparent)
        return 0;
    if (transparent)
    {
        switch (properties(block->id).m_render_type)
        {
        case RenderType::cross:
            return render_cross(block, pos);
        case RenderType::special:
            return render_special(block, pos);
        case RenderType::flat_ground:
            return render_flat_ground(block, pos);
        case RenderType::slab:
            return render_slab(block, pos);
        default:
            break;
        }
    }
    else
    {
        switch (block->get_blockid())
        {
        case BlockID::chest:
            return render_chest(block, pos);
        case BlockID::snow_layer:
            return render_snow_layer(block, pos);
        default:
            break;
        }
    }
    if (properties(block->id).m_render_type == RenderType::full_special)
    {
        return render_cube_special(block, pos, transparent);
    }
    return render_cube(block, pos, transparent);
}

int render_cube(Block *block, const Vec3i &pos, bool transparent)
{
    int vertexCount = 0;
    for (uint8_t face = 0; face < 6; face++)
    {
        if (block->get_opacity(face))
            vertexCount += render_face(pos, face, get_default_texture_index(block->get_blockid()), block);
    }
    return vertexCount;
}

int render_inverted_cube(Block *block, const Vec3i &pos, bool transparent)
{
    int vertexCount = 0;
    for (uint8_t face = 0; face < 6; face++)
    {
        if (block->get_opacity(face))
            vertexCount += render_back_face(pos, face, get_default_texture_index(block->get_blockid()), block);
    }
    return vertexCount;
}

int render_inverted_cube_special(Block *block, const Vec3i &pos, bool transparent)
{
    int vertexCount = 0;
    for (uint8_t face = 0; face < 6; face++)
    {
        if (block->get_opacity(face))
            vertexCount += render_back_face(pos, face, get_face_texture_index(block, face), block);
    }
    return vertexCount;
}

int render_cube_special(Block *block, const Vec3i &pos, bool transparent)
{
    int vertexCount = 0;
    for (uint8_t face = 0; face < 6; face++)
    {
        if (block->get_opacity(face))
            vertexCount += render_face(pos, face, get_face_texture_index(block, face), block);
    }
    return vertexCount;
}

int render_special(Block *block, const Vec3i &pos)
{
    switch (block->get_blockid())
    {
    case BlockID::torch:
    case BlockID::unlit_redstone_torch:
    case BlockID::redstone_torch:
    case BlockID::lever:
        return render_torch(block, pos);
    case BlockID::wooden_door:
    case BlockID::iron_door:
        return render_door(block, pos);
    case BlockID::cactus:
        return render_cactus(block, pos);
    case BlockID::leaves:
        return render_leaves(block, pos);
    default:
        return 0;
    }
}

int render_flat_ground(Block *block, const Vec3i &pos)
{
    uint8_t lighting = block->light;
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_default_texture_index(block->get_blockid());
    GX_VertexLit({vertex_pos + Vec3f{0.5, -.4375, -.5}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + Vec3f{0.5, -.4375, 0.5}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + Vec3f{-.5, -.4375, 0.5}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + Vec3f{-.5, -.4375, -.5}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PY);
    return 4;
}

int render_snow_layer(Block *block, const Vec3i &pos)
{
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_default_texture_index(block->get_blockid());
    int vertexCount = 4;

    // Top
    render_face(pos, FACE_PY, texture_index, block, 0, 2);

    BlockID neighbor_ids[6];
    {
        Block *neighbors[6];
        if (render_world)
            render_world->get_neighbors(pos, neighbors);
        for (int i = 0; i < 6; i++)
        {
            neighbor_ids[i] = neighbors[i] ? neighbors[i]->get_blockid() : BlockID::air;
        }
    }

    if (block->get_opacity(FACE_NX) && neighbor_ids[FACE_NX] != BlockID::snow_layer)
    {
        // Negative X
        render_face(pos, FACE_NX, texture_index, block, 0, 2);
        vertexCount += 4;
    }
    if (block->get_opacity(FACE_PX) && neighbor_ids[FACE_PX] != BlockID::snow_layer)
    {
        // Positive X
        render_face(pos, FACE_PX, texture_index, block, 0, 2);
        vertexCount += 4;
    }
    if (block->get_opacity(FACE_NZ) && neighbor_ids[FACE_NZ] != BlockID::snow_layer)
    {
        // Negative Z
        render_face(pos, FACE_NZ, texture_index, block, 0, 2);
        vertexCount += 4;
    }
    if (block->get_opacity(FACE_PZ) && neighbor_ids[FACE_PZ] != BlockID::snow_layer)
    {
        // Positive Z
        render_face(pos, FACE_PZ, texture_index, block, 0, 2);
        vertexCount += 4;
    }
    return vertexCount;
}

int render_chest(Block *block, const Vec3i &pos)
{
    int vertexCount = 0;
    for (uint8_t face = 0; face < 6; face++)
    {
        if (block->get_opacity(face))
            vertexCount += render_face(pos, face, get_chest_texture_index(block, pos, face), block);
    }
    return vertexCount;
}

int render_torch(Block *block, const Vec3i &pos)
{
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y + 0.1875, local_pos.z);
    switch (block->meta & 0x7)
    {
    case 1: // Facing east
        vertex_pos.x -= 0.125;
        return render_torch_with_angle(block, vertex_pos, -0.4, 0);
    case 2: // Facing west
        vertex_pos.x += 0.125;
        return render_torch_with_angle(block, vertex_pos, 0.4, 0);
    case 3: // Facing south
        vertex_pos.z -= 0.125;
        return render_torch_with_angle(block, vertex_pos, 0, -0.4);
    case 4: // Facing north
        vertex_pos.z += 0.125;
        return render_torch_with_angle(block, vertex_pos, 0, 0.4);
    default: // Facing up
        vertex_pos.y -= 0.1875;
        return render_torch_with_angle(block, vertex_pos, 0, 0);
    }
}

int render_torch_with_angle(Block *block, const Vec3f &vertex_pos, float ax, float az)
{
    uint8_t lighting = block->light;
    uint32_t texture_index = get_default_texture_index(block->get_blockid());

    // Negative X side
    GX_VertexLit({vertex_pos + Vec3f{-.0625f + ax, -.5f, -.5f + az}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + Vec3f{-.0625f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + Vec3f{-.0625f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + Vec3f{-.0625f + ax, -.5f, 0.5f + az}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    // Positive X side
    GX_VertexLit({vertex_pos + Vec3f{0.0625f + ax, -.5f, 0.5f + az}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PX);
    GX_VertexLit({vertex_pos + Vec3f{0.0625f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PX);
    GX_VertexLit({vertex_pos + Vec3f{0.0625f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PX);
    GX_VertexLit({vertex_pos + Vec3f{0.0625f + ax, -.5f, -.5f + az}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PX);
    // Negative Z side
    GX_VertexLit({vertex_pos + Vec3f{0.5f + ax, -.5f, -.0625f + az}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + Vec3f{0.5f, 0.5f, -.0625f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + Vec3f{-.5f, 0.5f, -.0625f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + Vec3f{-.5f + ax, -.5f, -.0625f + az}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NZ);
    // Positive Z side
    GX_VertexLit({vertex_pos + Vec3f{-.5f + ax, -.5f, 0.0625f + az}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + Vec3f{-.5f, 0.5f, 0.0625f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + Vec3f{0.5f, 0.5f, 0.0625f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + Vec3f{0.5f + ax, -.5f, 0.0625f + az}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PZ);
    // Top side
    GX_VertexLit({vertex_pos + Vec3f{0.0625f + ax * 0.375f, 0.125f, -.0625f + az * 0.375f}, TEXTURE_X(texture_index) + 9. * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + 8. * BASE3D_BLOCK_UV_SCALE}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + Vec3f{0.0625f + ax * 0.375f, 0.125f, 0.0625f + az * 0.375f}, TEXTURE_X(texture_index) + 9. * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + 6. * BASE3D_BLOCK_UV_SCALE}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + Vec3f{-.0625f + ax * 0.375f, 0.125f, 0.0625f + az * 0.375f}, TEXTURE_X(texture_index) + 7. * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + 6. * BASE3D_BLOCK_UV_SCALE}, lighting, FACE_PY);
    GX_VertexLit({vertex_pos + Vec3f{-.0625f + ax * 0.375f, 0.125f, -.0625f + az * 0.375f}, TEXTURE_X(texture_index) + 7. * BASE3D_PIXEL_UV_SCALE, TEXTURE_Y(texture_index) + 8. * BASE3D_BLOCK_UV_SCALE}, lighting, FACE_PY);

    return 20;
}

int render_door(Block *block, const Vec3i &pos)
{
    uint8_t lighting = block->light;
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos = Vec3f(local_pos.x, local_pos.y, local_pos.z) - Vec3f(0.5);

    uint32_t texture_index = get_default_texture_index(block->get_blockid());
    bool replace_texture = texture_index != properties(block->id).m_texture_index;
    bool top_half = block->meta & 8;
    bool open = block->meta & 4;
    uint8_t direction = block->meta & 3;
    if (!open)
        direction = (direction + 3) & 3;
    if (!top_half && !replace_texture)
        texture_index += 16;

    constexpr vfloat_t door_thickness = 0.1875;

    AABB door_bounds(Vec3f(0, 0, 0), Vec3f(1, 1, 1));
    if (direction == 0)
        door_bounds.max.z -= 1 - door_thickness;
    else if (direction == 1)
        door_bounds.min.x += 1 - door_thickness;
    else if (direction == 2)
        door_bounds.min.z += 1 - door_thickness;
    else if (direction == 3)
        door_bounds.max.x -= 1 - door_thickness;

    // We'll calculate the vertices and UVs based on the door's bounds

    // Bottom face (we'll not render the bottom face of the top half)
    if (!top_half)
    {
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_Y(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_Y(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_Y(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_Y(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NY);
    }

    // Top face (we'll not render the top face of the bottom half)
    if (top_half)
    {
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_Y(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PY);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_Y(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PY);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_Y(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PY);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_Y(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PY);
    }

    bool flip_states[4] = {false, false, false, false};

    for (int i = 0; i < 4; i++)
    {
        int mirrorFlag = (direction >> 1) + ((i & 1) ^ direction);
        mirrorFlag += open;
        if ((mirrorFlag & 1) != 0)
            flip_states[i] = true;
    }

    // Negative X side
    if (flip_states[1])
    {
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_X(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_X(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_X(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_X(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NX);
    }
    else
    {
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_PX(texture_index) - (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_PX(texture_index) - (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_PX(texture_index) - (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NX);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_PX(texture_index) - (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NX);
    }

    // Positive X side
    if (flip_states[3])
    {
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_X(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_X(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_X(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_X(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PX);
    }
    else
    {
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_PX(texture_index) - (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_PX(texture_index) - (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_PX(texture_index) - (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PX);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_PX(texture_index) - (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PX);
    }

    // Negative Z side
    if (flip_states[2])
    {
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NZ);
    }
    else
    {
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_PX(texture_index) - (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.min.z), TEXTURE_PX(texture_index) - (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_PX(texture_index) - (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NZ);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.min.z), TEXTURE_PX(texture_index) - (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_NZ);
    }

    // Positive Z side
    if (flip_states[0])
    {
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PZ);
    }
    else
    {
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_PX(texture_index) - (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.max.y, door_bounds.max.z), TEXTURE_PX(texture_index) - (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.max.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_PX(texture_index) - (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PZ);
        GX_VertexLit({vertex_pos + Vec3f(door_bounds.min.x, door_bounds.min.y, door_bounds.max.z), TEXTURE_PX(texture_index) - (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE), TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE)}, lighting, FACE_PZ);
    }

    return 20;
}

int render_cactus(Block *block, const Vec3i &pos)
{
    uint8_t lighting = block->light;
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_default_texture_index(block->get_blockid());
    uint32_t top_texture_index = texture_index - 1;
    uint32_t bottom_texture_index = texture_index + 1;
    int vertexCount = 16;

    // Positive X side
    GX_VertexLit({vertex_pos + Vec3f{0.4375, -.5, 0.5}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PX);
    GX_VertexLit({vertex_pos + Vec3f{0.4375, 0.5, 0.5}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PX);
    GX_VertexLit({vertex_pos + Vec3f{0.4375, 0.5, -.5}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PX);
    GX_VertexLit({vertex_pos + Vec3f{0.4375, -.5, -.5}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PX);

    // Negative X side
    GX_VertexLit({vertex_pos + Vec3f{-.4375, -.5, -.5}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + Vec3f{-.4375, 0.5, -.5}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + Vec3f{-.4375, 0.5, 0.5}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + Vec3f{-.4375, -.5, 0.5}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);

    // Bottom face
    if (block->get_opacity(FACE_NY))
    {
        GX_VertexLit({vertex_pos + Vec3f{-.5, -.5, -.5}, TEXTURE_NX(bottom_texture_index), TEXTURE_PY(bottom_texture_index)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + Vec3f{-.5, -.5, 0.5}, TEXTURE_NX(bottom_texture_index), TEXTURE_NY(bottom_texture_index)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + Vec3f{0.5, -.5, 0.5}, TEXTURE_PX(bottom_texture_index), TEXTURE_NY(bottom_texture_index)}, lighting, FACE_NY);
        GX_VertexLit({vertex_pos + Vec3f{0.5, -.5, -.5}, TEXTURE_PX(bottom_texture_index), TEXTURE_PY(bottom_texture_index)}, lighting, FACE_NY);
        vertexCount += 4;
    }
    if (block->get_opacity(FACE_PY))
    {
        // Top face
        GX_VertexLit({vertex_pos + Vec3f{0.5, .5, -.5}, TEXTURE_NX(top_texture_index), TEXTURE_PY(top_texture_index)}, lighting, FACE_PY);
        GX_VertexLit({vertex_pos + Vec3f{0.5, .5, 0.5}, TEXTURE_NX(top_texture_index), TEXTURE_NY(top_texture_index)}, lighting, FACE_PY);
        GX_VertexLit({vertex_pos + Vec3f{-.5, .5, 0.5}, TEXTURE_PX(top_texture_index), TEXTURE_NY(top_texture_index)}, lighting, FACE_PY);
        GX_VertexLit({vertex_pos + Vec3f{-.5, .5, -.5}, TEXTURE_PX(top_texture_index), TEXTURE_PY(top_texture_index)}, lighting, FACE_PY);
        vertexCount += 4;
    }
    // Positive Z side
    GX_VertexLit({vertex_pos + Vec3f{-.5, -.5, 0.4375}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + Vec3f{-.5, 0.5, 0.4375}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + Vec3f{0.5, 0.5, 0.4375}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + Vec3f{0.5, -.5, 0.4375}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PZ);

    // Negative Z side
    GX_VertexLit({vertex_pos + Vec3f{0.5, -.5, -.4375}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + Vec3f{0.5, 0.5, -.4375}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + Vec3f{-.5, 0.5, -.4375}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + Vec3f{-.5, -.5, -.4375}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NZ);

    return vertexCount;
}
int render_leaves(Block *block, const Vec3i &pos)
{
    return render_cube_special(block, pos, !render_fast_leaves);
}
int render_cross(Block *block, const Vec3i &pos)
{
    uint8_t lighting = block->light;
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_default_texture_index(block->get_blockid());

    // Front face

    // NX, NZ -> PX, PZ
    GX_VertexLit({vertex_pos + Vec3f{-.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + Vec3f{-.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + Vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + Vec3f{0.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    // PX, NZ -> NX, PZ
    GX_VertexLit({vertex_pos + Vec3f{0.5f, -.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + Vec3f{0.5f, 0.5f, -.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + Vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NX);
    GX_VertexLit({vertex_pos + Vec3f{-.5f, -.5f, 0.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NX);

    // Back face

    // NX, PZ -> PX, NZ
    GX_VertexLit({vertex_pos + Vec3f{-.5f, -.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + Vec3f{-.5f, 0.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + Vec3f{0.5f, 0.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_PZ);
    GX_VertexLit({vertex_pos + Vec3f{0.5f, -.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_PZ);

    // PX, PZ -> NX, NZ
    GX_VertexLit({vertex_pos + Vec3f{0.5f, -.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + Vec3f{0.5f, 0.5f, 0.5f}, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + Vec3f{-.5f, 0.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_NY(texture_index)}, lighting, FACE_NZ);
    GX_VertexLit({vertex_pos + Vec3f{-.5f, -.5f, -.5f}, TEXTURE_PX(texture_index), TEXTURE_PY(texture_index)}, lighting, FACE_NZ);

    return 16;
}

int render_slab(Block *block, const Vec3i &pos)
{
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);

    // Check for texture overrides
    uint32_t texture_index = get_default_texture_index(block->get_blockid());
    uint32_t top_index = texture_index;
    uint32_t side_index = top_index - 1;
    uint32_t bottom_index = top_index;
    bool render_top = true;
    bool render_bottom = true;
    bool top_half = block->meta & 8;
    int slab_type = block->meta & 7;
    switch (slab_type)
    {
    case 1: // Sandstone slab
        top_index = 176;
        side_index = 192;
        bottom_index = 208;
        break;
    case 2: // Wooden slab
        top_index = side_index = bottom_index = 4;
        break;
    case 3: // Cobblestone slab
        top_index = side_index = bottom_index = 16;
        break;
    case 4: // Brick slab
        top_index = side_index = bottom_index = 7;
        break;
    case 5: // Stone brick slab
        top_index = side_index = bottom_index = 54;
        break;
    default:
        break;
    }

    if (texture_index != properties(block->id).m_texture_index)
        bottom_index = side_index = top_index = texture_index;

    int vertexCount = 0;
    int min_y = top_half ? 8 : 0;
    int max_y = min_y + 8;
    if (top_half)
    {
        vertex_pos = vertex_pos + Vec3f(0, 0.5, 0);
        if (!block->get_opacity(FACE_PY))
            render_top = false;
    }
    else
    {
        if (!block->get_opacity(FACE_NY))
            render_bottom = false;
    }
    bool faces[6] = {
        block->get_opacity(FACE_NX) != 0,
        block->get_opacity(FACE_PX) != 0,
        render_bottom,
        render_top,
        block->get_opacity(FACE_NZ) != 0,
        block->get_opacity(FACE_PZ) != 0};

    for (int i = 0; i < 6; i++)
    {
        if (faces[i])
        {
            uint32_t face_texture_index = side_index;
            if (i == FACE_NY)
                face_texture_index = bottom_index;
            else if (i == FACE_PY)
                face_texture_index = top_index;
            vertexCount += render_face(pos, i, face_texture_index, block, min_y, max_y);
        }
    }
    return vertexCount;
}

int get_chest_texture_index(Block *block, const Vec3i &pos, uint8_t face)
{
    // Check for texture overrides
    uint32_t texture_index = get_default_texture_index(block->get_blockid());
    if (texture_index != properties(block->id).m_texture_index)
        return texture_index;

    // Bottom and top faces are always the same
    if (face == FACE_NY || face == FACE_PY)
        return 25;

    Block *neighbors[4];
    {
        Block *tmp_neighbors[6];
        if (render_world)
            render_world->get_neighbors(pos, tmp_neighbors);
        neighbors[0] = tmp_neighbors[0];
        neighbors[1] = tmp_neighbors[1];
        neighbors[2] = tmp_neighbors[4];
        neighbors[3] = tmp_neighbors[5];
    }

    if (std::none_of(neighbors, neighbors + 4, [](Block *block)
                     { return block && block->get_blockid() == BlockID::chest; }))
    {
        // Single chest
        uint8_t direction = FACE_PZ;
        if (!(block->visibility_flags & (1 << FACE_PZ)) && (block->visibility_flags & (1 << FACE_NZ)))
            direction = FACE_NZ;
        if (!(block->visibility_flags & (1 << FACE_NX)) && (block->visibility_flags & (1 << FACE_PX)))
            direction = FACE_PX;
        if (!(block->visibility_flags & (1 << FACE_PX)) && (block->visibility_flags & (1 << FACE_NX)))
            direction = FACE_NX;

        return 26 + (face == direction);
    }

    // Double chest

    if ((face != FACE_NZ && face != FACE_PZ) && neighbors[0]->get_blockid() != BlockID::chest && neighbors[1]->get_blockid() != BlockID::chest)
    {
        // X axis
        bool half = neighbors[2]->get_blockid() == BlockID::chest;
        uint8_t other_flags = neighbors[half ? 2 : 3]->visibility_flags;
        uint8_t direction = FACE_PX;
        if ((!(block->visibility_flags & (1 << FACE_PX)) || !(other_flags & (1 << FACE_PX))) && (block->visibility_flags & (1 << FACE_NX)) && (other_flags & (1 << FACE_NX)))
            direction = FACE_NX;

        if (face == FACE_NX)
            half = !half;

        return 26 + (face == direction ? 16 : 32) - half;
    }
    else if ((face != FACE_NX && face != FACE_PX) && neighbors[2]->get_blockid() != BlockID::chest && neighbors[3]->get_blockid() != BlockID::chest)
    {
        // Z axis
        bool half = neighbors[0]->get_blockid() == BlockID::chest;

        uint8_t other_flags = neighbors[half ? 0 : 1]->visibility_flags;
        uint8_t direction = FACE_PZ;
        if ((!(block->visibility_flags & (1 << FACE_PZ)) || !(other_flags & (1 << FACE_PZ))) && (block->visibility_flags & (1 << FACE_NZ)) && (other_flags & (1 << FACE_NZ)))
            direction = FACE_NZ;

        if (face == FACE_PZ)
            half = !half;

        return 26 + (face == direction ? 16 : 32) - half;
    }
    return 26;
}
