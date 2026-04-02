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

static const Vec3i v_off[] = {
    Vec3i{-1, +0, +0},  // Negative X
    Vec3i{+1, +0, +0},  // Positive X
    Vec3i{+0, +0, -1},  // Negative Z
    Vec3i{+0, +0, +1},  // Positive Z
    Vec3i{+0, -1, +0},  // Negative Y
    Vec3i{+0, +1, +0}}; // Positive Y

std::vector<Vec3i>
find_path(
    World *world,
    EntityPhysical *entity,
    Vec3i goal,
    int max_distance,
    int max_deviation)
{

    const int entity_width = (int)std::ceil(entity->width);
    const int entity_height = (int)std::ceil(entity->height);

    auto is_water = [&](const Vec3i &pos)
    {
        return block_properties[world->get_block_id_at(pos)].m_material == Materials::WATER;
    };

    auto is_solid = [&](const Vec3i &pos)
    {
        CollisionType coll = block_properties[world->get_block_id_at(pos)].m_collision;
        return coll != CollisionType::none && coll != CollisionType::fluid;
    };

    auto valid_pos = [&](const Vec3i &pos) -> bool
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

    auto has_ground = [&](const Vec3i &pos) -> bool
    {
        for (int x = 0; x < entity_width; x++)
            for (int z = 0; z < entity_width; z++)
            {
                if (is_solid(pos + Vec3i{x, -1, z}) || is_water(pos + Vec3i{x, -1, z}))
                    return true;
            }
        return false;
    };

    constexpr int max_jump_height = 1;

    auto can_reach = [&](const Vec3i &pos, const Vec3i &from) -> bool
    {
        if (!valid_pos(pos))
            return false;

        int rel_y = pos.y - from.y;

        if (rel_y == 0)
            return has_ground(pos);

        if (rel_y > 0)
        {
            for (int y = 0; y > -max_jump_height; y--)
                if (has_ground(pos + Vec3i{0, y, 0}) && valid_pos(from + Vec3i{0, -y, 0}))
                    return true;
        }
        else
        {
            for (int y = 0; y > -3; y--)
                if (has_ground(pos + Vec3i{0, y, 0}) && valid_pos(from + Vec3i{0, -y, 0}))
                    return true;
        }
        return false;
    };

    Vec3i start{int(entity->aabb.min.x), int(entity->aabb.min.y), int(entity->aabb.min.z)};

    if (start == goal || !valid_pos(start))
        return {};
    std::map<Vec3i, int> dist;
    std::map<Vec3i, Vec3i> prev;

    Vec3i v_min = start;
    Vec3i v_max = goal;
    if (v_min.x > v_max.x)
        std::swap(v_min.x, v_max.x);
    if (v_min.y > v_max.y)
        std::swap(v_min.y, v_max.y);
    if (v_min.z > v_max.z)
        std::swap(v_min.z, v_max.z);
    v_max += Vec3i{max_deviation, max_deviation, max_deviation};
    v_min -= Vec3i{max_deviation, max_deviation, max_deviation};

    using Node = std::pair<int, Vec3i>; // (distance, position)
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;
    std::vector<int> has_ground_map;
    has_ground_map.resize((v_max.x - v_min.x + 1) * (v_max.z - v_min.z + 1));

    for (int x = v_min.x; x <= v_max.x; x++)
        for (int z = v_min.z; z <= v_max.z; z++)
            for (int y = v_min.y; y <= v_max.y; y++)
            {
                Vec3i v{x, y, z};
                if (valid_pos(v))
                {
                    dist[v] = 1000000;
                    prev[v] = start;
                    pq.push({1000000, v});
                }
            }

    dist[start] = 0;
    pq.push({0, start});

    while (!pq.empty())
    {
        auto [d, u] = pq.top();
        pq.pop();

        if (d > dist[u])
            continue;

        if (u == goal)
        {
            std::vector<Vec3i> path;
            Vec3i cur = goal;

            while (cur != start)
            {
                path.push_back(cur);
                cur = prev[cur];
            }

            std::reverse(path.begin(), path.end());
            return path;
        }
        for (int x = -1; x <= 1; x++)
            for (int z = -1; z <= 1; z++)
                for (int y = -1; y <= 1; y++)
                {
                    if (x == 0 && y == 0 && z == 0)
                        continue;
                    if (x != 0 && y != 0 && z != 0)
                        continue;
                    int alt = d + (std::abs(x) + std::abs(y) + std::abs(z)) * 3 / 2;
                    if (alt > max_distance)
                        continue;
                    Vec3i v = u + Vec3i{x, y, z};

                    if (!can_reach(v, u))
                        continue;

                    if (alt < dist[v])
                    {
                        dist[v] = alt;
                        prev[v] = u;
                        pq.push({alt, v});
                    }
                }
    }

    return {};
}