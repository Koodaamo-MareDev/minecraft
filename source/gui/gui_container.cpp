#include <gui/gui_container.hpp>
#include <world/world.hpp>
#include <util/input/input.hpp>

GuiContainer::GuiContainer(EntityPhysical *owner, inventory::Container *container, uint8_t window_id, std::string title, uint8_t slots) : GuiGenericContainer(owner, container, window_id, title)
{
    int inventory_height = 192;
    int container_height = 36 + ((slots + 8) / 9) * 36;
    this->height = container_height + inventory_height;

    gertex::GXView viewport = gertex::get_state().view;

    int start_x = (viewport.width - width) / 2;
    int start_y = (viewport.aspect_correction * viewport.height - height) / 2;

    // Container slots
    for (uint32_t i = 0; i < slots; i++)
    {
        this->slots.push_back(new GuiSlot(start_x + 16 + (i % 9) * 36, start_y + 36 + (i / 9) * 36, inventory::ItemStack()));
    }

    // Inventory
    for (size_t i = 0; i < 27; i++)
    {
        this->slots.push_back(new GuiSlot(start_x + 16 + (i % 9) * 36, start_y + container_height + 28 + (i / 9) * 36, inventory::ItemStack()));
    }

    // Hotbar
    for (size_t i = 0; i < 9; i++)
    {
        this->slots.push_back(new GuiSlot(start_x + 16 + i * 36, start_y + container_height + 144, inventory::ItemStack()));
    }

    refresh();
}

void GuiContainer::draw()
{
    gertex::GXView viewport = gertex::get_state().view;

    draw_colored_quad(0, 0, viewport.width, viewport.height, 0, 0, 0, 128);

    int inventory_height = 192;
    int container_height = this->height - inventory_height;

    int start_x = (viewport.width - width) / 2;
    int start_y = (viewport.aspect_correction * viewport.height - height) / 2;

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Disable backface culling for the background
    GX_SetCullMode(GX_CULL_NONE);

    // Draw the container part of the background
    draw_textured_quad(container_texture, start_x, start_y, width, container_height - 2, 0, 0, width / 2, (container_height - 2) / 2);

    // Draw the inventory part of the background
    draw_textured_quad(container_texture, start_x, start_y + container_height - 2, width, inventory_height + 2, 0, 221 - inventory_height / 2, width / 2, 222);

    // Enable backface culling for the items
    GX_SetCullMode(GX_CULL_BACK);

    // Draw the hotbar
    for (GuiSlot *slot : slots)
    {
        draw_item(slot->x, slot->y, slot->item, viewport);
    }

    // Disable backface culling for the rest of the GUI
    GX_SetCullMode(GX_CULL_NONE);

    // Get the highlighted slot
    GuiSlot *highlighted_slot = nullptr;
    for (GuiSlot *slot : slots)
    {
        if (slot->contains(cursor_x, cursor_y))
        {
            highlighted_slot = slot;
            break;
        }
    }
    // Draw the highlight
    if (highlighted_slot)
    {
        // Enable direct colors
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

        // Reset the position matrix
        gertex::use_matrix(gui_item_matrix);

        draw_colored_quad(highlighted_slot->x, highlighted_slot->y, 32, 32, 255, 255, 255, 128);
    }

    if (item_in_hand.id != 0)
    {
        // Enable backface culling for the items
        GX_SetCullMode(GX_CULL_BACK);

        // Enable indexed colors
        GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

        // Draw the item in hand
        draw_item(cursor_x - 16, cursor_y - 16, item_in_hand, viewport);
    }
}

