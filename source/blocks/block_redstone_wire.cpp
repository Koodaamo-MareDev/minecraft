#include "block_redstone_wire.hpp"

#include <render/render_blocks.hpp>
#include <world/world.hpp>
#include <registry/block_list.hpp>
#include <item/item_id.hpp>

BlockRedstoneWire::BlockRedstoneWire(uint16_t id, uint8_t texture_index) : BlockBase(id, texture_index, Materials::CIRCUITS)
{
    data.sound_type = BlockSoundType::stone;
    data.render_func = render_flat_ground;
}

void BlockRedstoneWire::get_raycasting_aabb(World *world, const Vec3i &pos, const AABB &other, std::vector<AABB> &aabb_list)
{
    AABB bbox(Vec3f(pos.x, pos.y, pos.z), Vec3f(pos.x + 1, pos.y + 2 / 16.0f, pos.z + 1));
    if (bbox.intersects(other))
        aabb_list.push_back(bbox);
}

bool BlockRedstoneWire::is_opaque()
{
    return false;
}

bool BlockRedstoneWire::can_place(World *world, const Vec3i &pos)
{
    return block_at(world, pos + Vec3i{0, -1, 0})->is_opaque();
}

void BlockRedstoneWire::on_added(World *world, const Vec3i &pos)
{
    if (!world->is_remote())
    {
        update_and_propagate(world, pos);
        world->notify_at(pos + Vec3i{0, 1, 0}, data.block_id);
        world->notify_at(pos + Vec3i{0, -1, 0}, data.block_id);
        common_notify_neighbors(world, pos);
    }
}

void BlockRedstoneWire::on_removed(World *world, const Vec3i &pos)
{
    if (!world->is_remote())
    {
        world->notify_at(pos + Vec3i{0, 1, 0}, data.block_id);
        world->notify_at(pos + Vec3i{0, -1, 0}, data.block_id);
        update_and_propagate(world, pos);
        common_notify_neighbors(world, pos);
    }
}

uint16_t BlockRedstoneWire::drop_id(uint16_t meta, javaport::Random &random)
{
    return +ItemID::redstone_dust;
}

void BlockRedstoneWire::on_neighbor_changed(World *world, const Vec3i &pos, uint8_t neighbor_id)
{
    if (world->is_remote())
        return;

    // If in an invalid location, destroy this block
    if (!can_place(world, pos))
    {
        drop_item(world, pos, world->get_meta_at(pos));
        world->set_block_at(pos, BlockID::air);
        world->notify_at(pos);
    }
    else
        update_and_propagate(world, pos);
}

bool BlockRedstoneWire::provides_power(World *world, const Vec3i &pos, uint8_t face)
{
    // Global flag: if wires are set to not provide power (e.g. during update
    // propagation to prevent feedback loops), always return false.
    // Also return false if this wire has zero signal strength (metadata == 0).
    if (!wires_provide_power || world->get_meta_at(pos) == 0)
        return false;

    // A wire always powers the block directly above it (face PY = +Y = top).
    if (face == +BlockFace::PY)
        return true;

    const Vec3i offsets[4]{
        {-1, 0, 0}, // [0] X- neighbour
        {+1, 0, 0}, // [1] X+ neighbour
        {0, 0, -1}, // [2] Z- neighbour
        {0, 0, +1}, // [3] Z+ neighbour
    };

    // For each horizontal direction, check whether power is coming from:
    //   a) the direct horizontal neighbour (same Y), OR
    //   b) the block one step below that neighbour, if the neighbour itself
    //      is non-opaque (wire going down a step).
    bool has_power[4];
    for (uint8_t i = 0; i < 4; i++)
    {
        Vec3i o = pos + offsets[i];
        has_power[i] = is_source_or_wire(world, o) || (!block_at(world, o)->is_opaque() && is_source_or_wire(world, o - Vec3i{0, 1, 0}));
    }

    // If the block directly above this wire is non-opaque, a wire on top of
    // an adjacent opaque block (i.e. going UP a step) can also feed power in.
    if (!block_at(world, pos + Vec3i{0, 1, 0})->is_opaque())
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            Vec3i o = pos + offsets[i];
            // The neighbour must be opaque (acts as the "step"), and the wire
            // on top of it must be a power source.
            if (block_at(world, o)->is_opaque() && is_source_or_wire(world, o + Vec3i{0, 1, 0}))
            {
                has_power[i] = true;
            }
        }
    }

    // --- Determine which face is powered ---
    //
    // CASE 1: No horizontal connections at all.
    // An unconnected wire powers all four side faces equally.
    // Only valid for actual side faces (NX, PX, NZ, PZ); top was handled
    // above, bottom (NY) is never powered.
    if (!has_power[0] && !has_power[1] && !has_power[2] && !has_power[3])
    {
        return (face == +BlockFace::NX ||
                face == +BlockFace::PX ||
                face == +BlockFace::NZ ||
                face == +BlockFace::PZ);
    }

    // CASE 2: Wire has at least one connection - it only powers the face(s)
    // that match an active connection direction, provided the *opposite* axis
    // has no connections (so power doesn't bleed sideways).
    //
    // Face NX (-X) is powered when X- has power and neither Z axis is active.
    // Face PX (+X) is powered when X+ has power and neither Z axis is active.
    // Face NZ (-Z) is powered when Z- has power and neither X axis is active.
    // Face PZ (+Z) is powered when Z+ has power and neither X axis is active.
    return (face == +BlockFace::NX && has_power[0] && !has_power[2] && !has_power[3]) ||
           (face == +BlockFace::PX && has_power[1] && !has_power[2] && !has_power[3]) ||
           (face == +BlockFace::NZ && has_power[2] && !has_power[0] && !has_power[1]) ||
           (face == +BlockFace::PZ && has_power[3] && !has_power[0] && !has_power[1]);
}

