#include "gui.hpp"
#include "blocks.hpp"
#include "font_tile_widths.hpp"
#include "ported/Random.hpp"
#include "ported/SystemTime.hpp"

gertex::GXMatrix gui_block_matrix;
gertex::GXMatrix gui_item_matrix;

Gui *Gui::current_gui = nullptr;

static GXColor text_colors[16] = {
    {0x00, 0x00, 0x00, 0xFF}, // Black
    {0x00, 0x00, 0xAA, 0xFF}, // Dark Blue
    {0x00, 0xAA, 0x00, 0xFF}, // Dark Green
    {0x00, 0xAA, 0xAA, 0xFF}, // Dark Aqua
    {0xAA, 0x00, 0x00, 0xFF}, // Dark Red
    {0xAA, 0x00, 0xAA, 0xFF}, // Dark Purple
    {0xFF, 0xAA, 0x00, 0xFF}, // Gold
    {0xAA, 0xAA, 0xAA, 0xFF}, // Gray
    {0x55, 0x55, 0x55, 0xFF}, // Dark Gray
    {0x55, 0x55, 0xFF, 0xFF}, // Blue
    {0x55, 0xFF, 0x55, 0xFF}, // Green
    {0x55, 0xFF, 0xFF, 0xFF}, // Aqua
    {0xFF, 0x55, 0x55, 0xFF}, // Red
    {0xFF, 0x55, 0xFF, 0xFF}, // Light Purple
    {0xFF, 0xFF, 0x55, 0xFF}, // Yellow
    {0xFF, 0xFF, 0xFF, 0xFF}  // White
};

GXColor Gui::get_text_color(char c)
{
    if (c >= '0' && c <= '9')
        return text_colors[c - '0'];
    if (c >= 'a' && c <= 'f')
        return text_colors[c - 'a' + 10];
    if (c >= 'A' && c <= 'F')
        return text_colors[c - 'A' + 10];
    return text_colors[15]; // Default to white
}

GXColor Gui::get_text_color_at(int index)
{
    if (index < 0 || index >= 16)
        return text_colors[15]; // Default to white
    return text_colors[index];
}

void Gui::init_matrices(float aspect_correction)
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

void Gui::fill_rect(int x, int y, int width, int height, GXColor color)
{
    draw_colored_quad(x, y, width, height, color.r, color.g, color.b, color.a);
}

void Gui::draw_rect(int x, int y, int width, int height, int border_size, GXColor color)
{
    fill_rect(x, y, width, border_size, color);                                                        // Top
    fill_rect(x, y + height - border_size, width, border_size, color);                                 // Bottom
    fill_rect(x, y + border_size, border_size, height - 2 * border_size, color);                       // Left
    fill_rect(x + width - border_size, y + border_size, border_size, height - 2 * border_size, color); // Right
}

