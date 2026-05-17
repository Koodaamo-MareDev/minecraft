#pragma once

#include <blocks/block_container.hpp>

class BlockSign : public BlockContainer
{
public:
    BlockSign(uint16_t id, uint8_t texture_index, bool is_post);

    bool is_opaque() override;
    virtual void get_raycasting_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list) override;
    virtual TileEntity *get_tile_entity(World *world, const Vec3i &pos) override;
    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random) override;
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face) override;

private:
    bool is_post = false;
};