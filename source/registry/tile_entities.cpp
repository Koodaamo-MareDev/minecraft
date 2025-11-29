#include "tile_entities.hpp"

namespace registry
{
    void register_tile_entities()
    {
        TileEntity::register_ctor<TileEntityChest>("Chest");
    }
}