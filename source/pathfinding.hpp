#ifndef PATHFINDING_HPP
#define PATHFINDING_HPP

#include <map>
#include <queue>
#include <deque>
#include <math/vec3f.hpp>
#include <math/aabb.hpp>
#include <math/math_utils.h>

#include "chunk.hpp"

class pathfinding_t
{
public:
    struct node_t
    {
        vec3i pos;
        vec3i parent;
        int g;
        int f;
        bool in_air = false;
        bool horizontal_in_air = false;
        bool operator<(const node_t &other) const
        {
            return f > other.f;
        }
    };

    std::map<vec3i, vec3i> came_from;
    std::map<vec3i, int> cost_so_far;
    std::priority_queue<node_t> frontier;

    pathfinding_t() {}

    void init()
    {
        came_from.clear();
        cost_so_far.clear();
        frontier = std::priority_queue<node_t>();
    }

    void reconstruct_path(vec3i start, vec3i goal, std::deque<vec3i> &path);

    int heuristic(vec3i a, vec3i b)
    {
        return (std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z));
    }

    bool a_star_search(vec3i start, vec3i goal, std::deque<vec3i> &path);

    vec3f simple_pathfind(vec3f start, vec3f goal, std::deque<vec3i> &path);
};
#endif