#ifndef PATHFINDING_HPP
#define PATHFINDING_HPP

#include "vec3f.hpp"
#include "aabb.hpp"
#include "chunk_new.hpp"
#include "maths.hpp"

#include <map>
#include <queue>

class pathfinding_t
{
public:
    struct node_t
    {
        vec3i pos;
        vec3i parent;
        float g;
        float h;
        float f;
        bool operator<(const node_t &other) const
        {
            return f > other.f;
        }
    };

    std::map<vec3i, vec3i> came_from;
    std::map<vec3i, float> cost_so_far;
    std::priority_queue<node_t> frontier;

    pathfinding_t() {}

    void init()
    {
        came_from.clear();
        cost_so_far.clear();
        frontier = std::priority_queue<node_t>();
    }

    void reconstruct_path(vec3i start, vec3i goal, std::vector<vec3i> &path);

    float heuristic(vec3i a, vec3i b)
    {
        return (a - b).sqr_magnitude();
    }

    std::vector<vec3i> a_star_search(vec3i start, vec3i goal, chunk_t *chunk);

    vec3f simple_pathfind(vec3f start, vec3f goal, chunk_t *chunk);

    vec3f straighten_path(vec3f start, vec3f goal, chunk_t *chunk);

    vec3f pathfind_direction(vec3f start, vec3f goal, chunk_t *chunk);
};
#endif