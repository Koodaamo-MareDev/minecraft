#include "gui_survival.hpp"
#include "entity.hpp"
#include "world.hpp"
#include "util/input/input.hpp"

GuiSurvival::GuiSurvival(const gertex::GXView &viewport, inventory::Container &Container) : Gui(viewport), linked_container(Container)
{
    int start_x = (viewport.width - width) / 2;
    int start_y = (viewport.aspect_correction * viewport.height - height) / 2;

    // Crafting
    slots.push_back(new GuiResultSlot(start_x + 288, start_y + 72, inventory::ItemStack()));
    size_t slot_index = 1;
    for (size_t j = 0; slot_index < armor_start; slot_index++, j++)
    {
        slots.push_back(new GuiSlot(start_x + 176 + (j % 2) * 36, start_y + 52 + (j / 2) * 36, inventory::ItemStack()));
    }

    // Armor
    for (size_t i = 0; slot_index < inventory_start; slot_index++, i++)
    {
        slots.push_back(new GuiSlot(start_x + 16, start_y + 16 + i * 36, inventory::ItemStack()));
    }

    // Inventory
    for (size_t i = 0; slot_index < hotbar_start; slot_index++, i++)
    {
        slots.push_back(new GuiSlot(start_x + 16 + (i % 9) * 36, start_y + 168 + (i / 9) * 36, inventory::ItemStack()));
    }

    // Hotbar
    for (size_t i = 0; slot_index < linked_container.size(); slot_index++, i++)
    {
        slots.push_back(new GuiSlot(start_x + 16 + i * 36, start_y + 284, inventory::ItemStack()));
    }

    refresh();
}

void GuiSurvival::draw()
{
    draw_colored_quad(0, 0, viewport.width, viewport.height, 0, 0, 0, 128);

    int start_x = (viewport.width - width) / 2;
    int start_y = (viewport.aspect_correction * viewport.height - height) / 2;

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Disable backface culling for the background
    GX_SetCullMode(GX_CULL_NONE);

    // Draw the background
    draw_textured_quad(inventory_texture, start_x, start_y, 512, 512, 0, 0, 256, 256);

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

void GuiSurvival::update()
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
        for (GuiSlot *s : slots)
        {
            if (s->contains(cursor_x, cursor_y))
            {
                slot = s;
                break;
            }
        }

        // If the slot is valid, interact with it
        if (slot)
        {
            item_in_hand = slot->interact(item_in_hand, is_right_click);

            // Copy the GUI slots to the Container after interaction
            for (size_t i = 0; i < linked_container.size() && i < slots.size(); i++)
            {
                linked_container[i] = slots[i]->item;
            }
        }
    }
}

void GuiSurvival::close()
{
    // Copy the GUI slots to the Container
    for (size_t i = 0; i < linked_container.size() && i < slots.size(); i++)
    {
        linked_container[i] = slots[i]->item;
    }

    if (current_world->is_remote())
        return;

    // Drop the item in hand
    if (!item_in_hand.empty())
    {
        EntityPhysical &player = current_world->player;
        EntityItem *item_entity = new EntityItem(player.get_position(0), item_in_hand);
        item_entity->velocity = angles_to_vector(player.rotation.x, player.rotation.y) * 0.5;
        item_entity->chunk = get_chunk_from_pos(Vec3i(std::floor(player.position.x), std::floor(player.position.y), std::floor(player.position.z)));
        add_entity(item_entity);
    }
}

void GuiSurvival::refresh()
{
    // Copy the Container to the GUI slots
    for (size_t i = 0; i < linked_container.size() && i < slots.size(); i++)
    {
        slots[i]->item = linked_container[i];
    }
}

bool GuiSurvival::contains(int x, int y)
{
    return x >= (viewport.width - width) / 2 && x < (viewport.width + width) / 2 && y >= (viewport.height - height) / 2 && y < (viewport.height + height) / 2;
}