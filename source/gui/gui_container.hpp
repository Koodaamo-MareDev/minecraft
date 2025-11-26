#ifndef GUI_CONTAINER_HPP
#define GUI_CONTAINER_HPP

#include <gui/gui.hpp>

/** @brief Base class for all container gui classes
 *
 * While this can be instantiated directly, one should
 * either provide a proper implementation of the class
 * or use one of the existing implementations of it.
 *
 * There is a simple rule of thumb to determine if a
 * gui should be a container gui:
 * If the gui has item slots, it is a container gui.
 */

class GuiGenericContainer : public Gui
{
public:
    std::string title;

    int width = 352;
    int height = 444;

    /**
     * @brief Constructs a generic container
     *
     * @param owner The entity that owns this gui instance
     * @param container The container that this gui is linked to
     * @param window_id Multiplayer window id used for syncing
     * @param title Title that should be displayed in this gui
     *
     * @note This is the base constructor and should be called
     * from an inheriting class, not directly.
     **/
    GuiGenericContainer(EntityPhysical *owner, inventory::Container *container, uint8_t window_id, std::string title) : Gui(window_id), title(title), owner(owner), linked_container(container)
    {
    }
    virtual void refresh();
    virtual void update();
    virtual void close();
    virtual bool use_cursor() { return true; }
    virtual void on_result_taken();
    virtual void on_interact(size_t slot) {}
    virtual void set_slot(size_t slot, inventory::ItemStack item);
    virtual inventory::ItemStack get_slot(size_t slot);
    virtual ~GuiGenericContainer();

protected:
    EntityPhysical *owner;
    inventory::Container *linked_container;
    std::vector<GuiSlot *> slots;
};

class GuiContainer : public GuiGenericContainer
{
public:
    GuiContainer(EntityPhysical *owner, inventory::Container *container, uint8_t window_id = 1, std::string title = "Chest", uint8_t slots = 27);

    void draw() override;
    bool contains(int x, int y) override;
};
#endif