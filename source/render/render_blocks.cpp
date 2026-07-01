#include "render_blocks.hpp"

#include <functional>
#include <world/world.hpp>
#include <world/chunk.hpp>
#include <block/blocks.hpp>
#include <registry/block_list.hpp>
#include <render/render.hpp>
#include <gertex/displaylist.hpp>
#include <blocks/block_redstone_wire.hpp>

static std::map<RenderType, std::function<int(gertex::DisplayList<gertex::Vertex16> *list, BlockState *, const Vec3i &)>> render_functions = {
    {RenderType::full, render_cube},
    {RenderType::full_special, render_cube_special},
    {RenderType::cross, render_cross},
    {RenderType::flat_ground, render_flat_ground},
    {RenderType::slab, render_slab},
    {RenderType::special, render_special},
    {RenderType::fluid, render_nothing}};

int render_nothing(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
{
    return 0;
}

int render_cube(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
{
    int vertexCount = 0;
    BlockBase *b = block_list[block->id];
    for (uint8_t i = 0; i < 6; i++)
    {
        if (!render_world || b->should_render_side(render_world, pos, i))
            vertexCount += render_face(list, pos, i, get_texture_index(render_world, pos, i, block), block);
    }
    return vertexCount;
}

int render_inverted_cube(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
{
    int vertexCount = 0;
    BlockBase *b = block_list[block->id];
    for (uint8_t i = 0; i < 6; i++)
    {
        if (!render_world || b->should_render_side(render_world, pos, i))
            vertexCount += render_back_face(list, pos, i, get_texture_index(render_world, pos, i, block), block);
    }
    return vertexCount;
}

int render_inverted_cube_special(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
{
    int vertexCount = 0;
    BlockBase *b = block_list[block->id];
    for (uint8_t i = 0; i < 6; i++)
    {
        if (!render_world || b->should_render_side(render_world, pos, i))
            vertexCount += render_back_face(list, pos, i, get_texture_index(render_world, pos, i, block), block);
    }
    return vertexCount;
}

int render_cube_special(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
{
    int vertexCount = 0;
    BlockBase *b = block_list[block->id];
    for (uint8_t i = 0; i < 6; i++)
    {
        if (!render_world || b->should_render_side(render_world, pos, i))
            vertexCount += render_face(list, pos, i, get_texture_index(render_world, pos, i, block), block);
    }
    return vertexCount;
}

int render_special(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
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

int render_flat_ground(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
{
    uint8_t lighting = block->light;
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_face_texture_index(block, FACE_PY);

    int16_t x = (local_pos.x << BASE3D_POS_FRAC_BITS) - (BASE3D_POS_FRAC >> 1);
    int16_t y = (local_pos.y << BASE3D_POS_FRAC_BITS) - 14;
    int16_t z = (local_pos.z << BASE3D_POS_FRAC_BITS) - (BASE3D_POS_FRAC >> 1);

    int16_t x1 = ((local_pos.x + 1) << BASE3D_POS_FRAC_BITS) - (BASE3D_POS_FRAC >> 1);
    int16_t z1 = ((local_pos.z + 1) << BASE3D_POS_FRAC_BITS) - (BASE3D_POS_FRAC >> 1);

    list->put(gertex::Vertex16{.x = x1, .y = y, .z = z, .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_PY(texture_index))});
    list->put(gertex::Vertex16{.x = x1, .y = y, .z = z1, .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_NX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x, .y = y, .z = z1, .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_NY(texture_index))});
    list->put(gertex::Vertex16{.x = x, .y = y, .z = z, .i = lighting, .nrm = FACE_PY, .u = float(TEXTURE_PX(texture_index)), .v = float(TEXTURE_PY(texture_index))});

    return 4;
}

int render_snow_layer(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
{
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_face_texture_index(block, FACE_PY);
    int vertexCount = 4;

    // Top
    render_face(list, pos, FACE_PY, texture_index, block, 0, 2);

    BlockID neighbor_ids[6];
    {
        BlockState *neighbors[6];
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

int render_torch(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
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

int render_torch_with_angle(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3f &vertex_pos, float ax, float az)
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

int render_door(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
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

int render_cactus(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
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
int render_cross(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
{
    uint8_t lighting = block->light;
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = get_face_texture_index(block, FACE_PY);

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

int render_slab(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
{
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);

    // Check for texture overrides
    uint32_t texture_index = get_texture_index(render_world, pos, +BlockFace::PY, block);
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
        if (!block_list[block->id]->should_render_side(render_world, pos, +BlockFace::PY))
            render_top = false;
    }
    else
    {
        if (!block_list[block->id]->should_render_side(render_world, pos, +BlockFace::NY))
            render_bottom = false;
    }
    bool faces[6] = {
        block_list[block->id]->should_render_side(render_world, pos, +BlockFace::NX),
        block_list[block->id]->should_render_side(render_world, pos, +BlockFace::PX),
        render_bottom,
        render_top,
        block_list[block->id]->should_render_side(render_world, pos, +BlockFace::NZ),
        block_list[block->id]->should_render_side(render_world, pos, +BlockFace::PZ)};

    for (int i = 0; i < 6; i++)
    {
        if (faces[i])
        {
            uint32_t face_texture_index = side_index;
            if (i == +BlockFace::NY)
                face_texture_index = bottom_index;
            else if (i == +BlockFace::PY)
                face_texture_index = top_index;
            vertexCount += render_face(list, pos, i, face_texture_index, block, min_y, max_y);
        }
    }
    return vertexCount;
}

int render_wire(gertex::DisplayList<gertex::Vertex16> *list, BlockState *block, const Vec3i &pos)
{
    const Vec3i offsets[4]{
        {-1, 0, 0},
        {+1, 0, 0},
        {0, 0, -1},
        {0, 0, +1},
    };

    float power = float(int(block->meta) / 15.0f);
    uint8_t r = uint8_t(power * 170.0f) + 85;
    r = (light_map[(int(block->light)) << 2] / 255.0f) * r;
    uint8_t g = 0;
    uint8_t b = 0;
    Vec3i local_pos(pos.x & 0xF, pos.y & 0xF, pos.z & 0xF);
    Vec3f vertex_pos(local_pos.x, local_pos.y, local_pos.z);
    uint32_t texture_index = block_list[block->id]->face_texture_index(0, 0);

    int16_t x = (local_pos.x << BASE3D_POS_FRAC_BITS) - (BASE3D_POS_FRAC >> 1);
    int16_t y = (local_pos.y << BASE3D_POS_FRAC_BITS) - 14;
    int16_t z = (local_pos.z << BASE3D_POS_FRAC_BITS) - (BASE3D_POS_FRAC >> 1);

    bool has_connection[4] = {
        false,
        false,
        false,
        false,
    };

    bool has_diagonal_connection[4] = {
        false,
        false,
        false,
        false,
    };
    bool has_opaque_above = block_at(render_world, pos + Vec3i{0, 1, 0})->is_opaque();

    for (uint8_t i = 0; i < 4; i++)
        has_connection[i] = BlockRedstoneWire::is_source_or_wire(render_world, pos + offsets[i]) ||
                            (!block_at(render_world, pos + offsets[i])->is_opaque() && BlockRedstoneWire::is_source_or_wire(render_world, pos + offsets[i] - Vec3i{0, 1, 0}));

    if (!has_opaque_above)
    {
        for (uint8_t i = 0; i < 4; i++)
            has_connection[i] |= (has_diagonal_connection[i] = (block_at(render_world, pos + offsets[i])->is_opaque() && BlockRedstoneWire::is_source_or_wire(render_world, pos + offsets[i] + Vec3i{0, 1, 0})));
    }

    int16_t x0 = x;
    int16_t z0 = z;
    int16_t x1 = x + 32;
    int16_t z1 = z + 32;
    float u0 = float(TEXTURE_NX(texture_index));
    float v0 = float(TEXTURE_NY(texture_index));
    float u1 = float(TEXTURE_PX(texture_index));
    float v1 = float(TEXTURE_PY(texture_index));
    const int16_t no_connection_pos_margin = BASE3D_POS_FRAC * 5 / 16;
    const float no_connection_uv_margin = BASE3D_PIXEL_UV_SCALE * 5;

    // 0 = corner/dot, 1 = X line, 2 = Z line
    uint8_t state = 0;
    if ((has_connection[0] || has_connection[1]) && !has_connection[2] && !has_connection[3])
        state = 1;
    if ((has_connection[2] || has_connection[3]) && !has_connection[0] && !has_connection[1])
        state = 2;

    int vertex_count = 0;

    if (state == 0)
    {
        // Handle corners (if any)
        if (has_connection[0] || has_connection[1] || has_connection[2] || has_connection[3])
        {
            // -X
            if (!has_connection[0])
            {
                x0 += no_connection_pos_margin;
                u0 += no_connection_uv_margin;
            }

            // +X
            if (!has_connection[1])
            {
                x1 -= no_connection_pos_margin;
                u1 -= no_connection_uv_margin;
            }

            // -Z
            if (!has_connection[2])
            {
                z0 += no_connection_pos_margin;
                v0 += no_connection_uv_margin;
            }

            // +Z
            if (!has_connection[3])
            {
                z1 -= no_connection_pos_margin;
                v1 -= no_connection_uv_margin;
            }
        }
        list->put(gertex::Vertex16{.x = x1, .y = y, .z = z0, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v0});
        list->put(gertex::Vertex16{.x = x1, .y = y, .z = z1, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v1});
        list->put(gertex::Vertex16{.x = x0, .y = y, .z = z1, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v1});
        list->put(gertex::Vertex16{.x = x0, .y = y, .z = z0, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v0});
        vertex_count += 4;
    }
    else
    {
        u0 += BASE3D_BLOCK_UV_SCALE;
        u1 += BASE3D_BLOCK_UV_SCALE;
        if (state == 1)
        {
            list->put(gertex::Vertex16{.x = x1, .y = y, .z = z0, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v1});
            list->put(gertex::Vertex16{.x = x1, .y = y, .z = z1, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v0});
            list->put(gertex::Vertex16{.x = x0, .y = y, .z = z1, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v0});
            list->put(gertex::Vertex16{.x = x0, .y = y, .z = z0, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v1});
            vertex_count += 4;
        }
        else if (state == 2)
        {
            list->put(gertex::Vertex16{.x = x1, .y = y, .z = z0, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v1});
            list->put(gertex::Vertex16{.x = x1, .y = y, .z = z1, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v1});
            list->put(gertex::Vertex16{.x = x0, .y = y, .z = z1, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v0});
            list->put(gertex::Vertex16{.x = x0, .y = y, .z = z0, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v0});
            vertex_count += 4;
        }
        u0 -= BASE3D_BLOCK_UV_SCALE;
        u1 -= BASE3D_BLOCK_UV_SCALE;
    }
    if (!has_opaque_above)
    {
        u0 += BASE3D_BLOCK_UV_SCALE;
        u1 += BASE3D_BLOCK_UV_SCALE;

        // Wall -X
        if (has_diagonal_connection[0])
        {
            list->put(gertex::Vertex16{.x = int16_t(x + 2), .y = int16_t(y + 32), .z = z0, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v1});
            list->put(gertex::Vertex16{.x = int16_t(x + 2), .y = y, .z = z0, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v1});
            list->put(gertex::Vertex16{.x = int16_t(x + 2), .y = y, .z = z1, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v0});
            list->put(gertex::Vertex16{.x = int16_t(x + 2), .y = int16_t(y + 32), .z = z1, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v0});
            vertex_count += 4;
        }
        // Wall +X
        if (has_diagonal_connection[1])
        {
            list->put(gertex::Vertex16{.x = int16_t(x + 30), .y = y, .z = z0, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v0});
            list->put(gertex::Vertex16{.x = int16_t(x + 30), .y = int16_t(y + 32), .z = z0, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v0});
            list->put(gertex::Vertex16{.x = int16_t(x + 30), .y = int16_t(y + 32), .z = z1, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v1});
            list->put(gertex::Vertex16{.x = int16_t(x + 30), .y = y, .z = z1, .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v1});
            vertex_count += 4;
        }
        // Wall -Z
        if (has_diagonal_connection[2])
        {
            list->put(gertex::Vertex16{.x = x0, .y = y, .z = int16_t(z + 2), .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v0});
            list->put(gertex::Vertex16{.x = x0, .y = int16_t(y + 32), .z = int16_t(z + 2), .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v0});
            list->put(gertex::Vertex16{.x = x1, .y = int16_t(y + 32), .z = int16_t(z + 2), .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v1});
            list->put(gertex::Vertex16{.x = x1, .y = y, .z = int16_t(z + 2), .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v1});
            vertex_count += 4;
        }
        // Wall +Z
        if (has_diagonal_connection[3])
        {
            list->put(gertex::Vertex16{.x = x0, .y = int16_t(y + 32), .z = int16_t(z + 30), .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v1});
            list->put(gertex::Vertex16{.x = x0, .y = y, .z = int16_t(z + 30), .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v1});
            list->put(gertex::Vertex16{.x = x1, .y = y, .z = int16_t(z + 30), .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u0, .v = v0});
            list->put(gertex::Vertex16{.x = x1, .y = int16_t(y + 32), .z = int16_t(z + 30), .r = r, .g = g, .b = b, .a = 255, .nrm = FACE_PY, .u = u1, .v = v0});
            vertex_count += 4;
        }
    }
    return vertex_count;
}