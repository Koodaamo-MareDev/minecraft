#pragma once
#include <world/entity/entity.hpp>
#include <math/aabb.hpp>
#include <nbt/nbt.hpp>
#include <vector>

class Chunk;
class World;

class EntityPhysical : public EntityBase
{
public:
    enum class DragPhase : bool
    {
        before_friction = false,
        after_friction = true,
    };
    int32_t entity_id = 0;
    uint8_t type = 0;
    uint32_t ticks_existed = 0;
    AABB aabb;
    Vec3f rotation = Vec3f(0, 0, 0);
    Vec3f prev_position = Vec3f(0, 0, 0);
    Vec3f prev_rotation = Vec3f(0, 0, 0);
    Vec3f movement = Vec3f(0, 0, 0);
    vfloat_t width = 0;
    vfloat_t height = 0;
    vfloat_t y_offset = 0;
    vfloat_t y_size = 0;
    vfloat_t gravity = 0.08;
    Chunk *chunk = nullptr;
    World *world = nullptr;
    bool on_ground = false;
    bool in_water = false;
    bool jumping = false;
    bool horizontal_collision = false;
    bool walk_sound = true;
    bool dead = false;
    bool simulate_offline = false;
    Vec3i server_pos = Vec3i(0, 0, 0);
    Vec3f animation_pos = Vec3f(0, 0, 0);
    Vec3f animation_rot = Vec3f(0, 0, 0);
    uint8_t animation_tick = 0;
    DragPhase drag_phase = DragPhase::before_friction;
    uint8_t light_level = 0;

    EntityPhysical() : EntityBase()
    {
        width = 1;
        height = 1;
    }

    ~EntityPhysical() {}

    virtual bool collides(EntityPhysical *other);

    virtual bool can_remove();

    virtual void resolve_collision(EntityPhysical *b);

    void teleport(Vec3f pos);

    // NOTE: The position should be provided in 1/32ths of a block
    void set_server_position(Vec3i pos, uint8_t ticks = 0);

    // NOTE: The position should be provided in 1/32ths of a block
    void set_server_pos_rot(Vec3i pos, Vec3f rot, uint8_t ticks);

    virtual void tick();

    virtual void animate();

    virtual void render(float partial_ticks, bool transparency);

    virtual size_t size()
    {
        return sizeof(*this);
    }

    virtual void serialize(NBTTagCompound *);

    virtual void deserialize(NBTTagCompound *nbt);

    std::vector<AABB> get_colliding_aabbs(const AABB &aabb);

    bool is_colliding_fluid(const AABB &aabb);

    void move_and_check_collisions();

    Vec3f get_position(vfloat_t partial_ticks)
    {
        return this->prev_position + (this->position - this->prev_position) * partial_ticks;
    }

    Vec3f get_rotation(vfloat_t partial_ticks)
    {
        return this->prev_rotation + (this->rotation - this->prev_rotation) * partial_ticks;
    }

    virtual bool should_jump()
    {
        return false;
    }

    virtual bool use_fluid_physics()
    {
        return true;
    }

    Vec3i get_head_blockpos()
    {
        return Vec3i(std::floor(position.x), std::floor(aabb.min.y + y_offset), std::floor(position.z));
    }

    Vec3i get_foot_blockpos()
    {
        return Vec3i(std::floor(position.x), std::floor(aabb.min.y - y_size), std::floor(position.z));
    }
};
