#pragma once

#include <block/block_base.hpp>

class BlockSoil : public BlockBase
{
public:
    BlockSoil(uint16_t id, uint8_t texture_index);

    bool is_opaque() override;
    uint8_t face_texture_index(uint8_t face, uint8_t meta) override;
    void on_random_tick(World *world, const Vec3i &pos, javaport::Random &random) override;
    void on_entity_walk(EntityPhysical *entity, const Vec3i &pos) override;
    void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;
    uint16_t drop_id(uint16_t meta, javaport::Random &random) override;


protected:
    bool has_water(World* world, const Vec3i &pos);
};