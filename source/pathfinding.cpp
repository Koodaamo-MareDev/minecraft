#include "pathfinding.hpp"
#include "raycast.hpp"
#include "timers.hpp"

void PathFinding::reconstruct_path(Vec3i start, Vec3i goal, std::deque<Vec3i> &path)
{
    Vec3i current = goal;
    path.push_back(current);
    while (current != start)
    {
        current = came_from[current];
        path.push_back(current);
    }
}

bool PathFinding::a_star_search(Vec3i start, Vec3i goal, std::deque<Vec3i> &path)
{
    init();
    Node start_node = {start, start, 0, heuristic(start, goal)};
    frontier.push(start_node);
    came_from[start] = start;
    cost_so_far[start] = 0;

    Vec3i neighbors[] = {
        Vec3i{0, 0, 1},
        Vec3i{0, 0, -1},
        Vec3i{1, 0, 0},
        Vec3i{-1, 0, 0},
        Vec3i{0, -1, 0},
        Vec3i{0, 1, 0},
    };
    auto is_valid = [](Vec3i position, Chunk *chunk) -> bool
    {
        Block *block = get_block_at(position, nullptr);
        Block *block_above = get_block_at(position + Vec3i(0, 1, 0), nullptr);
        int y_below = checkbelow(position, nullptr);
        if (position.y - y_below >= 10)
            return false;
        return block && block_above &&
               (properties(block->id).m_collision == CollisionType::none || properties(block->id).m_collision == CollisionType::fluid) &&
               (properties(block_above->id).m_collision == CollisionType::none || properties(block_above->id).m_collision == CollisionType::fluid);
    };
    uint64_t start_time = time_get();
    while (!frontier.empty())
    {
        if (time_diff_us(start_time, time_get()) > 3000)
            break;
        Node current = frontier.top();
        frontier.pop();

        if (current.pos == goal)
        {
            path.clear();
            reconstruct_path(start, goal, path);
            return true;
        }
        int new_cost = cost_so_far[current.pos] + 1;
        if (new_cost > 64)
            break;
        for (Vec3i &new_pos : neighbors)
        {
            Vec3i next = current.pos + new_pos;
            if (!cost_so_far.count(next) || new_cost < cost_so_far[next])
            {
                if (!is_valid(next, nullptr))
                    continue;
                Block *block = get_block_at(next - Vec3i(0, 1, 0));
                bool next_in_air = !block || properties(block->id).m_collision == CollisionType::none;
                bool next_horizontal_in_air = false;

                if (current.in_air)
                {
                    if (new_pos.y == 0 && current.horizontal_in_air)
                        continue;
                    if (new_pos.y <= 0)
                        next_horizontal_in_air = next_in_air;
                }
                if (new_pos.y == 1 && current.in_air)
                    continue;

                cost_so_far[next] = new_cost;
                int priority = new_cost + heuristic(next, goal);
                Node next_node = {next, current.pos, new_cost, priority, next_in_air, next_horizontal_in_air};
                frontier.push(next_node);
                came_from[next] = current.pos;
            }
        }
    }
    return false;
}

Vec3f PathFinding::simple_pathfind(Vec3f start, Vec3f goal, std::deque<Vec3i> &path)
{
    Vec3i start_i = Vec3i(std::floor(start.x), std::floor(start.y), std::floor(start.z));
    Vec3i goal_i = Vec3i(std::floor(goal.x), std::floor(goal.y), std::floor(goal.z));

    bool path_valid = true;

    auto is_valid = [](Vec3i position) -> bool
    {
        Block *block = get_block_at(position, nullptr);
        Block *block_above = get_block_at(position + Vec3i(0, 1, 0), nullptr);
        int y_below = checkbelow(position, nullptr);
        if (position.y - y_below >= 10)
            return false;
        return block && block_above &&
               (properties(block->id).m_collision == CollisionType::none || properties(block->id).m_collision == CollisionType::fluid) &&
               (properties(block_above->id).m_collision == CollisionType::none || properties(block_above->id).m_collision == CollisionType::fluid);
    };

    std::deque<Vec3i>::iterator it = std::find_if(path.begin(), path.end(), [start_i, this](Vec3i pos)
                                                  { return heuristic(pos, start_i) < 1; });

    if (it != path.end())
        path.erase(path.begin(), it);
    else
        path.clear();

    if (path.size())
    {
        if (path[path.size() - 1] != goal_i)
            path_valid = false;
        else
        {
            for (size_t i = 0; i < path.size() - 1; i++)
            {
                if (!is_valid(path[i]))
                {
                    path_valid = false;
                }
            }
        }
    }
    else
        path_valid = false;
    if (!path_valid)
    {
        path_valid = a_star_search(start_i, goal_i, path);
        std::reverse(path.begin(), path.end());
    }
    Vec3f dir = Vec3f(0, 0, 0);
    if (path.size() > 1)
        dir = (path[1] + Vec3f(0.5, 0, 0.5) - start);
    if(path.size() > 2 && path[1].y != start_i.y)
        dir = dir + (path[2] + Vec3f(0.5, 0, 0.5) - start);
    return dir.fast_normalize();
}