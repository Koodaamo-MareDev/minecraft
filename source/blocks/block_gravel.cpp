#include "block_gravel.hpp"
#include <item/item_id.hpp>

BlockGravel::BlockGravel(uint16_t id, uint8_t texture_index, Materials material) : BlockFalling(id, texture_index, material)
{
}

uint16_t BlockGravel::drop_id(uint16_t meta, javaport::Random &random)
{
    return random.nextInt(10) == 0 ? +ItemID::flint : data.id;
}