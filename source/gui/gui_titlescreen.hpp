#ifndef GUI_TITLESCREEN_HPP
#define GUI_TITLESCREEN_HPP

#include <gui/gui.hpp>
#include <string>
#include <vector>
class SoundSystem;
class GuiTitleScreen : public Gui
{
public:
    std::string splash_text = "§ePorted by Myntti!";
    World **current_world;
    SoundSystem *sound_system;

    GuiTitleScreen(World **current_world);

    void draw() override;
    void update() override;
    bool contains(int x, int y) override { return true; }
    void close() override {}
    bool use_cursor() override { return false; }

private:
    std::vector<float> logo_block_z;
    std::vector<GuiButton> buttons;
    size_t selected_button = 0;
    bool joystick_pressed = true;
    int joystick_timer = 0;
    void navigate(bool left, bool right, bool up, bool down);
    void join_singleplayer();
    void join_multiplayer();
    void quit_game();
};
#endif