#ifndef BLOCK_TICK_HPP
#define BLOCK_TICK_HPP

#include <math/vec3i.hpp>
#include <block/block_id.hpp>
#include <cstddef>

struct BlockTick
{
    BlockID block_id = BlockID::air;
    Vec3i pos;
    int32_t ticks;
    int64_t order; // Used to maintain order of ticks scheduled for the same time

    BlockTick(BlockID block_id, Vec3i pos, int ticks);

    bool operator<(const BlockTick &other) const
    {
        if (ticks < other.ticks)
            return true;
        if (order < other.order && ticks == other.ticks)
            return true;
        return false;
    }

    bool operator==(const BlockTick &other) const
    {
        return pos == other.pos && block_id == other.block_id && ticks == other.ticks;
    }
};

#endif // BLOCK_TICK_HPP