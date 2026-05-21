#include "registry.hpp"

#include "input_devices.hpp"
#include "items.hpp"
#include "tile_entities.hpp"
#include "textures.hpp"
#include "block_list.hpp"

void registry::register_all()
{
    register_textures();
    register_input_devices();
    register_blocks();
    register_items();
    register_tile_entities();
}