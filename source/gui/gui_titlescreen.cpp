#include "gui_titlescreen.hpp"

#include <util/input/input.hpp>
#include <gui/gui_worldselect.hpp>
#include <gui/gui_dirtscreen.hpp>
#include <world/world.hpp>
#include <render/render.hpp>
#include <util/string_utils.hpp>
#include <ported/Random.hpp>
#include <ported/SystemTime.hpp>
#include <sounds.hpp>
#include <sound.hpp>

static std::string logo = " *   * * *   * *** *** *** *** *** ***"
                          " ** ** * **  * *   *   * * * * *    * "
                          " * * * * * * * **  *   **  *** **   * "
                          " *   * * *  ** *   *   * * * * *    * "
                          " *   * * *   * *** *** * * * * *    * ";

GuiTitleScreen::GuiTitleScreen(World **current_world)
    : current_world(current_world)
{
    gertex::GXView view = gertex::get_state().view;

    int view_height = view.aspect_correction * view.height;

    buttons.push_back(GuiButton((view.width - 400) / 2, view_height / 2 - 48, 400, 40, "Singleplayer", std::bind(&GuiTitleScreen::join_singleplayer, this)));
    buttons.push_back(GuiButton((view.width - 400) / 2, view_height / 2, 400, 40, "Multiplayer", std::bind(&GuiTitleScreen::join_multiplayer, this)));
    buttons.push_back(GuiButton((view.width - 400) / 2, view_height / 2 + 48, 400, 40, "Mods and Texture Packs", std::bind(&GuiTitleScreen::join_multiplayer, this)));
    buttons.push_back(GuiButton((view.width - 400) / 2, view_height / 2 + 96, 196, 40, "Options", []() {}));
    buttons.push_back(GuiButton((view.width - 400) / 2 + 204, view_height / 2 + 96, 196, 40, "Quit Game", std::bind(&GuiTitleScreen::quit_game, this)));
    buttons[2].enabled = false;

    // Randomize the initial Z positions of the logo blocks
    javaport::Random rng;
    logo_block_z.resize(logo.size());
    for (size_t i = 0; i < logo.size(); i++)
    {
        logo_block_z[i] = rng.nextFloat() * 10.0f;
    }

    // Select a splash text from splashes.txt
    std::ifstream file("/apps/minecraft/resources/title/splashes.txt");
    if (file.is_open())
    {
        std::vector<std::string> splashes;
        std::string line;
        while (std::getline(file, line))
        {
            // Remove any newline characters
            auto is_newline = [](char c)
            { return c == '\r' || c == '\n'; };
            line.erase(std::remove_if(line.begin(), line.end(), is_newline), line.end());

            splashes.push_back(line);
        }
        file.close();

        if (splashes.size() > 0)
            splash_text = "§e" + splashes[rng.nextInt(splashes.size())];
    }
}

