#ifndef GUI_CONTAINER_HPP
#define GUI_CONTAINER_HPP

#include "gui.hpp"
#include "inventory.hpp"

class GuiGenericContainer : public Gui
{
public:
    std::string title;

    int width = 352;
    int height = 444;

    GuiGenericContainer(inventory::Container *container, uint8_t slots, uint8_t window_id, std::string title) : Gui(), title(title), linked_container(container)
    {
        this->window_id = window_id;
    }
    virtual void refresh();
    virtual void update();
    virtual void close();
    virtual bool use_cursor() { return true; }
    virtual void on_result_taken();
    void set_slot(size_t slot, inventory::ItemStack item);
    inventory::ItemStack get_slot(size_t slot);
    ~GuiGenericContainer();

protected:
    inventory::Container *linked_container;
    std::vector<GuiSlot *> slots;
};

class GuiContainer : public GuiGenericContainer
{
public:
    GuiContainer(inventory::Container *container, uint8_t slots, uint8_t window_id = 1, std::string title = "Chest");

    void draw() override;
    bool contains(int x, int y) override;
};
#endif