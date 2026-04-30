#ifndef RENDER_CHUNKS_HPP
#define RENDER_CHUNKS_HPP

#include <render/base3d.hpp>
#include <gertex/displaylist.hpp>

class Section;

enum class RenderPass
{
    SOLID,
    TRANSPARENT
};

namespace ChunkRenderer
{
    void render_section(Section &section, bool transparent);
    uint16_t render_section_fluids(gertex::DisplayList<gertex::Vertex16> *list, Section &section, bool transparent, uint16_t max_vertex_count);
    uint16_t render_section_blocks(gertex::DisplayList<gertex::Vertex16> *list, Section &section, bool transparent, uint16_t max_vertex_count);
};

#endif