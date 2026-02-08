#ifndef RENDER_CHUNKS_HPP
#define RENDER_CHUNKS_HPP

#include <render/base3d.hpp>

class Section;

enum class RenderPass
{
    SOLID,
    TRANSPARENT
};

namespace ChunkRenderer
{
    void render_section(Section &section, bool transparent);
    int render_section_fluids(Section &section, bool transparent, uint16_t max_vertex_count);
    int render_section_blocks(Section &section, bool transparent, uint16_t max_vertex_count);
};

#endif