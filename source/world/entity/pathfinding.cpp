#include <world/world.hpp>
#include <vector>
#include <queue>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <map>
#include <list>
#include <block/blocks.hpp>
#include <registry/block_list.hpp>

std::vector<Vec3i>
find_path(
    World *world,
    EntityPhysical *entity,
    Vec3i goal,
    int max_distance,
    int max_deviation /*unused */)
{

    const int entity_width = (int)std::ceil(entity->width);
    const int entity_height = (int)std::ceil(entity->height);

    auto is_solid = [&](const Vec3i &pos)
    {
        CollisionType coll = block_properties[world->get_block_id_at(pos)].m_collision;
        return coll != CollisionType::none && coll != CollisionType::fluid;
    };

    auto can_fit = [&](const Vec3i &pos) -> bool
    {
        for (int x = 0; x < entity_width; x++)
            for (int z = 0; z < entity_width; z++)
                for (int y = 0; y < entity_height; y++)
                {
                    if (is_solid(pos + Vec3i{x, y, z}))
                        return false;
                }
        return true;
    };

    auto can_reach = [&](const Vec3i &pos, const Vec3i &from) -> bool
    {
        if (!can_fit(pos))
            return false;

        Vec3i min = Vec3i{std::min(from.x, pos.x), std::min(from.y, pos.y), std::min(from.z, pos.z)};
        Vec3i max = Vec3i{std::max(from.x, pos.x), std::max(from.y, pos.y), std::max(from.z, pos.z)};

        for (int x = min.x; x <= max.x; x++)
            for (int z = min.z; z <= max.z; z++)
                for (int y = min.y; y <= max.y; y++)
                    if (!can_fit(Vec3i{x, y, z}))
                        return false;
        return true;
    };

    auto penalize = [&](const Vec3i &pos) -> float
    {
        // Always consider block below (or its neighbors)
        Vec3i a = pos;
        a.y--;

        BlockID current_id = world->get_block_id_at(a);
        switch (current_id)
        {
        case BlockID::fence:
        case BlockID::lava:
            return std::numeric_limits<float>::infinity();

        case BlockID::flowing_water:
        case BlockID::water:
            // Check if it's breach water
            return block_at(world, pos)->material_type() == Materials::AIR ? 4 : 8;
        case BlockID::fire:
            return 16;
        default:
            break;
        }
        const static std::vector<int> to_check = {FACE_NX, FACE_PX, FACE_NZ, FACE_PZ};
        for (int i : to_check)
        {
            switch (world->get_block_id_at(a + block_face[i]))
            {
            case BlockID::cactus:
            case BlockID::fire:
            case BlockID::lava:
            case BlockID::flowing_water:
            case BlockID::water:
                return 8;

            default:
                break;
            }
        }
        return 0;
    };

    auto heuristic = [&](const Vec3i &a, const Vec3i &b) -> float
    {
        int dx = std::abs(b.x - a.x);
        int dy = std::abs(b.y - a.y);
        int dz = std::abs(b.z - a.z);

        return float(std::max(dx, dz) + dy) + penalize(a);
    };

    auto goal_heuristic = [&](const Vec3i &a) -> float
    {
        return heuristic(a, goal);
    };

    Vec3i start{int(entity->aabb.min.x), int(entity->aabb.min.y), int(entity->aabb.min.z)};

    if (start == goal || !can_fit(start) || !can_fit(goal))
        return {};

    std::list<Vec3i> open_set;
    std::map<Vec3i, Vec3i> came_from;
    std::map<Vec3i, float> g_score;
    std::map<Vec3i, float> f_score;

    open_set.push_back(start);
    g_score[start] = 0;
    f_score[start] = goal_heuristic(start);

    auto filter = [&](const Vec3i &a, const Vec3i &b)
    {
        float ad = std::numeric_limits<float>::infinity();
        float bd = std::numeric_limits<float>::infinity();
        if (auto it = f_score.find(a); it != f_score.end())
            ad = it->second;
        if (auto it = f_score.find(b); it != f_score.end())
            bd = it->second;
        return ad < bd;
    };

    const static std::vector<Vec3i> valid_moves = {
        // Prioritize cardinals
        Vec3i{0, 0, -1},
        Vec3i{1, 0, 0},
        Vec3i{0, 0, 1},
        Vec3i{-1, 0, 0},

        // Cardinals while falling
        Vec3i{0, -1, -1},
        Vec3i{1, -1, 0},
        Vec3i{0, -1, 1},
        Vec3i{-1, -1, 0},

        // Diagonals
        Vec3i{-1, 0, -1},
        Vec3i{1, 0, -1},
        Vec3i{1, 0, 1},
        Vec3i{-1, 0, 1},

        // Cardinals while jumping
        Vec3i{0, 1, -1},
        Vec3i{1, 1, 0},
        Vec3i{0, 1, 1},
        Vec3i{-1, 1, 0},

    };

    int visited = 0;
    while (!open_set.empty())
    {
        visited++;
        if (visited == 64)
            return {};
        auto found = std::min_element(open_set.begin(), open_set.end(), filter);
        Vec3i current = *found;
        if (current == goal)
        {
            std::vector<Vec3i> path;
            auto it = came_from.find(current);
            while (it != came_from.end())
            {
                current = it->second;
                path.push_back(current);

                it = came_from.find(current);
            }

            std::reverse(path.begin(), path.end());
            return path;
        }

        open_set.erase(found);

        for (const Vec3i &relative : valid_moves)
        {
            Vec3i neighbor = current + relative;
            if (!can_reach(neighbor, current))
                continue;

            float tentative_g = std::numeric_limits<float>::infinity();
            if (auto it = g_score.find(current); it != g_score.end())
                tentative_g = it->second + heuristic(neighbor, current);

            if (tentative_g > max_distance)
                continue;

            float dist_neighbor = std::numeric_limits<float>::infinity();
            if (auto it = g_score.find(neighbor); it != g_score.end())
                dist_neighbor = it->second;
            if (tentative_g < dist_neighbor)
            {
                came_from[neighbor] = current;
                g_score[neighbor] = tentative_g;
                f_score[neighbor] = tentative_g + goal_heuristic(neighbor);
                if (std::find(open_set.begin(), open_set.end(), neighbor) == open_set.end())
                {
                    open_set.push_back(neighbor);
                }
            }
        }
    }

    return {};
}