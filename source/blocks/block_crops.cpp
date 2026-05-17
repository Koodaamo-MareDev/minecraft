#include "block_crops.hpp"

#include <world/world.hpp>
#include <world/entity.hpp>
#include <item/item_id.hpp>
#include <block/block_id.hpp>

BlockCrops::BlockCrops(uint16_t id, uint8_t texture_index, Materials material) : BlockFlower(id, texture_index, material)
{
    data.tick_on_load = true;
    data.sound_type = BlockSoundType::grass;
    data.render_type = BlockRenderType::special;
}

uint8_t BlockCrops::face_texture_index(uint8_t face, uint8_t meta)
{
    return data.texture_index + meta;
}

bool BlockCrops::on_use(EntityPhysical *entity, const Vec3i &pos)
{
    EntityPlayerLocal *player = dynamic_cast<EntityPlayerLocal *>(entity);
    // Check if item in hand is bone meal (dye:15)
    if (!player || !player->selected_item || player->selected_item->id != +ItemID::dye || player->selected_item->meta != 15)
        return true;
    World *world = player->world;
    uint8_t meta = world->get_meta_at(pos);
    if (meta < 7)
    {
        player->selected_item->count--;
        world->set_meta_at(pos, 7);
    }
    return true;
}

void BlockCrops::on_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
    BlockState *block = world->get_block_at(pos);
    if (std::max(block->light, block->sky_light) >= 9)
    {
        uint8_t meta = world->get_meta_at(pos);
        if (meta < 7)
        {
            float growth_rate = get_growth_rate(world, pos);
            if (random.nextInt(100.0f / growth_rate) == 0)
            {
                world->set_meta_at(pos, meta + 1);
            }
        }
    }
}

bool BlockCrops::can_grow_on(uint8_t block_id)
{
    return block_id == +BlockID::farmland;
}

uint16_t BlockCrops::drop_id(uint16_t meta, javaport::Random &random)
{
    return meta == 7 ? +ItemID::wheat : 0;
}

uint16_t BlockCrops::drop_count(javaport::Random &random)
{
    return 1;
}

float BlockCrops::get_growth_rate(World *world, const Vec3i &pos)
{
    float total_weight = 1.0f;
    uint8_t block_ids[9];
    for (int i = 0, x = -1; x <= 1; x++)
        for (int z = -1; z <= 1; z++)
        {
            block_ids[i++] = world->get_block_id_at(pos + Vec3i{x, 0, z});
        }
    bool sideways_x = block_ids[3] == data.block_id || block_ids[5] == data.block_id;
    bool sideways_z = block_ids[1] == data.block_id || block_ids[7] == data.block_id;
    bool diagonal = block_ids[0] == data.block_id || block_ids[2] == data.block_id || block_ids[6] == data.block_id || block_ids[8] == data.block_id;
    for (int x = -1; x <= 1; x++)
        for (int z = -1; z <= 1; z++)
        {
            uint8_t block_id = world->get_block_id_at(pos + Vec3i{x, -1, z});
            float weight = 0.0f;
            if (block_id == +BlockID::farmland)
            {
                // Check moisture
                uint8_t meta = world->get_meta_at(pos + Vec3i{x, -1, z});
                if (meta > 0)
                    weight = 3.0f;
            }

            if (x != 0 || z != 0)
                weight *= 0.25f;

            total_weight += weight;
        }

    if (diagonal || (sideways_x && sideways_z))
        total_weight *= 0.5f;
    return total_weight;
}
