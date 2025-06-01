#include "gui.hpp"
#include "blocks.hpp"
#include "font_tile_widths.hpp"

gertex::GXMatrix gui_block_matrix;
gertex::GXMatrix gui_item_matrix;

gui *gui::current_gui = nullptr;

void gui::init_matrices(float aspect_correction)
{
    // Prepare matrices for rendering GUI elements
    Mtx tmp_matrix;
    guMtxIdentity(tmp_matrix);
    guMtxIdentity(gui_block_matrix.mtx);
    guMtxIdentity(gui_item_matrix.mtx);

    guMtxRotDeg(tmp_matrix, 'x', 202.5F);
    guMtxConcat(gui_block_matrix.mtx, tmp_matrix, gui_block_matrix.mtx);

    guMtxRotDeg(tmp_matrix, 'y', 45.0F);
    guMtxConcat(gui_block_matrix.mtx, tmp_matrix, gui_block_matrix.mtx);

    guMtxScaleApply(gui_block_matrix.mtx, gui_block_matrix.mtx, 0.65f, 0.65f / aspect_correction, 0.65f);
    guMtxTransApply(gui_block_matrix.mtx, gui_block_matrix.mtx, 0.0f, 0.0f, -10.0f);

    guMtxScaleApply(gui_item_matrix.mtx, gui_item_matrix.mtx, 1.0f, 1.0f / aspect_correction, 1.0f);
    guMtxTransApply(gui_item_matrix.mtx, gui_item_matrix.mtx, 0.0f, 0.0f, -10.0f);
}

int gui::text_width(std::string str)
{
    int width = 0;
    int max_width = 0;
    for (size_t i = 0; i < str.size(); i++)
    {
        uint8_t c = str[i] & 0xFF;
        if (c == '\n')
        {
            max_width = std::max(max_width, width);
            width = 0;
            continue;
        }
        if (c == ' ')
        {
            width += 6;
            continue;
        }
        width += font_tile_widths[c] * 2;
    }
    return std::max(max_width, width);
}

void gui::draw_text(int x, int y, std::string str, GXColor color)
{
    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    gertex::use_matrix(gui_item_matrix);

    use_texture(font_texture);

    int x_offset = 0;
    int y_offset = 0;
    for (size_t i = 0; i < str.size(); i++)
    {
        uint8_t c = str[i] & 0xFF;
        if (c == ' ')
        {
            x_offset += 6;
            continue;
        }
        if (c == '\n')
        {
            x_offset = 0;
            y_offset += 16;
            continue;
        }

        uint16_t cx = uint16_t(c & 15) << 3;
        uint16_t cy = uint16_t(c >> 4) << 3;

        // Draw the character
        draw_colored_sprite(font_texture, vec2i(x + x_offset, y + y_offset), vec2i(16, 16), cx, cy, cx + 8, cy + 8, color);

        // Move the cursor to the next column
        x_offset += font_tile_widths[c] * 2;
    }
}

void gui::draw_text_with_shadow(int x, int y, std::string str, GXColor color)
{
    GXColor shadow = color * 0.25;
    shadow.a = color.a;
    draw_text(x + 2, y + 2, str, shadow);
    draw_text(x, y, str, color);
}

void gui::draw_item(int x, int y, inventory::item_stack stack)
{
    if (stack.empty())
        return;

    // Enable backface culling for blocks
    GX_SetCullMode(GX_CULL_BACK);

    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    // Draw the item
    inventory::item item = stack.as_item();
    Mtx tmp_matrix;

    if (item.is_block())
    {
        use_texture(terrain_texture);

        block_t block = {uint8_t(item.id & 0xFF), 0x7F, uint8_t(stack.meta & 0xFF), 0xF, 0xF};

        RenderType render_type = properties(item.id).m_render_type;
        if (!properties(item.id).m_fluid && (render_type == RenderType::full || render_type == RenderType::full_special || render_type == RenderType::slab))
        {
            // Translate the block to the correct position
            guMtxCopy(gui_block_matrix.mtx, tmp_matrix);
            guMtxTransApply(tmp_matrix, tmp_matrix, x + 16, y + 16, 0.0f);
            GX_LoadPosMtxImm(tmp_matrix, GX_PNMTX0);

            // Draw the block
            render_single_block(block, false);
            render_single_block(block, true);
        }
        else
        {
            guMtxCopy(gui_item_matrix.mtx, tmp_matrix);
            guMtxTransApply(tmp_matrix, tmp_matrix, x + 16, y + 16, 0.0f);
            GX_LoadPosMtxImm(tmp_matrix, GX_PNMTX0);

            int texture_index = get_default_texture_index(BlockID(block.id));
            render_single_item(texture_index, false);
            render_single_item(texture_index, true);
        }
    }
    else
    {
        use_texture(items_texture);
        guMtxCopy(gui_item_matrix.mtx, tmp_matrix);
        guMtxTransApply(tmp_matrix, tmp_matrix, x + 16, y + 16, 0.0f);
        GX_LoadPosMtxImm(tmp_matrix, GX_PNMTX0);

        int texture_index = item.texture_index;
        render_single_item(texture_index, false);
        render_single_item(texture_index, true);
    }

    // Draw the item count if it's greater than 1
    if (stack.count > 1)
    {
        std::string count_str = std::to_string(stack.count);
        int count_width = text_width(count_str);
        draw_text_with_shadow(x + 34 - count_width, y + 16, count_str);
    }
}

void gui::draw_container(int x, int y, inventory::container &container)
{
    for (size_t i = 0; i < container.size(); i++)
    {
        draw_item(x + (i % 9) * 36, y + (i / 9) * 36, container[i]);
    }
}

inventory::item_stack gui_slot::interact(inventory::item_stack hand, bool right_click)
{
    // If the slot is empty, place the item in the slot
    if (item.empty())
    {
        // Do nothing if both the hand and the slot are empty
        if (hand.empty())
            return inventory::item_stack();

        if (right_click)
        {
            // Right click to place one item
            item = hand;
            item.count = 1;
            hand.count--;
            if (hand.empty())
                return inventory::item_stack();
            return hand;
        }
        else
        {
            // Left click to place the entire stack
            item = hand;
            return inventory::item_stack();
        }
    }

    // If the stack is the same, add the stack
    if (item.id == hand.id && item.meta == hand.meta)
    {
        if (right_click)
        {
            // Right click to place one item
            item.count++;
            hand.count--;
            if (item.count > item.as_item().max_stack)
            {
                // Reverse the changes if the stack is over the max stack size
                item.count--;
                hand.count++;
            }
            return hand;
        }
        else
        {

            item.count += hand.count;

            // If the stack is over the max stack size, split the stack
            if (item.count > item.as_item().max_stack)
            {
                hand.count = item.count - item.as_item().max_stack;
                item.count = item.as_item().max_stack;

                // Return the remaining stack
                return hand;
            }
        }

        // Return an empty stack
        return inventory::item_stack();
    }

    // Take half of the stack if right clicking with an empty hand
    if (right_click && hand.empty())
    {
        hand = item;
        item.count = hand.count / 2;
        hand.count -= item.count;
        return hand;
    }

    // Swap the stacks - in this case it doesn't matter if it's a right or left click
    inventory::item_stack temp = item;
    item = hand;
    return temp;
}