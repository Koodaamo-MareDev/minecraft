#include "block_base.hpp"
#include <registry/block_list.hpp>
#include <render/render_blocks.hpp>
#include <util/constants.hpp>
#include <world/entity.hpp>
#include <world/world.hpp>
#include <stdexcept>

uint8_t BlockBase::texture_index(World *world, const Vec3i &pos, uint8_t face)
{
    return face_texture_index(face, world->get_meta_at(pos));
}

BlockID BlockBase::block_id()
{
    return data.block_id;
}

float BlockBase::hardness()
{
    return data.hardness;
}

float BlockBase::resistance()
{
    return data.resistance;
}

uint8_t BlockBase::light_luminance()
{
    return data.light_luminance;
}

uint8_t BlockBase::light_opacity()
{
    return is_opaque() ? data.light_opacity : 0;
}

AABB BlockBase::aabb()
{
    return data.aabb;
}

BlockSoundType BlockBase::sound_type()
{
    return data.sound_type;
}

const Material &BlockBase::material()
{
    return get_material(data.material);
}

const Materials BlockBase::material_type()
{
    return data.material;
}

std::string BlockBase::name()
{
    return data.name;
}

bool BlockBase::is_opaque()
{
    return data.material != Materials::AIR;
}

bool BlockBase::can_place(World *world, const Vec3i &pos)
{
    BlockBase *block = block_at(world, pos);
    if (block->material().is_liquid || block->block_id() == BlockID::air)
        return true;
    return false;
}

bool BlockBase::tick_on_load()
{
    return false;
}

BlockBase &BlockBase::set_hardness(float value)
{
    this->data.hardness = value;
    return *this;
}

BlockBase &BlockBase::set_resistance(float value)
{
    this->data.resistance = value;
    return *this;
}

BlockBase &BlockBase::set_light_luminance(uint8_t value)
{
    this->data.light_luminance = value;
    return *this;
}

BlockBase &BlockBase::set_light_opacity(uint8_t value)
{
    this->data.light_opacity = value;
    return *this;
}

BlockBase &BlockBase::set_aabb(const AABB &value)
{
    this->data.aabb = value;
    return *this;
}

BlockBase &BlockBase::set_sound_type(BlockSoundType value)
{
    this->data.sound_type = value;
    return *this;
}

BlockBase &BlockBase::set_material(Materials value)
{
    this->data.material = value;
    return *this;
}

BlockBase &BlockBase::set_name(const std::string &value)
{
    this->data.name = value;
    return *this;
}

BlockBase &BlockBase::tool(item::ToolType tool, item::ToolTier tier)
{
    return *this;
}

bool BlockBase::should_render_side(World *world, const Vec3i &pos, uint8_t face)
{
    BlockBase *b = block_at(world, pos + block_face[face]);
    AABB bbox = b->aabb();
    if (face == +BlockFace::NX && bbox.min.x > 0.0)
        return true;
    if (face == +BlockFace::PX && bbox.max.x < 1.0)
        return true;
    if (face == +BlockFace::NY && bbox.min.y > 0.0)
        return true;
    if (face == +BlockFace::PY && bbox.max.y < 1.0)
        return true;
    if (face == +BlockFace::NZ && bbox.min.z > 0.0)
        return true;
    if (face == +BlockFace::PZ && bbox.max.z < 1.0)
        return true;
    return !b->is_opaque();
}

uint8_t BlockBase::face_texture_index(uint8_t face, uint8_t meta)
{
    return data.texture_index;
}

void BlockBase::get_colliding_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB bbox = aabb();
    bbox.translate(Vec3f(pos.x, pos.y, pos.z));
    if (bbox.intersects(other))
        aabb_list.push_back(bbox);
}

void BlockBase::get_raycasting_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{
    get_colliding_aabb(world, pos, other, aabb_list);
}

bool BlockBase::raycastable(uint8_t meta, bool include_fluids)
{
    return true;
}

void BlockBase::apply_entity_velocity(EntityPhysical *entity, const Vec3i &pos)
{
}

float BlockBase::strength(EntityPlayer *player)
{
    if (data.hardness < 0.0f)
        return 0.0f;
    if (true) // TODO: !player->can_harvest(this);
    {
        return 1.0f / data.hardness / 100.0f;
    }
    return 0.0f; // TODO: player->strength_against_block(this)
}

float BlockBase::get_explosion_resistance(EntityPhysical *entity)
{
    return data.resistance / 5.0f;
}

uint16_t BlockBase::drop_id(uint16_t meta, javaport::Random &random)
{
    return +data.block_id;
}

uint16_t BlockBase::drop_count(javaport::Random &random)
{
    return 1;
}

uint16_t BlockBase::drop_meta(uint16_t meta)
{
    return 0;
}

void BlockBase::drop_item_with_chance(World *world, const Vec3i &pos, uint8_t meta, float chance)
{
    world->spawn_drop(pos, item::ItemStack(drop_id(meta, world->random), drop_count(world->random), drop_meta(meta)));
}

