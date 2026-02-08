#ifndef GUI_SMELTING_HPP
#define GUI_SMELTING_HPP

#include <array>
#include <gui/gui_container.hpp>

class TileEntityFurnace;

class GuiSmelting : public GuiGenericContainer
{
public:
    int width = 352;
    int height = 332;
    int burn_time_percent = 0;
    int cook_time_percent = 0;

    TileEntityFurnace *furnace = nullptr;

    GuiSmelting(EntityPhysical *owner, inventory::Container *Container, uint8_t window_id, TileEntityFurnace *furnace = nullptr);

    void update() override;
    void draw() override;
    bool contains(int x, int y) override;
    void on_result_taken();
};
#endif