#include "block_ore.hpp"
#include <item/item_id.hpp>

BlockOre::BlockOre(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::ROCK)
{
}

uint16_t BlockOre::drop_id(uint16_t meta, javaport::Random &random)
{
    switch (data.block_id)
    {
    case BlockID::coal_ore:
        return +ItemID::coal;
    case BlockID::diamond_ore:
        return +ItemID::diamond;
    case BlockID::lapis_ore:
        return +ItemID::dye;
    default:
        return data.id;
    }
}

uint16_t BlockOre::drop_count(javaport::Random &random)
{
    return data.block_id == BlockID::lapis_ore ? (4 + random.nextInt(5)) : 1;
}

uint16_t BlockOre::drop_meta(uint16_t meta)
{
    return data.block_id == BlockID::lapis_ore ? 4 : 0;
}
