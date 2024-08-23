#ifndef MAPGENCAVES_H
#define MAPGENCAVES_H

#include "../chunk_new.hpp"
#include "JavaRandom.hpp"
#include <cstdint>
#include <cmath>

class MapGenCaves
{
public:
    MapGenCaves() {}
    const static int32_t max_off = 8;

    inline static void gen_node_simple(int32_t offX, int32_t offZ, BlockID *out_ids, double x, double y, double z)
    {
        gen_node_main(offX, offZ, out_ids, x, y, z, 1.0F + JavaLCGFloat() * 6.F, 0.0F, 0.0F, -1, -1, 0.5);
    }

    inline static void gen_node_main(int32_t chunkX, int32_t chunkZ, BlockID *out_ids, double x, double y, double z, float angleX, float angleY, float angleZ, int32_t offX, int32_t offZ, double radius)
    {
        double chunkCenterX = chunkX * 16 + 8;
        double chunkCenterZ = chunkZ * 16 + 8;
        float deltaX = 0.0F;
        float deltaZ = 0.0F;
        int64_t random_state = JavaLCGGetState();
        JavaLCGInit(JavaLCG());

        if (offZ <= 0)
        {
            int32_t branchCount = max_off * 16 - 16;

            offZ = branchCount - JavaLCGIntN(branchCount / 4);
        }
        bool flag = false;

        if (offX == -1)
        {
            offX = offZ / 2;
            flag = true;
        }
        int32_t branch = JavaLCGIntN(offZ / 2) + offZ / 4;
        bool flag1 = JavaLCGIntN(6) == 0;

        for (; offX < offZ; offX++)
        {
            double horizontalScale = 1.5 + (double)(std::sin(((float)offX * 3.141593F) / (float)offZ) * angleX * 1.0F);
            double size = horizontalScale * radius;
            float cosAngleZ = std::cos(angleZ);
            float sinAngleZ = std::sin(angleZ);

            x += std::cos(angleY) * cosAngleZ;
            y += sinAngleZ;
            z += std::sin(angleY) * cosAngleZ;
            if (flag1)
            {
                angleZ *= 0.92F;
            }
            else
            {
                angleZ *= 0.7F;
            }
            angleZ += deltaZ * 0.1F;
            angleY += deltaX * 0.1F;
            deltaZ *= 0.9F;
            deltaX *= 0.75F;
            deltaZ += (JavaLCGFloat() - JavaLCGFloat()) * JavaLCGFloat() * 2.0F;
            deltaX += (JavaLCGFloat() - JavaLCGFloat()) * JavaLCGFloat() * 4.F;
            if (!flag && offX == branch && angleX > 1.0F)
            {
                gen_node_main(chunkX, chunkZ, out_ids, x, y, z, JavaLCGFloat() * 0.5F + 0.5F, angleY - 1.570796F, angleZ / 3.F, offX, offZ, 1.0);
                gen_node_main(chunkX, chunkZ, out_ids, x, y, z, JavaLCGFloat() * 0.5F + 0.5F, angleY + 1.570796F, angleZ / 3.F, offX, offZ, 1.0);
                JavaLCGSetState(random_state); // Restore random state
                return;
            }
            if (!flag && JavaLCGIntN(4) == 0)
            {
                continue;
            }
            double distCenterX = x - chunkCenterX;
            double distCenterZ = z - chunkCenterZ;
            double axisDiff = offZ - offX;
            double maxTurn = angleX + 2.0F + 16.F;

            if ((distCenterX * distCenterX + distCenterZ * distCenterZ) - axisDiff * axisDiff > maxTurn * maxTurn)
            {
                JavaLCGSetState(random_state); // Restore random state
                return;
            }
            if (x < chunkCenterX - 16 - horizontalScale * 2 || z < chunkCenterZ - 16 - horizontalScale * 2 || x > chunkCenterX + 16 + horizontalScale * 2 || z > chunkCenterZ + 16 + horizontalScale * 2)
            {
                continue;
            }
            int32_t startX = std::floor(x - horizontalScale) - chunkX * 16 - 1;
            int32_t endX = (std::floor(x + horizontalScale) - chunkX * 16) + 1;
            int32_t startY = std::floor(y - size) - 1;
            int32_t endY = std::floor(y + size) + 1;
            int32_t startZ = std::floor(z - horizontalScale) - chunkZ * 16 - 1;
            int32_t endZ = (std::floor(z + horizontalScale) - chunkZ * 16) + 1;

            if (startX < 0)
            {
                startX = 0;
            }
            if (endX > 16)
            {
                endX = 16;
            }
            if (startY < 1)
            {
                startY = 1;
            }
            if (endY > 120)
            {
                endY = 120;
            }
            if (startZ < 0)
            {
                startZ = 0;
            }
            if (endZ > 16)
            {
                endZ = 16;
            }
            bool nearWater = false;

            for (int32_t waterX = startX; !nearWater && waterX < endX; waterX++)
            {
                for (int32_t waterZ = startZ; !nearWater && waterZ < endZ; waterZ++)
                {
                    for (int32_t waterY = endY + 1; !nearWater && waterY >= startY - 1; waterY--)
                    {
                        int32_t index = ((waterZ << 4) | waterX) | (waterY << 8);

                        if (waterY < 0 || waterY >= 128)
                        {
                            continue;
                        }
                        if (out_ids[index] == BlockID::flowing_water || out_ids[index] == BlockID::water)
                        {
                            nearWater = true;
                        }
                        if (waterY != startY - 1 && waterX != startX && waterX != endX - 1 && waterZ != startZ && waterZ != endZ - 1)
                        {
                            waterY = startY;
                        }
                    }
                }
            }

            if (nearWater)
            {
                continue;
            }
            for (int32_t carveX = startX; carveX < endX; carveX++)
            {
                double scaledX = (((double)(carveX + chunkX * 16) + 0.5) - x) / horizontalScale;

                for (int32_t carveZ = startZ; carveZ < endZ; carveZ++)
                {
                    double scaledZ = (((double)(carveZ + chunkZ * 16) + 0.5) - z) / horizontalScale;
                    int32_t index = ((carveZ << 4) | carveX) | (endY << 8);
                    bool hitGround = false;

                    for (int32_t carveY = endY - 1; carveY >= startY; carveY--)
                    {
                        double scaledY = (((double)carveY + 0.5) - y) / size;

                        if (scaledY > -0.7 && scaledX * scaledX + scaledY * scaledY + scaledZ * scaledZ < 1.0)
                        {
                            BlockID block_id = out_ids[index];

                            if (block_id == BlockID::grass)
                            {
                                hitGround = true;
                            }
                            if (block_id == BlockID::stone || block_id == BlockID::dirt || block_id == BlockID::grass)
                            {
                                if (carveY < 10)
                                {
                                    out_ids[index] = BlockID::lava;
                                }
                                else
                                {
                                    out_ids[index] = BlockID::air;
                                    if (hitGround && out_ids[index - 256] == BlockID::dirt)
                                    {
                                        out_ids[index - 256] = BlockID::grass;
                                    }
                                }
                            }
                        }
                        index -= 256;
                    }
                }
            }

            if (flag)
            {
                break;
            }
        }
        JavaLCGSetState(random_state); // Restore random state
    }

