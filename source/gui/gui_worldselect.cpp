#include "gui_worldselect.hpp"

#include <world/world.hpp>
#include <util/string_utils.hpp>
#include <util/input/input.hpp>
#include <filesystem>
#include <fstream>

GuiWorldSelect::GuiWorldSelect()
{
    namespace fs = std::filesystem;
    fs::current_path("/apps/minecraft");
    for (const auto &entry : fs::directory_iterator("saves"))
    {
        if (entry.is_directory())
        {
            std::string path = entry.path().string();
            WorldInfo world_info = {.folder_name = entry.path().filename().string()};
            if (validate_world(path, world_info))
            {
                world_info.baked_details = world_info.folder_name + " (" + world_info.last_modified + ", " + world_info.size + ")";
                world_list.push_back(world_info);
            }
        }
    }
}

bool GuiWorldSelect::validate_world(const std::string &path, WorldInfo &out_info)
{

    std::ifstream file(path + "/level.dat", std::ios::binary);
    if (!file.is_open())
        return false;
    uint32_t file_size = file.seekg(0, std::ios::end).tellg();
    file.seekg(0, std::ios::beg);
    NBTTagCompound *level = nullptr;
    try
    {
        level = NBTBase::readGZip(file, file_size);
    }
    catch (std::runtime_error &e)
    {
        printf("Failed to load level.dat: %s\n", e.what());
    }
    file.close();

    if (!level)
    {
        return false;
    }
    NBTTagCompound *level_data = level->getCompound("Data");
    if (!level_data)
    {
        delete level;
        return false;
    }
    out_info.world_name = level_data->getString("LevelName");
    int64_t last_modified = level_data->getLong("LastPlayed");
    if (last_modified > 0)
    {
        std::time_t mod_time = last_modified / 1000LL;
        char time_str[32];
        std::strftime(time_str, sizeof(time_str), "%m/%d/%y %I:%M %p", std::localtime(&mod_time));
        out_info.last_modified = time_str;
    }
    else
    {
        out_info.last_modified = "A long time ago";
    }

    int64_t size_on_disk = level_data->getLong("SizeOnDisk");
    if (size_on_disk < 0)
        size_on_disk = 0;
    if (size_on_disk < 1024)
        out_info.size = std::to_string(size_on_disk) + " B";
    else if (size_on_disk < 1024 * 1024)
        out_info.size = str::ftos(size_on_disk / 1024.0) + " KB";
    else if (size_on_disk < 1024 * 1024 * 1024)
        out_info.size = str::ftos(size_on_disk / (1024.0 * 1024.0)) + " MB";
    else
        out_info.size = str::ftos(size_on_disk / (1024.0 * 1024.0) / 1024.0) + " GB";

    delete level;
    return true;
}

