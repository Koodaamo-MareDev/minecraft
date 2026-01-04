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
    Configuration config;
    std::map<std::string, GuiButtonWithValue *> option_button_map;

    void toggle_option(std::string option);
    void quit_to_title();
};

#endif