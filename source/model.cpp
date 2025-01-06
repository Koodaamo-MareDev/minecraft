#include "model.hpp"

#include "base3d.hpp"
#include "render.hpp"

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
        faces[i].x_uv = (faces[i].x_uv + uv_off_x) * uv_scale_x * BASE3D_UV_FRAC;
        faces[i].y_uv = (faces[i].y_uv + uv_off_y) * uv_scale_y * BASE3D_UV_FRAC;
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
    if (display_list && display_list_size)
    {
        push_matrix();
        if (pos.sqr_magnitude() > 0)
        {
            guMtxApplyTrans(active_mtx, active_mtx, pos.x / 16, pos.y / 16, pos.z / 16);
        }
        if (rot.sqr_magnitude() > 0)
        {
            Mtx rot_mtx;
            guMtxRotDeg(rot_mtx, 'z', rot.z);
            guMtxConcat(active_mtx, rot_mtx, active_mtx);
            guMtxRotDeg(rot_mtx, 'y', rot.y);
            guMtxConcat(active_mtx, rot_mtx, active_mtx);
            guMtxRotDeg(rot_mtx, 'x', rot.x);
            guMtxConcat(active_mtx, rot_mtx, active_mtx);
        }
        guMtxScaleApply(active_mtx, active_mtx, -1, -1, -1);
        GX_LoadPosMtxImm(active_mtx, GX_PNMTX0);

        // Use floats for vertex positions
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);

        GX_CallDispList(display_list, display_list_size);
        pop_matrix();

        // Restore fixed point format
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);
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

void model_t::render(float ticks)
{
    prepare();
    use_texture(texture);
    transform_view(get_view_matrix(), vec3f(player_pos) * 2 - pos, vec3f(1), rot, false);
    for (auto &box : boxes)
    {
        box->render();
    }
}