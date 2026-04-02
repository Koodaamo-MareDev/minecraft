#ifndef PATHFINDING_HPP
#define PATHFINDING_HPP

#include <vector>
#include <math/vec3f.hpp>

class World;
class EntityPhysical;

std::vector<Vec3i> find_path(
    World *world,
    EntityPhysical *entity,
    Vec3i goal,
    int max_distance,
    int max_deviation);
#endif