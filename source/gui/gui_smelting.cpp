#include "gui_smelting.hpp"

#include <world/entity/entity.hpp>
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
    gertex::set_color_format(0, GX_DIRECT);

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

    draw_slots();
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
