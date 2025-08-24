#ifndef GUI_HPP
#define GUI_HPP

#include "render_gui.hpp"
#include "inventory.hpp"

extern int cursor_x;
extern int cursor_y;

class GuiSlot
{
public:
    int x = 0;
    int y = 0;
    uint8_t slot = 0;
    inventory::ItemStack item;
    GuiSlot() = default;
    GuiSlot(int x, int y, inventory::ItemStack item) : x(x), y(y), item(item) {}

    virtual inventory::ItemStack interact(inventory::ItemStack hand, bool right_click);

    bool contains(int x, int y)
    {
        return x >= this->x && x < this->x + 32 && y >= this->y && y < this->y + 32;
    }
};

class GuiResultSlot : public GuiSlot
{
public:
    GuiResultSlot(int x, int y, inventory::ItemStack item) : GuiSlot(x, y, item) {}

    inventory::ItemStack interact(inventory::ItemStack hand, bool right_click) override;
};

class Gui
{
public:
    inventory::ItemStack item_in_hand;
    Gui()
    {
    }
    virtual ~Gui() = default;
    virtual void draw() = 0;
    virtual void update() = 0;
    virtual bool contains(int x, int y) = 0;
    virtual void close() = 0;
    virtual void refresh() {}
    virtual bool use_cursor() { return true; }

    static void init_matrices(float aspect_correction);
    static int text_width(std::string str);
    static void draw_text(int x, int y, std::string str, GXColor color = {255, 255, 255, 255});
    static void draw_text_with_shadow(int x, int y, std::string str, GXColor color = {255, 255, 255, 255});
    static void draw_item(int x, int y, inventory::ItemStack item, gertex::GXView &viewport);

    /**
     * This function draws the items in the Container.
     * The width of the Container is 9 slots.
     * The Container is drawn row by row, starting from the top left corner.
     * NOTE: It is up to the user to draw the background.
     **/
    static void draw_container(int x, int y, inventory::Container &Container, gertex::GXView &viewport);

    static Gui *get_gui()
    {
        return current_gui;
    }

    static void set_gui(Gui *gui)
    {
        if (current_gui && current_gui != gui)
        {
            current_gui->close();
            delete current_gui;
        }
        current_gui = gui;
    }

protected:
    static Gui *current_gui;
};

extern gertex::GXMatrix gui_block_matrix;
extern gertex::GXMatrix gui_item_matrix;

#endif