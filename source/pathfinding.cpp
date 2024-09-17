#include "pathfinding.hpp"

void pathfinding_t::reconstruct_path(vec3i start, vec3i goal, std::vector<vec3i> &path)
{
    vec3i current = goal;
    path.push_back(current);
    while (current != start)
    {
        current = came_from[current];
        path.push_back(current);
    }
}

std::vector<vec3i> pathfinding_t::a_star_search(vec3i start, vec3i goal, chunk_t *chunk)
{
    init();
    node_t start_node = {start, start, 0, heuristic(start, goal), heuristic(start, goal)};
    frontier.push(start_node);
    came_from[start] = start;
    cost_so_far[start] = 0;

    vec3i neighbors[] = {
        vec3i{0, 0, 1},
        vec3i{0, 0, -1},
        vec3i{1, 0, 0},
        vec3i{-1, 0, 0},
        vec3i{0, -1, 0},
        vec3i{0, 1, 0},
    };

    while (!frontier.empty())
    {
        node_t current = frontier.top();
        frontier.pop();

        if (current.pos == goal)
        {
            std::vector<vec3i> path;
            reconstruct_path(start, goal, path);
            return path;
        }

        for (vec3i &new_pos : neighbors)
        {

            float new_cost = cost_so_far[current.pos] + 1;
            if (new_cost > 64)
                continue;

            vec3i next = current.pos + new_pos;
            if (next.y - current.parent.y > 1)
                continue;
            if (next == current.parent)
                continue;
            if ((current.pos.x & ~0xF) != (next.z & ~0xF) || (current.pos.z & ~0xF) != (next.z & ~0xF))
                chunk = get_chunk_from_pos(next, false, false);
            block_t *block = get_block_at(next, chunk);
            if (!block || properties(block->id).m_collision == CollisionType::solid)
                continue;
            block = get_block_at(next + vec3i(0, 1, 0), chunk);
            if (!block || properties(block->id).m_collision == CollisionType::solid)
                continue;
            if (new_pos.y == 1)
            {
                // Check if the block below is solid
                block = get_block_at(current.pos - vec3i(0, 1, 0), chunk);
                if (!block || properties(block->id).m_collision != CollisionType::solid)
                    continue;
            }
            if (next.y == current.pos.y && next.y == current.parent.y && current.parent != current.pos)
            {
                // Check if the block below the next node is solid
                int solid_count = 3;
                block = get_block_at(next - vec3i(0, 1, 0), chunk);
                if (!block || properties(block->id).m_collision != CollisionType::solid)
                    solid_count--;

                // Check if the block below the current node is solid
                block = get_block_at(current.pos - vec3i(0, 1, 0), chunk);
                if (!block || properties(block->id).m_collision != CollisionType::solid)
                    solid_count--;

                // Check if the block below the previous node is solid
                block = get_block_at(current.parent - vec3i(0, 1, 0), chunk);
                if (!block || properties(block->id).m_collision != CollisionType::solid)
                    solid_count--;
                if (solid_count < 1)
                    continue;
            }
            if (!cost_so_far.count(next) || new_cost < cost_so_far[next])
            {
                cost_so_far[next] = new_cost;
                float priority = new_cost + heuristic(next, goal);
                node_t next_node = {next, current.pos, new_cost, heuristic(next, goal), priority};
                frontier.push(next_node);
                came_from[next] = current.pos;
            }
        }
    }

    return std::vector<vec3i>();
}

vec3f pathfinding_t::simple_pathfind(vec3f start, vec3f goal, chunk_t *chunk)
{
    vec3i start_i = vec3i(std::floor(start.x), std::floor(start.y), std::floor(start.z));
    vec3i goal_i = vec3i(std::floor(goal.x), std::floor(goal.y), std::floor(goal.z));
    std::vector<vec3i> path = a_star_search(start_i, goal_i, chunk);
    if (path.size() > 1)
    {
        vec3i next = path[1];
        return vec3f(next.x, next.y, next.z);
    }
    return vec3f(0, -1, 0);
}

vec3f pathfinding_t::straighten_path(vec3f start, vec3f goal, chunk_t *chunk)
{
    vec3i start_i = vec3i(std::floor(start.x), std::floor(start.y), std::floor(start.z));
    vec3i goal_i = vec3i(std::floor(goal.x), std::floor(goal.y), std::floor(goal.z));
    std::vector<vec3i> path = a_star_search(start_i, goal_i, chunk);

    vec3f direction(0, 0, 0);

    if (path.size() > 1)
    {
        direction = vec3f(path[1].x + 0.5 - start.x, path[1].y - start.y, path[1].z + 0.5 - start.z);
    }
    if (path.size() > 2)
    {
        direction = direction + vec3f(path[2].x + 0.5 - start.x, path[2].y - start.y, path[2].z + 0.5 - start.z);
    }
    if (direction.sqr_magnitude() > 1)
        direction = direction.normalize();
    return direction;
}

vec3f pathfinding_t::pathfind_direction(vec3f start, vec3f goal, chunk_t *chunk)
{
    return straighten_path(start, goal, chunk);
}