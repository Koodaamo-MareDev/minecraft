#include "entity_falling_block.hpp"
#include <block/block_properties.hpp>
#include <world/world.hpp>
#include <world/chunk.hpp>
#include <render/render.hpp>


EntityFallingBlock::EntityFallingBlock(BlockState block_state, const Vec3i &position) : EntityPhysical()
{
    teleport(Vec3f(position.x, position.y, position.z) + Vec3f(0.5, 0, 0.5));
    this->walk_sound = false;
    this->drag_phase = DragPhase::after_friction;
    this->gravity = 0.04;
    this->simulate_offline = true;
    this->width = 0.999;
    this->height = 0.999;
    this->block_state = block_state;
}
void EntityFallingBlock::tick()
{
    fall_time++;
    EntityPhysical::tick();
    if (world->is_remote())
        return;
    if (placed)
        return;

    if (on_ground)
    {
        Vec3i int_pos = get_foot_blockpos();
        BlockState *block = world->get_block_at(int_pos);
        if (block && (block->blockid == BlockID::air || properties(block->id).m_fluid))
        {
            // Update the block
            world->set_block_and_meta_at(int_pos, block_state.blockid, block_state.meta);
            world->notify_at(int_pos);
            placed = true;
        }
        else
        {
            // Skip to the dead state
            fall_time = 601;
        }
    }

    // Remove the entity after 30 seconds
    if (fall_time > 600)
    {
        placed = true;

        // Drop the block as an item
        item::ItemStack item = properties(block_state.id).m_drops(block_state);
        world->spawn_drop(get_foot_blockpos(), item);
    }
}

bool EntityFallingBlock::can_remove()
{
    if (world->is_remote())
        return false;
    // This flag is prioritized
    if (dead)
        return true;

    int sect_num = get_foot_blockpos().y >> 4;

    // Check if the section number is valid
    if (sect_num >> 4 >= VERTICAL_SECTION_COUNT || sect_num < 0)
        return true;

    // Can't remove if it's not on the ground
    if (!on_ground)
        return false;

    Section &sect = chunk->sections[sect_num];

    // Mesh should not be currently updating as that would cause visual glitches
    bool is_section_stable = sect.stable();

    // Ensure that the chunk doesn't have any light updates
    return !chunk->light_pending && is_section_stable && !sect.dirty;
}

void EntityFallingBlock::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;

    // Prepare the block state
    Vec3i int_pos = Vec3i(std::floor(position.x), std::floor(position.y + 0.5), std::floor(position.z));
    BlockState *block_at_pos = world->get_block_at(int_pos);
    if (fall_time && block_at_pos && !properties(block_at_pos->id).m_solid)
        block_state.light = block_at_pos->light;

    Transform block_transform;
    block_transform.set_position(get_position(partial_ticks) + Vec3f(0, 0.5, 0));
    gertex::use_matrix(get_camera().apply_transform(block_transform));
    use_texture(terrain_texture);
    render_single_block(block_state);
}