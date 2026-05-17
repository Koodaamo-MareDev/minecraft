#include "block_workbench.hpp"
#include <world/world.hpp>
#include <world/entity.hpp>
#include <gui/gui_crafting.hpp>

BlockWorkbench::BlockWorkbench(int16_t id) : BlockBase(id, 59, Materials::WOOD)
{
}

uint8_t BlockWorkbench::face_texture_index(uint8_t face, uint8_t meta)
{
    switch (static_cast<BlockFace>(face))
    {
    case BlockFace::NX:
    case BlockFace::NZ:
        return 59;
    case BlockFace::PX:
    case BlockFace::PZ:
        return 60;
    case BlockFace::NY:
        return 4;
    default:
        return 43;
    }
}

bool BlockWorkbench::on_use(EntityPhysical *entity, const Vec3i &pos)
{
    if (entity->world->is_remote())
        return true;
    Gui::set_gui(new GuiCrafting(entity, nullptr, 0));
    return true;
}
