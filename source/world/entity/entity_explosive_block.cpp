#include "entity_explosive_block.hpp"
#include <world/world.hpp>
#include <render/render.hpp>
#include <sounds.hpp>

EntityExplosiveBlock::EntityExplosiveBlock(BlockState block_state, const Vec3i &position, uint16_t fuse) : EntityFallingBlock(block_state, position), EntityExplosive()
{
    // Randomize a tiny "jump" to the block
    javaport::Random rng;
    velocity = Vec3f(rng.nextDouble() * 0.04 - 0.02, 0.2, rng.nextDouble() * 0.04 - 0.02);

    this->fuse = fuse;

    // Play the fuse sound if there is a long enough fuse
    if (fuse > 45)
    {
        Sound sound = get_sound("random/fuse");
        sound.position = get_position(0);
        sound.volume = 0.5;
        sound.pitch = 1.0;
        world->play_sound(sound);
    }
}

void EntityExplosiveBlock::tick()
{

    EntityPhysical::tick();
    EntityExplosive::tick();
}

void EntityExplosiveBlock::render(float partial_ticks, bool transparency)
{
    if (!chunk)
        return;

    // Prepare the block state
    Vec3i int_pos = Vec3i(std::floor(position.x), std::floor(position.y + 0.5f), std::floor(position.z));
    BlockState *block_at_pos = chunk->get_block(int_pos);
    if (!chunk->light_pending && block_at_pos && !properties(block_at_pos->id).m_solid)
        block_state.light = block_at_pos->light;

    Transform block_transform;
    block_transform.set_position(get_position(partial_ticks));

    gertex::use_matrix(get_camera().apply_transform(block_transform));

    if ((fuse / 5) % 2 == 1)
        use_texture(white_texture);

    render_single_block(block_state);

    use_texture(terrain_texture);
}
