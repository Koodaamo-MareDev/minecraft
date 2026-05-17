#pragma once

#include <blocks/block_flower.hpp>

class BlockSapling : public BlockFlower
{
    
public:
    BlockSapling(uint16_t id, uint8_t texture_index);

    virtual void on_random_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    void grow_tree(World *world, const Vec3i &pos, javaport::Random &random);
};