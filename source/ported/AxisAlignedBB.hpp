#ifndef _AABB_H_
#define _AABB_H_
#include "Vec3D.hpp"
struct AxisAlignedBB {
    double minX = 0;
    double minY = 0;
    double minZ = 0;
    double maxX = 0;
    double maxY = 0;
    double maxZ = 0;
};
namespace AABB {
    AxisAlignedBB &getBoundingBoxFromPool(double minX, double minY, double minZ, double maxX, double maxY, double maxZ);
    void clearBoundingBoxPool();
    AxisAlignedBB &setBounds(AxisAlignedBB &bounding_box, double minX, double minY, double minZ, double maxX, double maxY, double maxZ);
    double calculateXOffset(AxisAlignedBB aabb1, AxisAlignedBB aabb2, double var2);
    double calculateYOffset(AxisAlignedBB aabb1, AxisAlignedBB aabb2, double var2);
    double calculateZOffset(AxisAlignedBB aabb1, AxisAlignedBB aabb2, double var2);
    bool intersectsWith(AxisAlignedBB aabb1, AxisAlignedBB aabb2);
    AxisAlignedBB &offset(AxisAlignedBB &bounding_box, double x, double y, double z);
    double getAverageEdgeLength(AxisAlignedBB &bounding_box);
    AxisAlignedBB copy(AxisAlignedBB &bounding_box);
    bool isVecInYZ(AxisAlignedBB &bounding_box, Vec3D var1);
    bool isVecInXZ(AxisAlignedBB &bounding_box, Vec3D var1);
    bool isVecInXY(AxisAlignedBB &bounding_box, Vec3D var1);
    void setBB(AxisAlignedBB &to, AxisAlignedBB &from);
}
#endif