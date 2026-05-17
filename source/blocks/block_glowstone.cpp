#include "block_glowstone.hpp"

#include <item/item_id.hpp>

BlockGlowstone::BlockGlowstone(uint16_t id, uint8_t texture_index, Materials material) : BlockBase(id, texture_index, material)
{
    data.light_luminance = 15;
}

uint16_t BlockGlowstone::drop_id(uint16_t meta, javaport::Random &random)
{
    return +ItemID::glowstone_dust;
}