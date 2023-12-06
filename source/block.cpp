#include "block.hpp"
#include "blocks.hpp"
#include <algorithm>
uint8_t block_t::get_cast_skylight()
{
    int opacity = get_block_opacity(get_blockid());
    int cast_light = int(get_skylight()) - std::max(opacity, 1);
    return cast_light < 0 ? 0 : cast_light;
}

uint8_t block_t::get_cast_blocklight()
{
    int opacity = get_block_opacity(get_blockid());
    int cast_light = int(get_blocklight()) - std::max(opacity, 1);
    return cast_light < 0 ? 0 : cast_light;
}

void block_t::set_opacity(uint8_t face, uint8_t flag)
{
    if (flag)
        this->visibility_flags |= (1 << face);
    else
        this->visibility_flags &= ~(1 << face);
}
void block_t::set_blockid(BlockID value)
{
    this->id = uint8_t(value);
    this->set_visibility(value != BlockID::air && !is_fluid(value));
}