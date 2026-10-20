#include "entity_pathfinder.hpp"
#include <world/world.hpp>
#include <world/util/raycast.hpp>
#include <world/entity/pathfinding.hpp>

bool EntityPathfinder::simple_pathfind(Vec3f target)
{
    bool changed = false;
    if (path.size() > 0)
    {
        changed = (target - path[path.size() - 1]).sqr_magnitude() > 2;
        changed |= (get_position(0) - path[path.size() - 1]).sqr_magnitude() > 2;
    }
    changed |= path_index >= path.size();

    if (changed)
    {
        std::vector<Vec3i> new_path = find_path(world, this, target, 48, 3);
        if (new_path.size() > path.size() - path_index)
        {
            path = new_path;
            path_index = 1;
        }
    }
    return path_index < path.size();
}

void EntityPathfinder::tick()
{
    if (world->is_remote())
        return;

    if (ticks_existed % 20 == 0)
    {
        Vec3f target = Vec3f(std::floor(follow_entity->position.x), std::floor(follow_entity->aabb.min.y), std::floor(follow_entity->position.z));
        Vec3i target_i = Vec3i(target.x, target.y, target.z);
        target.y = std::min(target_i.y, checkbelow(target_i, nullptr, world) + 1);
        simple_pathfind(target);
    }

    if (path_index < path.size())
    {
        Vec3f off(0.5 * std::ceil(width), 0, 0.5 * std::ceil(width));
        Vec3i target = path[path_index];
        Vec3f target_f = Vec3f(target.x, target.y, target.z);
        if (path_index + 1 < path.size())
        {
            Vec3i second = path[path_index + 1];
            if (target.z != second.z && target.x != second.x)
                target_f = (Vec3f(target + second) * 0.5);
            target_f.y = target.y;
        }
        goal = off + target_f;
        Vec3f move = goal - get_position(0);
        if (move.sqr_magnitude() < width - 1e-4)
            path_index++;
        movement = Vec3f(move.x, 0, move.z).fast_normalize() * 0.5 + Vec3f(0, move.y > 0.25, 0);
    }
    else
    {
        movement = Vec3f(0, 0, 0);
    }
}
