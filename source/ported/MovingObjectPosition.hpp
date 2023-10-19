#ifndef _MOVING_OBJECT_POSITION_HPP_
#define _MOVING_OBJECT_POSITION_HPP_

#include "Vec3D.hpp"
#include "Entity.hpp"

struct MovingObjectPosition
{
    int typeOfHit;
    int blockX;
    int blockY;
    int blockZ;
    int sideHit;
    Vec3D hitVec;
    Entity *entityHit;

    MovingObjectPosition(int x, int y, int z, int side, Vec3D hitVec)
    {
        typeOfHit = 0;
        blockX = x;
        blockY = y;
        blockZ = z;
        sideHit = side;
        hitVec = Vec3D{hitVec.xCoord, hitVec.yCoord, hitVec.zCoord};
    }

    MovingObjectPosition(Entity *entity)
    {
        if (!entity)
            return;
        typeOfHit = 1;
        entityHit = entity;
        hitVec = {entity->posX, entity->posY, entity->posZ};
    }
};

#endif