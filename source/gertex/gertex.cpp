#include "gertex.hpp"
#include <new>
namespace gertex
{
    GXState state;

    void set_state(const GXState &new_state)
    {
        state = new_state;
        state.apply();
    }

    const GXState get_state()
    {
        return state;
    }

    void use_fog(bool use)
    {
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
        guPerspective(prespective_mtx, view.fov, view.aspect / view.aspect_correction, view.near, view.far);
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
        guMtxScaleApply(flat_matrix, flat_matrix, 1.0F, 1.0F / view.aspect_correction, 1.0F);
        guMtxTransApply(flat_matrix, flat_matrix, 0.0F, 0.0F, -0.5F);
        GX_LoadPosMtxImm(flat_matrix, GX_PNMTX0);
        guMtxCopy(flat_matrix, state.mtx.mtx);
    }

    void set_blending(GXBlendMode mode)
    {
        state.blend_mode = mode;
        switch (mode)
        {
        case GXBlendMode::overwrite:
            GX_SetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_NOOP);
            break;
        case GXBlendMode::normal:
            GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
            break;
        case GXBlendMode::additive:
            GX_SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);
            break;
        case GXBlendMode::inverse:
            GX_SetBlendMode(GX_BM_LOGIC, 0x00, 0x00, GX_LO_INV);
            break;
        case GXBlendMode::multiply2:
            GX_SetBlendMode(GX_BM_BLEND, GX_BL_DSTCLR, GX_BL_SRCCLR, GX_LO_NOOP);
            break;
        default:
            break;
        }
    }

    void set_color_add(GXColor color)
    {
        state.color_add = color;
        GX_SetTevKColor(GX_KCOLOR0, color);
    }

    GXColor get_color_add()
    {
        return state.color_add;
    }

    void set_color_mul(GXColor color)
    {
        state.color_multiply = color;
        GX_SetTevKColor(GX_KCOLOR1, color);
    }

    GXColor get_color_mul()
    {
        return state.color_multiply;
    }

    void set_alpha_cutoff(uint8_t cutoff)
    {
        state.alpha_cutoff = cutoff;
        if (!cutoff)
            GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
        else
            GX_SetAlphaCompare(GX_GEQUAL, cutoff, GX_AOP_AND, GX_ALWAYS, 0);
    }

    uint8_t get_alpha_cutoff()
    {
        return state.alpha_cutoff;
    }

    void GXState::apply()
    {
        set_fog(fog);
        set_blending(blend_mode);
        set_color_add(color_add);
        set_color_mul(color_multiply);
        set_alpha_cutoff(alpha_cutoff);
        load_pos_matrix();
        load_proj_matrix();
    }

    // We don't want this to be touched directly
    // So we'll keep it in an anonymous namespace
    namespace
    {
        constexpr size_t DEFAULT_FIFO_SIZE = 256 * 1024;

        uint8_t *gp_fifo = nullptr;

        // These are the default universal rendering settings.
        // Feel free to change them directly via the GX APIs.
        void tev_init()
        {
            GX_SetNumChans(2);
            GX_SetNumTexGens(1);

            GX_SetNumTevStages(4);
            GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

            // Stage 0: Texture * CLR0

            GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

            GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC, GX_CC_ZERO);
            GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

            GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_ZERO);
            GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

            // Stage 1: PREV * CLR1

            GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP_DISABLE, GX_COLOR1A1);

            GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_CPREV, GX_CC_RASC, GX_CC_ZERO);
            GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

            GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_APREV, GX_CA_RASA, GX_CA_ZERO);
            GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

            // Stage 2: Add Konst0

            GX_SetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD0, GX_TEXMAP_DISABLE, GX_COLOR0A0);

            GX_SetTevKColor(GX_KCOLOR0, (GXColor){0, 0, 0, 255}); // Default to black (i.e. no effect)
            GX_SetTevKColorSel(GX_TEVSTAGE2, GX_TEV_KCSEL_K0);

            GX_SetTevColorIn(GX_TEVSTAGE2, GX_CC_CPREV, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);
            GX_SetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

            GX_SetTevAlphaIn(GX_TEVSTAGE2, GX_CA_APREV, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
            GX_SetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

            // Stage 3: Multiply by Konst1

            GX_SetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD0, GX_TEXMAP_DISABLE, GX_COLOR0A0);

            GX_SetTevKColor(GX_KCOLOR1, (GXColor){255, 255, 255, 255}); // White by default
            GX_SetTevKColorSel(GX_TEVSTAGE3, GX_TEV_KCSEL_K1);

            GX_SetTevColorIn(GX_TEVSTAGE3, GX_CC_ZERO, GX_CC_CPREV, GX_CC_KONST, GX_CC_ZERO);
            GX_SetTevColorOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

            GX_SetTevAlphaIn(GX_TEVSTAGE3, GX_CA_APREV, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
            GX_SetTevAlphaOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        }

        // Initializes the graphics subsystem
        bool gx_init(GXRModeObj *rmode)
        {
            // (Re)allocate the FIFO buffer
            if (gp_fifo)
            {
                delete[] gp_fifo;
            }
            gp_fifo = new (std::align_val_t(32), std::nothrow) uint8_t[DEFAULT_FIFO_SIZE];
            if (!gp_fifo)
            {
                return false;
            }

            // Now let's get the boring stuff out of the way...
            GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);

            // Initialize frame buffer copy params
            rmode->xfbHeight = GX_SetDispCopyYScale(GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight));
            GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
            GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
            GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
            GX_SetDispCopyDst(rmode->fbWidth, rmode->xfbHeight);
            GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
            GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
            GX_SetDispCopyGamma(GX_GM_1_0);

            // FIXME: I feel like this should be a setting.
            if (rmode->aa)
            {
                GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
            }
            else
            {
                GX_SetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
            }

            return true;
        }
    }

    void use_defaults()
    {

        /**
         * The default vertex format is as follows: POS, COLA, COLB, UV
         * POS: 3D coordinates (X, Y, Z), float
         * COLA: Primary color (RGBA), unsigned char, 8 bits per channel
         * COLB: Secondary color (RGBA), unsigned char, 8 bits per channel
         * UV: Texture coordinates (U, V), float
         *
         * The data should be provided in the above order unless manually
         * modified. Indirect lookup is not enabled by default but can be
         * specified later if needed.
         *
         * See tev_init() to see how colors are combined.
         */

        GX_ClearVtxDesc();
        GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GX_SetVtxDesc(GX_VA_CLR1, GX_INDEX8);
        GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR1, GX_CLR_RGBA, GX_RGBA8, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

        tev_init();
        GX_SetZCompLoc(GX_FALSE);
        GX_SetLineWidth(16, GX_VTXFMT0);
    }

    void init(GXRModeObj *rmode, float fov, float near, float far, bool apply_defaults)
    {
        gx_init(rmode);

        if (apply_defaults)
        {
            use_defaults();
        }

        GXState initial_state;
        initial_state.view = GXView(rmode->fbWidth, rmode->efbHeight, CONF_GetAspectRatio(), fov, near, far);

        set_state(initial_state);
    }
} // namespace gertex
