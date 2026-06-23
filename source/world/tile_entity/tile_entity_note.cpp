#include "tile_entity_note.hpp"
#include <world/world.hpp>
#include <registry/block_list.hpp>

TileEntityNote::TileEntityNote()
{
}

NBTTagCompound *TileEntityNote::serialize()
{
    NBTTagCompound *nbt = TileEntity::serialize();
    nbt->setTag("note", new NBTTagByte(note));
    return nbt;
}

void TileEntityNote::deserialize(NBTTagCompound *nbt)
{
    note = nbt->getByte("note");
    if (note < 0)
        note = 0;
    if (note > 24)
        note = 24;
}

std::string TileEntityNote::id()
{
    return "Music";
}

void TileEntityNote::trigger_note(World *world, const Vec3i &pos)
{
    if (!world->get_block_id_at(pos + Vec3i{0, 1, 0}))
    {
        Materials below = block_at(world, pos - Vec3i{0, -1, 0})->material_type();
        uint8_t note_type = 0;
        switch (below)
        {
        case Materials::ROCK:
            note_type = 1;
            break;
        case Materials::SAND:
            note_type = 2;
            break;
        case Materials::GLASS:
            note_type = 3;
            break;
        case Materials::WOOD:
            note_type = 4;
            break;
        default:
            break;
        }
        world->play_note_at(pos, note_type, note);
    }
}

void TileEntityNote::change_pitch()
{
    note = (note + 1) % 25;
}
