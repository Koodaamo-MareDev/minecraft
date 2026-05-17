#pragma once

#include <cstdint>
#include <math/aabb.hpp>
#include <string>
#include <vector>
#include <ported/Random.hpp>

#include "material.hpp"
#include "block_enums.hpp"
#include "block_id.hpp"

class World;
class EntityPlayer;
class EntityPhysical;

class BlockBase
{
public:
    BlockBase(uint16_t id, uint8_t texture_index, Materials material);
    BlockBase(uint16_t id, Materials material);

    virtual ~BlockBase() = default;

    // General per-block properties
    virtual BlockID block_id();
    virtual float hardness();
    virtual float resistance();
    virtual uint8_t light_luminance();
    virtual uint8_t light_opacity();
    virtual AABB aabb();
    virtual BlockSoundType sound_type();
    virtual const Material &material();
    virtual const Materials material_type();
    virtual std::string name();

    // Setters for per-block properties
    virtual BlockBase &set_hardness(float value);
    virtual BlockBase &set_resistance(float value);
    virtual BlockBase &set_light_luminance(uint8_t value);
    virtual BlockBase &set_light_opacity(uint8_t value);
    virtual BlockBase &set_aabb(const AABB &value);
    virtual BlockBase &set_sound_type(BlockSoundType value);
    virtual BlockBase &set_material(Materials value);
    virtual BlockBase &set_name(const std::string &value);

    // Per type properties
    virtual bool is_opaque();
    virtual bool tick_on_load();

    // Misc state/world related
    virtual uint8_t face_texture_index(uint8_t face, uint8_t meta);
    virtual bool raycastable(uint8_t meta, bool include_fluids);
    virtual uint8_t texture_index(World *world, const Vec3i &pos, uint8_t face);
    virtual bool can_place(World *world, const Vec3i &pos);
    virtual bool should_render_side(World *world, const Vec3i &pos, uint8_t face);
    virtual void get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list);
    virtual void get_raycasting_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list);
    virtual void apply_entity_velocity(EntityPhysical *entity, const Vec3i &pos);
    virtual float strength(EntityPlayer *player);
    virtual float get_explosion_resistance(EntityPhysical *entity);

    // Loot related
    virtual uint16_t drop_id(uint16_t meta, javaport::Random &random);
    virtual uint16_t drop_count(javaport::Random &random);
    virtual uint16_t drop_meta(uint16_t meta);
    virtual void drop_item_with_chance(World *world, const Vec3i &pos, uint8_t meta, float chance);
    virtual void drop_item(World *world, const Vec3i &pos, uint8_t meta);

    // Events
    virtual void on_random_tick(World *world, const Vec3i &pos, javaport::Random &random);
    virtual void on_tick(World *world, const Vec3i &pos, javaport::Random &random);
    virtual void on_added(World *world, const Vec3i &pos);
    virtual void on_placed(World *world, const Vec3i &pos, uint8_t face);
    virtual void on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_id);
    virtual void on_removed(World *world, const Vec3i &pos);
    virtual void on_destroyed(World *world, const Vec3i &pos);
    virtual void on_exploded(World *world, const Vec3i &pos);
    virtual bool on_use(EntityPhysical *entity, const Vec3i &pos);
    virtual void on_click(EntityPhysical *entity, const Vec3i &pos);
    virtual void on_entity_place(World *world, const Vec3i &pos, EntityPhysical *entity);
    virtual void on_entity_walk(EntityPhysical *entity, const Vec3i &pos);
    virtual void on_entity_collide(EntityPhysical *entity, const Vec3i &pos);
    virtual void on_harvest(World *world, const Vec3i &pos, uint8_t meta);
    virtual bool can_stay(World *world, const Vec3i &pos);

    // Redstone
    virtual bool provides_power(World *world, const Vec3i &pos, uint8_t face);
    virtual bool provides_indirect_power(World *world, const Vec3i &pos, uint8_t face);
    virtual bool is_power_source();

    // Internal redstone
    bool powers_directly(World *world, const Vec3i &pos, uint8_t face);
    bool has_direct_power(World *world, const Vec3i &pos);
    bool powers_indirectly(World *world, const Vec3i &pos, uint8_t face);
    bool has_indirect_power(World *world, const Vec3i &pos);

protected:
    struct Data
    {
        bool liquid;
        bool tick_on_load;
        bool is_power_source;
        uint8_t texture_index;
        uint8_t light_opacity;
        uint8_t light_luminance;

        union
        {
            uint8_t id;
            BlockID block_id;
        };
        BlockRenderType render_type;
        BlockSoundType sound_type;
        Materials material;

        float hardness;
        float resistance;
        float slipperiness;
        AABB aabb;
        std::string name;
    } data;
};