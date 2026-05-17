#include "block_clay.hpp"
#include <item/item_id.hpp>

BlockClay::BlockClay(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::CLAY)
{
    data.sound_type = BlockSoundType::dirt;
}

uint16_t BlockClay::drop_id(uint16_t meta, javaport::Random &random)
{
    return +ItemID::clay_ball;
}

uint16_t BlockClay::drop_count(javaport::Random &random)
{
    return 4;
}
