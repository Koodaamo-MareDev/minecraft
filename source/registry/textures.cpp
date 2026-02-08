#include "textures.hpp"
#include <render/render.hpp>
#include <ogcsys.h>
#include <pnguin/png_loader.hpp>

GXTexObj white_texture;
GXTexObj clouds_texture;
GXTexObj sun_texture;
GXTexObj moon_texture;
GXTexObj terrain_texture;
GXTexObj particles_texture;
GXTexObj icons_texture;
GXTexObj container_texture;
GXTexObj underwater_texture;
GXTexObj vignette_texture;
GXTexObj creeper_texture;
GXTexObj player_texture;
GXTexObj font_texture;
GXTexObj items_texture;
GXTexObj inventory_texture;
GXTexObj crafting_texture;
GXTexObj furnace_texture;
GXTexObj gui_texture;

// Animated textures
WaterTexAnim water_still_anim;
LavaTexanim lava_still_anim;

namespace registry
{

    void register_textures()
    {
        // Basic textures
        init_missing_texture(white_texture);
        init_png_texture(clouds_texture, "environment/clouds.png");
        init_png_texture(sun_texture, "terrain/sun.png");
        init_png_texture(moon_texture, "terrain/moon.png");
        init_png_texture(particles_texture, "particles.png");
        init_png_texture(terrain_texture, "terrain.png", 3);
        init_png_texture(icons_texture, "gui/icons.png");
        init_png_texture(container_texture, "gui/container.png");
        init_png_texture(underwater_texture, "misc/water.png");
        init_png_texture(vignette_texture, "misc/vignette.png");
        init_png_texture(creeper_texture, "mob/creeper.png");
        init_png_texture(player_texture, "mob/char.png");
        init_png_texture(font_texture, "font/default.png");
        init_png_texture(items_texture, "gui/items.png");
        init_png_texture(inventory_texture, "gui/inventory.png");
        init_png_texture(crafting_texture, "gui/crafting.png");
        init_png_texture(furnace_texture, "gui/furnace.png");
        init_png_texture(gui_texture, "gui/gui.png");

        GX_InitTexObjWrapMode(&clouds_texture, GX_REPEAT, GX_REPEAT);
        GX_InitTexObjWrapMode(&underwater_texture, GX_REPEAT, GX_REPEAT);
        GX_InitTexObjFilterMode(&vignette_texture, GX_LINEAR, GX_LINEAR);

        // Animated textures

        // Lava
        lava_still_anim.target = MEM_PHYSICAL_TO_K1(GX_GetTexObjData(&terrain_texture));
        lava_still_anim.dst_width = 256;
        lava_still_anim.tile_width = 16;
        lava_still_anim.tile_height = 16;
        lava_still_anim.dst_x = 208;
        lava_still_anim.dst_y = 224;
        lava_still_anim.flow_dst_x = 224;
        lava_still_anim.flow_dst_y = 224;

        // Water
        water_still_anim.target = MEM_PHYSICAL_TO_K1(GX_GetTexObjData(&terrain_texture));
        water_still_anim.dst_width = 256;
        water_still_anim.tile_width = 16;
        water_still_anim.tile_height = 16;
        water_still_anim.dst_x = 208;
        water_still_anim.dst_y = 192;
        water_still_anim.flow_dst_x = 224;
        water_still_anim.flow_dst_y = 192;
    }

    void init_missing_texture(GXTexObj &texture)
    {
        void *texture_buf = new (std::align_val_t(32)) uint8_t[64]; // 4x4 RGBA texture
        memset(texture_buf, 0xFF, 64);
        DCFlushRange(texture_buf, 64);

        GX_InitTexObj(&texture, texture_buf, 4, 4, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GX_InitTexObjFilterMode(&texture, GX_NEAR, GX_NEAR);
    }

    void init_png_texture(GXTexObj &texture, const std::string &filename, uint32_t mipmap_levels)
    {
        try
        {
            pnguin::PNGFile png_file(RESOURCES_DIR "textures/" + filename);
            png_file.to_tpl(texture, mipmap_levels);
        }
        catch (std::runtime_error &e)
        {
            printf("Failed to load %s: %s\n", filename.c_str(), e.what());
            init_missing_texture(texture);
        }
    }

} // namespace registry