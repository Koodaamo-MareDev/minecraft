#ifndef GUI_WORLDSELECT_HPP
#define GUI_WORLDSELECT_HPP

#include <gui/gui.hpp>
#include <string>
#include <vector>

struct WorldInfo
{
    std::string folder_name;
    std::string world_name;
    std::string last_modified;
    std::string size;

    std::string baked_details;
};

class GuiWorldSelect : public Gui
{
public:
    GuiWorldSelect(World **current_world);

    bool validate_world(const std::string &path, WorldInfo &out_info);

    void draw() override;
    void update() override;
    bool contains(int x, int y) override { return true; }
    void close() override {}
    bool use_cursor() override { return false; }

protected:
    size_t selected_world = 0;
    std::vector<WorldInfo> world_list;
    World **current_world;
private:
    bool joystick_released = true;
    int joystick_timer = 0;
    int scroll_offset = 0;
};

#endif