#ifndef GUI_TITLESCREEN_HPP
#define GUI_TITLESCREEN_HPP

#include <gui/gui.hpp>
#include <string>
#include <vector>
class GuiTitleScreen : public Gui
{
public:
    std::string splash_text = "§ePorted by Myntti!";
    World **current_world;

    GuiTitleScreen(World **current_world);

    void draw() override;
    void update() override;
    bool contains(int x, int y) override { return true; }
    void close() override {}
    bool use_cursor() override { return false; }

private:
    std::vector<float> logo_block_z;
    std::vector<float> logo_block_speed;
    void join_singleplayer();
    void join_multiplayer();
    void options();
    void quit_game();
};
#endif