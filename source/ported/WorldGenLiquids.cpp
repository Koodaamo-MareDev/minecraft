#include "WorldGenLiquids.hpp"

#include <world/world.hpp>
namespace javaport
{
    bool WorldGenLiquids::generate(Random &rng, Vec3i pos)
    {
        if (world->get_block_id_at(pos + Vec3i(0, 1, 0), BlockID::air) != BlockID::stone)
            return false;
        if (world->get_block_id_at(pos + Vec3i(0, -1, 0), BlockID::air) != BlockID::stone)
            return false;
        BlockID id = world->get_block_id_at(pos, BlockID::air);
        if (id != BlockID::air && id != BlockID::stone)
            return false;
        int air_count = 0;
        if (world->get_block_id_at(pos + Vec3i(1, 0, 0), BlockID::air) == BlockID::air)
            air_count++;
        if (world->get_block_id_at(pos + Vec3i(-1, 0, 0), BlockID::air) == BlockID::air)
            air_count++;
        if (world->get_block_id_at(pos + Vec3i(0, 0, 1), BlockID::air) == BlockID::air)
            air_count++;
        if (world->get_block_id_at(pos + Vec3i(0, 0, -1), BlockID::air) == BlockID::air)
            air_count++;

        if (air_count != 1)
            return false;

        int stone_count = 0;
        if (world->get_block_id_at(pos + Vec3i(1, 0, 0), BlockID::stone) == BlockID::stone)
            stone_count++;
        if (world->get_block_id_at(pos + Vec3i(-1, 0, 0), BlockID::stone) == BlockID::stone)
            stone_count++;
        if (world->get_block_id_at(pos + Vec3i(0, 0, 1), BlockID::stone) == BlockID::stone)
            stone_count++;
        if (world->get_block_id_at(pos + Vec3i(0, 0, -1), BlockID::stone) == BlockID::stone)
            stone_count++;

        if (stone_count == 3)
        {
            world->set_block_at(pos, liquid);
            return true;
        }
        return false;
    }
}
