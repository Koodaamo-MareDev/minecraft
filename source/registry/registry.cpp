#include "registry.hpp"

#include "input_devices.hpp"
#include "items.hpp"
#include "tile_entities.hpp"
#include "textures.hpp"

void registry::register_all()
{
    register_textures();
    register_input_devices();
    register_items();
    register_tile_entities();
}