void GuiTitleScreen::draw()
{
    gertex::GXView viewport = gertex::get_state().view;

    // Disable depth write
    GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);

    // Fill the screen with the dirt texture
    int texture_index = properties(BlockID::dirt).m_texture_index;
    fill_screen_texture(terrain_texture, viewport, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index));

    for (size_t i = 0; i < buttons.size(); i++)
    {
        buttons[i].draw(i == selected_button);
    }

    gertex::GXState state = gertex::get_state();
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, BASE3D_POS_FRAC_BITS);

    // We want to render the blocks in perspective mode
    gertex::perspective(state.view);

    // Enable backface culling for terrain
    GX_SetCullMode(GX_CULL_BACK);

    // Prepare opaque rendering parameters
    GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    gertex::set_blending(gertex::GXBlendMode::overwrite);
    GX_SetAlphaUpdate(GX_TRUE);

    gertex::set_alpha_cutoff(0);
    Block block = {.id = uint8_t(BlockID::stone), .visibility_flags = 0x7F, .meta = 0, .light = 0xFF};

    // Render the logo blocks
    use_texture(terrain_texture);
    for (size_t i = 0; i < logo.size(); i++)
    {
        if (logo[i] != '*')
            continue;
        logo_block_z[i] -= 0.3f;
        if (logo_block_z[i] < 0)
            logo_block_z[i] = 0;
        Vec3f logo_block_pos = Vec3f((int(i) % 38) - 19, 14 - (int(i) / 38), logo_block_z[i] - 16);
        Camera &camera = get_camera();
        camera.fov = 70.0f;
        camera.rot = Vec3f(10, 0, 0);
        camera.position = Vec3f(0);
        transform_view(gertex::get_view_matrix(), logo_block_pos, Vec3f(1.0f), Vec3f(90, 0, 0));
        render_single_block(block, false);
    }

    // Return back to GUI rendering
    GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    gertex::set_state(state);

    float sintime = std::sin(M_PI * (javaport::System::currentTimeMillis() % 2000) / 500.0f);

    float scale = 144.0f / std::max(text_width(splash_text), 72);

    // Prepare the splash text matrix
    transform_view_screen(gertex::get_view_matrix(), Vec3f((viewport.width - 128), viewport.height * 0.25f, 0), Vec3f(scale * (1 + std::abs(sintime) * 0.125f)), Vec3f(0, 0, 22.5f));

    gertex::GXMatrix old_item_matrix;
    guMtxCopy(gui_item_matrix.mtx, old_item_matrix.mtx);
    guMtxCopy(gertex::get_matrix().mtx, gui_item_matrix.mtx);

    // Draw splash text
    draw_text_with_shadow(-text_width(splash_text) / 2, 0, splash_text, GXColor{255, 255, 255, 255});

    guMtxCopy(old_item_matrix.mtx, gui_item_matrix.mtx);
}

void GuiTitleScreen::update()
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

            bool left = left_stick.x < -0.5f;
            bool right = left_stick.x > 0.5f;
            bool up = left_stick.y > 0.5f;
            bool down = left_stick.y < -0.5f;
            bool prev_joystick_pressed = joystick_pressed;
            joystick_pressed = left || right || up || down;

            if (joystick_pressed && !prev_joystick_pressed)
            {
                joystick_timer = 0;
                navigate(left, right, up, down);
            }
            else if (joystick_pressed)
            {
                joystick_timer++;
                if (joystick_timer > 20 && joystick_timer % 10 == 0)
                {
                    navigate(left, right, up, down);
                }
            }
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
        if (sound_system)
        {
            Sound click_sound = get_sound("random/click");
            click_sound.position = sound_system->head_position;
            sound_system->play_sound(click_sound);
        }
        buttons[selected_button].on_click();
    }
}
void GuiTitleScreen::navigate(bool left, bool right, bool up, bool down)
{
    size_t prev_selected = selected_button;
    while (true)
    {
        if (down && selected_button <= 2)
            selected_button = selected_button + 1 < buttons.size() ? selected_button + 1 : selected_button;
        else if (up)
        {
            // Special case where buttons 3 and 4 are on the same row
            if (selected_button > 2)
                selected_button = 2;
            else
                selected_button = (selected_button == 0) ? 0 : selected_button - 1;
        }
        else if (left && selected_button == 4)
            selected_button = 3;
        else if (right && selected_button == 3)
            selected_button = 4;
        if (selected_button == prev_selected || buttons[selected_button].enabled)
            break; // No more movement possible
    }
}

void GuiTitleScreen::join_singleplayer()
{
    Gui::set_gui(new GuiWorldSelect(current_world));
}

void GuiTitleScreen::join_multiplayer()
{

    Configuration config;
    config.load();

    // Optionally join a server if the Configuration specifies one
    std::string server_ip = config.get<std::string>("server", "");

    if (!server_ip.empty() && Crapper::initNetwork())
    {
        int32_t server_port = config.get<int32_t>("port", 25565);

        World *new_world = new World;
        try
        {
            Gui::set_gui(new GuiDirtscreen);

            // Attempt to connect to the server
            new_world->set_remote(true);
            new_world->client->joinServer(server_ip, server_port);
            *current_world = new_world;
        }
        catch (std::runtime_error &e)
        {
            // If connection fails, set the world to local
            new_world->set_remote(false);
            delete new_world;
            printf("Failed to connect to server: %s\n", e.what());
        }
    }
}

void GuiTitleScreen::quit_game()
{
    Gui::set_gui(nullptr);
}
