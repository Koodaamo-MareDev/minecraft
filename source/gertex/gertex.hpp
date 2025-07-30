#ifndef GERTEX_HPP
#define GERTEX_HPP

#include <ogc/gx.h>
#include <cstdint>
#include <string>
#include <stack>
#include <cstring>
namespace gertex
{
    constexpr float CAMERA_WIDTH(640.0f);
    constexpr float CAMERA_HEIGHT(480.0f);
    constexpr float CAMERA_NEAR(0.1f);
    constexpr float CAMERA_FAR(300.0f);

    class GXTexture
    {
    public:
        std::uint32_t id;
        std::string name;
        GXTexObj texture;

        GXTexture(std::string name) : name(name)
        {
            id = 0;
        }
    };

    struct GXView
    {
        float width = CAMERA_WIDTH;
        float height = CAMERA_HEIGHT;
        float fov = 90;
        float near = CAMERA_NEAR;
        float far = CAMERA_FAR;
        float aspect = CAMERA_WIDTH / CAMERA_HEIGHT;
        float aspect_correction = 1.0f; // Aspect correction factor for widescreen
        float yscale = 1.2f;
        float ystart = 0.0f; // Optional padding for the top of the screen - useful for preventing HUD overscan issues when in widescreen mode
        bool widescreen = false;
        static GXView *default_view;

        GXView(float width, float height, bool widescreen, float fov, float near, float far, float yscale)
        {

            // Aspect ratio correction
            this->yscale = yscale;
            this->aspect = width / height;
            this->width = width;
            this->height = height;
            this->fov = fov;
            this->near = near;
            this->far = far;
            this->widescreen = widescreen;
            if (widescreen)
                aspect_correction = 0.75f;
            ystart = 16;
            if (!default_view)
                default_view = this;
        }

        GXView()
        {
            if (!default_view)
                return;
            *this = *default_view;
        }

        ~GXView()
        {
            if (default_view == this)
                default_view = nullptr;
        }
    };

    enum class GXFogType : uint8_t
    {
        none = GX_FOG_NONE,
        linear = GX_FOG_LIN,
        exponential = GX_FOG_EXP,
        exponential2 = GX_FOG_EXP2
    };

    struct GXFog
    {
        bool enabled;
        GXFogType type;
        float start;
        float end;
        float near;
        float far;
        GXColor color;
    };

    enum class GXBlendMode
    {
        none,
        overwrite,
        normal,
        additive,
        inverse,
        multiply2
    };

    struct GXMatrix
    {
        Mtx mtx;
        GXMatrix()
        {
            guMtxIdentity(mtx);
        }
    };

    struct GXProjMatrix
    {
        bool ortho;
        Mtx44 mtx;
        GXProjMatrix()
        {
            ortho = false;
            guMtx44Identity(mtx);
        }
    };

    class GXState
    {
    public:
        std::stack<GXMatrix> matrix_stack;
        GXMatrix mtx;
        GXProjMatrix proj_mtx;
        GXFog fog = {false, GXFogType::none, 0.0f, 0.0f, CAMERA_NEAR, CAMERA_FAR, GXColor{0, 0, 0, 0xFF}};
        GXBlendMode blend_mode = GXBlendMode::none;
        GXColor color_multiply = {0xFF, 0xFF, 0xFF, 0xFF};
        GXColor color_add = {0, 0, 0, 0xFF};
        uint8_t alpha_cutoff = 0;
        GXState()
        {
        }

        void apply();
    };

    void set_state(const GXState &state);
    const GXState get_state();
    void use_fog(bool use);
    void set_fog(const GXFog &fog);
    void pop_matrix();
    void push_matrix();
    void use_matrix(const Mtx &mtx, bool load = true);
    void use_matrix(const GXMatrix &mtx, bool load = true);
    GXMatrix get_matrix();
    GXMatrix get_view_matrix();
    void load_pos_matrix();
    void load_proj_matrix();
    void perspective(GXView view);
    void ortho(GXView view);
    void set_blending(GXBlendMode mode);
    void set_color_add(GXColor color);
    GXColor get_color_add();
    void set_color_mul(GXColor color);
    GXColor get_color_mul();
    void set_alpha_cutoff(uint8_t cutoff);
    uint8_t get_alpha_cutoff();
} // namespace gortex

inline bool operator==(const GXColor &lhs, const GXColor &rhs)
{
    // Optimized comparison
    return *((uint32_t *)&lhs) == *((uint32_t *)&rhs);
}

inline GXColor operator*(const GXColor &lhs, const float &rhs)
{
    return GXColor{uint8_t(lhs.r * rhs), uint8_t(lhs.g * rhs), uint8_t(lhs.b * rhs), uint8_t(lhs.a * rhs)};
}
#endif