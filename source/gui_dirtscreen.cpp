#include "gui_dirtscreen.hpp"
#include "util/string_utils.hpp"

GuiDirtscreen::GuiDirtscreen(const gertex::GXView &viewport) : Gui(viewport)
{
}

void GuiDirtscreen::draw()
{
    // Fill the screen with the dirt texture
    int texture_index = properties(BlockID::dirt).m_texture_index;
    fill_screen_texture(terrain_texture, viewport, TEXTURE_NX(texture_index), TEXTURE_NY(texture_index), TEXTURE_PX(texture_index), TEXTURE_PY(texture_index));

    for (size_t i = 0; i < text_lines.size(); i++)
    {
        Gui::draw_text_with_shadow((viewport.width - Gui::text_width(text_lines[i])) / 2, viewport.aspect_correction * 200 + i * 16, text_lines[i]);
    }
    if (max_progress > 0)
    {
        draw_colored_quad((viewport.width - 200) / 2, viewport.aspect_correction * 200 + 72, 200, 4, 0x80, 0x80, 0x80, 0xFF);
        draw_colored_quad((viewport.width - 200) / 2, viewport.aspect_correction * 200 + 72, int(progress) * 200 / int(max_progress), 4, 0x80, 0xFF, 0x80, 0xFF);
    }
}

void GuiDirtscreen::update()
{
}

bool GuiDirtscreen::contains(int x, int y)
{
    return true; // Full screen
}

void GuiDirtscreen::close()
{
}

void GuiDirtscreen::refresh()
{
}

void GuiDirtscreen::set_text(const std::string &text)
{
    text_lines = str::split(text, '\n');
}

void GuiDirtscreen::set_progress(uint8_t progress, uint8_t max_progress)
{
    this->progress = progress;
    this->max_progress = max_progress;
}
