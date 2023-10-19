#include "Vec3D.hpp"
#include <cmath>
Vec3D createVector(double x, double y, double z)
{
    return {x, y, z};
}

Vec3D setComponents(Vec3D &vec, double var1, double var3, double var5)
{
    vec.xCoord = var1;
    vec.yCoord = var3;
    vec.zCoord = var5;
    return vec;
}

Vec3D subtract(Vec3D &vec, Vec3D var1)
{
    return createVector(var1.xCoord - vec.xCoord, var1.yCoord - vec.yCoord, var1.zCoord - vec.zCoord);
}

Vec3D normalize(Vec3D &vec)
{
    double var1 = (double)sqrt(vec.xCoord * vec.xCoord + vec.yCoord * vec.yCoord + vec.zCoord * vec.zCoord);
    return var1 < 1.0E-4 ? createVector(0.0, 0.0, 0.0) : createVector(vec.xCoord / var1, vec.yCoord / var1, vec.zCoord / var1);
}

Vec3D crossProduct(Vec3D &vec, Vec3D var1)
{
    return createVector(vec.yCoord * var1.zCoord - vec.zCoord * var1.yCoord, vec.zCoord * var1.xCoord - vec.xCoord * var1.zCoord, vec.xCoord * var1.yCoord - vec.yCoord * var1.xCoord);
}

Vec3D addVector(Vec3D &vec, double var1, double var3, double var5)
{
    return createVector(vec.xCoord + var1, vec.yCoord + var3, vec.zCoord + var5);
}

double distanceTo(Vec3D &vec, Vec3D var1)
{
    double var2 = var1.xCoord - vec.xCoord;
    double var4 = var1.yCoord - vec.yCoord;
    double var6 = var1.zCoord - vec.zCoord;
    return (double)sqrt(var2 * var2 + var4 * var4 + var6 * var6);
}

double squareDistanceTo(Vec3D &vec, Vec3D var1)
{
    double var2 = var1.xCoord - vec.xCoord;
    double var4 = var1.yCoord - vec.yCoord;
    double var6 = var1.zCoord - vec.zCoord;
    return var2 * var2 + var4 * var4 + var6 * var6;
}

double squareDistanceTo(Vec3D &vec, double var1, double var3, double var5)
{
    double var7 = var1 - vec.xCoord;
    double var9 = var3 - vec.yCoord;
    double var11 = var5 - vec.zCoord;
    return var7 * var7 + var9 * var9 + var11 * var11;
}

double lengthVector(Vec3D &vec)
{
    return (double)sqrt(vec.xCoord * vec.xCoord + vec.yCoord * vec.yCoord + vec.zCoord * vec.zCoord);
}

Vec3D getIntermediateWithXValue(Vec3D &vec, Vec3D var1, double var2)
{
    double var4 = var1.xCoord - vec.xCoord;
    double var6 = var1.yCoord - vec.yCoord;
    double var8 = var1.zCoord - vec.zCoord;
    if (var4 * var4 < 1.0000000116860974E-7)
    {
        return {0, 0, 0};
    }
    else
    {
        double var10 = (var2 - vec.xCoord) / var4;
        return var10 >= 0.0 && var10 <= 1.0 ? createVector(vec.xCoord + var4 * var10, vec.yCoord + var6 * var10, vec.zCoord + var8 * var10) : (Vec3D {0, 0, 0});
    }
}

Vec3D getIntermediateWithYValue(Vec3D &vec, Vec3D var1, double var2)
{
    double var4 = var1.xCoord - vec.xCoord;
    double var6 = var1.yCoord - vec.yCoord;
    double var8 = var1.zCoord - vec.zCoord;
    if (var6 * var6 < 1.0000000116860974E-7)
    {
        return {0, 0, 0};
    }
    else
    {
        double var10 = (var2 - vec.yCoord) / var6;
        return var10 >= 0.0 && var10 <= 1.0 ? createVector(vec.xCoord + var4 * var10, vec.yCoord + var6 * var10, vec.zCoord + var8 * var10) : (Vec3D {0, 0, 0});
    }
}

Vec3D getIntermediateWithZValue(Vec3D &vec, Vec3D var1, double var2)
{
    double var4 = var1.xCoord - vec.xCoord;
    double var6 = var1.yCoord - vec.yCoord;
    double var8 = var1.zCoord - vec.zCoord;
    if (var8 * var8 < 1.0000000116860974E-7)
    {
        return {0, 0, 0};
    }
    else
    {
        double var10 = (var2 - vec.zCoord) / var8;
        return var10 >= 0.0 && var10 <= 1.0 ? createVector(vec.xCoord + var4 * var10, vec.yCoord + var6 * var10, vec.zCoord + var8 * var10) : (Vec3D {0, 0, 0});
    }
}

void rotateAroundX(Vec3D &vec, float var1)
{
    float var2 = cos(var1);
    float var3 = sin(var1);
    double var4 = vec.xCoord;
    double var6 = vec.yCoord * (double)var2 + vec.zCoord * (double)var3;
    double var8 = vec.zCoord * (double)var2 - vec.yCoord * (double)var3;
    vec.xCoord = var4;
    vec.yCoord = var6;
    vec.zCoord = var8;
}

void rotateAroundY(Vec3D &vec, float var1)
{
    float var2 = cos(var1);
    float var3 = sin(var1);
    double var4 = vec.xCoord * (double)var2 + vec.zCoord * (double)var3;
    double var6 = vec.yCoord;
    double var8 = vec.zCoord * (double)var2 - vec.xCoord * (double)var3;
    vec.xCoord = var4;
    vec.yCoord = var6;
    vec.zCoord = var8;
}