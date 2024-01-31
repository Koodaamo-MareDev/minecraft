#ifndef _RENDER_GUI_HPP_
#define _RENDER_GUI_HPP_

#include <cstdint>
#include <ogc/gx.h>

#include "vec3i.hpp"
#include "texturedefs.h"
#include "render.hpp"
#include "base3d.hpp"

int draw_textured_quad(GXTexObj &texture, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t u1, uint32_t v1, uint32_t u2, uint32_t v2);

int draw_simple_textured_quad(GXTexObj &texture, int32_t x, int32_t y, int32_t w, int32_t h);

#endif