void GuiGenericContainer::update()
{
    bool clicked = false;
    bool is_right_click = false;
    for (input::Device *dev : input::devices)
    {
        if (!dev->connected())
            continue;
        uint32_t btn_down = dev->get_buttons_down();
        if (btn_down != 0)
        {
            if ((btn_down & input::BUTTON_CANCEL) || (btn_down & input::BUTTON_CONFIRM))
            {
                clicked = true;
                if ((btn_down & input::BUTTON_CANCEL))
                    is_right_click = true;
            }
            break;
        }
    }
    if (clicked)
    {
        // Get the slot that the cursor is hovering over
        GuiSlot *slot = nullptr;
        uint8_t index = 0;
        for (GuiSlot *s : slots)
        {
            if (s->contains(cursor_x, cursor_y))
            {
                slot = s;
                break;
            }
            index++;
        }

        // If the slot is valid, interact with it
        if (slot)
        {
            inventory::ItemStack prev_item = slot->item;

            item_in_hand = slot->interact(item_in_hand, is_right_click);

            GuiResultSlot *result_slot = dynamic_cast<GuiResultSlot *>(slot);
            if (result_slot && result_slot->taken)
            {
                on_result_taken();
                result_slot->taken = false;
            }
            on_interact(index);

            // Copy the last 36 slots to the player's inventory
            inventory::PlayerInventory &inventory = owner->world->player.items;
            for (size_t i = 0; i < 36; i++)
            {
                inventory[inventory.size() - 1 - i] = slots[slots.size() - 1 - i]->item;
            }

            // Copy the GUI slots to the container after interaction
            if (linked_container)
            {
                for (size_t i = 0; i < linked_container->size() && i < slots.size(); i++)
                {
                    (*linked_container)[i] = slots[i]->item;
                }
            }

            if (owner->world->is_remote())
            {
                // Send the interaction to the server
                owner->world->client->sendWindowClick(window_id, index, is_right_click, this->transaction_id++, prev_item.id <= 0 ? -1 : prev_item.id, prev_item.count, prev_item.meta);
            }
        }
    }
}

bool GuiContainer::contains(int x, int y)
{
    gertex::GXView viewport = gertex::get_state().view;

    return x >= (viewport.width - width) / 2 && x < (viewport.width + width) / 2 && y >= (viewport.height - height) / 2 && y < (viewport.height + height) / 2;
}

void GuiGenericContainer::close()
{
    if (owner->world->is_remote())
    {
        // Notify the server about the closed GUI
        owner->world->client->sendWindowClose(window_id);

        return;
    }

    // Drop the item in hand
    if (!item_in_hand.empty())
    {
        EntityItem *item_entity = new EntityItem(owner->get_position(0), item_in_hand);
        item_entity->velocity = angles_to_vector(owner->rotation.x, owner->rotation.y) * 0.5;
        item_entity->chunk = owner->world->get_chunk_from_pos(Vec3i(std::floor(owner->position.x), std::floor(owner->position.y), std::floor(owner->position.z)));
        owner->world->add_entity(item_entity);
    }
}

void GuiGenericContainer::on_result_taken()
{
}

void GuiGenericContainer::set_slot(size_t slot, inventory::ItemStack item)
{
    if (slot >= slots.size())
        return;

    slots[slot]->item = item;

    if (linked_container && slot < linked_container->size())
    {
        (*linked_container)[slot] = item;
    }
}

inventory::ItemStack GuiGenericContainer::get_slot(size_t slot)
{
    if (slot >= slots.size())
        return inventory::ItemStack();

    return slots[slot]->item;
}

void GuiGenericContainer::refresh()
{

    /*
     * The gui must have at least as many slots as the inventory and the same
     * number of slots as the linked container, which generally should be the
     * case. If not, it's best to just not sync.
     */
    if ((linked_container && slots.size() != linked_container->size()) || slots.size() < 36)
        return;

    size_t i = 0;

    /*
     * The player slots are always last so we need to reverse the destination
     * order. Only the last 36 slots of the player inventory are present in
     * all container windows, so we only copy those. Conveniently, this means
     * we can just copy from the end of the inventory to the end of the slots
     * and then keep on copying the rest of the slots in reverse order.
     *
     * TL;DR: We copy from the end of the inventory to the end of the slots.
     */

    inventory::PlayerInventory &inventory = owner->world->player.items;

    for (; i < 36; i++)
    {
        slots[slots.size() - 1 - i]->item = inventory[inventory.size() - 1 - i];
    }

    // Copy the container slots as mentioned above
    if (linked_container)
    {
        for (; i < slots.size(); i++)
        {
            slots[slots.size() - 1 - i]->item = (*linked_container)[slots.size() - 1 - i];
        }
    }
}

GuiGenericContainer::~GuiGenericContainer()
{
    for (GuiSlot *slot : slots)
    {
        delete slot;
    }
}