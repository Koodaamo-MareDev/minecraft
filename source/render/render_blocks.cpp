#include "render_blocks.hpp"

#include <functional>
#include <world/world.hpp>
#include <world/chunk.hpp>
#include <block/blocks.hpp>
#include <render/render.hpp>
#include <gertex/displaylist.hpp>

static std::map<RenderType, std::function<int(gertex::DisplayList<gertex::Vertex16> *list, Block *, const Vec3i &)>> render_functions = {
    {RenderType::full, render_cube},
    {RenderType::full_special, render_cube_special},
    {RenderType::cross, render_cross},
    {RenderType::flat_ground, render_flat_ground},
    {RenderType::slab, render_slab},
    {RenderType::special, render_special},
    {RenderType::fluid, [](gertex::DisplayList<gertex::Vertex16> *list, Block *, const Vec3i &)
     { return 0; }},
};

int render_block(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    switch (block->blockid)
    {
    case BlockID::air:
        return 0;
    case BlockID::chest:
        return render_chest(list, block, pos);
    case BlockID::snow_layer:
        return render_snow_layer(list, block, pos);
    default:
        return render_functions[properties(block->id).m_render_type](list, block, pos);
    }
}

int render_cube(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    int vertexCount = 0;
    for (uint8_t face = VIS_MIN, i = 0; face != VIS_MAX; face <<= 1, i++)
    {
        if ((block->visibility_flags & face))
            vertexCount += render_face(list, pos, i, get_default_texture_index(block->blockid), block);
    }
    return vertexCount;
}

int render_inverted_cube(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    int vertexCount = 0;
    for (uint8_t face = VIS_MIN, i = 0; face != VIS_MAX; face <<= 1, i++)
    {
        if ((block->visibility_flags & face))
            vertexCount += render_back_face(list, pos, i, get_default_texture_index(block->blockid), block);
    }
    return vertexCount;
}

int render_inverted_cube_special(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    int vertexCount = 0;
    for (uint8_t face = VIS_MIN, i = 0; face != VIS_MAX; face <<= 1, i++)
    {
        if ((block->visibility_flags & face))
            vertexCount += render_back_face(list, pos, i, get_face_texture_index(block, i), block);
    }
    return vertexCount;
}

int render_cube_special(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    int vertexCount = 0;
    for (uint8_t face = VIS_MIN, i = 0; face != VIS_MAX; face <<= 1, i++)
    {
        if ((block->visibility_flags & face))
            vertexCount += render_face(list, pos, i, get_face_texture_index(block, i), block);
    }
    return vertexCount;
}

int render_special(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    switch (block->blockid)
    {
    case BlockID::torch:
    case BlockID::unlit_redstone_torch:
    case BlockID::redstone_torch:
    case BlockID::lever:
        return render_torch(list, block, pos);
    case BlockID::wooden_door:
    case BlockID::iron_door:
        return render_door(list, block, pos);
    case BlockID::cactus:
        return render_cactus(list, block, pos);
    default:
        return 0;
    }
}