void GuiWorldSelect::draw()
{
    gertex::GXView viewport = gertex::get_state().view;

    constexpr int entry_width = 440;
    constexpr int entry_height = 72;
    constexpr int entry_padding = 8;

    const int entry_space = viewport.aspect_correction * (viewport.height - 192 - entry_padding);
    const int selected_world_y = selected_world * entry_height + entry_padding;

    // Follow the selection lazily
    if (selected_world_y - scroll_offset < 0)
        scroll_offset = selected_world_y;
    else if (selected_world_y - scroll_offset > entry_space - entry_height)
        scroll_offset = selected_world_y - (entry_space - entry_height);

    const int rect_count = world_list.size();
    const int rect_start_x = (viewport.width - entry_width) / 2;
    int rect_start_y = (68 + entry_padding - viewport.aspect_correction * scroll_offset);
    if (rect_count * entry_height < entry_space)
    {
        rect_start_y = ((entry_space - rect_count * entry_height) / 2 + 64);
    }

    uint32_t texture_index = properties(BlockID::dirt).m_texture_index;

    float u1 = TEXTURE_NX(texture_index) * 256;
    float v1 = TEXTURE_NY(texture_index) * 256;
    float u2 = TEXTURE_PX(texture_index) * 256;
    float v2 = TEXTURE_PY(texture_index) * 256;

    use_texture(terrain_texture);

    // Draw a darker strip in the middle
    for (int i = 0; i < viewport.width + 64; i += 64)
    {
        for (int j = 4; j < viewport.height - 64; j += 64)
        {
            draw_colored_sprite(terrain_texture, Vec2i(i, j + 4), Vec2i(64, 64), u1, v1, u2, v2, GXColor{0x1F, 0x1F, 0x1F, 0xFF});
        }
    }

    // Draw the world entries
    for (size_t i = 0; i < world_list.size(); i++)
    {
        int rect_y = rect_start_y + viewport.aspect_correction * i * entry_height;

        if (selected_world == i)
        {
            Gui::fill_rect(rect_start_x, rect_y, entry_width, viewport.aspect_correction * entry_height, GXColor{0, 0, 0, 0xFF});
            Gui::draw_rect(rect_start_x, rect_y, entry_width, viewport.aspect_correction * entry_height, 2, GXColor{0x80, 0x80, 0x80, 0xFF});
        }

        Gui::draw_text_with_shadow(rect_start_x + 8, (rect_y + 6), world_list[i].world_name);
        Gui::draw_text_with_shadow(rect_start_x + 8, (rect_y + 28), world_list[i].baked_details, GXColor{0x80, 0x80, 0x80, 0xFF});
    }

    // Draw gradient at the top
    use_texture(icons_texture);
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(Vertex(0.03125f * Vec3f(0, 64, 0), 0 * BASE3D_PIXEL_UV_SCALE, 16 * BASE3D_PIXEL_UV_SCALE, 0, 0, 0, 255));
    GX_Vertex(Vertex(0.03125f * Vec3f(viewport.width, 64, 0), 8 * BASE3D_PIXEL_UV_SCALE, 16 * BASE3D_PIXEL_UV_SCALE, 0, 0, 0, 255));
    GX_Vertex(Vertex(0.03125f * Vec3f(viewport.width, 72, 0), 8 * BASE3D_PIXEL_UV_SCALE, 24 * BASE3D_PIXEL_UV_SCALE, 0, 0, 0, 0));
    GX_Vertex(Vertex(0.03125f * Vec3f(0, 72, 0), 0 * BASE3D_PIXEL_UV_SCALE, 24 * BASE3D_PIXEL_UV_SCALE, 0, 0, 0, 0));
    GX_EndGroup();

    // Draw gradient at the bottom
    GX_BeginGroup(GX_QUADS, 4);
    GX_Vertex(Vertex(0.03125f * Vec3f(0, viewport.aspect_correction * (viewport.height - 128) - 8, 0), 0 * BASE3D_PIXEL_UV_SCALE, 16 * BASE3D_PIXEL_UV_SCALE, 0, 0, 0, 0));
    GX_Vertex(Vertex(0.03125f * Vec3f(viewport.width, viewport.aspect_correction * (viewport.height - 128) - 8, 0), 8 * BASE3D_PIXEL_UV_SCALE, 16 * BASE3D_PIXEL_UV_SCALE, 0, 0, 0, 0));
    GX_Vertex(Vertex(0.03125f * Vec3f(viewport.width, viewport.aspect_correction * (viewport.height - 128), 0), 8 * BASE3D_PIXEL_UV_SCALE, 24 * BASE3D_PIXEL_UV_SCALE, 0, 0, 0, 255));
    GX_Vertex(Vertex(0.03125f * Vec3f(0, viewport.aspect_correction * (viewport.height - 128), 0), 0 * BASE3D_PIXEL_UV_SCALE, 24 * BASE3D_PIXEL_UV_SCALE, 0, 0, 0, 255));
    GX_EndGroup();

    // Draw the top and bottom edges
    use_texture(terrain_texture);
    for (int i = 0; i < viewport.width + 64; i += 64)
    {
        for (int j = 0; j < viewport.height; j += 64)
        {
            if (j == 64)
                j = viewport.height - 128;
            draw_colored_sprite(terrain_texture, Vec2i(i, viewport.aspect_correction * j), Vec2i(64, 64), u1, v1, u2, v2, GXColor{0x3F, 0x3F, 0x3F, 0xFF});
        }
    }

    // Draw the title
    std::string title = "Select World";
    Gui::draw_text_with_shadow((viewport.width - Gui::text_width(title)) / 2, viewport.aspect_correction * 40, title);
}

void GuiWorldSelect::update()
{
    for (input::Device *dev : input::devices)
    {
        if (dev->connected())
        {
            if (dev->get_buttons_down() & input::BUTTON_CONFIRM)
            {
                if (selected_world < world_list.size())
                {
                    current_world = new World(world_list[selected_world].folder_name);
                    Gui::set_gui(nullptr);
                    break;
                }
            }

            Vec3f left_stick = dev->get_left_stick();
            if (left_stick.y < -0.5f && joystick_released)
            {
                joystick_released = false;
                joystick_timer = 0;
                selected_world = selected_world + 1 < world_list.size() ? selected_world + 1 : selected_world;
            }
            else if (left_stick.y > 0.5f && joystick_released)
            {
                joystick_released = false;
                joystick_timer = 0;
                selected_world = (selected_world == 0) ? 0 : selected_world - 1;
            }
            else if (std::abs(left_stick.y) < 0.5f)
            {
                joystick_released = true;
                joystick_timer = 0;
            }
            else if (!joystick_released)
            {
                joystick_timer++;
                if (joystick_timer > 20 && joystick_timer % 15 == 0)
                {
                    if (left_stick.y < -0.5f)
                        selected_world = selected_world + 1 < world_list.size() ? selected_world + 1 : selected_world;
                    else if (left_stick.y > 0.5f)
                        selected_world = (selected_world == 0) ? 0 : selected_world - 1;
                }
            }
            break;
        }
    }
}
