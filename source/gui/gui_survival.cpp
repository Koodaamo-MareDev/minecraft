#include "gui_survival.hpp"

#include <world/entity.hpp>
#include <world/world.hpp>
#include <util/input/input.hpp>

GuiSurvival::GuiSurvival(EntityPhysical *owner, inventory::Container *container) : GuiGenericContainer(owner, container, 45, 0, "Inventory")
{
    gertex::GXView viewport = gertex::get_state().view;

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
    for (size_t i = 0; slot_index < linked_container->size(); slot_index++, i++)
    {
        slots.push_back(new GuiSlot(start_x + 16 + i * 36, start_y + 284, inventory::ItemStack()));
    }

    refresh();
}

void GuiSurvival::draw()
{
    gertex::GXView viewport = gertex::get_state().view;

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

bool GuiSurvival::contains(int x, int y)
{
    gertex::GXView viewport = gertex::get_state().view;

    return x >= (viewport.width - width) / 2 && x < (viewport.width + width) / 2 && y >= (viewport.height - height) / 2 && y < (viewport.height + height) / 2;
}

void GuiSurvival::on_result_taken()
{
    for (size_t i = 1; i <= 4; i++)
    {
        inventory::ItemStack ingredient = slots[i]->item;
        if (ingredient.empty())
            continue;

        // Handle milk bucket conversion or reduce the item count
        if (ingredient.id == 335)
            ingredient.id = 325;
        else
            ingredient.count--;

        // Clear the slot if the item count is zero
        if (ingredient.empty())
            slots[0]->item = inventory::ItemStack();

        slots[i]->item = ingredient;
    }
}
