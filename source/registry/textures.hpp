#ifndef TEXTURES_HPP
#define TEXTURES_HPP

#include <string>
#include <cstdint>
#include <ogc/gx_struct.h>
#include <render/texanim.hpp>

namespace registry
{
    void register_textures();
    void init_missing_texture(GXTexObj &texture);
    void init_png_texture(GXTexObj &texture, const std::string &filename, uint32_t mipmap_levels = 0);
} // namespace registry

extern GXTexObj white_texture;
extern GXTexObj clouds_texture;
extern GXTexObj sun_texture;
extern GXTexObj moon_texture;
extern GXTexObj terrain_texture;
extern GXTexObj particles_texture;
extern GXTexObj icons_texture;
extern GXTexObj container_texture;
extern GXTexObj underwater_texture;
extern GXTexObj vignette_texture;
extern GXTexObj creeper_texture;
extern GXTexObj player_texture;
extern GXTexObj font_texture;
extern GXTexObj items_texture;
extern GXTexObj inventory_texture;
extern GXTexObj crafting_texture;
extern GXTexObj furnace_texture;
extern GXTexObj gui_texture;

// Animated textures
extern WaterTexAnim water_still_anim;
extern LavaTexanim lava_still_anim;

#endif