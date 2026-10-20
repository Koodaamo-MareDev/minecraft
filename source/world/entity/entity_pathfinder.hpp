#pragma once
#include <world/entity/entity_physical.hpp>

class EntityPathfinder : virtual public EntityPhysical
{
public:
    EntityPathfinder() {}
    EntityPhysical *follow_entity = nullptr;
    size_t path_index = 0;
    std::vector<Vec3i> path;
    Vec3f goal = Vec3f(0, -1, 0);
    bool simple_pathfind(Vec3f target);

    virtual void tick();
};
