#include "registry.hpp"

#include "input_devices.hpp"
#include "items.hpp"
#include "tile_entities.hpp"

void registry::register_all()
{
    register_input_devices();
    register_items();
    register_tile_entities();
}