bool BlockRedstoneWire::provides_indirect_power(World *world, const Vec3i &pos, uint8_t face)
{
    return wires_provide_power && provides_power(world, pos, face);
}

bool BlockRedstoneWire::is_power_source()
{
    return wires_provide_power;
}

void BlockRedstoneWire::common_notify_neighbors(World *world, const Vec3i &pos)
{
    const Vec3i up = block_face[+BlockFace::PY];
    const Vec3i down = block_face[+BlockFace::NY];
    notify_wire_neighbors(world, pos + block_face[+BlockFace::NX]);
    notify_wire_neighbors(world, pos + block_face[+BlockFace::PX]);
    notify_wire_neighbors(world, pos + block_face[+BlockFace::NZ]);
    notify_wire_neighbors(world, pos + block_face[+BlockFace::PZ]);
    if (block_at(world, pos + block_face[+BlockFace::NX])->is_opaque())
        notify_wire_neighbors(world, pos + block_face[+BlockFace::NX] + up);
    else
        notify_wire_neighbors(world, pos + block_face[+BlockFace::NX] + down);

    if (block_at(world, pos + block_face[+BlockFace::PX])->is_opaque())
        notify_wire_neighbors(world, pos + block_face[+BlockFace::PX] + up);
    else
        notify_wire_neighbors(world, pos + block_face[+BlockFace::PX] + down);

    if (block_at(world, pos + block_face[+BlockFace::NZ])->is_opaque())
        notify_wire_neighbors(world, pos + block_face[+BlockFace::NZ] + up);
    else
        notify_wire_neighbors(world, pos + block_face[+BlockFace::NZ] + down);

    if (block_at(world, pos + block_face[+BlockFace::PZ])->is_opaque())
        notify_wire_neighbors(world, pos + block_face[+BlockFace::PZ] + up);
    else
        notify_wire_neighbors(world, pos + block_face[+BlockFace::PZ] + down);
}

