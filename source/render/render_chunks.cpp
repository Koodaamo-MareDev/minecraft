#include "render_chunks.hpp"

#include "render_blocks.hpp"
#include "render_fluids.hpp"
#include "render.hpp"

#include <stdexcept>
#include <util/lock.hpp>
#include <world/chunk.hpp>
#include <world/world.hpp>
#include <gertex/displaylist.hpp>

extern bool render_fast_leaves;

namespace ChunkRenderer
{
    void render_section(Section &section, bool transparent)
    {
        VBO &section_vbo = transparent ? section.transparent : section.solid;

        gertex::DisplayList<gertex::Vertex16> list(64000, VERTEX_ATTR_LENGTH);
        // Render the block mesh
        uint16_t quad_vertices = render_section_blocks(&list, section, transparent, 64000U);

        size_t tri_offset = list.size();

        if (tri_offset <= 3)
        {
            // No quads drawn
            tri_offset = 0;
            list.rewind();
        }

        // Render the fluid mesh
        uint16_t tri_vertices = render_section_fluids(&list, section, transparent, 64000U);

        if (list.size() <= 3)
        {
            // Displaylist is essentially empty, detach and clean it up later.
            Lock lock(render_mutex);
            section_vbo.detach();
            return;
        }

        // Fix quad count
        std::memcpy(&list.buffer[1], &quad_vertices, 2);

        // Fix triangle count (will override quad count automatically if no quads were drawn)
        std::memcpy(&list.buffer[tri_offset + 1], &tri_vertices, 2);

        size_t displist_size = list.aligned_size();
        uint8_t *displist_buf = list.build();

        // Invalidate any caches
        DCFlushRange(displist_buf, displist_size);

        // Apply the buffer to the VBO
        Lock lock(render_mutex);
        section_vbo.buffer = displist_buf;
        section_vbo.length = displist_size;
    }

    uint16_t render_section_blocks(gertex::DisplayList<gertex::Vertex16> *list, Section &section, bool transparent, uint16_t max_vertex_count)
    {
        list->max_vertices = max_vertex_count;
        list->begin(GX_QUADS);

        uint16_t vertex_count = 0;

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
                        vertex_count += render_block(list, block, blockpos);
                }
            }
        }
        return vertex_count;
    }

    uint16_t render_section_fluids(gertex::DisplayList<gertex::Vertex16> *list, Section &section, bool transparent, uint16_t max_vertex_count)
    {
        list->max_vertices = max_vertex_count;
        list->begin(GX_TRIANGLES);

        uint16_t vertex_count = 0;

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
                        vertex_count += render_fluid(list, block, blockpos, section.chunk->world);
                }
            }
        }
        return vertex_count;
    }

} // namespace ChunkRenderer
