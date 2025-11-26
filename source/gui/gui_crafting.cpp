#include "gui_crafting.hpp"

#include <world/entity.hpp>
#include <world/world.hpp>
#include <util/input/input.hpp>
#include <crafting/recipe_manager.hpp>

GuiCrafting::GuiCrafting(EntityPhysical *owner, inventory::Container *container, uint8_t window_id) : GuiGenericContainer(owner, container, window_id, "Crafting")
{
    gertex::GXView viewport = gertex::get_state().view;

    int start_x = (viewport.width - width) / 2;
    int start_y = (viewport.aspect_correction * viewport.height - height) / 2;

    // Crafting
    slots.push_back(new GuiResultSlot(start_x + 248, start_y + 70, inventory::ItemStack()));
    for (size_t i = 0; i < 9; i++)
    {
        slots.push_back(new GuiSlot(start_x + 60 + (i % 3) * 36, start_y + 34 + (i / 3) * 36, inventory::ItemStack()));
    }

    // Inventory
    for (size_t i = 0; i < 27; i++)
    {
        slots.push_back(new GuiSlot(start_x + 16 + (i % 9) * 36, start_y + 168 + (i / 9) * 36, inventory::ItemStack()));
    }

    // Hotbar
    for (size_t i = 0; i < 9; i++)
    {
        slots.push_back(new GuiSlot(start_x + 16 + i * 36, start_y + 284, inventory::ItemStack()));
    }

    refresh();
}

void GuiCrafting::draw()
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
    draw_textured_quad(crafting_texture, start_x, start_y, 512, 512, 0, 0, 256, 256);

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

bool GuiCrafting::contains(int x, int y)
{
    gertex::GXView viewport = gertex::get_state().view;

    return x >= (viewport.width - width) / 2 && x < (viewport.width + width) / 2 && y >= (viewport.height - height) / 2 && y < (viewport.height + height) / 2;
}

void GuiCrafting::on_result_taken()
{
    for (size_t i = 1; i <= 9; i++)
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
            slots[i]->item = inventory::ItemStack();
        else
            slots[i]->item = ingredient;
    }
}

void GuiCrafting::on_interact(size_t slot)
{
    if (slot < 10)
    {
        // Build the recipe input
        std::vector<inventory::ItemStack> inputs;
        for (size_t i = 1; i <= 9; i++)
            inputs.push_back(slots[i]->item);

        crafting::Input current_input(3, 3, inputs);

        // Get result
        inventory::ItemStack result = crafting::RecipeManager::instance().craft(current_input);
        set_slot(0, result);
    }
}

void GuiCrafting::close()
{
    GuiGenericContainer::close();

    if (owner->world->is_remote())
        return;

    for (size_t i = 1; i <= 9; i++)
    {
        inventory::ItemStack stack = slots[i]->item;
        if (!stack.empty())
        {
            EntityItem *item_entity = new EntityItem(owner->get_position(0), stack);
            item_entity->velocity = angles_to_vector(owner->rotation.x, owner->rotation.y) * 0.5;
            item_entity->chunk = owner->world->get_chunk_from_pos(Vec3i(std::floor(owner->position.x), std::floor(owner->position.y), std::floor(owner->position.z)));
            owner->world->add_entity(item_entity);
        }
    }
}