void BlockRedstoneWire::update_and_propagate(World *world, const Vec3i &pos)
{
    propagate(world, pos, pos);

    std::set<std::pair<World *, Vec3i>> updates_copy = updates;
    updates.clear();
    for (auto &&u : updates_copy)
    {
        u.first->notify_at(u.second, data.block_id);
    }
}
void BlockRedstoneWire::propagate(World *world, const Vec3i &to, const Vec3i &from)
{
    const Vec3i offsets[4]{
        {-1, 0, 0},
        {+1, 0, 0},
        {0, 0, -1},
        {0, 0, +1},
    };

    if (world->get_block_id_at(to) != data.block_id)
        return;

    int signal_strength = world->get_meta_at(to);
    int new_signal_strength = 0;

    // Temporarily disable wire power emission so that has_indirect_power()
    // only sees "real" power sources (levers, repeaters, etc.) and not other
    // wires - prevents the wire from seeing its own reflected signal.
    this->wires_provide_power = false;
    bool has_full_power = has_indirect_power(world, to);
    this->wires_provide_power = true;

    if (has_full_power)
    {
        // Directly powered by a strong source (repeater output, etc.) -> max strength.
        new_signal_strength = 15;
    }
    else
    {
        for (int i = 0; i < 4; i++)
        {
            Vec3i off = to + offsets[i]; // horizontal neighbour in this direction

            // Avoid going back and forth
            if (off != from)
                new_signal_strength = max_current_strength(world, off, new_signal_strength);

            if (block_at(world, off)->is_opaque())
            {
                // Neighbour is a solid block - a wire could be sitting on TOP
                // of it (one step up). Check that upper diagonal, but only if
                // the block above us isn't opaque (otherwise the upper wire
                // can't visually/logically connect to us).
                // Also respect the 'from' exclusion for the upper position.
                Vec3i upper = off + Vec3i{0, 1, 0};
                if (!block_at(world, to + Vec3i{0, 1, 0})->is_opaque() && upper != from)
                    new_signal_strength = max_current_strength(world, upper, new_signal_strength);
            }
            else
            {
                // Neighbour is non-opaque - a wire could be sitting on the
                // block one step BELOW it (going down a step).
                // Bug fix: the 'from' exclusion must be applied here too,
                // independently of the is_opaque branch above.
                Vec3i lower = off - Vec3i{0, 1, 0};
                if (lower != from)
                    new_signal_strength = max_current_strength(world, lower, new_signal_strength);
            }
        }

        // Each wire hop attenuates the signal by 1 (minimum 0).
        new_signal_strength -= (new_signal_strength > 0) ? 1 : 0;
    }

    if (signal_strength == new_signal_strength)
        return; // Nothing changed - stop propagation.

    world->set_meta_at(to, new_signal_strength);

    // Capture the decayed strength once before the loop.
    // Re-reading from the world inside the loop is unsafe if a recursive
    // propagate() call modifies this wire's metadata mid-iteration.
    uint8_t decayed = new_signal_strength - ((new_signal_strength > 0) ? 1 : 0);

    for (int i = 0; i < 4; i++)
    {
        // Work with a clean, unmodified horizontal offset each iteration.
        // Bug fix: do NOT mutate 'off' in place - compute upper/lower as
        // separate named variables so both checks use the correct position.
        const Vec3i off_h = to + offsets[i]; // purely horizontal, same Y as 'to'

        // Determine whether to propagate to the upper or lower diagonal
        // position (mirrors the read logic above).
        Vec3i off_v;
        if (block_at(world, off_h)->is_opaque())
            off_v = off_h + Vec3i{0, 1, 0}; // wire on top of opaque neighbour
        else
            off_v = off_h - Vec3i{0, 1, 0}; // wire below non-opaque neighbour

        decayed = world->get_meta_at(to);
        // --- Propagate to the vertical-diagonal neighbour ---
        {
            // max_current_strength returns the neighbour's signal if it IS a
            // wire, or -1 (as int) if it isn't. We only recurse into wires.
            int ps = max_current_strength(world, off_v, -1);
            if (ps >= 0 && (uint8_t)ps != decayed)
            {
                // Bug fix: was '{off.x, to.y, off.x}' - the Z component was
                // incorrectly set to off.x, corrupting diagonal propagation.
                propagate(world, off_v, to);
            }
        }
        decayed = world->get_meta_at(to);

        // --- Propagate to the horizontal neighbour ---
        {
            int ps = max_current_strength(world, off_h, -1);
            if (ps >= 0 && (uint8_t)ps != decayed)
            {
                propagate(world, off_h, to);
            }
        }
    }

    // Schedule block-update notifications when a wire turns fully on or off.
    // Neighbours need to re-evaluate (doors, pistons, lamps, etc.).
    if (signal_strength == 0 || new_signal_strength == 0)
    {
        updates.emplace(std::make_pair(world, to));
        for (uint8_t i = 0; i < 6; i++)
            updates.emplace(std::make_pair(world, to + block_face[i]));
    }
}

void BlockRedstoneWire::notify_wire_neighbors(World *world, const Vec3i &pos)
{
    // Only notify if the block at this position actually is redstone wire.
    if (world->get_block_id_at(pos) == data.block_id)
    {
        world->notify_at(pos, data.block_id);
        // Also notify all 6 face-adjacent blocks so they can react to the
        // wire's new state (e.g. a lamp directly next to it).
        for (uint8_t i = 0; i < 6; i++)
            world->notify_at(pos + block_face[i], data.block_id);
    }
}

int BlockRedstoneWire::max_current_strength(World *world, const Vec3i &pos, int other)
{
    if (world->get_block_id_at(pos) != data.block_id)
        return other;
    int meta = world->get_meta_at(pos);
    return meta > other ? meta : other;
}

bool BlockRedstoneWire::is_source_or_wire(World *world, const Vec3i &pos)
{
    BlockBase *block = block_at(world, pos);
    return (block->block_id() == BlockID::redstone_wire || block->is_power_source());
}