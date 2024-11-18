#ifndef GUI_SURVIVAL_HPP
#define GUI_SURVIVAL_HPP

#include <array>
#include "gui.hpp"

class gui_survival : public gui
{
public:
    std::array<gui_slot, 9> hotbar_slots;
    std::array<gui_slot, 27> inventory_slots;
    std::array<gui_slot, 4> armor_slots;

    inventory::container &linked_container;

    int width = 352;
    int height = 332;

    gui_survival(const view_t &viewport, inventory::container &container);

    void draw() override;
    void update() override;
    bool contains(int x, int y) override;
    void close() override;
    void refresh() override;
};
#endif