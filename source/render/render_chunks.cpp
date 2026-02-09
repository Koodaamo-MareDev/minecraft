#include "render_chunks.hpp"

#include "render_blocks.hpp"
#include "render_fluids.hpp"

#include <stdexcept>
#include <util/lock.hpp>
#include <world/chunk.hpp>
#include <world/world.hpp>

extern bool render_fast_leaves;

namespace ChunkRenderer
{
    static uint8_t vbo_buffer[64000 * VERTEX_ATTR_LENGTH] __attribute__((aligned(32)));

    void render_section(Section &section, bool transparent)
    {
        assert((((uint32_t)vbo_buffer) & 31) == 0);

        VBO &section_vbo = transparent ? section.transparent : section.solid;

        DCInvalidateRange(vbo_buffer, sizeof(vbo_buffer));

        GX_BeginDispList(vbo_buffer, sizeof(vbo_buffer));

        // The first byte (GX_Begin command) is skipped, and after that the vertex count is stored as a uint16_t
        uint16_t *quad_vertices_ptr = (uint16_t *)(&vbo_buffer[1]);

        // Render the block mesh
        int quad_vertices = render_section_blocks(section, transparent, 64000U);

        // 3 bytes for the GX_Begin command, with 1 byte offset to access the vertex count (uint16_t)
        int offset = quad_vertices * VERTEX_ATTR_LENGTH + 3 + 1;

        // The vertex count for the fluid mesh is stored as a uint16_t after the block mesh vertex data
        uint16_t *tri_vertices_ptr = (uint16_t *)(&vbo_buffer[offset]);

        // Render the fluid mesh
        int tri_vertices = render_section_fluids(section, transparent, 64000U);

        // End the display list - we don't need to store the size, as we can calculate it from the vertex counts
        uint32_t success = GX_EndDispList();

        if (!success)
            throw std::length_error("Generated display list too large.");

        if (!quad_vertices && !tri_vertices)
        {
            // Will be cleaned up later by renderer.
            section_vbo.detach();
            return;
        }

        if (quad_vertices)
        {
            // Store the vertex count for the block mesh
            quad_vertices_ptr[0] = quad_vertices;
        }

        if (tri_vertices)
        {
            // Store the vertex count for the fluid mesh
            tri_vertices_ptr[0] = tri_vertices;
        }

        uint32_t displist_size = (tri_vertices + quad_vertices) * VERTEX_ATTR_LENGTH;
        if (quad_vertices)
            displist_size += 3;
        if (tri_vertices)
            displist_size += 3;

        // Add at least 32 bytes of padding to the final buffer size.
        displist_size = (displist_size + 32 + 31) & ~31;

        // Create the final buffer and copy the data into it
        uint8_t *displist_vbo = new (std::align_val_t(32)) uint8_t[displist_size];

        uint32_t pos = 0;
        // Copy the quad data
        if (quad_vertices)
        {
            uint32_t quad_len = quad_vertices * VERTEX_ATTR_LENGTH + 3;
            std::memcpy(displist_vbo, vbo_buffer, quad_len);
            pos += quad_len;
        }

        // Copy the triangle data
        if (tri_vertices)
        {
            uint32_t tri_len = tri_vertices * VERTEX_ATTR_LENGTH + 3;
            std::memcpy(&displist_vbo[pos], &vbo_buffer[quad_vertices * VERTEX_ATTR_LENGTH + 3], tri_len);
            pos += tri_len;
        }

        // Set the rest of the buffer to 0
        memset(&displist_vbo[pos], 0, displist_size - pos);

        // Invalidate any caches
        DCFlushRange(displist_vbo, displist_size);

        // Apply the buffer to the VBO
        section_vbo.buffer = displist_vbo;
        section_vbo.length = displist_size;
    }

    int render_section_blocks(Section &section, bool transparent, uint16_t max_vertex_count)
    {
        GX_BeginGroup(GX_QUADS, max_vertex_count);

        uint16_t expected_vertex_count = 0;

        // Build the mesh from the blockstates
        Vec3i section_offset = Vec3i(section.x, section.y, section.z);
        Block *block = &section.chunk->blockstates[section.y << 8];
        for (int _y = 0; _y < 16; _y++)
        {
            for (int _z = 0; _z < 16; _z++)
            {
                for (int _x = 0; _x < 16; _x++, block++)
                {
                    Vec3i blockpos = Vec3i(_x, _y, _z) + section_offset;
                    BlockProperties &props = properties(block->id);
                    if (transparent == props.m_transparent)
                        expected_vertex_count += render_block(block, blockpos);
                }
            }
        }
        uint16_t result_vertex_count = GX_EndGroup();
        if (result_vertex_count != expected_vertex_count)
        {
            throw std::runtime_error("Vertex count mismatch: " + std::to_string(expected_vertex_count) + " != " + std::to_string(result_vertex_count));
        }
        return expected_vertex_count;
    }

    int render_section_fluids(Section &section, bool transparent, uint16_t max_vertex_count)
    {
        GX_BeginGroup(GX_TRIANGLES, max_vertex_count);

        uint16_t expected_vertex_count = 0;

        // Build the mesh from the blockstates
        Vec3i chunk_offset = Vec3i(section.x, section.y, section.z);
        Block *block = &section.chunk->blockstates[section.y << 8];
        for (int _y = 0; _y < 16; _y++)
        {
            for (int _z = 0; _z < 16; _z++)
            {
                for (int _x = 0; _x < 16; _x++, block++)
                {
                    Vec3i blockpos = Vec3i(_x, _y, _z) + chunk_offset;
                    if (properties(block->id).m_fluid && transparent == properties(block->id).m_transparent)
                        expected_vertex_count += render_fluid(block, blockpos, section.chunk->world);
                }
            }
        }
        uint16_t result_vertex_count = GX_EndGroup();
        if (result_vertex_count != expected_vertex_count)
        {
            throw std::runtime_error("Vertex count mismatch: " + std::to_string(expected_vertex_count) + " != " + std::to_string(result_vertex_count));
        }
        return expected_vertex_count;
    }

} // namespace ChunkRenderer
