#include "model.hpp"

#include "base3d.hpp"
#include "render.hpp"
#include "blocks.hpp"

void modelbox_t::prepare()
{
    vertex_property_t faces[24];
    vec3f start = pivot - (vec3f(inflate, inflate, inflate));
    vec3f end = pivot + size + (vec3f(inflate, inflate, inflate));

    constexpr float uv_scale_x = 1 / 64.0f;
    constexpr float uv_scale_y = 1 / 32.0f;

    // Define quads and uvs for each face of the cube

    // Negative X
    faces[0] = vertex_property_t(vec3f(start.x, start.y, start.z), size.z, size.z);
    faces[1] = vertex_property_t(vec3f(start.x, start.y, end.z), 0, size.z);
    faces[2] = vertex_property_t(vec3f(start.x, end.y, end.z), 0, size.y + size.z);
    faces[3] = vertex_property_t(vec3f(start.x, end.y, start.z), size.z, size.y + size.z);

    // Positive X
    faces[4] = vertex_property_t(vec3f(end.x, start.y, start.z), size.z + size.x, size.z);
    faces[5] = vertex_property_t(vec3f(end.x, end.y, start.z), size.z + size.x, size.y + size.z);
    faces[6] = vertex_property_t(vec3f(end.x, end.y, end.z), size.z + size.x + size.z, size.y + size.z);
    faces[7] = vertex_property_t(vec3f(end.x, start.y, end.z), size.z + size.x + size.z, size.z);

    // Positive Y
    faces[8] = vertex_property_t(vec3f(start.x, end.y, end.z), size.z + size.x, size.z);
    faces[9] = vertex_property_t(vec3f(end.x, end.y, end.z), size.z + size.x + size.x, size.z);
    faces[10] = vertex_property_t(vec3f(end.x, end.y, start.z), size.z + size.x + size.x, 0);
    faces[11] = vertex_property_t(vec3f(start.x, end.y, start.z), size.z + size.x, 0);

    // Negative Y
    faces[12] = vertex_property_t(vec3f(end.x, start.y, start.z), size.z + size.x, 0);
    faces[13] = vertex_property_t(vec3f(end.x, start.y, end.z), size.z + size.x, size.z);
    faces[14] = vertex_property_t(vec3f(start.x, start.y, end.z), size.z, size.z);
    faces[15] = vertex_property_t(vec3f(start.x, start.y, start.z), size.z, 0);

    // Negative Z
    faces[16] = vertex_property_t(vec3f(start.x, start.y, start.z), size.z + size.x, size.z);
    faces[17] = vertex_property_t(vec3f(start.x, end.y, start.z), size.z + size.x, size.z + size.y);
    faces[18] = vertex_property_t(vec3f(end.x, end.y, start.z), size.z, size.z + size.y);
    faces[19] = vertex_property_t(vec3f(end.x, start.y, start.z), size.z, size.z);

    // Positive Z
    faces[20] = vertex_property_t(vec3f(start.x, start.y, end.z), size.z + size.x + size.z, size.z);
    faces[21] = vertex_property_t(vec3f(end.x, start.y, end.z), size.z + size.x + size.z + size.x, size.z);
    faces[22] = vertex_property_t(vec3f(end.x, end.y, end.z), size.z + size.x + size.z + size.x, size.z + size.y);
    faces[23] = vertex_property_t(vec3f(start.x, end.y, end.z), size.z + size.x + size.z, size.z + size.y);

    uint32_t buffer_size = 24 * VERTEX_ATTR_LENGTH_FLOATPOS + 3; // The GX_BeginGroup call will add 3 bytes
    buffer_size = (buffer_size + 31) & ~31;                      // Align to 32 bytes
    buffer_size += 32;                                           // Add padding for GX_EndDispList

    // Create display list
    display_list = memalign(32, buffer_size);

    if (!display_list)
    {
        display_list_size = 0;
        return;
    }

    DCInvalidateRange(display_list, buffer_size);

    GX_BeginDispList(display_list, buffer_size);

    GX_BeginGroup(GX_QUADS, 24);
    for (int i = 0; i < 24; i++)
    {
        faces[i].x_uv = (faces[i].x_uv + uv_off_x) * uv_scale_x;
        faces[i].y_uv = (faces[i].y_uv + uv_off_y) * uv_scale_y;
        faces[i].pos = faces[i].pos * (1 / 16.f);
        GX_VertexLitF(faces[i], 255); // Default to full brightness
    }

    uint32_t vtxcount = GX_EndGroup();
    display_list_size = GX_EndDispList();
    if (!display_list_size || vtxcount != 24)
    {
        free(display_list);
        display_list = nullptr;
        display_list_size = 0;
        return;
    }

    DCFlushRange(display_list, display_list_size);
}
void modelbox_t::render()
{
    // Use floats for vertex positions
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);

    if (display_list && display_list_size)
    {
        gertex::GXMatrix pos_mtx = gertex::get_matrix();
        gertex::push_matrix();
        guMtxApplyTrans(pos_mtx.mtx, pos_mtx.mtx, pos.x / 16, pos.y / 16, pos.z / 16);
        Mtx rot_mtx;
        guMtxRotDeg(rot_mtx, 'z', rot.z);
        guMtxConcat(pos_mtx.mtx, rot_mtx, pos_mtx.mtx);
        guMtxRotDeg(rot_mtx, 'y', rot.y);
        guMtxConcat(pos_mtx.mtx, rot_mtx, pos_mtx.mtx);
        guMtxRotDeg(rot_mtx, 'x', rot.x);
        guMtxConcat(pos_mtx.mtx, rot_mtx, pos_mtx.mtx);
        guMtxScaleApply(pos_mtx.mtx, pos_mtx.mtx, -1, -1, -1);
        gertex::use_matrix(pos_mtx.mtx, true);

        GX_CallDispList(display_list, display_list_size);
        gertex::pop_matrix();
    }
}

