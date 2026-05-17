#pragma once

#include "block_flower.hpp"

class BlockCrops : public BlockFlower
{

public:
    BlockCrops(uint16_t id, uint8_t texture_index, Materials material);
    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta) override;
    virtual bool on_use(EntityPhysical *entity, const Vec3i &pos) override;
    virtual void on_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;
    virtual uint16_t drop_count(javaport::Random &random) override;

protected:
    virtual float get_growth_rate(World *world, const Vec3i &pos);
    virtual bool can_grow_on(uint8_t block_id);
};