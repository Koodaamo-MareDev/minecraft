#ifndef GUI_SURVIVAL_HPP
#define GUI_SURVIVAL_HPP

#include <array>
#include "gui_container.hpp"

class GuiSurvival : public GuiGenericContainer
{
public:
    const static size_t hotbar_start = 36;
    const static size_t inventory_start = 9;
    const static size_t armor_start = 5;

    int width = 352;
    int height = 332;

    GuiSurvival(inventory::Container *Container);

    void draw() override;
    bool contains(int x, int y) override;
    void on_result_taken() override;
};
#endif