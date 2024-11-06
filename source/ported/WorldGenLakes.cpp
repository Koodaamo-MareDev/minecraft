#include "WorldGenLakes.hpp"
#include "../chunk_new.hpp"
namespace javaport
{
    bool WorldGenLakes::generate(Random &rng, vec3i pos)
    {
        chunk_t *chunk = get_chunk_from_pos(pos, false, false);
        if (!chunk)
            return false;

        while (pos.y > 0 && get_block_id_at(pos, BlockID::air) == BlockID::air)
            pos.y--;

        pos.y -= 4;

        bool placements[2048] = {0};

        int count = rng.nextInt(4) + 4;

        for (int i = 0; i < count; i++)
        {
            double center_x = rng.nextDouble() * 6.0 + 3.0;
            double center_z = rng.nextDouble() * 6.0 + 3.0;
            double center_y = rng.nextDouble() * 4.0 + 2.0;

            double size_x = rng.nextDouble() * (16.0 - center_x - 2.0) + 1.0 + center_x / 2.0;
            double size_z = rng.nextDouble() * (16.0 - center_z - 2.0) + 1.0 + center_z / 2.0;
            double size_y = rng.nextDouble() * (8.0 - center_y - 4.0) + 2.0 + center_y / 2.0;

            // Generate the lake shape
            for (int x = 1; x < 15; x++)
            {
                for (int z = 1; z < 15; z++)
                {
                    for (int y = 1; y < 7; y++)
                    {
                        double dx = (x - size_x) / (center_x / 2.0);
                        double dy = (y - size_y) / (center_y / 2.0);
                        double dz = (z - size_z) / (center_z / 2.0);
                        double dist = dx * dx + dy * dy + dz * dz;
                        if (dist < 1.0)
                            placements[(x * 16 + z) * 8 + y] = true;
                    }
                }
            }
        }

        // Fill the lake
        for (int x = 0; x < 16; x++)
        {
            for (int z = 0; z < 16; z++)
            {
                for (int y = 0; y < 8; y++)
                {
                    bool flag = !placements[(x * 16 + z) * 8 + y] && ((x < 15 && placements[((x + 1) * 16 + z) * 8 + y]) || (x > 0 && placements[((x - 1) * 16 + z) * 8 + y]) || (z < 15 && placements[(x * 16 + (z + 1)) * 8 + y]) || (z > 0 && placements[(x * 16 + (z - 1)) * 8 + y]) || (y < 7 && placements[(x * 16 + z) * 8 + (y + 1)]) || (y > 0 && placements[(x * 16 + z) * 8 + (y - 1)]));
                    if (flag)
                    {
                        BlockID id = get_block_id_at(pos + vec3i(x, y, z), BlockID::air);
                        if (y >= 4 && properties(id).m_fluid)
                            return false;
                        if (y < 4 && !properties(id).m_solid && id != liquid)
                            return false;
                    }
                }
            }
        }

        // Place the liquid
        for (int x = 0; x < 16; x++)
        {
            for (int z = 0; z < 16; z++)
            {
                for (int y = 0; y < 8; y++)
                {
                    if (placements[(x * 16 + z) * 8 + y])
                    {
                        set_block_at(pos + vec3i(x, y, z), y >= 4 ? BlockID::air : liquid);
                    }
                }
            }
        }

        // Coat the top layer with grass
        for (int x = 0; x < 16; x++)
        {
            for (int z = 0; z < 16; z++)
            {
                for (int y = 4; y < 8; y++)
                {
                    if (placements[(x * 16 + z) * 8 + y] && get_block_id_at(pos + vec3i(x, y - 1, z), BlockID::air) == BlockID::dirt)
                    {
                        set_block_at(pos + vec3i(x, y - 1, z), BlockID::grass);
                    }
                }
            }
        }

        // Place stone under the lake if it's lava
        if (liquid == BlockID::lava)
        {
            for (int x = 0; x < 16; x++)
            {
                for (int z = 0; z < 16; z++)
                {
                    for (int y = 0; y < 8; y++)
                    {
                        bool flag = !placements[(x * 16 + z) * 8 + y] && ((x < 15 && placements[((x + 1) * 16 + z) * 8 + y]) || (x > 0 && placements[((x - 1) * 16 + z) * 8 + y]) || (z < 15 && placements[(x * 16 + (z + 1)) * 8 + y]) || (z > 0 && placements[(x * 16 + (z - 1)) * 8 + y]) || (y < 7 && placements[(x * 16 + z) * 8 + (y + 1)]) || (y > 0 && placements[(x * 16 + z) * 8 + (y - 1)]));
                        if (flag && (y < 4 || rng.nextInt(2) == 0) && get_block_id_at(pos + vec3i(x, y, z), BlockID::air) == BlockID::air)
                        {
                            set_block_at(pos + vec3i(x, y, z), BlockID::stone);
                        }
                    }
                }
            }
        }

        return true;
    }
}