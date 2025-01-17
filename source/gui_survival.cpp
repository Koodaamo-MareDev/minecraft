#include "gui_survival.hpp"
#include "entity.hpp"
extern aabb_entity_t *player;
gui_survival::gui_survival(const gertex::GXView &viewport, inventory::container &container) : gui(viewport), linked_container(container)
{
    init_matrices();

    int start_x = (viewport.width - width) / 2;
    int start_y = (viewport.height - height) / 2;

    // Initialize the hotbar
    for (size_t i = 0; i < hotbar_slots.size(); i++)
    {
        hotbar_slots[i] = gui_slot(start_x + 16 + i * 36, start_y + 284, inventory::item_stack());
    }

    // Initialize the inventory
    for (size_t i = 0; i < inventory_slots.size(); i++)
    {
        inventory_slots[i] = gui_slot(start_x + 16 + (i % 9) * 36, start_y + 168 + (i / 9) * 36, inventory::item_stack());
    }

    // Initialize the armor
    for (size_t i = 0; i < armor_slots.size(); i++)
    {
        armor_slots[i] = gui_slot(start_x + 16, start_y + 16 + i * 36, inventory::item_stack());
    }
    refresh();
}

void gui_survival::draw()
{
    draw_colored_quad(0, 0, viewport.width, viewport.height, 0, 0, 0, 128);

    int start_x = (viewport.width - width) / 2;
    int start_y = (viewport.height - height) / 2;

    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    // Disable backface culling for the background
    GX_SetCullMode(GX_CULL_NONE);

    // Draw the background
    draw_textured_quad(inventory_texture, start_x, start_y, 512, 512, 0, 0, 256, 256);

    // Enable backface culling for the items
    GX_SetCullMode(GX_CULL_BACK);

    // Draw the hotbar
    for (gui_slot &slot : hotbar_slots)
    {
        draw_item(slot.x, slot.y, slot.item);
    }

    // Draw the inventory
    for (gui_slot &slot : inventory_slots)
    {
        draw_item(slot.x, slot.y, slot.item);
    }

    // Draw the armor
    for (gui_slot &slot : armor_slots)
    {
        draw_item(slot.x, slot.y, slot.item);
    }

    // Disable backface culling for the rest of the GUI
    GX_SetCullMode(GX_CULL_NONE);

    // Get the highlighted slot
    gui_slot *highlighted_slot = nullptr;
    for (gui_slot &slot : hotbar_slots)
    {
        if (slot.contains(cursor_x, cursor_y))
        {
            highlighted_slot = &slot;
            break;
        }
    }
    if (!highlighted_slot)
    {
        for (gui_slot &slot : inventory_slots)
        {
            if (slot.contains(cursor_x, cursor_y))
            {
                highlighted_slot = &slot;
                break;
            }
        }
    }
    if (!highlighted_slot)
    {
        for (gui_slot &slot : armor_slots)
        {
            if (slot.contains(cursor_x, cursor_y))
            {
                highlighted_slot = &slot;
                break;
            }
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
        draw_item(cursor_x - 16, cursor_y - 16, item_in_hand);
    }
}

void gui_survival::update()
{
    if ((raw_wiimote_down & (WPAD_CLASSIC_BUTTON_Y | WPAD_CLASSIC_BUTTON_B)) != 0)
    {
        bool right_click = raw_wiimote_down & WPAD_CLASSIC_BUTTON_Y;

        // Get the slot that the cursor is hovering over
        gui_slot *slot = nullptr;
        for (gui_slot &s : hotbar_slots)
        {
            if (s.contains(cursor_x, cursor_y))
            {
                slot = &s;
                break;
            }
        }
        if (!slot)
        {
            for (gui_slot &s : inventory_slots)
            {
                if (s.contains(cursor_x, cursor_y))
                {
                    slot = &s;
                    break;
                }
            }
        }
        if (!slot)
        {
            for (gui_slot &s : armor_slots)
            {
                if (s.contains(cursor_x, cursor_y))
                {
                    slot = &s;
                    break;
                }
            }
        }

        // If the slot is valid, interact with it
        if (slot)
        {
            item_in_hand = slot->interact(item_in_hand, right_click);

            // Copy the GUI slots to the container after interaction
            for (size_t i = 0; i < linked_container.size(); i++)
            {
                if (i < hotbar_slots.size())
                    linked_container[i] = hotbar_slots[i].item;
                else if (i < hotbar_slots.size() + inventory_slots.size())
                    linked_container[i] = inventory_slots[i - hotbar_slots.size()].item;
                else if (i < hotbar_slots.size() + inventory_slots.size() + armor_slots.size())
                    linked_container[i] = armor_slots[i - hotbar_slots.size() - inventory_slots.size()].item;
            }
        }
    }
}

void gui_survival::close()
{
    // Copy the GUI slots to the container
    for (size_t i = 0; i < linked_container.size(); i++)
    {
        if (i < hotbar_slots.size())
            linked_container[i] = hotbar_slots[i].item;
        else if (i < hotbar_slots.size() + inventory_slots.size())
            linked_container[i] = inventory_slots[i - hotbar_slots.size()].item;
        else if (i < hotbar_slots.size() + inventory_slots.size() + armor_slots.size())
            linked_container[i] = armor_slots[i - hotbar_slots.size() - inventory_slots.size()].item;
    }

    // Drop the item in hand
    if (!item_in_hand.empty())
    {
        if (!player)
            return;
        item_entity_t *item_entity = new item_entity_t(player->get_position(0), item_in_hand);
        item_entity->velocity = angles_to_vector(player->rotation.x, player->rotation.y) * 0.5;
        item_entity->chunk = get_chunk_from_pos(vec3i(std::floor(player->position.x), std::floor(player->position.y), std::floor(player->position.z)));

        // Probably unnecessary but better safe than sorry
        if (item_entity->chunk)
            item_entity->chunk->entities.push_back(item_entity);
        else
            delete item_entity;
    }
}

void gui_survival::refresh()
{
    // Copy the container to the GUI slots
    for (size_t i = 0; i < linked_container.size(); i++)
    {
        if (i < hotbar_slots.size())
            hotbar_slots[i].item = linked_container[i];
        else if (i < hotbar_slots.size() + inventory_slots.size())
            inventory_slots[i - hotbar_slots.size()].item = linked_container[i];
        else if (i < hotbar_slots.size() + inventory_slots.size() + armor_slots.size())
            armor_slots[i - hotbar_slots.size() - inventory_slots.size()].item = linked_container[i];
    }
}

bool gui_survival::contains(int x, int y)
{
    return x >= (viewport.width - width) / 2 && x < (viewport.width + width) / 2 && y >= (viewport.height - height) / 2 && y < (viewport.height + height) / 2;
}