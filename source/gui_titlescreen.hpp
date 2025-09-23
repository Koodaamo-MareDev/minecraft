#ifndef GUI_TITLESCREEN_HPP
#define GUI_TITLESCREEN_HPP

#include "gui.hpp"
#include <string>
#include <vector>
class GuiTitleScreen : public Gui
{
public:
    GuiTitleScreen();

    void draw() override;
    void update() override;
    bool contains(int x, int y) override { return true; }
    void close() override {}
    bool use_cursor() override { return false; }

private:
    std::vector<GuiButton> buttons;
    size_t selected_button = 0;
    bool joystick_pressed = true;
    int joystick_timer = 0;
    void navigate(bool left, bool right, bool up, bool down);
    static void join_singleplayer();
    static void join_multiplayer();
    static void quit_game();
};
#endif