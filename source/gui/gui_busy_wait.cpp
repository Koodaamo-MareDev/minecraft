#include "gui_busy_wait.hpp"
#include <ported/SystemTime.hpp>
#include <util/string_utils.hpp>
const static std::string loading_text[4] = {"O o o", "o O o", "o o O", "o O o"};

void GuiBusyWait::draw()
{
    gertex::GXView viewport = gertex::get_state().view;

    // Fill the screen with the dirt texture
    int texture_index = properties(BlockID::dirt).m_texture_index;
    fill_screen_texture(terrain_texture, viewport, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index));
    size_t i = 0;
    for (; i < text_lines.size(); i++)
    {
        Gui::draw_text_with_shadow((viewport.width - Gui::text_width(text_lines[i])) / 2, viewport.aspect_correction * 200 + i * 16, text_lines[i]);
    }
    i++;
    std::string current_loading_text = loading_text[(javaport::System::currentTimeMillis() / 300) % 4];
    Gui::draw_text_with_shadow((viewport.width - Gui::text_width(current_loading_text)) / 2, viewport.aspect_correction * 200 + i * 16, current_loading_text);
}

void GuiBusyWait::update()
{
}

bool GuiBusyWait::contains(int x, int y)
{
    return true; // Full screen
}

void GuiBusyWait::close()
{
}

void GuiBusyWait::refresh()
{
}

void GuiBusyWait::set_text(const std::string &text)
{
    text_lines = str::split(text, '\n');
}
