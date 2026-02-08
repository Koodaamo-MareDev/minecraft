#include "gui_smelting.hpp"

#include <world/entity.hpp>
#include <world/world.hpp>
#include <util/input/input.hpp>
#include <crafting/recipe_manager.hpp>
#include <world/tile_entity/tile_entity_furnace.hpp>
GuiSmelting::GuiSmelting(EntityPhysical *owner, inventory::Container *container, uint8_t window_id, TileEntityFurnace *furnace) : GuiGenericContainer(owner, container, window_id, "Crafting"), furnace(furnace)
{
    gertex::GXView viewport = gertex::get_state().view;

    int start_x = (viewport.width - width) / 2;
    int start_y = (viewport.aspect_correction * viewport.height - height) / 2;

    // Smelting
    for (size_t i = 0; i < 2; i++)
    {
        slots.push_back(new GuiSlot(start_x + 112, start_y + 34 + (i * 72), item::ItemStack()));
    }
    slots.push_back(new GuiResultSlot(start_x + 232, start_y + 70, item::ItemStack()));

    // Inventory
    for (size_t i = 0; i < 27; i++)
    {
        slots.push_back(new GuiSlot(start_x + 16 + (i % 9) * 36, start_y + 168 + (i / 9) * 36, item::ItemStack()));
    }

    // Hotbar
    for (size_t i = 0; i < 9; i++)
    {
        slots.push_back(new GuiSlot(start_x + 16 + i * 36, start_y + 284, item::ItemStack()));
    }

    refresh();
}

void GuiSmelting::update()
{
    GuiGenericContainer::update();

    if (furnace)
    {
        burn_time_percent = 100 * furnace->burn_time / furnace->item_burn_time;
        cook_time_percent = 100 * furnace->cook_time / 200;
    }
}

void GuiSmelting::draw()
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
    draw_textured_quad(furnace_texture, start_x, start_y, 352, 332, 0, 0, 176, 166);

    // Draw arrow
    int arrow_progress = cook_time_percent * 24 / 100;
    draw_textured_quad(furnace_texture, start_x + 158, start_y + 68, arrow_progress * 2, 34, 176, 14, 176 + arrow_progress, 31);

    // Draw flame
    int flame_progress = std::ceil(burn_time_percent * 14 / 100.0f);
    draw_textured_quad(furnace_texture, start_x + 112, start_y + 100 - (flame_progress * 2), 28, flame_progress * 2, 176, 14 - flame_progress, 190, 14);

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

bool GuiSmelting::contains(int x, int y)
{
    gertex::GXView viewport = gertex::get_state().view;

    return x >= (viewport.width - width) / 2 && x < (viewport.width + width) / 2 && y >= (viewport.height - height) / 2 && y < (viewport.height + height) / 2;
}

void GuiSmelting::on_result_taken()
{
    set_slot(2, item::ItemStack());
}