void model_t::prepare()
{
    if (ready)
        return;
    for (auto &box : boxes)
    {
        box->prepare();
    }
    ready = true;
}

void model_t::render(vfloat_t distance, float partialTicks, bool transparency)
{
    prepare();
    use_texture(texture);
    transform_view(gertex::get_view_matrix(), vec3f(player_pos) * 2 - pos, vec3f(1), rot, false);
    for (auto &box : boxes)
    {
        box->render();
    }

    // Restore fixed point format
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
}

void model_t::render_handitem(modelbox_t *box, inventory::item_stack &item, vec3f offset, vec3f rot, vec3f scale, bool transparency)
{
    if (item.empty())
    {
        return;
    }
    if (!box)
        return;

    auto is_flat = [](const inventory::item_stack &item)
    {
        if (item.as_item().is_block())
        {
            RenderType render_type = properties(item.id).m_render_type;

            if (!properties(item.id).m_fluid && (properties(item.id).m_nonflat || render_type == RenderType::full || render_type == RenderType::full_special || render_type == RenderType::slab))
            {
                return false;
            }
        }
        return true;
    };

    float scale_factor = 0.5f;
    if (!is_flat(item))
    {
        scale_factor = 0.25; // Non-flat items are rendered at quarter scale
    }
    scale = scale * scale_factor;

    gertex::GXState state = gertex::get_state();
    // Use fixed point format for vertex positions
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);

    // Setup matrices for rendering the item
    transform_view(gertex::get_view_matrix(), pos, vec3f(1), this->rot, false);
    gertex::GXMatrix pos_mtx = gertex::get_matrix();
    guMtxApplyTrans(pos_mtx.mtx, pos_mtx.mtx, (box->pos.x) / 16, (box->pos.y) / 16, (box->pos.z) / 16);
    guMtxApplyScale(pos_mtx.mtx, pos_mtx.mtx, scale.x, scale.y, scale.z);
    Mtx tmp_mtx;
    guMtxRotDeg(tmp_mtx, 'z', box->rot.z + rot.z);
    guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
    guMtxRotDeg(tmp_mtx, 'y', box->rot.y + rot.y);
    guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
    guMtxRotDeg(tmp_mtx, 'x', box->rot.x + rot.x);
    guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
    guMtxTrans(tmp_mtx, offset.x / 16 / scale_factor, offset.y / 16 / scale_factor, offset.z / 16 / scale_factor);
    guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
    if (!is_flat(item))
    {
        guMtxRotDeg(tmp_mtx, 'y', -22.5);
        guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
        guMtxRotDeg(tmp_mtx, 'x', -22.5);
        guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
    }
    gertex::use_matrix(pos_mtx.mtx, true);

    int texture_index;

    // Check if the selected item is a block
    if (item.as_item().is_block())
    {
        block_t selected_block = block_t{uint8_t(item.id & 0xFF), 0x7F, uint8_t(item.meta & 0xFF)};
        selected_block.light = 0xFF;

        if (!is_flat(item))
        {
            // Render as a block
            render_single_block(selected_block, transparency);

            gertex::set_state(state);
            return;
        }

        // Setup flat item properties

        // Get the texture index of the selected block
        texture_index = get_default_texture_index(BlockID(item.id));

        // Use the terrain texture
        use_texture(terrain_texture);
    }
    else
    {
        // Setup item properties

        // Get the texture index of the selected item
        texture_index = item.as_item().texture_index;

        // Use the item texture
        use_texture(items_texture);
    }

    // Render as an item
    if (transparency)
        render_single_item(texture_index, transparency);

    gertex::set_state(state);
}
