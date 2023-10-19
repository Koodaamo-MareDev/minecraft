#include "AxisAlignedBB.hpp"
#include "vec3d.hpp"

#include <vector>

namespace AABB
{

    std::vector<AxisAlignedBB> bounding_boxes;

    AxisAlignedBB getBoundingBox(double minX, double minY, double minZ, double maxX, double maxY, double maxZ)
    {
        return {
            minX,
            minY,
            minZ,
            maxX,
            maxY,
            maxZ,
        };
    }
    AxisAlignedBB &getBoundingBoxFromPool(double minX, double minY, double minZ, double maxX, double maxY, double maxZ)
    {
        bounding_boxes.push_back({
            minX,
            minY,
            minZ,
            maxX,
            maxY,
            maxZ,
        });
        return bounding_boxes[bounding_boxes.size() - 1];
    }
    void clearBoundingBoxPool()
    {
        bounding_boxes.clear();
    }
    AxisAlignedBB &setBounds(AxisAlignedBB &bounding_box, double minX, double minY, double minZ, double maxX, double maxY, double maxZ)
    {
        bounding_box.minX = minX;
        bounding_box.minY = minY;
        bounding_box.minZ = minZ;
        bounding_box.maxX = maxX;
        bounding_box.maxY = maxY;
        bounding_box.maxZ = maxZ;
        return bounding_box;
    }

    double calculateXOffset(AxisAlignedBB aabb1, AxisAlignedBB aabb2, double var2)
    {
        if (aabb1.maxY > aabb2.minY && aabb1.minY < aabb2.maxY)
        {
            if (aabb1.maxZ > aabb2.minZ && aabb1.minZ < aabb2.maxZ)
            {
                double var4;
                if (var2 > 0.0 && aabb1.maxX <= aabb2.minX)
                {
                    var4 = aabb2.minX - aabb1.maxX;
                    if (var4 < var2)
                    {
                        var2 = var4;
                    }
                }

                if (var2 < 0.0 && aabb1.minX >= aabb2.maxX)
                {
                    var4 = aabb2.maxX - aabb1.minX;
                    if (var4 > var2)
                    {
                        var2 = var4;
                    }
                }

                return var2;
            }
            else
            {
                return var2;
            }
        }
        else
        {
            return var2;
        }
    }

    double calculateYOffset(AxisAlignedBB aabb1, AxisAlignedBB aabb2, double var2)
    {
        if (aabb1.maxX > aabb2.minX && aabb1.minX < aabb2.maxX)
        {
            if (aabb1.maxZ > aabb2.minZ && aabb1.minZ < aabb2.maxZ)
            {
                double var4;
                if (var2 > 0.0 && aabb1.maxY <= aabb2.minY)
                {
                    var4 = aabb2.minY - aabb1.maxY;
                    if (var4 < var2)
                    {
                        var2 = var4;
                    }
                }

                if (var2 < 0.0 && aabb1.minY >= aabb2.maxY)
                {
                    var4 = aabb2.maxY - aabb1.minY;
                    if (var4 > var2)
                    {
                        var2 = var4;
                    }
                }

                return var2;
            }
            else
            {
                return var2;
            }
        }
        else
        {
            return var2;
        }
    }

    double calculateZOffset(AxisAlignedBB aabb1, AxisAlignedBB aabb2, double var2)
    {
        if (aabb1.maxX > aabb2.minX && aabb1.minX < aabb2.maxX)
        {
            if (aabb1.maxY > aabb2.minY && aabb1.minY < aabb2.maxY)
            {
                double var4;
                if (var2 > 0.0 && aabb1.maxZ <= aabb2.minZ)
                {
                    var4 = aabb2.minZ - aabb1.maxZ;
                    if (var4 < var2)
                    {
                        var2 = var4;
                    }
                }

                if (var2 < 0.0 && aabb1.minZ >= aabb2.maxZ)
                {
                    var4 = aabb2.maxZ - aabb1.minZ;
                    if (var4 > var2)
                    {
                        var2 = var4;
                    }
                }

                return var2;
            }
            else
            {
                return var2;
            }
        }
        else
        {
            return var2;
        }
    }

    bool intersectsWith(AxisAlignedBB aabb1, AxisAlignedBB aabb2)
    {
        return aabb1.maxX > aabb2.minX && aabb1.minX < aabb2.maxX ? (aabb1.maxY > aabb2.minY && aabb1.minY < aabb2.maxY ? aabb1.maxZ > aabb2.minZ && aabb1.minZ < aabb2.maxZ : false) : false;
    }
    AxisAlignedBB &offset(AxisAlignedBB &bounding_box, double x, double y, double z)
    {
        bounding_box.minX += x;
        bounding_box.minY += y;
        bounding_box.minZ += z;
        bounding_box.maxX += x;
        bounding_box.maxY += y;
        bounding_box.maxZ += z;
        return bounding_box;
    }
    double getAverageEdgeLength(AxisAlignedBB &bounding_box)
    {
        double x = bounding_box.maxX - bounding_box.minX;
        double y = bounding_box.maxY - bounding_box.minY;
        double z = bounding_box.maxZ - bounding_box.minZ;
        return (x + y + z) / 3.0;
    }
    AxisAlignedBB copy(AxisAlignedBB &bounding_box)
    {
        return getBoundingBoxFromPool(bounding_box.minX, bounding_box.minY, bounding_box.minZ, bounding_box.maxX, bounding_box.maxY, bounding_box.maxZ);
    }
    bool isVecInYZ(AxisAlignedBB &bounding_box, Vec3D var1)
    {
        return var1.yCoord >= bounding_box.minY && var1.yCoord <= bounding_box.maxY && var1.zCoord >= bounding_box.minZ && var1.zCoord <= bounding_box.maxZ;
    }

    bool isVecInXZ(AxisAlignedBB &bounding_box, Vec3D var1)
    {
        return var1.xCoord >= bounding_box.minX && var1.xCoord <= bounding_box.maxX && var1.zCoord >= bounding_box.minZ && var1.zCoord <= bounding_box.maxZ;
    }

    bool isVecInXY(AxisAlignedBB &bounding_box, Vec3D var1)
    {
        return var1.xCoord >= bounding_box.minX && var1.xCoord <= bounding_box.maxX && var1.yCoord >= bounding_box.minY && var1.yCoord <= bounding_box.maxY;
    }

    void setBB(AxisAlignedBB &to, AxisAlignedBB &from)
    {
        to.minX = from.minX;
        to.minY = from.minY;
        to.minZ = from.minZ;
        to.maxX = from.maxX;
        to.maxY = from.maxY;
        to.maxZ = from.maxZ;
    }
}