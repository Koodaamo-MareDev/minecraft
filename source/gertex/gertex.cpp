#include "gertex.hpp"

namespace gertex
{
    GXState state;
    void use_fog(bool use)
    {
        if (state.fog.enabled == use)
            return;
        state.fog.enabled = use;
        GX_SetFog(state.fog.enabled ? uint8_t(state.fog.type) : GX_FOG_NONE, state.fog.start, state.fog.end, state.fog.near, state.fog.far, state.fog.color);
    }

    void set_fog(const GXFog &fog)
    {
        state.fog = fog;
        GX_SetFog(state.fog.enabled ? uint8_t(state.fog.type) : GX_FOG_NONE, state.fog.start, state.fog.end, state.fog.near, state.fog.far, state.fog.color);
    }

    void pop_matrix()
    {
        if (state.matrix_stack.size() > 0)
        {
            GXMatrix mtx = state.matrix_stack.top();
            state.matrix_stack.pop();
            std::memcpy(state.mtx.mtx, mtx.mtx, sizeof(Mtx));
        }
    }

    void push_matrix()
    {
        GXMatrix mtx;
        memcpy(mtx.mtx, state.mtx.mtx, sizeof(Mtx));
        state.matrix_stack.push(mtx);
    }

    void use_matrix(const Mtx &mtx, bool load)
    {
        guMtxCopy(mtx, state.mtx.mtx);
        if (load)
            GX_LoadPosMtxImm(state.mtx.mtx, GX_PNMTX0);
    }

    void use_matrix(const GXMatrix &mtx, bool load)
    {
        guMtxCopy(mtx.mtx, state.mtx.mtx);
        if (load)
            GX_LoadPosMtxImm(state.mtx.mtx, GX_PNMTX0);
    }

    GXMatrix get_matrix()
    {
        return state.mtx;
    }

    GXMatrix get_view_matrix()
    {
        static GXMatrix mtx;
        static bool view_mtx_init = false;
        if (!view_mtx_init)
        {
            // Setup our view matrix at the origin looking down the -Z axis with +Y up
            guVector cam = {0.0F, 0.0F, 0.0F},
                     up = {0.0F, 1.0F, 0.0F},
                     look = {0.0F, 0.0F, -1.0F};
            guLookAt(mtx.mtx, &cam, &up, &look);
            view_mtx_init = true;
        }
        return mtx;
    }

    void load_pos_matrix()
    {
        GX_LoadPosMtxImm(state.mtx.mtx, GX_PNMTX0);
    }

    void load_proj_matrix()
    {
        GX_LoadProjectionMtx(state.proj_mtx.mtx, state.proj_mtx.ortho ? GX_ORTHOGRAPHIC : GX_PERSPECTIVE);
    }

    // Called internally to initialize the fog adjustment table
    void init_fog(gertex::GXView view)
    {
        // Initialize fog
        static bool fog_init = false;
        if (fog_init)
            return;
        static GXFogAdjTbl fog_adjust_table;
        GX_InitFogAdjTable(&fog_adjust_table, view.width, state.proj_mtx.mtx);
        GX_SetFogRangeAdj(GX_ENABLE, uint16_t(view.width) >> 1, &fog_adjust_table);
        fog_init = true;
    }

    void perspective(gertex::GXView view)
    {
        // Prepare the perspective matrix
        Mtx44 prespective_mtx;
        guPerspective(prespective_mtx, view.fov, view.aspect, view.near, view.far);
        guMtx44Copy(prespective_mtx, state.proj_mtx.mtx);

        // Update the state
        state.proj_mtx.ortho = false;
        load_proj_matrix();

        // Initialize the fog adjustment table (if not already initialized)
        init_fog(view);
    }

    void ortho(gertex::GXView view)
    {
        // Prepare the orthogonal matrix
        Mtx44 ortho_mtx;
        guOrtho(ortho_mtx, 0, view.height, 0, view.width, 0, view.far);
        guMtx44Copy(ortho_mtx, state.proj_mtx.mtx);

        // Update the state
        state.proj_mtx.ortho = true;
        load_proj_matrix();

        // Typically, fog is not used in orthographic projections so we'll skip it

        // FIXME: Not consistent with the rest of the code
        // Construct a flat (2D) matrix
        Mtx flat_matrix;
        guMtxIdentity(flat_matrix);
        guMtxTransApply(flat_matrix, flat_matrix, 0.0F, 0.0F, -0.5F);
        GX_LoadPosMtxImm(flat_matrix, GX_PNMTX0);
        guMtxCopy(flat_matrix, state.mtx.mtx);
    }

    void set_blending(GXBlendMode mode)
    {
        if (mode == state.blend_mode)
            return;
        state.blend_mode = mode;
        switch (mode)
        {
        case GXBlendMode::normal:
            GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
            break;
        case GXBlendMode::additive:
            GX_SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);
            break;
        default:
            break;
        }
    }

    void set_color_add(GXColor color)
    {
        if (state.color_add == color)
            return;
        state.color_add = color;
        GX_SetTevKColor(GX_KCOLOR0, color);
    }

    GXColor get_color_add()
    {
        return state.color_add;
    }

    void set_color_mul(GXColor color)
    {
        if (state.color_multiply == color)
            return;
        state.color_multiply = color;
        GX_SetTevKColor(GX_KCOLOR1, color);
    }

    GXColor get_color_mul()
    {
        return state.color_multiply;
    }

    void GXState::apply()
    {
        set_fog(fog);
        set_blending(blend_mode);
        set_color_add(color_add);
        set_color_mul(color_multiply);
        load_pos_matrix();
        load_proj_matrix();
    }
    GXView *GXView::default_view = nullptr;
} // namespace gertex
