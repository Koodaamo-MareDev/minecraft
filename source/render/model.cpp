#include "model.hpp"

#include <render/base3d.hpp>
#include <render/render.hpp>
#include <block/blocks.hpp>

void ModelBox::prepare()
{
    Vertex faces[24];
    Vec3f start = pivot - (Vec3f(inflate, inflate, inflate));
    Vec3f end = pivot + size + (Vec3f(inflate, inflate, inflate));

    constexpr float uv_scale_x = 1 / 64.0f;
    constexpr float uv_scale_y = 1 / 32.0f;

    // Define quads and uvs for each face of the cube

    // Negative X
    faces[0] = Vertex(Vec3f(start.x, start.y, start.z), size.z, size.z);
    faces[1] = Vertex(Vec3f(start.x, start.y, end.z), 0, size.z);
    faces[2] = Vertex(Vec3f(start.x, end.y, end.z), 0, size.y + size.z);
    faces[3] = Vertex(Vec3f(start.x, end.y, start.z), size.z, size.y + size.z);

    // Positive X
    faces[4] = Vertex(Vec3f(end.x, start.y, start.z), size.z + size.x, size.z);
    faces[5] = Vertex(Vec3f(end.x, end.y, start.z), size.z + size.x, size.y + size.z);
    faces[6] = Vertex(Vec3f(end.x, end.y, end.z), size.z + size.x + size.z, size.y + size.z);
    faces[7] = Vertex(Vec3f(end.x, start.y, end.z), size.z + size.x + size.z, size.z);

    // Positive Y
    faces[8] = Vertex(Vec3f(start.x, end.y, end.z), size.z + size.x, size.z);
    faces[9] = Vertex(Vec3f(end.x, end.y, end.z), size.z + size.x + size.x, size.z);
    faces[10] = Vertex(Vec3f(end.x, end.y, start.z), size.z + size.x + size.x, 0);
    faces[11] = Vertex(Vec3f(start.x, end.y, start.z), size.z + size.x, 0);

    // Negative Y
    faces[12] = Vertex(Vec3f(end.x, start.y, start.z), size.z + size.x, 0);
    faces[13] = Vertex(Vec3f(end.x, start.y, end.z), size.z + size.x, size.z);
    faces[14] = Vertex(Vec3f(start.x, start.y, end.z), size.z, size.z);
    faces[15] = Vertex(Vec3f(start.x, start.y, start.z), size.z, 0);

    // Negative Z
    faces[16] = Vertex(Vec3f(start.x, start.y, start.z), size.z + size.x, size.z);
    faces[17] = Vertex(Vec3f(start.x, end.y, start.z), size.z + size.x, size.z + size.y);
    faces[18] = Vertex(Vec3f(end.x, end.y, start.z), size.z, size.z + size.y);
    faces[19] = Vertex(Vec3f(end.x, start.y, start.z), size.z, size.z);

    // Positive Z
    faces[20] = Vertex(Vec3f(start.x, start.y, end.z), size.z + size.x + size.z, size.z);
    faces[21] = Vertex(Vec3f(end.x, start.y, end.z), size.z + size.x + size.z + size.x, size.z);
    faces[22] = Vertex(Vec3f(end.x, end.y, end.z), size.z + size.x + size.z + size.x, size.z + size.y);
    faces[23] = Vertex(Vec3f(start.x, end.y, end.z), size.z + size.x + size.z, size.z + size.y);

    uint32_t buffer_size = 24 * VERTEX_ATTR_LENGTH_FLOATPOS + 3; // The GX_BeginGroup call will add 3 bytes
    buffer_size = (buffer_size + 31) & ~31;                      // Align to 32 bytes
    buffer_size += 32;                                           // Add padding for GX_EndDispList

    // Create display list
    display_list = new (std::align_val_t(32)) uint8_t[buffer_size];

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
void ModelBox::render()
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

void Model::prepare()
{
    if (ready)
        return;
    for (auto &box : boxes)
    {
        box->prepare();
    }
    ready = true;
}

void Model::render(vfloat_t distance, float partialTicks, bool transparency)
{
    prepare();
    use_texture(texture);
    Camera &camera = get_camera();
    /*
     *
     * The Y offset (1.5 + 1 / 128) is a shared offset for all entity models.
     * As to why this specific offset is used, it appears to be a historical
     * value chosen to align models correctly in the world. It likely accounts
     * for the typical eye height of entities and ensures that models are
     * rendered at the correct vertical position.
     *
     * Another thing to note is the inversion of the model. Its likely caused
     * from a bug where the matrix was inverted at some point and thus models
     * had to be inverted to compensate. This inversion has been preserved for
     * compatibility reasons. By adding camera position multiplied by 2 and
     * subtracting the model's position, we effectively convert the position.
     * In short, we go from -CameraPosition to CameraPosition - ModelPosition
     */
    transform_view(gertex::get_view_matrix(), Vec3f(camera.position) * 2 - (Vec3f(pos.x, pos.y + 1.5078125F, pos.z)), Vec3f(1), rot, false);
    for (auto &box : boxes)
    {
        box->render();
    }

    // Restore fixed point format
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
}

void Model::render_handitem(ModelBox *box, item::ItemStack &item, Vec3f offset, Vec3f offset_rot, Vec3f scale, bool transparency)
{
    if (item.empty())
    {
        return;
    }
    if (!box)
        return;

    auto is_flat = [](const item::ItemStack &item)
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
    gertex::GXState state = gertex::get_state();
    // Use fixed point format for vertex positions
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);

    // Setup matrices for rendering the item
    transform_view(gertex::get_view_matrix(), pos + Vec3f(0, 1.5078125F, 0), Vec3f(-1), this->rot, false);
    gertex::GXMatrix pos_mtx = gertex::get_matrix();
    guMtxApplyTrans(pos_mtx.mtx, pos_mtx.mtx, -(box->pos.x) / 16, (box->pos.y) / 16, -(box->pos.z) / 16);
    Mtx tmp_mtx;
    guMtxRotDeg(tmp_mtx, 'z', box->rot.z + offset_rot.z);
    guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
    guMtxRotDeg(tmp_mtx, 'y', box->rot.y + offset_rot.y + 180);
    guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
    guMtxRotDeg(tmp_mtx, 'x', box->rot.x + offset_rot.x);
    guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
    guMtxApplyTrans(pos_mtx.mtx, pos_mtx.mtx, -1.0f / 16, 7.0f / 16, 1.0f / 16);
    if (!is_flat(item))
    {
        scale = scale * 0.375f;
        guMtxApplyTrans(pos_mtx.mtx, pos_mtx.mtx, 0, 3.0f / 16, 3.0f / 16);
        guMtxRotDeg(tmp_mtx, 'x', -20);
        guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
        guMtxRotDeg(tmp_mtx, 'y', -45);
        guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
        guMtxApplyScale(pos_mtx.mtx, pos_mtx.mtx, -scale.x, -scale.y, -scale.z);
    }
    else if (item.as_item().tool != item::ToolType::none || item.id == 392) // Fishing rod and tools
    {
        guMtxApplyTrans(pos_mtx.mtx, pos_mtx.mtx, 0, 2.0f / 16, 5.0f / 16);
        guMtxApplyScale(pos_mtx.mtx, pos_mtx.mtx, -scale.x, -scale.y, -scale.z);
        guMtxRotDeg(tmp_mtx, 'y', 90);
        guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
        guMtxRotDeg(tmp_mtx, 'z', 45);
        guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
    }
    else
    {
        scale = scale * 0.375f;
        guMtxApplyTrans(pos_mtx.mtx, pos_mtx.mtx, 0, 3.0f / 16, 3.0f / 16);
        guMtxApplyScale(pos_mtx.mtx, pos_mtx.mtx, scale.x, -scale.y, scale.z);
        guMtxRotDeg(tmp_mtx, 'x', 90);
        guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
        guMtxRotDeg(tmp_mtx, 'z', 180);
        guMtxConcat(pos_mtx.mtx, tmp_mtx, pos_mtx.mtx);
    }
    gertex::use_matrix(pos_mtx.mtx, true);

    int texture_index;

    // Check if the selected item is a block
    if (item.as_item().is_block())
    {
        Block selected_block = Block{uint8_t(item.id & 0xFF), 0x7F, uint8_t(item.meta & 0xFF)};
        selected_block.light = 0xFF;

        if (!is_flat(item))
        {
            // Use the terrain texture
            use_texture(terrain_texture);

            // Render as a block
            if (transparency == properties(item.id).m_transparent)
                render_single_block(selected_block);

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
        texture_index = item.get_texture_index();

        // Use the item texture
        use_texture(items_texture);
    }

    // Render as an item without culling
    GX_SetCullMode(GX_CULL_NONE);
    if (transparency)
        render_single_item(texture_index, transparency);
    GX_SetCullMode(GX_CULL_BACK);

    gertex::set_state(state);
}
