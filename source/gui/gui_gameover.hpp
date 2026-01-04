#ifndef GUI_GAMEOVER_HPP
#define GUI_GAMEOVER_HPP

#include <gui/gui.hpp>
#include <string>
#include <vector>
class World;

class GuiGameOver : public Gui
{
public:
    GuiGameOver(World *world);
    void draw() override;
    void update() override;
    bool contains(int x, int y) override { return true; }
    void close() override {}
    bool use_cursor() override { return false; }

private:
    World *world = nullptr;

    void respawn();
    void quit_to_title();
};

#endif