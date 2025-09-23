#include "gui_titlescreen.hpp"
#include "util/input/input.hpp"
#include "gui_worldselect.hpp"
#include "gui_dirtscreen.hpp"
#include "world.hpp"

GuiTitleScreen::GuiTitleScreen()
{
    gertex::GXView view = gertex::get_state().view;

    int view_height = view.aspect_correction * view.height;

    buttons.push_back(GuiButton((view.width - 400) / 2, view_height / 2 - 48, 400, 40, "Singleplayer", join_singleplayer));
    buttons.push_back(GuiButton((view.width - 400) / 2, view_height / 2, 400, 40, "Multiplayer", join_multiplayer));
    buttons.push_back(GuiButton((view.width - 400) / 2, view_height / 2 + 48, 400, 40, "Mods and Texture Packs", join_multiplayer));
    buttons.push_back(GuiButton((view.width - 400) / 2, view_height / 2 + 96, 196, 40, "Options", []() {}));
    buttons.push_back(GuiButton((view.width - 400) / 2 + 204, view_height / 2 + 96, 196, 40, "Quit Game", quit_game));
    buttons[2].enabled = false;
}

void GuiTitleScreen::draw()
{
    gertex::GXView viewport = gertex::get_state().view;

    // Fill the screen with the dirt texture
    int texture_index = properties(BlockID::dirt).m_texture_index;
    fill_screen_texture(terrain_texture, viewport, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index));

    for (size_t i = 0; i < buttons.size(); i++)
    {
        buttons[i].draw(i == selected_button);
    }
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
        buttons[selected_button].on_click();
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
    Gui::set_gui(new GuiWorldSelect);
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

        current_world = new World;
        try
        {
            Gui::set_gui(new GuiDirtscreen);

            // Attempt to connect to the server
            current_world->set_remote(true);
            current_world->client->joinServer(server_ip, server_port);
        }
        catch (std::runtime_error &e)
        {
            // If connection fails, set the world to local
            current_world->set_remote(false);
            delete current_world;
            printf("Failed to connect to server: %s\n", e.what());
        }
    }
}

void GuiTitleScreen::quit_game()
{
    Gui::set_gui(nullptr);
}
