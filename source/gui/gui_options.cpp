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
        int button_x = buttons.size() % 2 == 0 ? view.width / 2 - 310 : view.width / 2 + 10;
        int button_y = 68 + (buttons.size() / 2) * 48;
        std::string value = ((int)config.get<int>(key, 0)) ? "ON" : "OFF";
        GuiButtonWithValue *button = new GuiButtonWithValue(button_x, button_y, 300, 40, friendly_name, key, value, std::bind(&GuiOptions::toggle_option, this, key));
        buttons.push_back(button);
        option_button_map[key] = button;
    };

    add_toggle_button("vsync", "Use VSync");
    add_toggle_button("mono_lighting", "Monochrome lighting");
    add_toggle_button("smooth_lighting", "Smooth lighting");
    add_toggle_button("fast_leaves", "Fast leaves");

    buttons.push_back(new GuiButton((view.width - 400) / 2, view.height - 48, 400, 40, "Done", std::bind(&GuiOptions::quit_to_title, this)));
}

void GuiOptions::draw()
{
    gertex::GXView view = gertex::get_state().view;

    // Fill the screen with the dirt texture
    int texture_index = properties(BlockID::dirt).m_texture_index;
    fill_screen_texture(terrain_texture, view, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index));

    const std::string title_text = "Options";

    // Draw title
    draw_text_with_shadow((view.width - text_width(title_text)) / 2, 40, title_text);

    Gui::draw_buttons();
}

void GuiOptions::update()
{
    Gui::update_buttons();
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
