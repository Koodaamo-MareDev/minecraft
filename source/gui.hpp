#ifndef GUI_HPP
#define GUI_HPP

#include "render_gui.hpp"
#include "inventory.hpp"

extern uint32_t raw_wiimote_down;
extern uint32_t raw_wiimote_held;
extern int cursor_x;
extern int cursor_y;
extern bool wiimote_ir_visible;

class gui_slot
{
public:
    int x = 0;
    int y = 0;
    uint8_t slot = 0;
    inventory::item_stack item;
    gui_slot() = default;
    gui_slot(int x, int y, inventory::item_stack item) : x(x), y(y), item(item) {}

    inventory::item_stack interact(inventory::item_stack hand, bool right_click);

    bool contains(int x, int y)
    {
        return x >= this->x && x < this->x + 32 && y >= this->y && y < this->y + 32;
    }
};

class gui
{
public:
    gertex::GXView viewport;
    gui() = delete;
    inventory::item_stack item_in_hand;
    gui(const gertex::GXView &viewport) : viewport(viewport)
    {
    }
    virtual ~gui() = default;
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
    static void draw_item(int x, int y, inventory::item_stack item, gertex::GXView &viewport);

    /**
     * This function draws the items in the container.
     * The width of the container is 9 slots.
     * The container is drawn row by row, starting from the top left corner.
     * NOTE: It is up to the user to draw the background.
     **/
    static void draw_container(int x, int y, inventory::container &container, gertex::GXView &viewport);

    static gui *get_gui()
    {
        return current_gui;
    }

    static void set_gui(gui *gui)
    {
        if (current_gui && current_gui != gui)
        {
            current_gui->close();
            delete current_gui;
        }
        current_gui = gui;
    }

protected:
    static gui *current_gui;
};

extern gertex::GXMatrix gui_block_matrix;
extern gertex::GXMatrix gui_item_matrix;

#endif