#ifndef PATHFINDING_HPP
#define PATHFINDING_HPP

#include <map>
#include <queue>
#include <deque>
#include <math/vec3f.hpp>
#include <math/aabb.hpp>
#include <math/math_utils.h>

#include <world/chunk.hpp>

class PathFinding
{
public:
    struct Node
    {
        Vec3i pos;
        Vec3i parent;
        int g;
        int f;
        bool in_air = false;
        bool horizontal_in_air = false;
        bool operator<(const Node &other) const
        {
            return f > other.f;
        }
    };

    std::map<Vec3i, Vec3i> came_from;
    std::map<Vec3i, int> cost_so_far;
    std::priority_queue<Node> frontier;
    World *current_world = nullptr;

    PathFinding() {}

    void init()
    {
        came_from.clear();
        cost_so_far.clear();
        frontier = std::priority_queue<Node>();
    }

    void reconstruct_path(Vec3i start, Vec3i goal, std::deque<Vec3i> &path);

    int heuristic(Vec3i a, Vec3i b)
    {
        return (std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z));
    }

    bool a_star_search(Vec3i start, Vec3i goal, std::deque<Vec3i> &path);

    Vec3f simple_pathfind(Vec3f start, Vec3f goal, std::deque<Vec3i> &path);
};
#endif