int Gui::text_width(std::string str)
{
    int width = 0;
    int max_width = 0;
    for (size_t i = 0; i < str.size(); i++)
    {
        uint8_t c = str[i] & 0xFF;
        // This is poor handling of multibyte UTF-8 characters but it works for the characters we need
        if ((c == 0xC2 || c == 0xC3) && i + 1 < str.size())
            c = str[++i] & 0xFF;

        if (c == 0xA7 && i + 1 < str.size())
        {
            i++; // Skip the color code character
            continue;
        }
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

void Gui::draw_text(int x, int y, std::string str, GXColor color)
{
    // Enable direct colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

    gertex::use_matrix(gui_item_matrix);

    use_texture(font_texture);

    int x_offset = 0;
    int y_offset = 0;

    bool obfuscated = false;

    GXColor original_color = color;

    // Change the obfuscation seed every 10ms - this needs to be slow enough for shadows to display properly
    // but fast enough to look like it's constantly changing
    javaport::Random rng(javaport::System::currentTimeMillis() / 10);

    for (size_t i = 0; i < str.size(); i++)
    {
        uint8_t c = str[i] & 0xFF;

        // This is poor handling of multibyte UTF-8 characters but it works for the characters we need
        if ((c == 0xC2 || c == 0xC3) && i + 1 < str.size())
            c = str[++i] & 0xFF;

        if (c == 0xA7 && i + 1 < str.size())
        {
            i++; // Skip the color code character
            obfuscated = str[i] == 'k';
            color = original_color * get_text_color(str[i]);
            continue;
        }
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
        if (obfuscated)
        {
            uint8_t orig_width = font_tile_widths[c];

            // Find a random character with the same width
            uint8_t attempt = rng.nextInt(122) + 33;
            uint8_t total_attempts = 0;
            while (font_tile_widths[attempt] != orig_width)
            {
                attempt = (attempt + 1) & 0xFF;
                // Skip control characters and space
                if (attempt < 33)
                    attempt = 33;
                // Skip unsupported characters
                if (attempt >= 155 && attempt <= 159)
                    attempt = 160;
                // Wrap around to the start of the printable ASCII range
                if (attempt > 165)
                    attempt = 33;
                // Prevent infinite loop
                if (++total_attempts == 255)
                    break;
            }
            c = attempt;
        }

        uint16_t cx = uint16_t(c & 15) << 3;
        uint16_t cy = uint16_t(c >> 4) << 3;

        // Draw the character
        draw_colored_sprite(font_texture, Vec2i(x + x_offset, y + y_offset), Vec2i(16, 16), cx, cy, cx + 8, cy + 8, color);

        // Move the cursor to the next column
        x_offset += font_tile_widths[c] * 2;
    }
}

void Gui::draw_text_with_shadow(int x, int y, std::string str, GXColor color)
{
    GXColor shadow = color * 0.25;
    shadow.a = color.a;
    draw_text(x + 2, y + 2, str, shadow);
    draw_text(x, y, str, color);
}

void Gui::draw_item(int x, int y, inventory::ItemStack stack, gertex::GXView &viewport)
{
    if (stack.empty())
        return;

    // Enable backface culling for blocks
    GX_SetCullMode(GX_CULL_BACK);

    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    // Draw the item
    inventory::Item item = stack.as_item();
    Mtx tmp_matrix;

    if (item.is_block())
    {
        use_texture(terrain_texture);

        Block block = {uint8_t(item.id & 0xFF), 0x7F, uint8_t(stack.meta & 0xFF), 0xF, 0xF};

        RenderType render_type = properties(item.id).m_render_type;
        if (!properties(item.id).m_fluid && (properties(item.id).m_nonflat || render_type == RenderType::full || render_type == RenderType::full_special || render_type == RenderType::slab))
        {
            // Translate the block to the correct position
            guMtxCopy(gui_block_matrix.mtx, tmp_matrix);
            guMtxTransApply(tmp_matrix, tmp_matrix, x + 16, (y + 16) / viewport.aspect_correction, 0.0f);
            GX_LoadPosMtxImm(tmp_matrix, GX_PNMTX0);

            // Draw the block
            render_single_block(block, false);
            render_single_block(block, true);
        }
        else
        {
            guMtxCopy(gui_item_matrix.mtx, tmp_matrix);
            guMtxTransApply(tmp_matrix, tmp_matrix, x + 16, (y + 16) / viewport.aspect_correction, 0.0f);
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
        guMtxTransApply(tmp_matrix, tmp_matrix, x + 16, (y + 16) / viewport.aspect_correction, 0.0f);
        GX_LoadPosMtxImm(tmp_matrix, GX_PNMTX0);

        int texture_index = stack.get_texture_index();
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

void Gui::draw_container(int x, int y, inventory::Container &Container, gertex::GXView &viewport)
{
    for (size_t i = 0; i < Container.size(); i++)
    {
        draw_item(x + (i % 9) * 36, y + (i / 9) * 36, Container[i], viewport);
    }
}

inventory::ItemStack GuiSlot::interact(inventory::ItemStack hand, bool right_click)
{
    // If the slot is empty, place the item in the slot
    if (item.empty())
    {
        // Do nothing if both the hand and the slot are empty
        if (hand.empty())
            return inventory::ItemStack();

        if (right_click)
        {
            // Right click to place one item
            item = hand;
            item.count = 1;
            hand.count--;
            if (hand.empty())
                return inventory::ItemStack();
            return hand;
        }
        else
        {
            // Left click to place the entire stack
            item = hand;
            return inventory::ItemStack();
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
        return inventory::ItemStack();
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
    inventory::ItemStack temp = item;
    item = hand;
    return temp;
}

inventory::ItemStack GuiResultSlot::interact(inventory::ItemStack hand, bool right_click)
{
    if (hand.empty())
    {
        // Take the item from the slot
        hand = item;
        taken = true;
    }
    else if (hand.id == item.id && hand.meta == item.meta && hand.count + item.count <= hand.as_item().max_stack)
    {
        // The item fits in the hand so we'll take it
        hand.count += item.count;
        taken = true;
    }

    return hand;
}
