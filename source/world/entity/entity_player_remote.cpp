#include "entity_player_remote.hpp"
#include <world/world.hpp>
#include <render/model.hpp>
#include <render/render.hpp>
#include <render/render_gui.hpp>

static PlayerModel player_model = PlayerModel();

EntityPlayerMp::EntityPlayerMp(const Vec3f &position, std::string player_name) : EntityPlayer(position)
{
    this->player_name = player_name;
    player_model.texture = &player_texture;
}

void EntityPlayerMp::tick()
{
    EntityLiving::tick();
}

void EntityPlayerMp::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;

    // Get the player's position and rotation
    Vec3f entity_position = get_position(partial_ticks) - Vec3f(0, y_offset, 0);
    Vec3f entity_rotation = get_rotation(partial_ticks);

    // The name tag should render in the transparent pass
    if (transparency)
    {
        Camera &camera = get_camera();

        Transform transform;
        transform.set_position(entity_position);

        gertex::use_matrix(camera.apply_transform(transform));

        gertex::use_fog(false);

        // Enable direct colors
        gertex::set_color_format(0, GX_DIRECT);

        // Render through walls
        GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);

        // Use floats for vertex positions (this will later be restored to fixed point by draw_text_3d)
        gertex::set_pos_precision(GX_F32, 0);

        // Use solid white as the texture
        use_texture(white_texture);

        // Render the background for the name tag
        Vec3f bg_size = Vec3f(0.25);
        bg_size.x *= (text_width_3d(player_name) + 2) * 0.125;
        bg_size.y += 0.03125;
        Vec3f cam_rot = camera.transform.get_rotation();
        Vec3f right_vec = -angles_to_vector(0, cam_rot.y + 90);
        Vec3f up_vec = -angles_to_vector(cam_rot.x + 90, cam_rot.y);
        draw_colored_sprite_3d(white_texture, Vec3f(0, 2.375, 0), bg_size, Vec3f(0, -0.03125 * 0.5, 0), right_vec, up_vec, 0, 0, 1, 1, GXColor{0, 0, 0, 0x3F});

        // Render partially transparent name tag (as seen behind walls)
        draw_text_3d(Vec3f(0, 2.375, 0), player_name, GXColor{0xFF, 0xFF, 0xFF, 0x3F});

        // Render the name tag normally
        gertex::use_fog(true);
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        draw_text_3d(Vec3f(0, 2.375, 0), player_name, GXColor{0xFF, 0xFF, 0xFF, 0xFF});
    }
    EntityLiving::render(partial_ticks, transparency);

    player_model.speed = std::min(1.0f, last_walk_speed + (walk_speed - last_walk_speed) * partial_ticks);
    player_model.pos = entity_position;
    player_model.rot = Vec3f(0, body_rotation_y, 0);
    player_model.head_rot = Vec3f(entity_rotation.x, entity_rotation.y, 0);

    gertex::set_color_mul(get_lightmap_color(light_level) * (14 / 16.));
    for (size_t i = 0; i < 5; i++)
    {
        player_model.equipment[i] = equipment[i];
    }
    player_model.pose(accumulated_walk_distance - walk_speed * (1 - partial_ticks));
    player_model.render(partial_ticks, transparency);
}