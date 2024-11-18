#ifndef _RENDER_GUI_HPP_
#define _RENDER_GUI_HPP_

#include <cstdint>
#include <ogc/gx.h>

#include "vec3i.hpp"
#include "texturedefs.h"
#include "render.hpp"
#include "base3d.hpp"

constexpr float GUI_DEFAULT_SCALE = 1.0f / BASE3D_POS_FRAC;

int draw_textured_quad(GXTexObj &texture, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t u1, uint32_t v1, uint32_t u2, uint32_t v2, float scale = GUI_DEFAULT_SCALE);

int draw_colored_quad(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float scale = GUI_DEFAULT_SCALE);

int draw_colored_sprite(GXTexObj &texture, vec2i pos, vec2i size, uint32_t u1, uint32_t v1, uint32_t u2, uint32_t v2, GXColor color = GXColor{0xFF, 0xFF, 0xFF, 0xFF}, float scale = GUI_DEFAULT_SCALE);

int fill_screen_texture(GXTexObj &texture, view_t &view, int32_t u1, int32_t v1, int32_t u2, int32_t v2, float scale = GUI_DEFAULT_SCALE);

int draw_simple_textured_quad(GXTexObj &texture, int32_t x, int32_t y, int32_t w, int32_t h, float scale = GUI_DEFAULT_SCALE);

#endif