#include "block.hpp"
#include "blocks.hpp"
#include <algorithm>
void block_t::set_visibility(uint8_t flag)
{
    this->set_opacity(6, flag);
}

uint8_t block_t::get_visibility()
{
    return this->get_opacity(6);
}

uint8_t block_t::get_opacity(uint8_t face)
{
    return (this->visibility_flags & (1 << face));
}

uint8_t block_t::get_skylight()
{
    return (this->light >> 4) & 0xF;
}

uint8_t block_t::get_blocklight()
{
    return this->light & 0xF;
}

uint8_t block_t::get_castlight()
{
    int opacity = get_block_opacity(get_blockid());
    int cast_light = int(get_blocklight()) - opacity - 1;
    return cast_light < 0 ? 0 : cast_light;
}

BlockID block_t::get_blockid()
{
    return BlockID(this->id);
}

void block_t::set_opacity(uint8_t face, uint8_t flag)
{
    if (flag)
        this->visibility_flags |= (1 << face);
    else
        this->visibility_flags &= ~(1 << face);
}

void block_t::set_skylight(uint8_t value)
{
    this->light = (this->light & 0xF) | (value << 4);
}

void block_t::set_blocklight(uint8_t value)
{
    this->light = (this->light & 0xF0) | value;
}

void block_t::set_blockid(BlockID value)
{
    this->id = uint16_t(value);
}