void BlockBase::drop_item(World *world, const Vec3i &pos, uint8_t meta)
{
    drop_item_with_chance(world, pos, meta, 1.0f);
}

// Events
void BlockBase::on_random_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
}

void BlockBase::on_tick(World *world, const Vec3i &pos, javaport::Random &random)
{
}

void BlockBase::on_added(World *world, const Vec3i &pos)
{
}

void BlockBase::on_placed(World *world, const Vec3i &pos, uint8_t face)
{
}

void BlockBase::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_face)
{
}

void BlockBase::on_removed(World *world, const Vec3i &pos)
{
}

void BlockBase::on_destroyed(World *world, const Vec3i &pos)
{
}

void BlockBase::on_exploded(World *world, const Vec3i &pos)
{
}

bool BlockBase::on_use(EntityPhysical *entity, const Vec3i &pos)
{
    return false;
}

void BlockBase::on_click(EntityPhysical *entity, const Vec3i &pos)
{
}

void BlockBase::on_entity_place(World *world, const Vec3i &pos, EntityPhysical *entity)
{
}

void BlockBase::on_entity_walk(EntityPhysical *entity, const Vec3i &pos)
{
}

void BlockBase::on_entity_collide(EntityPhysical *entity, const Vec3i &pos)
{
}

void BlockBase::on_harvest(World *world, const Vec3i &pos, uint8_t meta)
{
}

// Redstone
bool BlockBase::provides_power(World *world, const Vec3i &pos, uint8_t face)
{
    return false;
}

bool BlockBase::provides_indirect_power(World *world, const Vec3i &pos, uint8_t face)
{
    return false;
}

bool BlockBase::is_power_source()
{
    return data.is_power_source;
}

bool BlockBase::powers(World *world, const Vec3i &pos, uint8_t face)
{
    return block_at(world, pos)->provides_indirect_power(world, pos, face);
}

bool BlockBase::has_power(World *world, const Vec3i &pos)
{
    return (powers(world, pos + Vec3i{-1, 0, 0}, +BlockFace::NX) ||
            powers(world, pos + Vec3i{+1, 0, 0}, +BlockFace::PX) ||
            powers(world, pos + Vec3i{0, -1, 0}, +BlockFace::NY) ||
            powers(world, pos + Vec3i{0, +1, 0}, +BlockFace::PY) ||
            powers(world, pos + Vec3i{0, 0, -1}, +BlockFace::NZ) ||
            powers(world, pos + Vec3i{0, 0, +1}, +BlockFace::PZ));
}

bool BlockBase::powers_indirectly(World *world, const Vec3i &pos, uint8_t face)
{
    if (block_at(world, pos)->is_opaque())
        return has_power(world, pos);
    return block_at(world, pos)->provides_power(world, pos, face);
}

bool BlockBase::has_indirect_power(World *world, const Vec3i &pos)
{
    return (powers_indirectly(world, pos + Vec3i{-1, 0, 0}, +BlockFace::NX) ||
            powers_indirectly(world, pos + Vec3i{+1, 0, 0}, +BlockFace::PX) ||
            powers_indirectly(world, pos + Vec3i{0, -1, 0}, +BlockFace::NY) ||
            powers_indirectly(world, pos + Vec3i{0, +1, 0}, +BlockFace::PY) ||
            powers_indirectly(world, pos + Vec3i{0, 0, -1}, +BlockFace::NZ) ||
            powers_indirectly(world, pos + Vec3i{0, 0, +1}, +BlockFace::PZ));
}

bool BlockBase::colored()
{
    return false;
}

int BlockBase::render(gertex::DisplayList<gertex::Vertex16> *list, BlockState *state, const Vec3i &pos)
{
    return data.render_func(list, state, pos);
}

bool BlockBase::can_stay(World *world, const Vec3i &pos)
{
    return true;
}

BlockBase::BlockBase(uint16_t id, uint8_t texture_index, Materials material)
{
    data.slipperiness = 0.6f;
    data.liquid = false;
    data.tick_on_load = false;
    data.is_power_source = false;
    data.texture_index = texture_index;
    data.light_luminance = 0;
    data.light_opacity = 15;
    data.id = id;
    data.hardness = 0.0f;
    data.resistance = 0.0f;
    data.aabb = AABB(Vec3f(0.0, 0.0, 0.0), Vec3f(1.0, 1.0, 1.0));
    data.sound_type = BlockSoundType::none;
    data.material = material;
    data.render_func = material == Materials::AIR ? render_nothing : render_cube_special;
    if (block_list[id] != nullptr)
        throw std::runtime_error("Attempt to override block " + std::to_string(id));
    block_list[id] = this;
}

BlockBase::BlockBase(uint16_t id, Materials material) : BlockBase(id, 0, material)
{
}
