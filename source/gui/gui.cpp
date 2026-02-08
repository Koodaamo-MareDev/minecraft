#include "gui.hpp"
#include <block/blocks.hpp>
#include <font_tile_widths.hpp>
#include <ported/Random.hpp>
#include <ported/SystemTime.hpp>
#include <util/string_utils.hpp>
#include <util/input/input.hpp>
#include <limits>
#include <sounds.hpp>
#include <sound.hpp>

gertex::GXMatrix gui_block_matrix;
gertex::GXMatrix gui_item_matrix;

Gui *Gui::current_gui = nullptr;

Gui::~Gui()
{
    for (GuiButton *button : buttons)
    {
        if (button)
            delete button;
    }
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

int Gui::text_width(std::string text)
{
    text = str::utf8_to_cp437(text);

    int width = 0;
    int max_width = 0;
    for (size_t i = 0; i < text.size(); i++)
    {
        uint8_t c = static_cast<uint8_t>(text[i]);

        if (c == U'§' && i + 1 < text.size())
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

void Gui::draw_text(int x, int y, std::string text, GXColor color)
{
    text = str::utf8_to_cp437(text);

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

    for (size_t i = 0; i < text.size(); i++)
    {
        uint8_t c = static_cast<uint8_t>(text[i]);

        if (c == U'§' && i + 1 < text.size())
        {
            i++; // Skip the color code character
            if (text[i] == 'k')
            {
                obfuscated = true;
            }
            else
            {
                obfuscated = false;
                color = original_color * get_text_color(text[i]);
            }
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
            c = obfuscate_char(rng, c);
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

void Gui::draw_item(int x, int y, item::ItemStack stack, gertex::GXView &viewport)
{
    if (stack.empty())
        return;

    // Enable backface culling for blocks
    GX_SetCullMode(GX_CULL_BACK);

    // Enable indexed colors
    GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);

    // Draw the item
    item::Item item = stack.as_item();
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
        draw_text_with_shadow(x + 34 - count_width, y + 17, count_str);
    }
}

void Gui::draw_container(int x, int y, inventory::Container &Container, gertex::GXView &viewport)
{
    for (size_t i = 0; i < Container.size(); i++)
    {
        draw_item(x + (i % 9) * 36, y + (i / 9) * 36, Container[i], viewport);
    }
}

void Gui::update_buttons()
{
    bool pointer_visible = false;
    bool confirm = false;

    for (input::Device *dev : input::devices)
    {
        if (dev->connected())
        {
            if ((dev->get_buttons_down() & input::BUTTON_CONFIRM))
            {
                confirm = true;
            }
            if (dev->is_pointer_visible())
            {
                pointer_visible = true;
                break;
            }

            Vec3f left_stick = dev->get_left_stick();

            bool left = left_stick.x < -0.5f;
            bool right = left_stick.x > 0.5f;
            bool up = left_stick.y > 0.5f;
            bool down = left_stick.y < -0.5f;
            bool prev_joystick_pressed = joystick_pressed;
            joystick_pressed = left || right || up || down;

            if (joystick_pressed && !prev_joystick_pressed)
            {
                joystick_timer = 0;
                navigate(left, right, up, down);
            }
            else if (joystick_pressed)
            {
                joystick_timer++;
                if (joystick_timer > 20 && joystick_timer % 10 == 0)
                {
                    navigate(left, right, up, down);
                }
            }
            break;
        }
    }

    if (pointer_visible)
    {
        // Handle pointer input
        for (size_t i = 0; i < buttons.size(); i++)
        {
            if (buttons[i]->contains(cursor_x, cursor_y) && buttons[i]->enabled)
            {
                selected_button = i;
                break;
            }
        }
    }

    // Handle button press
    if (confirm && buttons[selected_button]->on_click && buttons[selected_button]->enabled)
    {
        if (sound_system)
        {
            Sound click_sound = get_sound("random/click");
            click_sound.position = sound_system->head_position;
            sound_system->play_sound(click_sound);
        }
        buttons[selected_button]->on_click();
    }
}

void Gui::draw_buttons()
{
    for (size_t i = 0; i < buttons.size(); i++)
    {
        buttons[i]->draw(i == selected_button);
    }
}

void Gui::navigate(bool left, bool right, bool up, bool down)
{
    GuiButton *prev_selected = buttons[selected_button];

    const std::function<bool(GuiButton *, GuiButton *)> conditions[4] = {
        [](GuiButton *from, GuiButton *to)
        { return from->x > to->x; },
        [](GuiButton *from, GuiButton *to)
        { return from->x < to->x; },
        [](GuiButton *from, GuiButton *to)
        { return from->y > to->y; },
        [](GuiButton *from, GuiButton *to)
        { return from->y < to->y; }};

    std::function<bool(GuiButton *, GuiButton *)> condition = nullptr;

    bool vertical = false;

    if (left)
        condition = conditions[0];
    else if (right)
        condition = conditions[1];
    else if (up)
    {
        vertical = true;
        condition = conditions[2];
    }
    else if (down)
    {
        vertical = true;
        condition = conditions[3];
    }
    else
        return;
    int dist = std::numeric_limits<int>::max();
    std::vector<uint32_t> candidates;
    for (size_t i = 0; i < buttons.size(); i++)
    {
        // Only get buttons on the requested side.
        if (i != selected_button && buttons[i]->enabled && condition(prev_selected, buttons[i]))
        {
            // Sort by distance on the primary axis
            int d = vertical ? std::abs(buttons[i]->cx() - prev_selected->cx()) : std::abs(buttons[i]->cy() - prev_selected->cy());

            // Secondary axis distance will be used to prevent steep jumps
            int sec_d = vertical ? std::abs(buttons[i]->cy() - prev_selected->cy()) : std::abs(buttons[i]->cx() - prev_selected->cx());
            if (d > sec_d)
                continue;

            if (d < dist)
            {
                dist = d;
                candidates.clear();
                candidates.push_back(i);
            }
            else if (d == dist)
            {
                candidates.push_back(i);
            }
        }
    }
    dist = std::numeric_limits<int>::max();
    for (uint32_t i : candidates)
    {
        // Sort by distance on secondary axis
        int d = vertical ? std::abs(buttons[i]->cy() - prev_selected->cy()) : std::abs(buttons[i]->cx() - prev_selected->cx());
        if (d < dist)
        {
            dist = d;
            selected_button = i;
        }
    }
}

item::ItemStack GuiSlot::interact(item::ItemStack hand, bool right_click)
{
    // If the slot is empty, place the item in the slot
    if (item.empty())
    {
        // Do nothing if both the hand and the slot are empty
        if (hand.empty())
            return item::ItemStack();

        if (right_click)
        {
            // Right click to place one item
            item = hand;
            item.count = 1;
            hand.count--;
            if (hand.empty())
                return item::ItemStack();
            return hand;
        }
        else
        {
            // Left click to place the entire stack
            item = hand;
            return item::ItemStack();
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
        return item::ItemStack();
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
    item::ItemStack temp = item;
    item = hand;
    return temp;
}

item::ItemStack GuiResultSlot::interact(item::ItemStack hand, bool right_click)
{
    // No-op if the slot is empty
    if (item.empty())
        return hand;

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

void GuiButton::draw(bool selected)
{
    GXColor text_color = {0xA0, 0xA0, 0xA0, 0xFF};
    int v_offset = 46;
    if (enabled)
    {
        v_offset += 20;
        text_color = {0xFF, 0xFF, 0xFF, 0xFF};
        if (selected)
        {
            v_offset += 20;
            text_color = {0xFF, 0xFF, 0xA0, 0xFF};
        }
    }
    int draw_width = std::min(width, 400) - 40;

    // Draw the left edge
    draw_textured_quad(gui_texture, x, y, draw_width / 2, height, 0, v_offset, draw_width / 4, v_offset + 20);

    // Keep drawing the right edge until the button is fully drawn
    for (int i = draw_width / 2; i < width; i += 200)
    {
        int section_width = std::min(200, width - i);
        draw_textured_quad(gui_texture, x + i, y, section_width, height, 180 - (section_width / 2), v_offset, 180, v_offset + 20);
    }
    draw_textured_quad(gui_texture, x + width - 40, y, 40, height, 180, v_offset, 200, v_offset + 20);

    Gui::draw_text_with_shadow(x + (width - Gui::text_width(text)) / 2, y + (height - 16) / 2, text, text_color);
}

void GuiButtonWithValue::draw(bool selected)
{
    GXColor text_color = {0xA0, 0xA0, 0xA0, 0xFF};
    int v_offset = 46;
    if (enabled)
    {
        v_offset += 20;
        text_color = {0xFF, 0xFF, 0xFF, 0xFF};
        if (selected)
        {
            v_offset += 20;
            text_color = {0xFF, 0xFF, 0xA0, 0xFF};
        }
    }
    int draw_width = std::min(width, 400) - 40;

    // Draw the left edge
    draw_textured_quad(gui_texture, x, y, draw_width / 2, height, 0, v_offset, draw_width / 4, v_offset + 20);

    // Keep drawing the right edge until the button is fully drawn
    for (int i = draw_width / 2; i < width; i += 200)
    {
        int section_width = std::min(200, width - i);
        draw_textured_quad(gui_texture, x + i, y, section_width, height, 180 - (section_width / 2), v_offset, 180, v_offset + 20);
    }
    draw_textured_quad(gui_texture, x + width - 40, y, 40, height, 180, v_offset, 200, v_offset + 20);

    std::string full_text = text + ": " + value;
    Gui::draw_text_with_shadow(x + (width - Gui::text_width(full_text)) / 2, y + (height - 16) / 2, full_text, text_color);
}
