#include "block_tick.hpp"

static int64_t global_order = 0;

BlockTick::BlockTick(BlockID block_id, Vec3i pos, int ticks) : block_id(block_id), pos(pos), ticks(ticks), order(global_order++) {}