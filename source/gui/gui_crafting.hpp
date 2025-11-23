#ifndef GUI_SURVIVAL_HPP
#define GUI_SURVIVAL_HPP

#include <array>
#include <gui/gui_container.hpp>

class GuiCrafting : public GuiGenericContainer
{
public:
    int width = 352;
    int height = 332;

    GuiCrafting(EntityPhysical *owner, inventory::Container *Container, uint8_t window_id);

    void draw() override;
    bool contains(int x, int y) override;
    void on_result_taken() override;
    void on_interact(size_t slot);
    void close() override;
};
#endif