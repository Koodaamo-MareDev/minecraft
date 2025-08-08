#include "gui_dirtscreen.hpp"
#include "util/string_utils.hpp"

gui_dirtscreen::gui_dirtscreen(const gertex::GXView &viewport) : gui(viewport)
{
}

void gui_dirtscreen::draw()
{
    // Fill the screen with the dirt texture
    int texture_index = properties(BlockID::dirt).m_texture_index;
    fill_screen_texture(terrain_texture, viewport, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index));

    for (size_t i = 0; i < text_lines.size(); i++)
    {
        gui::draw_text_with_shadow((viewport.width - gui::text_width(text_lines[i])) / 2, viewport.aspect_correction * 200 + i * 16, text_lines[i]);
    }
    if (max_progress > 0)
    {
        draw_colored_quad((viewport.width - 200) / 2, viewport.aspect_correction * 200 + 72, 200, 4, 0x80, 0x80, 0x80, 0xFF);
        draw_colored_quad((viewport.width - 200) / 2, viewport.aspect_correction * 200 + 72, int(progress) * 200 / int(max_progress), 4, 0x80, 0xFF, 0x80, 0xFF);
    }
}

void gui_dirtscreen::update()
{
}

bool gui_dirtscreen::contains(int x, int y)
{
    return true; // Full screen
}

void gui_dirtscreen::close()
{
}

void gui_dirtscreen::refresh()
{
}

void gui_dirtscreen::set_text(const std::string &text)
{
    text_lines = str::split(text, '\n');
}

void gui_dirtscreen::set_progress(uint8_t progress, uint8_t max_progress)
{
    this->progress = progress;
    this->max_progress = max_progress;
}
