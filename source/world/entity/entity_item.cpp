#include "entity_item.hpp"
#include <world/world.hpp>
#include <world/entity/entity_player_local.hpp>
#include <block/blocks.hpp>
#include <gui/gui.hpp>
#include <render/render.hpp>

constexpr static int item_pickup_ticks = 2;
constexpr static int item_lifetime = 6000;

EntityItem::EntityItem(const Vec3f &position, const item::ItemStack &item_stack) : EntityPhysical()
{
    this->walk_sound = false;
    this->gravity = 0.04;
    this->y_offset = 0.125;
    this->item_stack = item_stack;
    this->pickup_pos = position;
    this->type = 1;
    this->simulate_offline = true;
    this->width = 0.25;
    this->height = 0.25;
    teleport(position);
}

void EntityItem::tick()
{
    EntityPhysical::tick();

    if (world->is_remote())
        return;

    if (ticks_existed >= 6000)
        dead = true;
}

void EntityItem::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;

    Vec3f entity_position = get_position(partial_ticks);

    // Adjust position for pickup animation
    if (picked_up)
    {
        // Lerp the position towards the target in <item_pickup_ticks> ticks
        Vec3f target = pickup_pos;
        vfloat_t factor = (ticks_existed + partial_ticks - (item_lifetime - item_pickup_ticks)) / vfloat_t(item_pickup_ticks); // The lifetime is adjusted to line up with the pickup animation
        if (factor > 1)
            factor = 1;
        entity_position = Vec3f::lerp(entity_position, target, factor);
    }

    Vec3i block_pos = entity_position.round();
    BlockState *light_block = world->get_block_at(block_pos);
    if (light_block && !properties(light_block->id).m_solid)
    {
        light_level = light_block->light;
    }

    // Default item rotation
    Vec3f item_rot = Vec3f(0, 0, 0);

    // Get the direction towards the player
    item_rot.y = -world->player.rotation.y + 180;

    // Lock the item rotation to the y-axis
    item_rot.x = 0;
    item_rot.z = 0;

    int multi = item_stack.count > 1 ? 1 : 0;

    item::Item item = item_stack.as_item();

    RenderType render_type = properties(item.id).m_render_type;
    BlockState block = {uint8_t(item.id & 0xFF), uint8_t(item_stack.meta & 0xFF), 0xF, 0xF};
    block.light = light_level;

    Vec3f anim_offset = Vec3f(0, std::sin((ticks_existed + partial_ticks) * M_1_PI * 0.25) * 0.125 + 0.125, 0);
    Vec3f dupe_offset = Vec3f(0.0625);

    // Draw the item (twice if there are multiple items)
    for (int i = 0; i <= multi; i++)
    {
        Transform transform;
        transform.set_position(entity_position + anim_offset + dupe_offset * i + Vec3f(0, 0.125, 0));
        transform.set_scale({0.5f, 0.5f, 0.5f});

        if (item.is_block())
        {
            use_texture(terrain_texture);

            if (!properties(item.id).m_fluid && (properties(item.id).m_nonflat || render_type == RenderType::full || render_type == RenderType::full_special || render_type == RenderType::slab))
            {
                // Draw the block
                item_rot.y = (ticks_existed + partial_ticks) * 4;

                transform.set_position(entity_position + anim_offset + dupe_offset * i + Vec3f(0, 0.25, 0));
                transform.set_rotation(item_rot);
                transform.set_scale({0.25f, 0.25f, 0.25f});

                gertex::use_matrix(get_camera().apply_transform(transform));

                BlockProperties props = properties(block.id);
                if (bool(props.m_transparent) == transparency)
                    render_single_block(block);
            }
            else
            {
                // Draw the item
                item_rot.z = 180;

                transform.set_rotation(item_rot);

                gertex::use_matrix(get_camera().apply_transform(transform));

                if (transparency)
                    render_single_item(get_default_texture_index(BlockID(block.id)), true, light_level);
            }
        }
        else
        {
            use_texture(items_texture);

            // Draw the item
            item_rot.z = 180;
            transform.set_rotation(item_rot);

            gertex::use_matrix(get_camera().apply_transform(transform));
            if (transparency)
                render_single_item(item_stack.get_texture_index(), true, light_level);
        }
    }
}

void EntityItem::resolve_collision(EntityPhysical *b)
{
    if (world->is_remote())
        return;
    if (dead || picked_up || ticks_existed < 20)
        return;

    EntityPlayerLocal *player = dynamic_cast<EntityPlayerLocal *>(b);
    if (!player || player->dead)
        return;

    item::ItemStack left_over = world->player.items.add(item_stack);
    if (left_over.count)
    {
        if (left_over.count != item_stack.count)
        {
            // Play the pop sound if the player picks up at least one item
            javaport::Random rng;
            Sound sound = get_sound("random/pop");
            sound.position = position;
            sound.volume = 0.5;
            sound.pitch = rng.nextFloat() * 0.8 + 0.6;
            world->play_sound(sound);
            if (Gui::get())
                Gui::get()->refresh();
        }
        item_stack = left_over;
    }
    else
    {
        // Play the pop sound if the player picks up the stack
        pickup(b->get_position(0) - Vec3f(0, 0.5, 0));

        if (Gui::get())
            Gui::get()->refresh();
    }
}

bool EntityItem::collides(EntityPhysical *other)
{
    return aabb.inflate(0.7).intersects(other->aabb);
}

void EntityItem::pickup(Vec3f pos)
{
    // Play the pop sound if the player picks up the stack
    javaport::Random rng;
    Sound sound = get_sound("random/pop");
    sound.position = position;
    sound.volume = 0.5;
    sound.pitch = rng.nextFloat() * 0.8 + 0.6;
    world->play_sound(sound);

    // Mark the item as picked up
    picked_up = true;

    // Start the pickup animation
    pickup_pos = pos;
    ticks_existed = item_lifetime - item_pickup_ticks;
}
