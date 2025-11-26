#ifndef GUI_SURVIVAL_HPP
#define GUI_SURVIVAL_HPP

#include <array>
#include <gui/gui_container.hpp>

class GuiSurvival : public GuiGenericContainer
{
public:
    const static size_t hotbar_start = 36;

    int width = 352;
    int height = 332;

    GuiSurvival(EntityPhysical *owner, inventory::Container *container);

    void draw() override;
    bool contains(int x, int y) override;
    void on_result_taken() override;
    void on_interact(size_t slot) override;
    void close() override;
};
#endif