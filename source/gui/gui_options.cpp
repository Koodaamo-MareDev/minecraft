#include "gui_options.hpp"

#include <gui/gui_dirtscreen.hpp>
#include <util/input/input.hpp>
#include <world/world.hpp>
#include <sounds.hpp>

GuiOptions::GuiOptions()
{
    try
    {
        // Attempt to load the Configuration file from the default location
        config.load();
    }
    catch (std::runtime_error &e)
    {
        debug::print("Using config defaults. %s\n", e.what());
    }
    gertex::GXView view = gertex::get_state().view;

    auto add_toggle_button = [&](std::string key, std::string friendly_name)
    {
        int button_x = option_buttons.size() % 2 == 0 ? view.width / 2 - 310 : view.width / 2 + 10;
        int button_y = 68 + (option_buttons.size() / 2) * 48;
        std::string value = ((int)config.get<int>(key, 0)) ? "ON" : "OFF";
        option_buttons.push_back(GuiButtonWithValue(button_x, button_y, 300, 40, friendly_name, key, value, std::bind(&GuiOptions::toggle_option, this, key)));
    };

    buttons.push_back(GuiButton((view.width - 400) / 2, view.height - 48, 400, 40, "Done", std::bind(&GuiOptions::quit_to_title, this)));

    add_toggle_button("vsync", "Use VSync");
    add_toggle_button("mono_lighting", "Monochrome lighting");
    add_toggle_button("smooth_lighting", "Smooth lighting");

    for (size_t i = 0; i < option_buttons.size(); i++)
    {
        option_button_map[option_buttons[i].key] = &option_buttons[i];
    }
}

void GuiOptions::draw()
{
    gertex::GXView view = gertex::get_state().view;

    // int view_height = view.aspect_correction * view.height;

    // Draw the background
    // Fill the screen with the dirt texture
    int texture_index = properties(BlockID::dirt).m_texture_index;
    fill_screen_texture(terrain_texture, view, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index));

    // Draw buttons
    for (size_t i = 0; i < option_buttons.size(); i++)
    {
        option_buttons[i].draw(i == selected_button);
    }

    for (size_t i = 0; i < buttons.size(); i++)
    {
        buttons[i].draw(selected_button >= option_buttons.size() && i == selected_button - option_buttons.size());
    }

    const std::string title_text = "Options";

    // Draw splash text
    draw_text_with_shadow((view.width - text_width(title_text)) / 2, 40, title_text);
}

void GuiOptions::update()
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
            bool left = left_stick.x < -0.5f;
            bool right = left_stick.x > 0.5f;

            bool prev_joystick_pressed = joystick_pressed;
            joystick_pressed = left || right || up || down;

            if (joystick_pressed && !prev_joystick_pressed)
            {
                joystick_timer = 0;
                navigate(up, down, left, right);
            }
            else if (joystick_pressed)
            {
                joystick_timer++;
                if (joystick_timer > 20 && joystick_timer % 10 == 0)
                {
                    navigate(up, down, left, right);
                }
            }
            break;
        }
    }

    if (pointer_visible)
    {
        // Handle pointer input
        for (size_t i = 0; i < option_buttons.size(); i++)
        {
            if (option_buttons[i].contains(cursor_x, cursor_y) && option_buttons[i].enabled)
            {
                selected_button = i;
                break;
            }
        }
        for (size_t i = 0; i < buttons.size(); i++)
        {
            if (buttons[i].contains(cursor_x, cursor_y) && buttons[i].enabled)
            {
                selected_button = option_buttons.size() + i;
                break;
            }
        }
    }

    // Handle button press
    if (confirm)
    {
        std::function<void()> on_click = nullptr;
        if (selected_button < option_buttons.size())
        {
            if (option_buttons[selected_button].enabled)
                on_click = option_buttons[selected_button].on_click;
        }
        else
        {
            if (option_buttons[selected_button - option_buttons.size()].enabled)
                on_click = buttons[selected_button - option_buttons.size()].on_click;
        }
        if (on_click)
        {
            if (sound_system)
            {
                Sound click_sound = get_sound("random/click");
                click_sound.position = sound_system->head_position;
                sound_system->play_sound(click_sound);
            }
            on_click();
        }
    }
}

void GuiOptions::navigate(bool up, bool down, bool left, bool right)
{
    size_t max_selection = option_buttons.size() + buttons.size();
    if (max_selection == 0) // No buttons
        return;
    bool was_regular = selected_button >= option_buttons.size();
    if (was_regular)
    {
        // The regular button layout is vertical
        if (up && selected_button > 0)
            selected_button--;
        else if (down)
            selected_button++;
    }
    else
    {
        // Bit 0 indicates whether the button is on the right or left.
        if (left)
            selected_button &= ~1;
        else if (right && option_buttons.size() >= 2)
            selected_button |= 1;

        // Two entries per row
        if (up && selected_button >= 2)
            selected_button -= 2;
        else if (down)
            selected_button += 2;
    }
    if ((selected_button < option_buttons.size()) != was_regular)
    {
        if (was_regular)
            selected_button = option_buttons.size();
    }
    if (selected_button >= max_selection)
        selected_button = max_selection - 1;
}

void GuiOptions::toggle_option(std::string key)
{
    int prev_value = config.get<int>(key, 0);
    int new_value = !prev_value;
    config[key] = new_value;
    std::string value = ((int)config.get<int>(key, 0)) ? "ON" : "OFF";
    if (option_button_map.find(key) != option_button_map.end())
        option_button_map[key]->value = value;
}

void GuiOptions::quit_to_title()
{
    quitting = true;
    config.save();
}
