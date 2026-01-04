#ifndef GUI_OPTIONS_HPP
#define GUI_OPTIONS_HPP

#include <gui/gui.hpp>
#include <string>
#include <vector>
#include <util/config.hpp>

class GuiOptions : public Gui
{
public:
    GuiOptions();
    void draw() override;
    void update() override;
    bool contains(int x, int y) override { return true; }
    void close() override {}
    bool use_cursor() override { return false; }

private:
    bool joystick_pressed = true;
    int joystick_timer = 0;
    std::vector<GuiButton> buttons;

    Configuration config;
    std::vector<GuiButtonWithValue> option_buttons;
    std::map<std::string, GuiButtonWithValue *> option_button_map;
    size_t selected_button = 0;

    void navigate(bool up, bool down, bool left, bool right);
    void toggle_option(std::string option);
    void quit_to_title();
};

#endif