    inline static void gen_node(int32_t offX, int32_t offZ, int32_t chunkX, int32_t chunkZ, BlockID *out_ids)
    {
        int32_t branchCount = JavaLCGIntN(JavaLCGIntN(JavaLCGIntN(40) + 1) + 1);

        if (JavaLCGIntN(15) != 0)
        {
            branchCount = 0;
        }
        for (int32_t branch = 0; branch < branchCount; branch++)
        {
            double x = offX * 16 + JavaLCGIntN(16);
            double y = JavaLCGIntN(JavaLCGIntN(120) + 8);
            double z = offZ * 16 + JavaLCGIntN(16);
            int32_t endX = 1;

            if (JavaLCGIntN(4) == 0)
            {
                gen_node_simple(chunkX, chunkZ, out_ids, x, y, z);
                endX += JavaLCGIntN(4);
            }
            for (int32_t i = 0; i < endX; i++)
            {
                float angleX = JavaLCGFloat() * 3.141593F * 2.0F;
                float angleY = ((JavaLCGFloat() - 0.5F) * 2.0F) / 8.F;
                float angleZ = JavaLCGFloat() * 2.0F + JavaLCGFloat();

                gen_node_main(chunkX, chunkZ, out_ids, x, y, z, angleZ, angleX, angleY, 0, 0, 1.0);
            }
        }
    }
};
#endif