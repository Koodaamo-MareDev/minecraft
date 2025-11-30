#include "gui_gameover.hpp"

#include <gui/gui_dirtscreen.hpp>
#include <util/input/input.hpp>
#include <world/world.hpp>
#include <sounds.hpp>

GuiGameOver::GuiGameOver(World *world) : world(world)
{
    gertex::GXView view = gertex::get_state().view;

    int view_height = view.aspect_correction * view.height;
    buttons.push_back(GuiButton((view.width - 400) / 2, view_height / 4 + 144, 400, 40, "Respawn", std::bind(&GuiGameOver::respawn, this)));
    buttons.push_back(GuiButton((view.width - 400) / 2, view_height / 4 + 192, 400, 40, "Title menu", std::bind(&GuiGameOver::quit_to_title, this)));
}

void GuiGameOver::draw()
{
    gertex::GXView view = gertex::get_state().view;

    int view_height = view.aspect_correction * view.height;

    // Draw the background

    use_texture(icons_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(Vertex(0.03125f * Vec3f(0, 0, 0), 0 * BASE3D_PIXEL_UV_SCALE, 16 * BASE3D_PIXEL_UV_SCALE, 0x50, 0, 0, 0x60));
    GX_Vertex(Vertex(0.03125f * Vec3f(view.width, 0, 0), 8 * BASE3D_PIXEL_UV_SCALE, 16 * BASE3D_PIXEL_UV_SCALE, 0x50, 0, 0, 0x60));
    GX_Vertex(Vertex(0.03125f * Vec3f(view.width, view_height, 0), 8 * BASE3D_PIXEL_UV_SCALE, 24 * BASE3D_PIXEL_UV_SCALE, 0x80, 0x30, 0x30, 0xA0));
    GX_Vertex(Vertex(0.03125f * Vec3f(0, view_height, 0), 0 * BASE3D_PIXEL_UV_SCALE, 24 * BASE3D_PIXEL_UV_SCALE, 0x80, 0x30, 0x30, 0xA0));
    GX_EndGroup();

    // Save current gui matrix
    gertex::GXMatrix old_item_matrix;
    guMtxCopy(gui_item_matrix.mtx, old_item_matrix.mtx);

    // Double the scale for the game over text
    guMtxScaleApply(gertex::get_matrix().mtx, gui_item_matrix.mtx, 2.0f, 2.0f, 1.0f);

    // Draw the text
    std::string game_over_text = "Game over!";
    Gui::draw_text_with_shadow((view.width - Gui::text_width(game_over_text) * 2) / 4, 60, game_over_text);

    // Restore gui matrix
    guMtxCopy(old_item_matrix.mtx, gui_item_matrix.mtx);

    // Draw score text
    std::string score_text = "§eScore: 0";
    Gui::draw_text_with_shadow((view.width - Gui::text_width(score_text)) / 2, 200, score_text);

    // Draw buttons
    for (size_t i = 0; i < buttons.size(); i++)
    {
        buttons[i].draw(i == selected_button);
    }
}

void GuiGameOver::update()
{
    bool pointer_visible = false;
    bool confirm = false;

    for (input::Device *dev : input::devices)
    {
        if (dev->connected())
        {
            if ((dev->get_buttons_down() & input::BUTTON_CONFIRM))
            {
                confirm = true;
            }
            if (dev->is_pointer_visible())
            {
                pointer_visible = true;
                break;
            }

            Vec3f left_stick = dev->get_left_stick();

            bool up = left_stick.y > 0.5f;
            bool down = left_stick.y < -0.5f;
            navigate(up, down);
            break;
        }
    }

    if (pointer_visible)
    {
        // Handle pointer input
        for (size_t i = 0; i < buttons.size(); i++)
        {
            if (buttons[i].contains(cursor_x, cursor_y) && buttons[i].enabled)
            {
                selected_button = i;
                break;
            }
        }
    }

    // Handle button press
    if (confirm && buttons[selected_button].on_click && buttons[selected_button].enabled)
    {
        Sound click_sound = get_sound("random/click");
        click_sound.position = world->spawn_pos;
        world->play_sound(click_sound);
        buttons[selected_button].on_click();
    }
}

void GuiGameOver::navigate(bool up, bool down)
{
    if (up)
        selected_button = 0;
    else if (down)
        selected_button = 1;
}

void GuiGameOver::respawn()
{
    World *world = this->world;
    GuiDirtscreen *dirt_screen = new GuiDirtscreen;
    dirt_screen->set_text("Respawning...");
    dirt_screen->set_progress(0, 100);
    Gui::set_gui(dirt_screen);

    // Send player to spawn
    world->player.teleport(world->spawn_pos);

    // Reset camera rotation
    get_camera().rot = Vec3f(0, 0, 0);

    // Reset player state
    world->player.dead = false;
    world->loaded = false;
    world->player.chunk = nullptr;
    world->player.health = 20;
    world->player.velocity = Vec3f(0, 0, 0);
    world->player.fall_distance = 0;
    world->player.rotation = Vec3f(0, 0, 0);
    world->player.prev_rotation = Vec3f(0, 0, 0);

    // Tick the world once to update the sound system position
    world->tick();
}

void GuiGameOver::quit_to_title()
{
    quitting = true;
}
