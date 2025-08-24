#ifndef GUI_SURVIVAL_HPP
#define GUI_SURVIVAL_HPP

#include <array>
#include "gui.hpp"

class GuiSurvival : public Gui
{
public:
    std::vector<GuiSlot*> slots;
    const static size_t hotbar_start = 36;
    const static size_t inventory_start = 9;
    const static size_t armor_start = 5;

    inventory::Container &linked_container;

    int width = 352;
    int height = 332;

    GuiSurvival(inventory::Container &Container);

    void draw() override;
    void update() override;
    bool contains(int x, int y) override;
    void close() override;
    void refresh() override;
};
#endif