int render_flat_ground(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    uint8_t lighting = block->light;
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_default_texture_index(block->blockid);

    int16_t x = local_pos.x << BASE3D_POS_FRAC_BITS;
    int16_t y = (local_pos.y << BASE3D_POS_FRAC_BITS) - 14;
    int16_t z = local_pos.z << BASE3D_POS_FRAC_BITS;

    int16_t x1 = (local_pos.x + 1) << BASE3D_POS_FRAC_BITS;
    int16_t z1 = (local_pos.z + 1) << BASE3D_POS_FRAC_BITS;

    list->put(gertex::Vertex16{.x = x1, .y = y, .z = z, .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = x1, .y = y, .z = z1, .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x, .y = y, .z = z1, .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x, .y = y, .z = z, .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    return 4;
}

int render_snow_layer(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_default_texture_index(block->blockid);
    int vertexCount = 4;

    // Top
    render_face(list, pos, FACE_PY, texture_index, block, 0, 2);

    BlockID neighbor_ids[6];
    {
        Block *neighbors[6];
        if (render_world)
            render_world->get_neighbors(pos, neighbors);
        for (int i = 0; i < 6; i++)
        {
            neighbor_ids[i] = neighbors[i] ? neighbors[i]->blockid : BlockID::air;
        }
    }

    if ((block->visibility_flags & VIS_NX) && neighbor_ids[FACE_NX] != BlockID::snow_layer)
    {
        // Negative X
        render_face(list, pos, FACE_NX, texture_index, block, 0, 2);
        vertexCount += 4;
    }
    if ((block->visibility_flags & VIS_PX) && neighbor_ids[FACE_PX] != BlockID::snow_layer)
    {
        // Positive X
        render_face(list, pos, FACE_PX, texture_index, block, 0, 2);
        vertexCount += 4;
    }
    if ((block->visibility_flags & VIS_NZ) && neighbor_ids[FACE_NZ] != BlockID::snow_layer)
    {
        // Negative Z
        render_face(list, pos, FACE_NZ, texture_index, block, 0, 2);
        vertexCount += 4;
    }
    if ((block->visibility_flags & VIS_PZ) && neighbor_ids[FACE_PZ] != BlockID::snow_layer)
    {
        // Positive Z
        render_face(list, pos, FACE_PZ, texture_index, block, 0, 2);
        vertexCount += 4;
    }
    return vertexCount;
}

int render_chest(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    int vertexCount = 0;
    for (uint8_t face = VIS_MIN, i = 0; face != VIS_MAX; face <<= 1, i++)
    {
        if ((block->visibility_flags & face))
            vertexCount += render_face(list, pos, i, get_chest_texture_index(block, pos, i), block);
    }
    return vertexCount;
}

int render_torch(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y + 0.1875, local_pos.z);
    switch (block->meta & 0x7)
    {
    case 1: // Facing east
        vertex_pos.x -= 0.125;
        return render_torch_with_angle(list, block, vertex_pos, -0.4, 0);
    case 2: // Facing west
        vertex_pos.x += 0.125;
        return render_torch_with_angle(list, block, vertex_pos, 0.4, 0);
    case 3: // Facing south
        vertex_pos.z -= 0.125;
        return render_torch_with_angle(list, block, vertex_pos, 0, -0.4);
    case 4: // Facing north
        vertex_pos.z += 0.125;
        return render_torch_with_angle(list, block, vertex_pos, 0, 0.4);
    default: // Facing up
        vertex_pos.y -= 0.1875;
        return render_torch_with_angle(list, block, vertex_pos, 0, 0);
    }
}

int render_torch_with_angle(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3f &vertex_pos, float ax, float az)
{
    uint8_t lighting = block->light;
    uint32_t texture_index = get_default_texture_index(block->blockid);

    // Negative X side
    list->put(gertex::Vertex16{.x = int16_t(-2 + vertex_pos.x * 32 + ax * 16), int16_t(vertex_pos.y * 32 - 16), int16_t(vertex_pos.z * 32 - 16 + az * 16), .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(-2 + vertex_pos.x * 32), int16_t(vertex_pos.y * 32 + 16), int16_t(vertex_pos.z * 32 - 16), .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(-2 + vertex_pos.x * 32), int16_t(vertex_pos.y * 32 + 16), int16_t(vertex_pos.z * 32 + 16), .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(-2 + vertex_pos.x * 32 + ax * 16), int16_t(vertex_pos.y * 32 - 16), int16_t(vertex_pos.z * 32 + 16 + az * 16), .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    // Positive X side
    list->put(gertex::Vertex16{.x = int16_t(+2 + vertex_pos.x * 32 + ax * 16), int16_t(vertex_pos.y * 32 + -16), int16_t(vertex_pos.z * 32 + 16 + az * 16), .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(+2 + vertex_pos.x * 32), int16_t(vertex_pos.y * 32 + 16), int16_t(vertex_pos.z * 32 + 16), .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(+2 + vertex_pos.x * 32), int16_t(vertex_pos.y * 32 + 16), int16_t(vertex_pos.z * 32 - 16), .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(+2 + vertex_pos.x * 32 + ax * 16), int16_t(vertex_pos.y * 32 + -16), int16_t(vertex_pos.z * 32 - 16 + az * 16), .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    // Negative Z side
    list->put(gertex::Vertex16{.x = int16_t(+16 + vertex_pos.x * 32 + ax * 16), int16_t(vertex_pos.y * 32 + -16), int16_t(vertex_pos.z * 32 - 2 + az * 16), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(+16 + vertex_pos.x * 32), int16_t(vertex_pos.y * 32 + 16), int16_t(vertex_pos.z * 32 - 2), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(-16 + vertex_pos.x * 32), int16_t(vertex_pos.y * 32 + 16), int16_t(vertex_pos.z * 32 - 2), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(-16 + vertex_pos.x * 32 + ax * 16), int16_t(vertex_pos.y * 32 + -16), int16_t(vertex_pos.z * 32 - 2 + az * 16), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    // Positive Z side
    list->put(gertex::Vertex16{.x = int16_t(-16 + vertex_pos.x * 32 + ax * 16), int16_t(vertex_pos.y * 32 + -16), int16_t(vertex_pos.z * 32 + 2 + az * 16), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(-16 + vertex_pos.x * 32), int16_t(vertex_pos.y * 32 + 16), int16_t(vertex_pos.z * 32 + 2), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(+16 + vertex_pos.x * 32), int16_t(vertex_pos.y * 32 + 16), int16_t(vertex_pos.z * 32 + 2), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(+16 + vertex_pos.x * 32 + ax * 16), int16_t(vertex_pos.y * 32 + -16), int16_t(vertex_pos.z * 32 + 2 + az * 16), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    // Top side
    list->put(gertex::Vertex16{.x = int16_t(+2 + vertex_pos.x * 32 + ax * 6), int16_t(vertex_pos.y * 32 + 4), int16_t(-2 + vertex_pos.z * 32 + az * 6), .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_X(texture_index) + 7. * BASE3D_PIXEL_UV_SCALE), .v = float(TEXTURE_Y(texture_index) + 8. * BASE3D_PIXEL_UV_SCALE)});
    list->put(gertex::Vertex16{.x = int16_t(+2 + vertex_pos.x * 32 + ax * 6), int16_t(vertex_pos.y * 32 + 4), int16_t(+2 + vertex_pos.z * 32 + az * 6), .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_X(texture_index) + 9. * BASE3D_PIXEL_UV_SCALE), .v = float(TEXTURE_Y(texture_index) + 8. * BASE3D_PIXEL_UV_SCALE)});
    list->put(gertex::Vertex16{.x = int16_t(-2 + vertex_pos.x * 32 + ax * 6), int16_t(vertex_pos.y * 32 + 4), int16_t(+2 + vertex_pos.z * 32 + az * 6), .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_X(texture_index) + 9. * BASE3D_PIXEL_UV_SCALE), .v = float(TEXTURE_Y(texture_index) + 6. * BASE3D_PIXEL_UV_SCALE)});
    list->put(gertex::Vertex16{.x = int16_t(-2 + vertex_pos.x * 32 + ax * 6), int16_t(vertex_pos.y * 32 + 4), int16_t(-2 + vertex_pos.z * 32 + az * 6), .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_X(texture_index) + 7. * BASE3D_PIXEL_UV_SCALE), .v = float(TEXTURE_Y(texture_index) + 6. * BASE3D_PIXEL_UV_SCALE)});

    return 20;
}

int render_door(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    uint8_t lighting = block->light;
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos = Vec3f(local_pos.x, local_pos.y, local_pos.z) - Vec3f(0.5);

    uint32_t texture_index = get_default_texture_index(block->blockid);
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
        list->put(gertex::Vertex16{.x = int16_t((vertex_pos.x + door_bounds.min.x) * 16), int16_t((vertex_pos.y + door_bounds.min.y) * 16), int16_t((vertex_pos.z + door_bounds.min.z) * 16), .i = lighting, .nrm = FACE_NY, .u = float(TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_Y(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t((vertex_pos.x + door_bounds.min.x) * 16), int16_t((vertex_pos.y + door_bounds.min.y) * 16), int16_t((vertex_pos.z + door_bounds.max.z) * 16), .i = lighting, .nrm = FACE_NY, .u = float(TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_Y(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t((vertex_pos.x + door_bounds.max.x) * 16), int16_t((vertex_pos.y + door_bounds.min.y) * 16), int16_t((vertex_pos.z + door_bounds.max.z) * 16), .i = lighting, .nrm = FACE_NY, .u = float(TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_Y(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t((vertex_pos.x + door_bounds.max.x) * 16), int16_t((vertex_pos.y + door_bounds.min.y) * 16), int16_t((vertex_pos.z + door_bounds.min.z) * 16), .i = lighting, .nrm = FACE_NY, .u = float(TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_Y(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE))});
    }

    // Top face (we'll not render the top face of the bottom half)
    if (top_half)
    {
        list->put(gertex::Vertex16{.x = int16_t((vertex_pos.x + door_bounds.min.x) * 16), int16_t((vertex_pos.y + door_bounds.max.y) * 16), int16_t((vertex_pos.z + door_bounds.min.z) * 16), .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_Y(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t((vertex_pos.x + door_bounds.min.x) * 16), int16_t((vertex_pos.y + door_bounds.max.y) * 16), int16_t((vertex_pos.z + door_bounds.max.z) * 16), .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_Y(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t((vertex_pos.x + door_bounds.max.x) * 16), int16_t((vertex_pos.y + door_bounds.max.y) * 16), int16_t((vertex_pos.z + door_bounds.max.z) * 16), .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_Y(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t((vertex_pos.x + door_bounds.max.x) * 16), int16_t((vertex_pos.y + door_bounds.max.y) * 16), int16_t((vertex_pos.z + door_bounds.min.z) * 16), .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_Y(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE))});
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
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_X(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_X(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_X(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_X(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
    }
    else
    {
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_PX(texture_index) - (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_PX(texture_index) - (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_PX(texture_index) - (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_PX(texture_index) - (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
    }

    // Positive X side
    if (flip_states[3])
    {
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_X(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_X(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_X(texture_index) + (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_X(texture_index) + (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
    }
    else
    {
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_PX(texture_index) - (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_PX(texture_index) - (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_PX(texture_index) - (door_bounds.max.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_PX(texture_index) - (door_bounds.min.z * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
    }

    // Negative Z side
    if (flip_states[2])
    {
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
    }
    else
    {
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_PX(texture_index) - (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_PX(texture_index) - (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_PX(texture_index) - (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.min.z), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_PX(texture_index) - (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
    }

    // Positive Z side
    if (flip_states[0])
    {
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_X(texture_index) + (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_X(texture_index) + (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
    }
    else
    {
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_PX(texture_index) - (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.max.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_PX(texture_index) - (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.max.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.max.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_PX(texture_index) - (door_bounds.max.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
        list->put(gertex::Vertex16{.x = int16_t(vertex_pos.x + door_bounds.min.x), int16_t(vertex_pos.y + door_bounds.min.y), int16_t(vertex_pos.z + door_bounds.max.z), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_PX(texture_index) - (door_bounds.min.x * BASE3D_BLOCK_UV_SCALE)), .v = float(TEXTURE_PY(texture_index) - (door_bounds.min.y * BASE3D_BLOCK_UV_SCALE))});
    }
    return 20;
}

int render_cactus(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{

    uint8_t lighting = block->light;
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    uint32_t texture_index = get_default_texture_index(block->blockid);
    uint32_t top_texture_index = texture_index - 1;
    uint32_t bottom_texture_index = texture_index + 1;

    int vertexCount = 16;

    int16_t x = (local_pos.x << BASE3D_POS_FRAC_BITS);
    int16_t y = (local_pos.y << BASE3D_POS_FRAC_BITS);
    int16_t z = (local_pos.z << BASE3D_POS_FRAC_BITS);

    int16_t x0 = x - 16;
    int16_t x1 = x + 16;
    int16_t z0 = z - 16;
    int16_t z1 = z + 16;

    int16_t y0 = y - 16;
    int16_t y1 = y + 16;

    // Positive X side
    list->put(gertex::Vertex16{.x = int16_t(x1 - 1), .y = y0, .z = z1, .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(x1 - 1), .y = y1, .z = z1, .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(x1 - 1), .y = y1, .z = z0, .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(x1 - 1), .y = y0, .z = z0, .i = lighting, .nrm = FACE_PX, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    // Negative X side
    list->put(gertex::Vertex16{.x = int16_t(x0 + 1), .y = y0, .z = z0, .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(x0 + 1), .y = y1, .z = z0, .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(x0 + 1), .y = y1, .z = z1, .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = int16_t(x0 + 1), .y = y0, .z = z1, .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    // Bottom face
    if ((block->visibility_flags & VIS_NY))
    {
        list->put(gertex::Vertex16{.x = x0, .y = y0, .z = z0, .i = lighting, .nrm = FACE_NY, .u = float(TEXTURE_NX(bottom_texture_index)), .v = float(TEXTURE_PY(bottom_texture_index))});
        list->put(gertex::Vertex16{.x = x0, .y = y0, .z = z1, .i = lighting, .nrm = FACE_NY, .u = float(TEXTURE_NX(bottom_texture_index)), .v = float(TEXTURE_NY(bottom_texture_index))});
        list->put(gertex::Vertex16{.x = x1, .y = y0, .z = z1, .i = lighting, .nrm = FACE_NY, .u = float(TEXTURE_PX(bottom_texture_index)), .v = float(TEXTURE_NY(bottom_texture_index))});
        list->put(gertex::Vertex16{.x = x1, .y = y0, .z = z0, .i = lighting, .nrm = FACE_NY, .u = float(TEXTURE_PX(bottom_texture_index)), .v = float(TEXTURE_PY(bottom_texture_index))});
        vertexCount += 4;
    }
    if ((block->visibility_flags & VIS_PY))
    {
        // Top face
        list->put(gertex::Vertex16{.x = x1, .y = y1, .z = z0, .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_NX(top_texture_index)), .v = float(TEXTURE_PY(top_texture_index))});
        list->put(gertex::Vertex16{.x = x1, .y = y1, .z = z1, .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_NX(top_texture_index)), .v = float(TEXTURE_NY(top_texture_index))});
        list->put(gertex::Vertex16{.x = x0, .y = y1, .z = z1, .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_PX(top_texture_index)), .v = float(TEXTURE_NY(top_texture_index))});
        list->put(gertex::Vertex16{.x = x0, .y = y1, .z = z0, .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_PX(top_texture_index)), .v = float(TEXTURE_PY(top_texture_index))});
        vertexCount += 4;
    }
    // Positive Z side
    list->put(gertex::Vertex16{.x = x0, .y = y0, .z = int16_t(z1 - 1), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = x0, .y = y1, .z = int16_t(z1 - 1), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x1, .y = y1, .z = int16_t(z1 - 1), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x1, .y = y0, .z = int16_t(z1 - 1), .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    // Negative Z side
    list->put(gertex::Vertex16{.x = x1, .y = y0, .z = int16_t(z0 + 1), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = x1, .y = y1, .z = int16_t(z0 + 1), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x0, .y = y1, .z = int16_t(z0 + 1), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x0, .y = y0, .z = int16_t(z0 + 1), .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    return vertexCount;
}
int render_cross(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    uint8_t lighting = block->light;
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_default_texture_index(block->blockid);
    
    int16_t x0 = (local_pos.x << BASE3D_POS_FRAC_BITS) - 16;
    int16_t x1 = (local_pos.x << BASE3D_POS_FRAC_BITS) + 16;
    int16_t y0 = (local_pos.y << BASE3D_POS_FRAC_BITS) - 16;
    int16_t y1 = (local_pos.y << BASE3D_POS_FRAC_BITS) + 16;
    int16_t z0 = (local_pos.z << BASE3D_POS_FRAC_BITS) - 16;
    int16_t z1 = (local_pos.z << BASE3D_POS_FRAC_BITS) + 16;

    // NX, NZ -> PX, PZ
    list->put(gertex::Vertex16{.x = x0, .y = y0, .z = z0, .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = x0, .y = y1, .z = z0, .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x1, .y = y1, .z = z1, .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x1, .y = y0, .z = z1, .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    // PX, NZ -> NX, PZ
    list->put(gertex::Vertex16{.x = x1, .y = y0, .z = z0, .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = x1, .y = y1, .z = z0, .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x0, .y = y1, .z = z1, .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x0, .y = y0, .z = z1, .i = lighting, .nrm = FACE_NX, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    // NX, PZ -> PX, NZ
    list->put(gertex::Vertex16{.x = x0, .y = y0, .z = z1, .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = x0, .y = y1, .z = z1, .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x1, .y = y1, .z = z0, .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x1, .y = y0, .z = z0, .i = lighting, .nrm = FACE_PZ, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    // PX, PZ -> NX, NZ
    list->put(gertex::Vertex16{.x = x1, .y = y0, .z = z1, .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = x1, .y = y1, .z = z1, .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x0, .y = y1, .z = z0, .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x0, .y = y0, .z = z0, .i = lighting, .nrm = FACE_NZ, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    return 16;
}

int render_slab(gertex::DisplayList<gertex::Vertex16> *list, Block *block, const Vec3i &pos)
{
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);

    // Check for texture overrides
    uint32_t texture_index = get_default_texture_index(block->blockid);
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
        if (!(block->visibility_flags & VIS_PY))
            render_top = false;
    }
    else
    {
        if (!(block->visibility_flags & VIS_NY))
            render_bottom = false;
    }
    bool faces[6] = {
        (block->visibility_flags & VIS_NX) != 0,
        (block->visibility_flags & VIS_PX) != 0,
        render_bottom,
        render_top,
        (block->visibility_flags & VIS_NZ) != 0,
        (block->visibility_flags & VIS_PZ) != 0};

    for (int i = 0; i < 6; i++)
    {
        if (faces[i])
        {
            uint32_t face_texture_index = side_index;
            if (i == FACE_NY)
                face_texture_index = bottom_index;
            else if (i == FACE_PY)
                face_texture_index = top_index;
            vertexCount += render_face(list, pos, i, face_texture_index, block, min_y, max_y);
        }
    }
    return vertexCount;
}

int get_chest_texture_index(Block *block, const Vec3i &pos, uint8_t face)
{
    // Check for texture overrides
    uint32_t texture_index = get_default_texture_index(block->blockid);
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
                     { return block && block->blockid == BlockID::chest; }))
    {
        // Single chest
        uint8_t direction = FACE_PZ;
        if (!(block->visibility_flags & (VIS_PZ)) && (block->visibility_flags & (VIS_NZ)))
            direction = FACE_NZ;
        if (!(block->visibility_flags & (VIS_NX)) && (block->visibility_flags & (VIS_PX)))
            direction = FACE_PX;
        if (!(block->visibility_flags & (VIS_PX)) && (block->visibility_flags & (VIS_NX)))
            direction = FACE_NX;

        return 26 + (face == direction);
    }

    // Double chest

    if ((face != FACE_NZ && face != FACE_PZ) && neighbors[0]->blockid != BlockID::chest && neighbors[1]->blockid != BlockID::chest)
    {
        // X axis
        bool half = neighbors[2]->blockid == BlockID::chest;
        uint8_t other_flags = neighbors[half ? 2 : 3]->visibility_flags;
        uint8_t direction = FACE_PX;
        if ((!(block->visibility_flags & (VIS_PX)) || !(other_flags & (VIS_PX))) && (block->visibility_flags & (VIS_NX)) && (other_flags & (VIS_NX)))
            direction = FACE_NX;

        if (face == FACE_NX)
            half = !half;

        return 26 + (face == direction ? 16 : 32) - half;
    }
    else if ((face != FACE_NX && face != FACE_PX) && neighbors[2]->blockid != BlockID::chest && neighbors[3]->blockid != BlockID::chest)
    {
        // Z axis
        bool half = neighbors[0]->blockid == BlockID::chest;

        uint8_t other_flags = neighbors[half ? 0 : 1]->visibility_flags;
        uint8_t direction = FACE_PZ;
        if ((!(block->visibility_flags & (VIS_PZ)) || !(other_flags & (VIS_PZ))) && (block->visibility_flags & (VIS_NZ)) && (other_flags & (VIS_NZ)))
            direction = FACE_NZ;

        if (face == FACE_PZ)
            half = !half;

        return 26 + (face == direction ? 16 : 32) - half;
    }
    return 26;
}
