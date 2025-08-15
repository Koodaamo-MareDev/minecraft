#ifndef GUI_SURVIVAL_HPP
#define GUI_SURVIVAL_HPP

#include <array>
#include "gui.hpp"

class GuiSurvival : public Gui
{
public:
    std::array<GuiSlot, 9> hotbar_slots;
    std::array<GuiSlot, 27> inventory_slots;
    std::array<GuiSlot, 4> armor_slots;

    inventory::Container &linked_container;

    int width = 352;
    int height = 332;

    GuiSurvival(const gertex::GXView &viewport, inventory::Container &Container);

    void draw() override;
    void update() override;
    bool contains(int x, int y) override;
    void close() override;
    void refresh() override;
};
#endif